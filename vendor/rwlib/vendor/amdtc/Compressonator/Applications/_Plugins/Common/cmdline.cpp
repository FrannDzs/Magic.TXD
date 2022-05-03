//=====================================================================
// Copyright 2016 (c), Advanced Micro Devices, Inc. All rights reserved.
//=====================================================================
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "Compressonator.h"
#include "Common.h"

#include "cmdline.h"
#include "ATIFormats.h"
#include "Texture.h"
#include "TextureIO.h"
#include "PluginManager.h"
#include "PluginInterface.h"
#include "TC_PluginInternal.h"
#include "Version.h"

#pragma warning( push )
#pragma warning(disable:4100)
#include <ImfStandardAttributes.h>
#include <ImathBox.h>
#include <ImfArray.h>
#include <ImfRgba.h>
#pragma warning( pop )

#include <boost/algorithm/string.hpp>

#ifdef USE_MESH_CLI
#include "gltf/tiny_gltf2.h"
#include "GltfCommon.h"
#include "ModelData.h"
#include "UtilFuncs.h"

using namespace tinygltf2;
#ifdef _DEBUG
#pragma comment(lib, "CMP_MeshCompressor_MDd.lib")
#else
#pragma comment(lib, "CMP_MeshCompressor_MD.lib")
#endif

#ifdef USE_MESHOPTIMIZER
#include "mesh_optimizer.h"
#else
#include "mesh_tootle.h"
#endif
#endif

#ifdef USE_CMP_TRANSCODE
#include "CMP_Transcoder\transcoders.h"
#endif



// #define SHOW_PROCESS_MEMORY

#define USE_SWIZZLE

#ifndef _WIN32
#include <stdarg.h>
#include <fcntl.h>  /* For O_RDWR */
#include <unistd.h> /* For open(), creat() */
#include <time.h>
#endif

#ifdef SHOW_PROCESS_MEMORY
#include "windows.h"
#include "psapi.h"
#endif

CCmdLineParamaters g_CmdPrams;

string DefaultDestination(string SourceFile, CMP_FORMAT DestFormat, string DestFileExt)
{
    string                  DestFile = "";
//#ifdef USE_BOOST
    boost::filesystem::path fp(SourceFile);
    string                  file_name = fp.stem().string();
    string                  file_ext  = fp.extension().string();
//#else
//    string                  file_name;
//    string                  file_ext;
//#endif
    DestFile.append(file_name);
    DestFile.append("_");
    file_ext.erase(std::remove(file_ext.begin(), file_ext.end(), '.'), file_ext.end());
    DestFile.append(file_ext);
    DestFile.append("_");
    DestFile.append(GetFormatDesc(DestFormat));

    if (DestFileExt.find('.') != std::string::npos) {
        DestFile.append(DestFileExt);
    }
	else {
        if (DestFormat == CMP_FORMAT_ASTC)
            DestFile.append(".astc");
        else
        {
            DestFile.append(".dds");
        }
    }

    return DestFile;
}

// check if file is glTF format extension
bool fileIsGLTF(string SourceFile)
{
//#ifdef USE_BOOST
    boost::filesystem::path fp(SourceFile);
    string                  file_ext = fp.extension().string();
    boost::algorithm::to_upper(file_ext);
//#else
//    string                  file_ext;
//#endif
    return (file_ext.compare(".GLTF") == 0);
}

// check if file is OBJ format extension
bool fileIsOBJ(string SourceFile)
{
//#ifdef USE_BOOST
    boost::filesystem::path fp(SourceFile);
    string                  file_ext = fp.extension().string();
    boost::algorithm::to_upper(file_ext);
//#else
//    string                  file_ext;
//#endif
    return (file_ext.compare(".OBJ") == 0);
}

// check if file is DRC (draco compressed OBJ file) format extension
bool fileIsDRC(string SourceFile)
{
//#ifdef USE_BOOST
    boost::filesystem::path fp(SourceFile);
    string                  file_ext = fp.extension().string();
    boost::algorithm::to_upper(file_ext);
//#else
//    string                  file_ext;
//#endif
    return (file_ext.compare(".DRC") == 0);
}

bool fileIsModel(string SourceFile)
{
    return (fileIsGLTF(SourceFile) || fileIsOBJ(SourceFile) || fileIsDRC(SourceFile));
}

#ifdef USE_WITH_COMMANDLINE_TOOL

int GetNumberOfCores(wchar_t* envp[])
{
    int i, cores = 1;

    for (i = 0; envp[i] != NULL; i++)
    {
        //wprintf(L"var = %ws\n", envp[i]);
        cores = wcsncmp(envp[i], L"NUMBER_OF_PROCESSORS", 20);
        if (cores == 0)
        {
            wchar_t* p_envp;
            size_t   equal = wcscspn(envp[i], L"=");
            if ((equal > 0) && (equal < wcslen(envp[i])))
            {
                p_envp = envp[i] + equal + 1;
                wchar_t num[16];
                wcsncpy_s(num, p_envp, 16);
                cores = _wtoi(num);
                return cores > 0 ? cores : 1;
            }
            break;
        }
    }
    return 1;
}

#else  // Code is shared with GUI
#endif

CMP_GPUDecode DecodeWith(const char* strParameter)
{
    if (strcmp(strParameter, "DirectX") == 0)
        return GPUDecode_DIRECTX;
    else if (strcmp(strParameter, "OGL") == 0)
        return GPUDecode_OPENGL;
    else if (strcmp(strParameter, "OpenGL") == 0)
        return GPUDecode_OPENGL;
    else if (strcmp(strParameter, "Vulkan") == 0)
        return GPUDecode_VULKAN;
    else
        return GPUDecode_INVALID;
}

CMP_Compute_type EncodeWith(const char* strParameter)
{
   if (strcmp(strParameter, "CPU") == 0)
        return CMP_HPC;
   else
   if (strcmp(strParameter, "HPC") == 0)
        return CMP_CPU;
#ifdef USE_GPUEncoders
   else if (strcmp(strParameter, "GPU") == 0)
       return CMP_GPU_OCL;
   else if (strcmp(strParameter, "OCL") == 0)
       return CMP_GPU_OCL;
   else if (strcmp(strParameter, "DXC") == 0)
       return CMP_GPU_DXC;
   else if (strcmp(strParameter, "VLK") == 0)
       return CMP_GPU_VLK;
#endif
   else
        return CMP_UNKNOWN;
}

bool ProcessSingleFlags(const char* strCommand)
{
    bool isset = false;
    if ((strcmp(strCommand, "-nomipmap") == 0))
    {
        g_CmdPrams.use_noMipMaps = true;
        isset                    = true;
    }
#ifdef USE_MESH_DRACO_EXTENSION
    else if ((strcmp(strCommand, "-draco") == 0))
    {
        g_CmdPrams.use_Draco_Encode = true;
        isset                       = true;
    }
#endif
    else if ((strcmp(strCommand, "-silent") == 0))
    {
        g_CmdPrams.silent = true;
        isset             = true;
    }
    else if ((strcmp(strCommand, "-performance") == 0))
    {
        g_CmdPrams.showperformance = true;
        isset                      = true;
    }
    else if ((strcmp(strCommand, "-noprogress") == 0))
    {
        g_CmdPrams.noprogressinfo = true;
        isset                     = true;
    }
    else if ((strcmp(strCommand, "-noswizzle") == 0))
    {
        g_CmdPrams.noswizzle = true;
        isset                = true;
    }
    else if ((strcmp(strCommand, "-doswizzle") == 0))
    {
        g_CmdPrams.doswizzle = true;
        isset                = true;
    }
    else if ((strcmp(strCommand, "-analysis") == 0) || (strcmp(strCommand, "-Analysis") == 0))
    {
        g_CmdPrams.analysis = true;
        isset               = true;
    }
    else if ((strcmp(strCommand, "-diff_image") == 0))
    {
        g_CmdPrams.diffImage = true;
        isset                = true;
    }
    else if ((strcmp(strCommand, "-imageprops") == 0))
    {
        g_CmdPrams.imageprops = true;
        isset                 = true;
    }
#ifdef USE_3DMESH_OPTIMIZE
    else if ((strcmp(strCommand, "-meshopt") == 0))
    {
        g_CmdPrams.doMeshOptimize = true;
        isset                     = true;
    }
#endif
    else if ((strcmp(strCommand, "-log") == 0))
    {
        g_CmdPrams.logresultsToFile  = true;
        g_CmdPrams.logresults        = true;
        isset                        = true;
    }
    else if (strcmp(strCommand, "-UseGPUDecompress") == 0)
    {
        g_CmdPrams.CompressOptions.bUseGPUDecompress = true;
        isset                                        = true;
    }
    return isset;
}

bool ProcessCMDLineOptions(const char* strCommand, const char* strParameter)
{
    try
    {
        // ====================================
        // Prechecks prior to sending to Codec
        // ====================================

        // Special Case for ASTC - improve this remove in next revision
        // This command param is passed down to ASTC Codex
        // We are doing an early capture to get some dimensions for ASTC file Save
        if (strcmp(strCommand, "-BlockRate") == 0)
        {
            if (strchr(strParameter, 'x') != NULL)
            {
                int dimensions = sscanf(strParameter, "%dx%dx", &g_CmdPrams.BlockWidth, &g_CmdPrams.BlockHeight);
                if (dimensions < 2)
                    throw "Command Parameter is invalid";
                else
                {
                    astc_find_closest_blockxy_2d(&g_CmdPrams.BlockWidth, &g_CmdPrams.BlockHeight, 0);
                }
            }
            else
            {
                float m_target_bitrate = static_cast<float>(atof(strParameter));
                if (m_target_bitrate > 0)
                    astc_find_closest_blockdim_2d(m_target_bitrate, &g_CmdPrams.BlockWidth, &g_CmdPrams.BlockHeight, 0);
                else
                    throw "Command Parameter is invalid";
            }
        }
        else if (strcmp(strCommand, "-Quality") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No Quality value specified";
            }
            float value = std::stof(strParameter);
            if ((value < 0) || (value > 1.0))
            {
                throw "Quality value should be in range of 0 to 1.0";
            }
            g_CmdPrams.CompressOptions.fquality = value;
        }
#ifdef ENABLE_MAKE_COMPATIBLE_API
        else if (strcmp(strCommand, "-InExposure") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No input exposure value specified";
            }
            float value = std::stof(strParameter);
            if ((value < -10) || (value > 10))
            {
                throw "Input Exposure value should be in range of -10 to 10";
            }
            g_CmdPrams.CompressOptions.fInputExposure = value;
        }
        else if (strcmp(strCommand, "-InDefog") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No input defog value specified";
            }
            float value = std::stof(strParameter);
            if ((value < 0) || (value > 0.01))
            {
                throw "Input Defog value should be in range of 0.0000 to 0.0100";
            }
            g_CmdPrams.CompressOptions.fInputDefog = value;
        }
        else if (strcmp(strCommand, "-InKneeLow") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No input kneelow value specified";
            }
            float value = std::stof(strParameter);
            if ((value < -3) || (value > 3))
            {
                throw "Input Knee Low value should be in range of -3 to 3";
            }
            g_CmdPrams.CompressOptions.fInputKneeLow = value;
        }
        else if (strcmp(strCommand, "-InKneeHigh") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No input kneehigh value specified";
            }
            float value = std::stof(strParameter);
            if ((value < 3.5) || (value > 7.5))
            {
                throw "Input Knee High value should be in range of 3.5 to 7.5";
            }
            g_CmdPrams.CompressOptions.fInputKneeHigh = value;
        }
        else if (strcmp(strCommand, "-Gamma") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No gamma value specified";
            }
            float value = std::stof(strParameter);
            if ((value < 1.0) || (value > 2.6))
            {
                throw "Gamma supported is in range of 1.0 to 2.6";
            }
            g_CmdPrams.CompressOptions.fInputGamma = value;
        }
#endif
        else if (strcmp(strCommand, "-WeightR") == 0)
        {
            if (!g_CmdPrams.CompressOptions.bUseChannelWeighting)
            {
                throw "Please enable \"-UseChannelWeighting 1\" first before setting weight for color channel";
            }

            if (strlen(strParameter) == 0)
            {
                throw "No WeightR value specified";
            }
            float value = std::stof(strParameter);
            if ((value < 0) || (value > 1.0))
            {
                throw "WeightR value should be in range of 0 to 1.0";
            }
            g_CmdPrams.CompressOptions.fWeightingRed = value;
        }
        else if (strcmp(strCommand, "-WeightG") == 0)
        {
            if (!g_CmdPrams.CompressOptions.bUseChannelWeighting)
            {
                throw "Please enable \"-UseChannelWeighting 1\" first before setting weight for color channel";
            }
            if (strlen(strParameter) == 0)
            {
                throw "No WeightG value specified";
            }
            float value = std::stof(strParameter);
            if ((value < 0) || (value > 1.0))
            {
                throw "WeightG value should be in range of 0 to 1.0";
            }
            g_CmdPrams.CompressOptions.fWeightingGreen = value;
        }
        else if (strcmp(strCommand, "-WeightB") == 0)
        {
            if (!g_CmdPrams.CompressOptions.bUseChannelWeighting)
            {
                throw "Please enable \"-UseChannelWeighting 1\" first before setting weight for color channel";
            }
            if (strlen(strParameter) == 0)
            {
                throw "No WeightB value specified";
            }
            float value = std::stof(strParameter);
            if ((value < 0) || (value > 1.0))
            {
                throw "WeightB value should be in range of 0 to 1.0";
            }
            g_CmdPrams.CompressOptions.fWeightingBlue = value;
        }
        else if (strcmp(strCommand, "-UseChannelWeighting") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No UseChannelWeighting value specified (default is 0:off; 1:on)";
            }
            int value = std::stof(strParameter);
            if ((value < 0) || (value > 1))
            {
                throw "UseChannelWeighting value should be 1 or 0";
            }
            g_CmdPrams.CompressOptions.bUseChannelWeighting = bool(value);
        }
        else if (strcmp(strCommand, "-AlphaThreshold") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No Alpha threshold value specified";
            }
            int value = std::stof(strParameter);
            if ((value < 1) || (value > 255))
            {
                throw "Alpha threshold value should be in range of 1 to 255";
            }
            g_CmdPrams.CompressOptions.nAlphaThreshold = value;
        }
        else if (strcmp(strCommand, "-DXT1UseAlpha") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No DXT1UseAlpha value specified (default is 0:off; 1:on)";
            }
            int value = std::stof(strParameter);
            if ((value < 0) || (value > 1))
            {
                throw "DXT1UseAlpha value should be 1 or 0";
            }
            g_CmdPrams.CompressOptions.bDXT1UseAlpha   = bool(value);
            g_CmdPrams.CompressOptions.nAlphaThreshold = 128;  //default to 128
        }
        else if (strcmp(strCommand, "-DecodeWith") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No API specified (set either OpenGL or DirectX (Default is OpenGL).";
            }

            g_CmdPrams.CompressOptions.nGPUDecode = DecodeWith((char*)strParameter);

            if (g_CmdPrams.CompressOptions.nGPUDecode == GPUDecode_INVALID)
            {
                throw "Unknown API format specified.";
            }

            g_CmdPrams.CompressOptions.bUseGPUDecompress = true;
        }
#ifdef USE_3DMESH_OPTIMIZE
        else if (strcmp(strCommand, "-optVCacheSize") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No hardware vertices cache size specified.";
            }

            int value = std::stoi(strParameter);
            if (value < 1)
            {
                throw "Hardware vertices cache size must be a positive value.";
            }
            g_CmdPrams.CompressOptions.iVcacheSize = value;
        }
        else if (strcmp(strCommand, "-optVCacheFIFOSize") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No hardware vertices cache(FIFO replacement policy) size specified.";
            }

            int value = std::stoi(strParameter);
            if (value < 1)
            {
                throw "Hardware vertices cache(FIFO replacement policy) size should be > 1.";
            }
            g_CmdPrams.CompressOptions.iVcacheFIFOSize = value;
        }
        else if (strcmp(strCommand, "-optOverdrawACMRThres") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No ACMR threshold value specified for overdraw optimization.";
            }

            float value = std::stof(strParameter);
            if ((value < 1) || (value > 3))
            {
                throw "ACMR (Average Cache Miss Ratio) should be in the range of 1 - 3.";
            }
            g_CmdPrams.CompressOptions.fOverdrawACMR = value;
        }
        else if (strcmp(strCommand, "-simplifyMeshLOD") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No LOD (Level of Details) specified for mesh simplication.";
            }

            int value = std::stoi(strParameter);
            if (value < 1)
            {
                throw "LOD (Level of Details) for mesh simplication should be > 1.";
            }
            g_CmdPrams.CompressOptions.iSimplifyLOD = value;
        }
        else if (strcmp(strCommand, "-optVFetch") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No boolean value (1 or 0) specified to enable vertex fetch optimization or not.";
            }

            int value = std::stof(strParameter);
            if ((value < 0) || (value > 1))
            {
                throw "Optimize vertex fetch value should be 1(enabled) or 0(disabled).";
            }
            g_CmdPrams.CompressOptions.bVertexFetch = bool(value);
        }
#endif
        else if (strcmp(strCommand, "-EncodeWith") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "No API specified.";
            }

            g_CmdPrams.CompressOptions.nEncodeWith = EncodeWith((char*)strParameter);

            if (g_CmdPrams.CompressOptions.nEncodeWith == CMP_Compute_type::CMP_UNKNOWN)
            {
                throw "Unknown API format specified.";
            }

            g_CmdPrams.CompressOptions.bUseCGCompress = true;
        }
        else if (strcmp(strCommand, "-decomp") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "no destination file specified";
            }
            g_CmdPrams.DecompressFile = (char*)strParameter;
            g_CmdPrams.doDecompress   = true;
        }
#ifdef USE_MESH_DRACO_EXTENSION
#ifdef USE_MESH_DRACO_SETTING
        else if (strcmp(strCommand, "-dracolvl") == 0)  //draco compression level
        {
            if (strlen(strParameter) == 0)
            {
                throw "No draco compression level specified";
            }
            int value = std::stoi(strParameter);
            if ((value < 0) || (value > 10))
            {
                throw "draco compression level supported is in range of 0 to 10";
            }
            g_CmdPrams.CompressOptions.iCmpLevel = value;
        }
        else if (strcmp(strCommand, "-qpos") == 0)  //quantization bit for positon
        {
            if (strlen(strParameter) == 0)
            {
                throw "No quantization bits for position specified";
            }
            int value = std::stoi(strParameter);
            if ((value < 1) || (value > 30))
            {
                throw "quantization bits for position supported is in range of 1 to 30";
            }
            g_CmdPrams.CompressOptions.iPosBits = value;
        }
        else if (strcmp(strCommand, "-qtexc") == 0)  //quantization bit for texture coordinates
        {
            if (strlen(strParameter) == 0)
            {
                throw "No quantization bits for texture coordinates specified";
            }
            int value = std::stoi(strParameter);
            if ((value < 1) || (value > 30))
            {
                throw "quantization bits for texture coordinates supported is in range of 1 to 30";
            }
            g_CmdPrams.CompressOptions.iTexCBits = value;
        }
        else if (strcmp(strCommand, "-qnorm") == 0)  //quantization bit for normal
        {
            if (strlen(strParameter) == 0)
            {
                throw "No quantization bits for normal specified";
            }
            int value = std::stoi(strParameter);
            if ((value < 1) || (value > 30))
            {
                throw "quantization bits for normal supported is in range of 1 to 30";
            }
            g_CmdPrams.CompressOptions.iNormalBits = value;
        }
        else if (strcmp(strCommand, "-qgen") == 0)  //quantization bit for generic
        {
            if (strlen(strParameter) == 0)
            {
                throw "No quantization bits for generic (i.e. tangent) specified";
            }
            int value = std::stoi(strParameter);
            if ((value < 1) || (value > 30))
            {
                throw "quantization bits for generic (i.e. tangent) supported is in range of 1 to 30";
            }
            g_CmdPrams.CompressOptions.iGenericBits = value;
        }
#endif
#endif
        else if (strcmp(strCommand, "-logfile") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "no log file specified";
            }
            g_CmdPrams.logresults       = true;
            g_CmdPrams.logresultsToFile = true;
            g_CmdPrams.LogProcessResultsFile.assign((char*)strParameter);
        }
        else if (strcmp(strCommand, "-fs") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "no format specified";
            }

            g_CmdPrams.CompressOptions.SourceFormat = CMP_ParseFormat((char*)strParameter);

            if (g_CmdPrams.CompressOptions.SourceFormat == CMP_FORMAT_Unknown)
            {
                throw "unknown format specified";
            }
        }
        else if ((strcmp(strCommand, "-ff") == 0)  ||   // FileFilter used for collecting list of source files in a given source Dir
                 (strcmp(strCommand, "-fx") == 0))      //and FileOutExt used for file output extension at given output Dir
        {
            if (strlen(strParameter) == 0)
            {
                throw "no file filter specified";
            }

            std::string filterParameter = strParameter;
            std::transform(filterParameter.begin(), filterParameter.end(), filterParameter.begin(), ::toupper);

            string  supported_ExtListings = { "DDS,KTX,TGA,EXR,PNG,BMP,HDR,JPG,TIFF,PPM" };

            istringstream ff(filterParameter);
            string sff;
            int filter_num = 0;
            while (getline(ff, sff, ',')) {
                filter_num++;
                if (supported_ExtListings.find(sff) == std::string::npos)
                {
                    char err[128];
                    sprintf(err, "[%s] file filter is not supported", sff.c_str());
                    throw (err);
                }
                else if (filter_num > 1 && strcmp(strCommand, "-fx") == 0) {
                    char err[128];
                    sprintf(err, "Only one file extension for output file.");
                    throw (err);
                }
            }

            if (strcmp(strCommand, "-ff") == 0) {
                g_CmdPrams.FileFilter = filterParameter;
            }
            else if (strcmp(strCommand, "-fx") == 0) {
                std::transform(filterParameter.begin(), filterParameter.end(), filterParameter.begin(), ::tolower); //change to lower
                g_CmdPrams.FileOutExt = "."+filterParameter; //add dot
            }

        }
        else if (strcmp(strCommand, "-fd") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "no format specified";
            }

            g_CmdPrams.CompressOptions.DestFormat = CMP_ParseFormat((char*)strParameter);

            if (g_CmdPrams.CompressOptions.DestFormat == CMP_FORMAT_Unknown)
            {
                throw "unknown format specified";
            }
        }
        else if ((strcmp(strCommand, "-miplevels") == 0))
        {
            if (strlen(strParameter) == 0)
            {
                throw "no level is specified";
            }
//#ifdef USE_BOOST
            try
            {
                g_CmdPrams.MipsLevel = boost::lexical_cast<int>(strParameter);
            }
            catch (boost::bad_lexical_cast)
            {
                throw "conversion failed for miplevel value";
            }
//#else
//#endif
        }
        else if ((strcmp(strCommand, "-mipsize") == 0))
        {
            if (strlen(strParameter) == 0)
            {
                throw "no size is specified";
            }
//#ifdef USE_BOOST
            try
            {
                g_CmdPrams.nMinSize = boost::lexical_cast<int>(strParameter);
            }
            catch (boost::bad_lexical_cast)
            {
                throw "conversion failed for mipsize value";
            }
//#else
//#endif
            g_CmdPrams.MipsLevel = 2;
        }
        else if (strcmp(strCommand, "-r") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "no read codec framework is specified";
            }

            if (strcmp(strParameter, "ocv") == 0)
                g_CmdPrams.use_OCV = true;
            else
            {
                throw "unsupported codec framework is specified";
            }
        }
        else if (strcmp(strCommand, "-w") == 0)
        {
            if (strlen(strParameter) == 0)
            {
                throw "no read codec framework is specified";
            }

            if (strcmp(strParameter, "ocv") == 0)
                g_CmdPrams.use_OCV_out = true;
            else
            {
                throw "unsupported codec framework is specified";
            }
        }
        else if ((strcmp(strCommand, "-version") == 0) || (strcmp(strCommand, "-v") == 0))
        {
            printf("version %d.%d.%d\n", VERSION_MAJOR_MAJOR, VERSION_MAJOR_MINOR, VERSION_MINOR_MAJOR);
        }
        else
        {
            if ((strlen(strParameter) > 0) || (strCommand[0] == '-'))
            {
                // List of command the codecs use. 
                // A call back loop should be used command for all codecs that can validate the seetings 
                // For now we list all of the commands here to check. Prior to passing down to Codecs!
                if (
                (strcmp(strCommand, "-NumThreads") == 0) ||
                (strcmp(strCommand, "-Quality") == 0) ||
                (strcmp(strCommand, "-ModeMask") == 0) ||
                (strcmp(strCommand, "-PatternRec") == 0) ||
                (strcmp(strCommand, "-ColourRestrict") == 0) ||
                (strcmp(strCommand, "-AlphaRestrict") == 0) ||
                (strcmp(strCommand, "-AlphaThreshold") == 0) ||
                (strcmp(strCommand, "-ImageNeedsAlpha") == 0) ||
                (strcmp(strCommand, "-UseSSE2") == 0) ||
                (strcmp(strCommand, "-DXT1UseAlpha") == 0) ||
                (strcmp(strCommand, "-WeightR") == 0) ||
                (strcmp(strCommand, "-WeightG") == 0) ||
                (strcmp(strCommand, "-WeightB") == 0) ||
                (strcmp(strCommand, "-3DRefinement") == 0) ||
                (strcmp(strCommand, "-UseAdaptiveWeighting") == 0) ||
                (strcmp(strCommand, "-UseChannelWeighting") == 0) ||
                (strcmp(strCommand, "-RefinementSteps") == 0) ||
                (strcmp(strCommand, "-ForceFloatPath") == 0) ||
                (strcmp(strCommand, "-CompressionSpeed") == 0) ||
                (strcmp(strCommand, "-SwizzleChannels") == 0) ||
                (strcmp(strCommand, "-CompressionSpeed") == 0) ||
                (strcmp(strCommand, "-MultiThreading") == 0))
                {
                         // Reserved for future dev: command options passed down to codec levels
                         const char* str;
                         // strip leading - or --
                         str = strCommand;
                         if (strCommand[0] == '-')
                             str++;
                         if (strCommand[1] == '-')
                             str++;
                         
                         if (strlen(str) > AMD_MAX_CMD_STR)
                         {
                             throw "Command option is invalid";
                         }
                         
                         if (strlen(strParameter) > AMD_MAX_CMD_PARAM)
                         {
                             throw "Command Parameter is invalid";
                         }
                         
                         strcpy(g_CmdPrams.CompressOptions.CmdSet[g_CmdPrams.CompressOptions.NumCmds].strCommand, str);
                         strcpy(g_CmdPrams.CompressOptions.CmdSet[g_CmdPrams.CompressOptions.NumCmds].strParameter, strParameter);
                         
                         g_CmdPrams.CompressOptions.NumCmds++;
                }
                else
                    throw "Command option is invalid";
            }
            else
            {   // Flags or Source and destination files specified
                if ((g_CmdPrams.SourceFile.length() == 0) && (g_CmdPrams.SourceFileList.size() == 0))
                {
                    if (CMP_PathType(strCommand) == CMP_PATHTYPES::CMP_PATH_IS_FILE)
                        g_CmdPrams.SourceFile = strCommand;
                    else
                    {
                        const std::string directory(CMP_GetFullPath(strCommand));
                        CMP_GetDirList(directory,g_CmdPrams.SourceFileList,g_CmdPrams.FileFilter);
                        if (g_CmdPrams.SourceFileList.size() > 0)
                        {
                            // Set the first file in the new list to SourceFile and delete it from the list
                            g_CmdPrams.SourceFile = g_CmdPrams.SourceFileList[0].c_str();
                            g_CmdPrams.SourceFileList.erase(g_CmdPrams.SourceFileList.begin());
                        }
                        else
                             throw "No files to process in source dir";

                    }
                }
                else // now check for destination once sourcefile or filelist has been added
                if ((g_CmdPrams.DestFile.length() == 0) && (g_CmdPrams.SourceFile.length() || g_CmdPrams.SourceFileList.size()))
                {
                    if (CMP_PathType(strCommand) == CMP_PATHTYPES::CMP_PATH_IS_FILE)
                    {
                        g_CmdPrams.DestFile   = strCommand;
//#ifdef USE_BOOST
                        string file_extension = boost::filesystem::extension(strCommand);
//#else
//                        string file_extension;
//#endif
                        // User did not supply a destination extension default
                        if (file_extension.length() == 0)
                        {
                            if (g_CmdPrams.DestFile.length() == 0)
                            {
                                g_CmdPrams.DestFile = DefaultDestination(g_CmdPrams.SourceFile, g_CmdPrams.CompressOptions.DestFormat, g_CmdPrams.FileOutExt);
                                PrintInfo("Destination texture file was not supplied: Defaulting to %s\n", g_CmdPrams.DestFile.c_str());
                            }
                            else
                            {
                                if (g_CmdPrams.CompressOptions.DestFormat == CMP_FORMAT_ASTC)
                                    g_CmdPrams.DestFile.append(".astc");
                                else
                                    g_CmdPrams.DestFile.append(".dds");
                            }
                        }
                    }
                    else
                    {
                        const std::string fullPath = CMP_GetFullPath(strCommand);

                        if (fullPath.length() > 0)
                        {
                            const std::string directory(CMP_GetFullPath(strCommand));
                            // check if dir exist if not create it!
                            if (!CMP_DirExists(directory))
                            {
                               if (!CMP_CreateDir(directory))
                                   throw "Unable to create destination dir";
                               // check and wait for system to generate a valid dir,
                               // typically this should not happen on local a dir
                               int delayloop = 0;
                               while (!CMP_DirExists(directory) && (delayloop < 5))
                               {
                                   #ifdef _WIN32
                                   Sleep(100);
                                   #else
                                   usleep(100000);
                                   #endif
                                   delayloop++;
                               }
                               if (delayloop == 5)
                                   throw "Unable to create destination dir";
                            }
                            g_CmdPrams.DestDir = directory;
                            std::string destFileName;
                            //since  DestFile is empty we need to create one from the source file
                            destFileName = DefaultDestination(g_CmdPrams.SourceFile, g_CmdPrams.CompressOptions.DestFormat, g_CmdPrams.FileOutExt);
                            g_CmdPrams.DestFile = directory + "\\" + destFileName;
                        }
                        else
                        {
                            throw "Unable to create destination dir or file path is invalid";
                        }
                    }
                }
                else
                {
                    throw "unknown source, destination file or dir path specified";
                }
            }
        }
    }  // Try code

    catch (const char* str)
    {
        PrintInfo("Option [%s] : %s\n\n", strCommand, str);
        return false;
    }
    catch (std::exception &e)
    {
        PrintInfo("Option [%s] : %s\n\n", strCommand, e.what());
        return false;
    }

    return true;
}

/*
 This function can be called using one of these examples:
 from a main(), it will parse the parmameters and act on them.

    main(int argc,  char* argv[])
    ParseParams(argc,argv);
 
 or from code 
 
     char *argv[5];
     argv[0]="foo.exe";
     argv[1]="-x";
     argv[2]="myfile";
     argv[3]="-f";
     argv[4]="myflag";
     ParseParams(5,argv);
*/

bool ParseParams(int argc, CMP_CHAR* argv[])
{
    try
    {
        g_CmdPrams.SetDefault();

#ifdef USE_WITH_COMMANDLINE_TOOL1
        if (argc == 1)
        {
            PrintUsage();
        }
#endif

        std::string strCommand;
        std::string strParameter;
        std::string strTemp;

        for (int i = 1; i < argc; i++)
        {
            strTemp = argv[i];

            //
            // Cmd line options can be -option value
            //
            strCommand = strTemp;
            if ((strTemp[0] == '-') && (i < (argc - 1)))
            {
                if (!ProcessSingleFlags(strCommand.c_str()))
                {
                    i++;
                    strParameter = argv[i];
                    if (!ProcessCMDLineOptions(strCommand.c_str(), strParameter.c_str()))
                    {
                        throw "Invalid Command";
                    }
                }
            }
            else
            {
                if (!ProcessCMDLineOptions(strCommand.c_str(), ""))
                {
                    throw "Invalid Command";
                }
            }

        }  // for loop
    }
    catch (const char* str)
    {
        PrintInfo("%s\n", str);
        return false;
    }

    return true;
}

bool SouceAndDestCompatible(CCmdLineParamaters CmdPrams)
{
    return true;
}

/*
class MyCMIPS : CMIPS
{
   public:
    void PrintInfo(const char* Format, ...);
};

void MyCMIPS::PrintInfo(const char* Format, ...)
{
    char buff[128];
    // define a pointer to save argument list
    va_list args;
    // process the arguments into our debug buffer
    va_start(args, Format);
    vsprintf(buff, Format, args);
    va_end(args);

    printf(buff);
}
*/

extern PluginManager g_pluginManager;  // Global plugin manager instance
extern bool          g_bAbortCompression;
extern CMIPS*        g_CMIPS;  // Global MIPS functions shared between app and all IMAGE plugins

MipSet g_MipSetIn;
MipSet g_MipSetCmp;
MipSet g_MipSetOut;
int    g_MipLevel  = 1;
float  g_fProgress = -1;

bool CompressionCallback(float fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
{
    if (g_fProgress != fProgress)
    {
        UNREFERENCED_PARAMETER(pUser1);
        UNREFERENCED_PARAMETER(pUser2);

        if ((g_CmdPrams.noprogressinfo) || (g_CmdPrams.silent))
            return g_bAbortCompression;

        if (g_MipLevel > 1)
            PrintInfo("\rProcessing progress = %2.0f  MipLevel = %2d", fProgress, g_MipLevel);
        else
            PrintInfo("\rProcessing progress = %2.0f", fProgress);

        g_fProgress = fProgress;
    }

    return g_bAbortCompression;
}

int CalcMinMipSize(int nHeight, int nWidth, int MipsLevel)
{
    while (MipsLevel > 0)
    {
        nWidth  = max(nWidth >> 1, 1);
        nHeight = max(nHeight >> 1, 1);
        MipsLevel--;
    }

    if (nWidth > nHeight)
        return (nHeight);
    return (nWidth);
}

bool TC_PluginCodecSupportsFormat(const MipSet* pMipSet)
{
    return (pMipSet->m_ChannelFormat == CF_8bit || pMipSet->m_ChannelFormat == CF_16bit || pMipSet->m_ChannelFormat == CF_2101010 ||
            pMipSet->m_ChannelFormat == CF_32bit || pMipSet->m_ChannelFormat == CF_Float16 || pMipSet->m_ChannelFormat == CF_Float32);
}

void cleanup(bool Delete_gMipSetIn, bool SwizzleMipSetIn)
{
#ifdef _WIN32
    SetDllDirectory(NULL);
#endif
    if (Delete_gMipSetIn)
    {
        if (g_MipSetIn.m_pMipLevelTable)
        {
            g_CMIPS->FreeMipSet(&g_MipSetIn);
            g_MipSetIn.m_pMipLevelTable = NULL;
        }
    }
    else
    {
        if (SwizzleMipSetIn)
        {
            SwizzleMipMap(&g_MipSetIn);
        }
    }

    if (g_MipSetCmp.m_pMipLevelTable)
    {
        g_CMIPS->FreeMipSet(&g_MipSetCmp);
        g_MipSetCmp.m_pMipLevelTable = NULL;
    }

    if (g_MipSetOut.m_pMipLevelTable)
    {
        g_CMIPS->FreeMipSet(&g_MipSetOut);
        g_MipSetOut.m_pMipLevelTable = NULL;
    }
}

// mesh optimization process
// only support case glTF->glTF, case obj->obj
bool OptimizeMesh(std::string SourceFile, std::string DestFile)
{
    if (!(CMP_FileExists(SourceFile)))
    {
        PrintInfo("Error: Source Model Mesh File is not found.\n");
        return false;
    }

    void*       modelDataIn  = nullptr;
    void*       modelDataOut = nullptr;
    GLTFCommon* gltfdata     = nullptr;

    // load model
//#ifdef USE_BOOST
    string file_extension = boost::filesystem::extension(SourceFile);
    boost::algorithm::to_upper(file_extension);
    boost::erase_all(file_extension, ".");
//#else
//    string file_extension;
//#endif
    PluginInterface_3DModel_Loader* plugin_loader;
    plugin_loader = reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", (char*)file_extension.c_str()));

    if (plugin_loader)
    {
        plugin_loader->TC_PluginSetSharedIO(g_CMIPS);
        void* msgHandler = NULL;

        int result;
        if (result = plugin_loader->LoadModelData(g_CmdPrams.SourceFile.c_str(), "", &g_pluginManager, msgHandler, &CompressionCallback) != 0)
        {
            if (result != 0)
                throw("Error Loading Model Data");
        }
        if (fileIsGLTF(g_CmdPrams.SourceFile))
        {
            gltfdata = (GLTFCommon*)plugin_loader->GetModelData();
            if (gltfdata)
            {
                if (gltfdata->m_meshBufferData.m_meshData[0].vertices.size() > 0)
                    modelDataIn = (void*)&(gltfdata->m_meshBufferData);
                else
                {
                    modelDataIn = nullptr;
                    PrintInfo("[Mesh Optimization] Error in processing mesh. Mesh data format size is not supported.");
                    return false;
                }
            }
        }
        else
            modelDataIn = plugin_loader->GetModelData();
    }
    else
    {
        PrintInfo("[Mesh Optimization] Loading file error.: %s\n.", SourceFile.c_str());
        if (plugin_loader)
        {
            delete plugin_loader;
            plugin_loader = nullptr;
        }
        return false;
    }

#ifdef USE_3DMESH_OPTIMIZE 
    PrintInfo("Processing: Mesh Optimization...\n");
    // perform mesh optimization
    PluginInterface_Mesh* plugin_Mesh = NULL;

    plugin_Mesh = reinterpret_cast<PluginInterface_Mesh*>(g_pluginManager.GetPlugin("MESH_OPTIMIZER", "TOOTLE_MESH"));

    if (plugin_Mesh)
    {
        if (plugin_Mesh->Init() == 0)
        {
            plugin_Mesh->TC_PluginSetSharedIO(g_CMIPS);
            MeshSettings meshSettings;
            meshSettings.bOptimizeOverdraw = (g_CmdPrams.CompressOptions.fOverdrawACMR > 0);
            if (meshSettings.bOptimizeOverdraw)
                meshSettings.nOverdrawACMRthreshold = (float)g_CmdPrams.CompressOptions.fOverdrawACMR;
            meshSettings.bOptimizeVCache = (g_CmdPrams.CompressOptions.iVcacheSize > 0);
            if (meshSettings.bOptimizeVCache)
                meshSettings.nCacheSize = g_CmdPrams.CompressOptions.iVcacheSize;
            meshSettings.bOptimizeVCacheFifo = (g_CmdPrams.CompressOptions.iVcacheFIFOSize > 0);
            if (meshSettings.bOptimizeVCacheFifo)
                meshSettings.nCacheSize = g_CmdPrams.CompressOptions.iVcacheFIFOSize;
            meshSettings.bOptimizeVFetch = g_CmdPrams.CompressOptions.bVertexFetch;
            meshSettings.bRandomizeMesh  = false;
            meshSettings.bSimplifyMesh   = (g_CmdPrams.CompressOptions.iSimplifyLOD > 0);
            if (meshSettings.bSimplifyMesh)
                meshSettings.nlevelofDetails = g_CmdPrams.CompressOptions.iSimplifyLOD;

            try
            {
                modelDataOut = plugin_Mesh->ProcessMesh(modelDataIn, (void*)&meshSettings, NULL, &CompressionCallback);
            }
            catch (std::exception& e)
            {
                PrintInfo("[Mesh Optimization] Error: %s\n.", e.what());
                if (plugin_Mesh)
                    plugin_Mesh->CleanUp();
                return false;
            }
        }
    }

    //save model
    if (modelDataOut)
    {
        std::vector<CMP_Mesh>*          optimized          = ((std::vector<CMP_Mesh>*)modelDataOut);
        PluginInterface_3DModel_Loader* plugin_save        = NULL;
//#ifdef USE_BOOST
        string                          destfile_extension = boost::filesystem::extension(DestFile);
        boost::algorithm::to_upper(destfile_extension);
        boost::erase_all(destfile_extension, ".");
//#else
//        string                          destfile_extension;
//#endif
        plugin_save =
            reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", (char*)destfile_extension.c_str()));
        if (plugin_save)
        {
            plugin_save->TC_PluginSetSharedIO(g_CMIPS);

            int result = 0;
            if (fileIsGLTF(g_CmdPrams.DestFile))
            {
                if (gltfdata)
                {
                    GLTFCommon optimizedGltf;
                    optimizedGltf.buffersData                 = gltfdata->buffersData;
                    optimizedGltf.isBinFile                   = gltfdata->isBinFile;
                    optimizedGltf.j3                          = gltfdata->j3;
                    optimizedGltf.m_CommonLoadTime            = gltfdata->m_CommonLoadTime;
                    optimizedGltf.m_distance                  = gltfdata->m_distance;
                    optimizedGltf.m_filename                  = gltfdata->m_filename;
                    optimizedGltf.m_meshes                    = gltfdata->m_meshes;
                    optimizedGltf.m_path                      = gltfdata->m_path;
                    optimizedGltf.m_scenes                    = gltfdata->m_scenes;
                    optimizedGltf.m_meshBufferData.m_meshData = *optimized;

                    if (plugin_save->SaveModelData(g_CmdPrams.DestFile.c_str(), &optimizedGltf) == -1)
                    {
                        PrintInfo("[Mesh Optimization] Failed to save optimized gltf data.\n");
                        plugin_Mesh->CleanUp();
                        if (plugin_save)
                        {
                            delete plugin_save;
                            plugin_save = nullptr;
                        }
                        return false;
                    }
                }
                else
                {
                    PrintInfo("[Mesh Optimization] Failed to save optimized gltf data.\n");
                    plugin_Mesh->CleanUp();
                    if (plugin_save)
                    {
                        delete plugin_save;
                        plugin_save = nullptr;
                    }
                    return false;
                }
            }
            else
            {
                if (plugin_save->SaveModelData(g_CmdPrams.DestFile.c_str(), &((*optimized)[0])) != -1)
                {
                    PrintInfo("[Mesh Optimization] Success in saving optimized obj data.\n");
                }
                else
                {
                    PrintInfo("[Mesh Optimization] Failed to save optimized obj data.\n");
                    plugin_Mesh->CleanUp();
                    if (plugin_save)
                    {
                        delete plugin_save;
                        plugin_save = nullptr;
                    }
                    return false;
                }
            }

            if (result != 0)
            {
                PrintInfo("[Mesh Optimization] Error in saving mesh file.");
                plugin_Mesh->CleanUp();
                if (plugin_save)
                {
                    delete plugin_save;
                    plugin_save = nullptr;
                }
                return false;
            }
            else
            {
                PrintInfo("[Mesh Optimization] Saving %s done.\n", g_CmdPrams.DestFile.c_str());
            }
        }
        else
        {
            PrintInfo("[Mesh Optimization]Saving: File format not supported.\n");
            plugin_Mesh->CleanUp();
            if (plugin_save)
            {
                delete plugin_save;
                plugin_save = nullptr;
            }
            return false;
        }

        plugin_Mesh->CleanUp();
        if (plugin_save)
        {
            delete plugin_save;
            plugin_save = nullptr;
        }
    }
#endif
    return true;
}

// mesh draco compression/decompression
bool CompressDecompressMesh(std::string SourceFile, std::string DestFile)
{
    if (!(CMP_FileExists(SourceFile)))
    {
        PrintInfo("Error: Source Model Mesh File is not found.\n");
        return false;
    }

    // Case: glTF -> glTF handle both compression and decompression
    if (fileIsGLTF(SourceFile))
    {
        if (fileIsGLTF(DestFile))
        {
            std::string err;
            Model       model;
            TinyGLTF    loader;
            TinyGLTF    saver;

            //clean up draco mesh buffer
#ifdef USE_MESH_DRACO_EXTENSION
            model.dracomeshes.clear();
#endif
            std::string srcFile = SourceFile;
            std::string dstFile = DestFile;
            // Check if mesh optimization was done if so then source is optimized file
            if (g_CmdPrams.doMeshOptimize)
            {
                srcFile = DestFile;
                if (!CMP_FileExists(srcFile))
                {
                    PrintInfo("Error: Source Model Mesh File is not found.\n");
                    return false;
                }
                std::size_t dotPos = srcFile.rfind('.');
                dstFile            = srcFile.substr(0, dotPos) + "_drc.glTF";
            }

            bool ret = loader.LoadASCIIFromFile(&model, &err, srcFile, g_CmdPrams.use_Draco_Encode);
            if (!err.empty())
            {
                PrintInfo("Error processing gltf source:[%s] file [%s]\n", srcFile.c_str(), err.c_str());
                return false;
            }
            if (!ret)
            {
                PrintInfo("Failed in loading glTF file : [%s].\n", srcFile.c_str());
                return false;
            }
            else
            {
                PrintInfo("Success in loading glTF file : [%s].\n", srcFile.c_str());
            }

            bool is_draco_src = false;
#ifdef USE_MESH_DRACO_EXTENSION
            if (model.dracomeshes.size() > 0)
            {
                is_draco_src = true;
            }
#endif
            err.clear();

            ret = saver.WriteGltfSceneToFile(&model, &err, dstFile, g_CmdPrams.CompressOptions, is_draco_src, g_CmdPrams.use_Draco_Encode);

            if (!err.empty())
            {
                PrintInfo("Error processing gltf destination:[%s] file [%s]\n", dstFile.c_str(), err.c_str());
                return false;
            }
            if (!ret)
            {
                PrintInfo("Failed to save glTF file %s\n", dstFile.c_str());
                return false;
            }
            else
            {
                PrintInfo("Success in writting glTF file : [%s].\n", dstFile.c_str());
            }
        }
        else
        {
            PrintInfo("Destination file must also be a gltf file\n");
            return false;
        }
        return true;
    }

    // Case: obj -> drc compression
    // Case: drc -> obj decompression
    //PluginInterface_3DModel_Loader* m_plugin_loader_drc = NULL;

    PluginInterface_Mesh* plugin_MeshComp;
    plugin_MeshComp = reinterpret_cast<PluginInterface_Mesh*>(g_pluginManager.GetPlugin("MESH_COMPRESSOR", "DRACO"));

    if (plugin_MeshComp)
    {
        if (plugin_MeshComp->Init() == 0)
        {
            //showProgressDialog("Process Mesh Data");
            plugin_MeshComp->TC_PluginSetSharedIO(g_CMIPS);

            CMP_DracoOptions DracoOptions;
            DracoOptions.is_point_cloud               = false;
            DracoOptions.use_metadata                 = false;
            DracoOptions.compression_level            = g_CmdPrams.CompressOptions.iCmpLevel;
            DracoOptions.pos_quantization_bits        = g_CmdPrams.CompressOptions.iPosBits;
            DracoOptions.tex_coords_quantization_bits = g_CmdPrams.CompressOptions.iTexCBits;
            DracoOptions.normals_quantization_bits    = g_CmdPrams.CompressOptions.iNormalBits;
            DracoOptions.generic_quantization_bits    = g_CmdPrams.CompressOptions.iGenericBits;

            // Check if mesh optimization was done if so then source is optimized file
            if (g_CmdPrams.doMeshOptimize)
            {
                DracoOptions.input = DestFile;
                if (fileIsOBJ(DracoOptions.input))
                    DracoOptions.output = DestFile + ".drc";
            }
            else
            {
                DracoOptions.input = SourceFile;
                //obj->obj
                if (fileIsOBJ(SourceFile) && fileIsOBJ(DestFile))
                {
                    //this obj->obj case only support for encode, a new encode.drc will be created
                    if (g_CmdPrams.use_Draco_Encode)
                        DracoOptions.output = DestFile + ".drc";
                    else
                    {
                        PrintInfo("Error: please use -draco option to perform encode on the source obj file.\n");
                        return false;
                    }
                }

                //drc->obj or obj->drc
                else if (fileIsDRC(SourceFile) || fileIsDRC(DestFile))
                    DracoOptions.output = DestFile;
            }

            PrintInfo("Processing: Mesh Compression/Decompression...\n");

            void* modelDataOut = nullptr;
            void* modelDataIn  = nullptr;

            PluginInterface_3DModel_Loader* m_plugin_loader_drc;
            m_plugin_loader_drc = reinterpret_cast<PluginInterface_3DModel_Loader*>(g_pluginManager.GetPlugin("3DMODEL_LOADER", "DRC"));

            if (m_plugin_loader_drc)
            {
                m_plugin_loader_drc->TC_PluginSetSharedIO(g_CMIPS);

                int result;
                if (fileIsOBJ(DracoOptions.input))
                {
                    if (result = m_plugin_loader_drc->LoadModelData("OBJ", NULL, &g_pluginManager, &DracoOptions , &CompressionCallback) != 0)
                    {
                        if (result != 0)
                        {
                            PrintInfo("[Mesh Compression] Error Loading Model Data.\n");
                            plugin_MeshComp->CleanUp();
                            return false;
                        }
                    }
                }
                else if (fileIsDRC(DracoOptions.input))
                {
                    if (result =
                            m_plugin_loader_drc->LoadModelData(DracoOptions.input.c_str(), NULL, &g_pluginManager, &DracoOptions, &CompressionCallback) != 0)
                    {
                        if (result != 0)
                        {
                            PrintInfo("[Mesh Compression] Error Loading Model Data.\n");
                            plugin_MeshComp->CleanUp();
                            return false;
                        }
                    }
                    plugin_MeshComp->CleanUp();
                    return true;
                }

                modelDataIn = m_plugin_loader_drc->GetModelData();

                try
                {
                    if (modelDataIn)
                        modelDataOut = plugin_MeshComp->ProcessMesh(modelDataIn, (void*)&DracoOptions, NULL, &CompressionCallback);
                }
                catch (std::exception& e)
                {
                    PrintInfo("[Mesh Compression] Error: %s . Please try another setting.\n", e.what());
                    plugin_MeshComp->CleanUp();
                    return false;
                }

                if (!modelDataOut)
                {
                    PrintInfo("[Mesh Compression] Error in processing mesh.\n");
                    plugin_MeshComp->CleanUp();
                    return false;
                }
            }
        }
        else
        {
            PrintInfo("[Mesh Compression] Error in init mesh plugin.\n");
            plugin_MeshComp->CleanUp();
            return false;
        }
    }
    else
    {
        PrintInfo("[Mesh Compression] Error in loading mesh compression plugin.\n");
        return false;
    }

    return true;
}

//cmdline only
bool GenerateAnalysis(std::string SourceFile, std::string DestFile)
{
    if (!(CMP_FileExists(SourceFile)))
    {
        PrintInfo("Error: Source Image File is not found.\n");
        return false;
    }

    if (!(CMP_FileExists(DestFile)))
    {
        PrintInfo("Error: Destination Image File is not found.\n");
        return false;
    }


    // Note this feature is also provided by -log option
    PluginInterface_Analysis* Plugin_Analysis;
    int                       testpassed = 0;
    Plugin_Analysis                      = reinterpret_cast<PluginInterface_Analysis*>(g_pluginManager.GetPlugin("IMAGE", "ANALYSIS"));
    if (Plugin_Analysis)
    {
        if (g_CmdPrams.diffImage)
        {
            g_CmdPrams.DiffFile = DestFile;
            int lastindex       = (int)g_CmdPrams.DiffFile.find_last_of(".");
            g_CmdPrams.DiffFile = g_CmdPrams.DiffFile.substr(0, lastindex);
            g_CmdPrams.DiffFile.append("_diff.bmp");
        }
        else
        {
            g_CmdPrams.DiffFile = "";
        }

        string results_file = "";
        if (g_CmdPrams.analysis)
        {
            results_file = DestFile;
            int index    = (int)results_file.find_last_of("/");
            results_file = results_file.substr(0, (index + 1));
            results_file.append("Analysis_Result.xml");
        }

        testpassed = Plugin_Analysis->TC_ImageDiff(SourceFile.c_str(),
                                                   DestFile.c_str(), 
                                                   g_CmdPrams.DiffFile.c_str(), 
                                                   (char*)results_file.c_str(),
                                                   NULL,
                                                   &g_pluginManager, 
                                                   NULL);
        delete Plugin_Analysis;
        if (testpassed != 0)
        {
            return false;
        }
    }
    else
    {
        printf("Error: Plugin for image analysis is not loaded\n");
        return false;
    }

    return true;
}

//cmdline only: print image properties (i.e. image name, path, file size, image size, image width, height, miplevel and format)
bool GenerateImageProps(std::string ImageFile)
{
    if (!(CMP_FileExists(ImageFile)))
    {
        PrintInfo("Error: Image File is not found.\n");
        return false;
    }

    // file name
    PrintInfo("File Name: %s\n", ImageFile.c_str());

    // full path
//#ifdef USE_BOOST
    boost::filesystem::path p(ImageFile);
    boost::filesystem::path fullpath = boost::filesystem::absolute(p);
    PrintInfo("File Full Path: %s\n", fullpath.generic_string().c_str());

    // file size
    uintmax_t filesize = boost::filesystem::file_size(ImageFile);

    if (filesize > 1024000)
    {
        filesize /= 1024000;
        PrintInfo("File Size: %ju MB\n", filesize);
    }
    else if (filesize > 1024)
    {
        filesize /= 1024;
        PrintInfo("File Size: %ju KB\n", filesize);
    }
    else
    {
        PrintInfo("File Size: %ju Bytes\n", filesize);
    }
//#else
//#endif

    // load image into mipset
    if (AMDLoadMIPSTextureImage(ImageFile.c_str(), &g_MipSetIn, g_CmdPrams.use_OCV, &g_pluginManager) != 0)
    {
        PrintInfo("Error: reading image, data type not supported.\n");
        return false;
    }

    if (&g_MipSetIn)
    {
        //image size

        CMIPS     CMips;
        MipLevel* pInMipLevel = CMips.GetMipLevel(&g_MipSetIn, 0, 0);
        uintmax_t imagesize   = pInMipLevel->m_dwLinearSize;

        if (imagesize > 1024000)
        {
            imagesize /= 1024000;
            PrintInfo("Image Size: %ju MB\n", imagesize);
        }
        else if (imagesize > 1024)
        {
            imagesize /= 1024;
            PrintInfo("Image Size: %ju KB\n", imagesize);
        }
        else
        {
            PrintInfo("Image Size: %ju Bytes\n", imagesize);
        }

        //width, height
        PrintInfo("Image Width: %u px\n", g_MipSetIn.m_nWidth);
        PrintInfo("Image Height: %u px\n", g_MipSetIn.m_nHeight);

        //miplevel, format
        int miplevel = g_MipSetIn.m_nMipLevels;

        PrintInfo("Mip Levels: %u\n", miplevel);


        if (g_MipSetIn.m_format != CMP_FORMAT_Unknown)
            PrintInfo("Format: %s\n", GetFormatDesc(g_MipSetIn.m_format));
        else
        {
            switch (g_MipSetIn.m_ChannelFormat)
            {
                case CF_8bit      :  //< 8-bit integer data.
                                   PrintInfo("Channel Format: 8-bit integer data\n");
                                   break;
                case CF_Float16   :  //< 16-bit float data.
                                   PrintInfo("Channel Format: 16-bit float data\n");
                                   break;
                case CF_Float32   :  //< 32-bit float data.
                                   PrintInfo("Channel Format: 32-bit float data\n");
                                   break;
                case CF_Compressed:  //< Compressed data.
                                   PrintInfo("Channel Format: Compressed data\n");
                                   break;
                case CF_16bit     :  //< 16-bit integer data.
                                   PrintInfo("Channel Format: 16-bit integer data\n");
                                   break;
                case CF_2101010   :  //< 10-bit integer data in the color channels & 2-bit integer data in the alpha channel.
                                   PrintInfo("Channel Format: 10-bit integer data in the color channels & 2-bit integer data in the alpha channel\n");
                                   break;
                case CF_32bit     :  //< 32-bit integer data.
                                   PrintInfo("Channel Format: 32-bit integer data\n");
                                   break;
                case CF_Float9995E:  //< 32-bit partial precision float.
                                   PrintInfo("Channel Format: 9995E 32-bit partial precision float\n");
                                   break;
                case CF_YUV_420   :  //< YUV Chroma formats 
                                   PrintInfo("Channel Format: YUV 420 Chroma formats\n");
                                   break;
                case CF_YUV_422   :  //< YUV Chroma formats
                                   PrintInfo("Channel Format: YUV 422 Chroma formats\n");
                                   break;
                case CF_YUV_444   :  //< YUV Chroma formats
                                   PrintInfo("Channel Format: YUV 444 Chroma formats\n");
                                   break;
                case CF_YUV_4444  :  //< YUV Chroma formats
                                   PrintInfo("Channel Format: YUV 4444 Chroma formats\n");
                                   break;
            };
        }
    }

    return true;
}

void LocalPrintF(char* buff)
{
#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-security"            // warning : warning: format string is not a string literal
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wformat-security"              // warning : format string is not a string literal (potentially insecure)
#endif
    printf(buff);
}

#include "Common_KernelDef.h"
#include "Compute_Base.h"


bool SVMInitCodec(KernelOptions* options)
{
    CMP_FORMAT format = options->format;
    switch (format)
    {
    case CMP_FORMAT_BC1:
    case CMP_FORMAT_DXT1:
    case CMP_FORMAT_BC7:
    case CMP_FORMAT_ASTC:
    {
        unsigned char* src = (unsigned char*)options->data;
        unsigned char* dst = (unsigned char*)options->dataSVM;
        memcpy(dst, src, options->size);
        return true;
    }
    break;
    }
    return false;
}

//#endif

double timeStampsec()
{
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    if (frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return now.QuadPart / double(frequency.QuadPart);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1000000000.0;
#endif
}

CMP_ERROR CMP_ConvertMipTextureCGP(MipSet* pMipSetIn, MipSet* p_MipSetOut, CMP_CompressOptions* pCompressOptions, CMP_Feedback_Proc pFeedbackProc)
{
    assert(pMipSetIn);
    assert(p_MipSetOut);
    assert(pCompressOptions);

    // -------------
    // Output
    // -------------
    memset(p_MipSetOut, 0, sizeof(CMP_MipSet));
    p_MipSetOut->m_Flags   = MS_FLAG_Default;
    p_MipSetOut->m_format  = pCompressOptions->DestFormat;
    p_MipSetOut->m_nHeight = pMipSetIn->m_nHeight;
    p_MipSetOut->m_nWidth  = pMipSetIn->m_nWidth;

    //=====================================================
    // Case Uncompressed Source to Compressed Destination
    //=====================================================
    // Allocate compression data
    p_MipSetOut->m_ChannelFormat    = CF_Compressed;
    p_MipSetOut->m_nMaxMipLevels    = pMipSetIn->m_nMaxMipLevels;
    p_MipSetOut->m_nMipLevels       = 1;  // this is overwriiten depending on input.
    p_MipSetOut->m_nBlockWidth      = 4;  // Update is required for other sizes.
    p_MipSetOut->m_nBlockHeight     = 4;  // - need to fix pMipSetIn m_nBlock settings for this to work
    p_MipSetOut->m_nDepth           = pMipSetIn->m_nDepth;
    p_MipSetOut->m_TextureType      = pMipSetIn->m_TextureType;

    if (!g_CMIPS->AllocateMipSet(p_MipSetOut, p_MipSetOut->m_ChannelFormat, TDT_ARGB,  p_MipSetOut->m_TextureType, pMipSetIn->m_nWidth, pMipSetIn->m_nHeight, p_MipSetOut->m_nDepth)) // depthsupport, what should nDepth be set as here?
    {
        return CMP_ERR_MEM_ALLOC_FOR_MIPSET;
    }

    CMP_Texture srcTexture;
    srcTexture.dwSize = sizeof(srcTexture);
    int DestMipLevel = pMipSetIn->m_nMipLevels;

    p_MipSetOut->m_nMipLevels = DestMipLevel;

    for (int nMipLevel = 0; nMipLevel < DestMipLevel; nMipLevel++)
    {
        for (int nFaceOrSlice = 0; nFaceOrSlice < CMP_MaxFacesOrSlices(pMipSetIn, nMipLevel); nFaceOrSlice++)
        {
            //=====================
            // Uncompressed source
            //======================
            MipLevel* pInMipLevel = g_CMIPS->GetMipLevel(pMipSetIn, nMipLevel, nFaceOrSlice);
            srcTexture.dwPitch      = 0;
            srcTexture.nBlockWidth  = pMipSetIn->m_nBlockWidth;
            srcTexture.nBlockHeight = pMipSetIn->m_nBlockHeight;
            srcTexture.nBlockDepth  = pMipSetIn->m_nBlockDepth;
            srcTexture.format       = pMipSetIn->m_format;
            srcTexture.dwWidth      = pInMipLevel->m_nWidth;
            srcTexture.dwHeight     = pInMipLevel->m_nHeight;
            srcTexture.pData        = pInMipLevel->m_pbData;
            srcTexture.dwDataSize   = CMP_CalculateBufferSize(&srcTexture);

            // Temporary setting!
            pMipSetIn->dwWidth     = pInMipLevel->m_nWidth;
            pMipSetIn->dwHeight    = pInMipLevel->m_nHeight;
            pMipSetIn->pData       = pInMipLevel->m_pbData;
            pMipSetIn->dwDataSize  = CMP_CalculateBufferSize(&srcTexture);

            //========================
            // Compressed Destination
            //========================
            CMP_Texture destTexture;
            destTexture.dwSize          = sizeof(destTexture);
            destTexture.dwWidth         = pInMipLevel->m_nWidth;
            destTexture.dwHeight        = pInMipLevel->m_nHeight;
            destTexture.dwPitch         = 0;
            destTexture.nBlockWidth     = pMipSetIn->m_nBlockWidth;
            destTexture.nBlockHeight    = pMipSetIn->m_nBlockHeight;
            destTexture.format          = pCompressOptions->DestFormat;
            destTexture.dwDataSize      = CMP_CalculateBufferSize(&destTexture);

            p_MipSetOut->m_format        = pCompressOptions->DestFormat;;
            p_MipSetOut->dwDataSize      = CMP_CalculateBufferSize(&destTexture);
            p_MipSetOut->dwWidth         = pInMipLevel->m_nWidth;
            p_MipSetOut->dwHeight        = pInMipLevel->m_nHeight;

            MipLevel* pOutMipLevel = g_CMIPS->GetMipLevel(p_MipSetOut, nMipLevel, nFaceOrSlice);
            if (!g_CMIPS->AllocateCompressedMipLevelData(pOutMipLevel, destTexture.dwWidth, destTexture.dwHeight, destTexture.dwDataSize))
            {
                return CMP_ERR_MEM_ALLOC_FOR_MIPSET;
            }

            destTexture.pData  = pOutMipLevel->m_pbData;
            p_MipSetOut->pData = pOutMipLevel->m_pbData;

            //========================
            // Process ConvertTexture
            //========================
            pCompressOptions->format_support_gpu = true;
            KernelOptions   kernel_options;
            memset(&kernel_options, 0, sizeof(KernelOptions));
            kernel_options.format       = pCompressOptions->DestFormat;
            kernel_options.encodeWith   = pCompressOptions->nEncodeWith;
            kernel_options.fquality     = pCompressOptions->fquality;
            kernel_options.threads      = pCompressOptions->dwnumThreads;

            //------------------------------------------------
            // Initializing the Host Framework
            // if it fails revert to CPU version of the codec
            //------------------------------------------------
            do
            {
                ComputeOptions options;
                options.force_rebuild = false;   // set this to true if you want the shader source code  to be allways compiled!

                //===============================================================================
                // Initalize the  Pipeline that will be used for the codec to run on HPC or GPU
                //===============================================================================
                if (CMP_CreateComputeLibrary(&g_MipSetIn, &kernel_options, g_CMIPS) != CMP_OK)
                {
                    PrintInfo("Warning! Failed to init HOST Lib. CPU will be used for compression\n");
                    pCompressOptions->format_support_gpu = false;
                    break;
                }

                // Init Compute Codec info IO
                if ((g_CMIPS->PrintLine == NULL) && (PrintStatusLine != NULL))
                {
                    g_CMIPS->PrintLine = PrintStatusLine;
                }

                // Set any addition feature as needed for the Host
                if (CMP_SetComputeOptions(&options) != CMP_OK)
                {
                    PrintInfo("Failed to setup (SPMD or GPU) host options\n");
                    return CMP_ERR_FAILED_HOST_SETUP;
                }

                // Do the compression 
                if (CMP_CompressTexture(&kernel_options, *pMipSetIn, *p_MipSetOut,pFeedbackProc) != CMP_OK)
                {
                    PrintInfo("Warning: GPU Shader is not supported or failed to build. CPU will be used\n");
                    pCompressOptions->format_support_gpu = false;
                    break;
                }

                //===============================================================================
                // Close the Pipeline with option to cache as needed
                //===============================================================================
                CMP_DestroyComputeLibrary(false);

            } while (0);

            // SPMD/GPU failed run CPU version
            if (!pCompressOptions->format_support_gpu)
            {
                if (CMP_ConvertTexture(&srcTexture, &destTexture, pCompressOptions, pFeedbackProc) != CMP_OK)
                {
                    PrintInfo("Error in compressing destination texture\n");
                    return CMP_ERR_CMP_DESTINATION;
                }
            }
        }
    }

    if (pFeedbackProc)
        pFeedbackProc(100, NULL, NULL);

    CMP_Format2FourCC(pCompressOptions->DestFormat,p_MipSetOut);

    return CMP_OK;
}

// ToDo replace with plugin scan, qt checks and src dest format checks.
bool SupportedFileTypes(std::string fileExt)
{
    char *supportedTypes[19] = {"DDS","KTX","BMP","PNG","JPEG","JPG","EXR","TGA","TIF","TIFF","OBJ","GLTF","PBM","PGM","PPM","XBM","XPM","ASTC","DRC"};
    for (int i=0; i<19; i++)
    {
        if (fileExt.compare(supportedTypes[i]) == 0) return true;
    }
    return false;
}

void PrintInfoStr(const char* InfoStr )
{
    PrintInfo(InfoStr);
}


int ProcessCMDLine(CMP_Feedback_Proc pFeedbackProc, MipSet* p_userMipSetIn)
{
    int processResult = 0;
    double frequency, conversion_loopStartTime = {0}, conversion_loopEndTime = {0}, compress_loopStartTime = {0}, compress_loopEndTime = {0},
        decompress_loopStartTime = {0}, decompress_loopEndTime = {0};
    double ssim_sum = 0.0;
    double psnr_sum = 0.0;
    double process_time_sum = 0.0;
    int    total_processed_items = 0;

    // These flags indicate if the source and destination files are compressed
    bool SourceFormatIsCompressed      = false;
    //bool DestinationFileIsCompressed   = false;
    bool DestinationFormatIsCompressed = false;

#ifdef USE_BASIS
    bool CMP_TranscodeFormat = false;
#endif
    bool TranscodeBits       = false;
    bool MidwayDecompress    = false;

    //  With a user suppiled Mip Map dont delete it on exit
    bool Delete_gMipSetIn = false;

    // flags if an user suppiled Mip Map input was swizzled during compression / decompression
    bool SwizzledMipSetIn = false;

    // Currently active input and output mips buffers
    // These point to the allocated g_MipSetxxx buffers
    // depending on the users requirement for input and output file formats
    MipSet* p_MipSetIn;
    MipSet* p_MipSetOut;

#ifdef SHOW_PROCESS_MEMORY
    PROCESS_MEMORY_COUNTERS memCounter1;
    PROCESS_MEMORY_COUNTERS memCounter2;

#ifdef SHOW_PROCESS_MEMORY
    bool result = GetProcessMemoryInfo(GetCurrentProcess(), &memCounter1, sizeof(memCounter1));
#endif

#endif

    bool MoreSourceFiles = false;
    PluginInterface_Analysis* Plugin_Analysis = NULL;

    if (g_CmdPrams.logresults)
    {
        // Check for vaild -log usage
         if ((!fileIsModel(g_CmdPrams.SourceFile)) && (!fileIsModel(g_CmdPrams.DestFile))) 
         {
            //int             testpassed = 0;
            Plugin_Analysis            = reinterpret_cast<PluginInterface_Analysis*>(g_pluginManager.GetPlugin("IMAGE", "ANALYSIS"));
         }
         else
         {
             PrintInfo("Warning: -log is only valid for Images, option is turned off!\n");
             g_CmdPrams.logresults = false;
         }
    }

    // Fix to output view to look the same as v3.1 print info for calls to CMP_ConvertMipTexture
    g_CmdPrams.CompressOptions.m_PrintInfoStr = PrintInfoStr;

    do
    {

    // Initailize stats data and defaults for repeated use in do while()!
    g_CmdPrams.compress_nIterations   = 0;
    g_CmdPrams.decompress_nIterations = 0;
    g_CmdPrams.CompressOptions.format_support_gpu = false;

    if ((!fileIsModel(g_CmdPrams.SourceFile)) && (!fileIsModel(g_CmdPrams.DestFile)))
    {
        // Check if print status line has been assigned
        // if not get it a default to printf
        if (PrintStatusLine == NULL)
        {
            PrintStatusLine = &LocalPrintF;
        }



        if (g_CmdPrams.analysis || g_CmdPrams.diffImage)
        {
            if (!(GenerateAnalysis(g_CmdPrams.SourceFile, g_CmdPrams.DestFile)))
            {
                PrintInfo("Error: Image Analysis Failed\n");
                return -1;
            }
            return 0;
        }

        if (g_CmdPrams.imageprops)
        {
            if (!(GenerateImageProps(g_CmdPrams.SourceFile)))
            {
                PrintInfo("Error: Failed to retrieve image properties\n");
                return -1;
            }
            return 0;
        }

        frequency = timeStampsec();

        // ---------
        // Input
        // ---------
        CMP_FORMAT srcFormat;
        memset(&g_MipSetIn, 0, sizeof(MipSet));

        //===========================
        // Set the destination format
        //===========================
        CMP_FORMAT destFormat;

        std::string FileName(g_CmdPrams.DestFile);
        std::string DestExt = CMP_GetFilePathExtension(FileName);
        std::transform(DestExt.begin(), DestExt.end(), DestExt.begin(), ::toupper);

        // Check if destination file is supported, else print warning and continue.
        if (!SupportedFileTypes(DestExt))
        {
            PrintInfo("Error: Destination file type not is supported\n");
            processResult = -2;
            continue;
        }


        // Try some known format to attach by supported file ext types
        if (g_CmdPrams.CompressOptions.DestFormat == CMP_FORMAT_Unknown)
        {
            if (DestExt.compare("ASTC") == 0)
                g_CmdPrams.CompressOptions.DestFormat = CMP_FORMAT_ASTC;
            else
#ifdef USE_BASIS
            if (DestExt.compare("BASIS") == 0)
                g_CmdPrams.CompressOptions.DestFormat = CMP_FORMAT_BASIS;
            else
#endif
            if (DestExt.compare("EXR") == 0)
                g_CmdPrams.CompressOptions.DestFormat = CMP_FORMAT_ARGB_16F;
            else
            if (DestExt.compare("BMP") == 0)
                g_CmdPrams.CompressOptions.DestFormat = CMP_FORMAT_ARGB_8888;
            else
            if (DestExt.compare("PNG") == 0)
                g_CmdPrams.CompressOptions.DestFormat = CMP_FORMAT_ARGB_8888;
        }

        destFormat = g_CmdPrams.CompressOptions.DestFormat;

        // Determin if destinationfile is to be Compressed
        DestinationFormatIsCompressed = CompressedFormat(destFormat);

        if (destFormat != CMP_FORMAT_ASTC)
        {
            // Check for valid format to destination for ASTC

            if (DestExt.compare("ASTC") == 0)
            {
                PrintInfo("Error: destination file type only supports ASTC compression format\n");
                return -1;
            }
        }
        else
        {
            // Check for valid format to destination for ASTC
            if (!((DestExt.compare("ASTC") == 0) || (DestExt.compare("KTX") == 0)))
            {
                PrintInfo("Error: destination file type for ASTC must be set to .astc or .ktx\n");
                return -1;
            }
        }


        //========================
        // Set the source format
        //========================
        //----------------
        // Read Input file
        //----------------
        if (p_userMipSetIn)  //for GUI
        {
            memcpy(&g_MipSetIn, p_userMipSetIn, sizeof(MipSet));
            // Data in DXTn Files are expected to be in BGRA as input to CMP_ConvertTexture
            // Data in ASTC BC6 BC7 etc - expect data to be RGBA as input to CMP_ConvertTexture
            //g_MipSetIn.m_swizzle        = KeepSwizzle(destFormat);
            g_MipSetIn.m_pMipLevelTable = p_userMipSetIn->m_pMipLevelTable;

            Delete_gMipSetIn = false;
        }
        else  //for CLI
        {
            Delete_gMipSetIn = true;

            // ===============================================
            // INPUT IMAGE Swizzling options for DXT formats
            // ===============================================
#ifdef USE_SWIZZLE
            //if (!g_CmdPrams.CompressOptions.bUseCGCompress)
            //    g_MipSetIn.m_swizzle = KeepSwizzle(destFormat);
#endif

            // Flag to image loader that data is going to be compressed
            // and that we want data to be in ARGB format
            // g_MipSetIn.m_swizzle = (bool)DestinationFormatIsCompressed;
            //
            //-----------------------
            // User swizzle overrides
            //-----------------------
            // Did User set no swizzle
            if (g_CmdPrams.noswizzle)
                g_MipSetIn.m_swizzle = false;

            // Did User set do swizzle
            if (g_CmdPrams.doswizzle)
                g_MipSetIn.m_swizzle = true;

            //---------------------------------------
            // Set user specification for Block sizes
            //----------------------------------------
            if (AMDLoadMIPSTextureImage(g_CmdPrams.SourceFile.c_str(), &g_MipSetIn, g_CmdPrams.use_OCV, &g_pluginManager) != 0)
            {
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                PrintInfo("Error: loading source image\n");
                return -1;
            }
        }

        //--------------------------------------------
        // Set user specification for ASTC Block sizes
        //---------------------------------------------
        g_MipSetIn.m_nBlockWidth  = g_CmdPrams.BlockWidth;
        g_MipSetIn.m_nBlockHeight = g_CmdPrams.BlockHeight;
        g_MipSetIn.m_nBlockDepth  = g_CmdPrams.BlockDepth;
        if (g_CmdPrams.use_noMipMaps)
        {
            g_MipSetIn.m_Flags = MS_FLAG_DisableMipMapping;
        }

        // check if CubeMap is supported in destination file 
        if (g_MipSetIn.m_TextureType ==TT_CubeMap)
        {
            if (!(DestExt.compare("DDS") == 0 || DestExt.compare("KTX") == 0))
            {
                PrintInfo("Error: Cube Maps is not supported in destination file.\n");
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                return -1;
            }
        }

        conversion_loopStartTime = timeStampsec();
        //QueryPerformanceCounter(&conversion_loopStartTime);

        // User setting overrides file setting in this case
        if (g_CmdPrams.CompressOptions.SourceFormat != CMP_FORMAT_Unknown)
            srcFormat = g_CmdPrams.CompressOptions.SourceFormat;
        else
            srcFormat = g_MipSetIn.m_format;

        // Load MIPS did not return a format try to set one
        if (srcFormat == CMP_FORMAT_Unknown)
        {
            g_MipSetIn.m_format = GetFormat(&g_MipSetIn);
            if (g_MipSetIn.m_format == CMP_FORMAT_Unknown)
            {
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                PrintInfo("Error: unsupported input image file format\n");
                return -1;
            }

            srcFormat = g_MipSetIn.m_format;
        }

        // Determin if Source file Is Compressed
        SourceFormatIsCompressed = CompressedFormat(srcFormat);

#ifndef ENABLE_MAKE_COMPATIBLE_API
        if ((FloatFormat(srcFormat) && !FloatFormat(destFormat)) || (!FloatFormat(srcFormat) && FloatFormat(destFormat)))
        {
            cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
            PrintInfo("Error: Processing floating point format <-> non-floating point format is not supported\n");
            return -1;
        }
#endif
        //=====================================================
        // Check for Transcode else Unsupported conversion 
        // ====================================================
        if (SourceFormatIsCompressed && DestinationFormatIsCompressed)
        {
#ifdef USE_CMP_TRANSCODE 

#ifdef USE_GTC
            if ((g_MipSetIn.m_format == CMP_FORMAT_GTC) && 
                (
                (destFormat == CMP_FORMAT_ASTC)     ||
                (destFormat == CMP_FORMAT_BC1)      ||
                (destFormat == CMP_FORMAT_BC7)      ||
                (destFormat == CMP_FORMAT_ETC2_RGB)
                )
                )
            {
                CMP_TranscodeFormat = true;
            }
            else
#endif
#ifdef USE_BASIS
            if ((g_MipSetIn.m_format == CMP_FORMAT_BASIS) && 
                (
                (destFormat == CMP_FORMAT_BC1)      ||
                (destFormat == CMP_FORMAT_BC7)      ||
                (destFormat == CMP_FORMAT_ETC2_RGB)
                )
                )
            {
                CMP_TranscodeFormat = true;
            }
            else
#endif

#endif
            {
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                PrintInfo("Trascoding Error: Compressed source and compressed destination selection is not supported\n");
                return -1;
            }
        }

        //=====================================================
        // Perform swizzle
        // ===================================================
        if (p_userMipSetIn)  //for GUI
        {
#ifdef USE_SWIZZLE
            if (g_MipSetIn.m_swizzle && !g_CmdPrams.CompressOptions.bUseCGCompress)
            {
                //SwizzleMipMap(&g_MipSetIn);
                //SwizzledMipSetIn = true;
            }
#endif
        }
        else
        {
            if (g_MipSetIn.m_swizzle)
                SwizzleMipMap(&g_MipSetIn);
        }

        //======================================================
        // Determine if MIP mapping is required
        // if so generate the MIP levels for the source file
        //=======================================================
        if (((g_CmdPrams.MipsLevel > 1) && (g_MipSetIn.m_nMipLevels == 1)) && (!g_CmdPrams.use_noMipMaps))
        {
            int nMinSize;

            // User has two option to specify MIP levels
            if (g_CmdPrams.nMinSize > 0)
                nMinSize = g_CmdPrams.nMinSize;
            else
                nMinSize = CMP_CalcMinMipSize(g_MipSetIn.m_nHeight, g_MipSetIn.m_nWidth, g_CmdPrams.MipsLevel);
            CMP_GenerateMIPLevels((CMP_MipSet *)&g_MipSetIn, nMinSize);
        }

        // --------------------------------
        // Setup Compressed Mip Set
        // --------------------------------
        CMP_FORMAT cmpformat = destFormat;
        memset(&g_MipSetCmp, 0, sizeof(MipSet));
        g_MipSetCmp.m_Flags = MS_FLAG_Default;
        g_MipSetCmp.m_format = destFormat;

        // -------------
        // Output
        // -------------
        memset(&g_MipSetOut, 0, sizeof(MipSet));

        //----------------------------------
        // Now set data sets pointers for processing
        //----------------------------------
        p_MipSetIn  = &g_MipSetIn;
        p_MipSetOut = &g_MipSetOut;

        //=====================================================
        // Case Uncompressed Source to Compressed Destination
        //
        // Example: BMP -> DDS  with -fd Compression flag
        //
        //=====================================================
        if ((!SourceFormatIsCompressed) && (DestinationFormatIsCompressed))
        {
            compress_loopStartTime = timeStampsec();

            //--------------------------------------------
            // V3.1.9000+  new SDK interface using MipSets
            //--------------------------------------------
            CMP_ERROR cmp_status;

            // Use CGP only if it is not a transcoder format
            if (g_CmdPrams.CompressOptions.bUseCGCompress
#ifdef USE_BASIS
                && (g_MipSetCmp.m_format != CMP_FORMAT_BASIS)
#endif
            ) {
                // KernelOptions   kernel_options;
                // memset(&kernel_options, 0, sizeof(KernalOptions));
                // 
                // kernel_options.encodeWith  = (unsigned int)(CMP_HPC);
                // kernel_options.data_type    = CMP_FORMAT_BC7;
                // kernel_options.fquality     = 0.05;
                // kernel_options.threads      = 8;
                // cmp_status = CMP_ProcessTexture(&g_MipSetIn,&g_MipSetCmp,kernel_options,pFeedbackProc);

                cmp_status = CMP_ConvertMipTextureCGP(&g_MipSetIn,&g_MipSetCmp,&g_CmdPrams.CompressOptions, pFeedbackProc);
            }
            else
            {
                cmp_status = CMP_ConvertMipTexture((CMP_MipSet *)&g_MipSetIn, (CMP_MipSet *)&g_MipSetCmp,&g_CmdPrams.CompressOptions, pFeedbackProc);
            }

            if (cmp_status != CMP_OK)
            {
                PrintInfo("Error %d: Compressing Texture\n",cmp_status);
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                return -1;
            }

            compress_loopEndTime = timeStampsec();

            // set m_dwFourCC format for default DDS file save, we can check the ext of the destination
            // but in some cases the FourCC maybe used on other file types!
            CMP_Format2FourCC(destFormat, &g_MipSetCmp);
        }

        //==============================================
        // Save to file destination buffer if
        // Uncomprssed file to Compressed File format
        //==============================================
        if ((!SourceFormatIsCompressed) && (DestinationFormatIsCompressed) &&
            (IsDestinationUnCompressed((const char*)g_CmdPrams.DestFile.c_str()) == false)

        )
        {
            //-------------------------------------------------------------
            // Set user specification for ASTC Block sizes that was used!
            //-------------------------------------------------------------
            g_MipSetCmp.m_nBlockWidth  = g_CmdPrams.BlockWidth;
            g_MipSetCmp.m_nBlockHeight = g_CmdPrams.BlockHeight;
            g_MipSetCmp.m_nBlockDepth  = g_CmdPrams.BlockDepth;

#ifdef USE_WITH_COMMANDLINE_TOOL
            PrintInfo("\n");
#endif
            if (AMDSaveMIPSTextureImage(g_CmdPrams.DestFile.c_str(), &g_MipSetCmp, g_CmdPrams.use_OCV_out, g_CmdPrams.CompressOptions) != 0)
            {
                PrintInfo("Error: saving image failed, write permission denied or format is unsupported for the file extension.\n");
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                return -1;
            }

            // User requested a DECOMPRESS file also
            // Set a new destinate and flag a Midway Decompress
            if (g_CmdPrams.doDecompress)
            {
                // Clean the Mipset if any was set
                if (g_MipSetOut.m_pMipLevelTable)
                {
                    g_CMIPS->FreeMipSet(&g_MipSetOut);
                }
                g_CmdPrams.DestFile = g_CmdPrams.DecompressFile;
                MidwayDecompress    = true;
            }
        }

        //=====================================================
        // Case Compressed Source to UnCompressed Destination
        // Transcoding file formats
        //
        // Case example: BMP -> BMP with -fd uncompression flag
        //
        //=====================================================
        if ((!SourceFormatIsCompressed) && (!DestinationFormatIsCompressed))
        {
            TranscodeBits = true;
            // Check if source and destinatation types are supported for transcoding
            if ((g_MipSetOut.m_TextureType == TT_2D)&&(g_MipSetIn.m_TextureType ==TT_CubeMap))
            {
                PrintInfo("Error: Transcoding Cube Maps is not supported.\n");
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                return -1;
            }
        }

        //=====================================================
        // Case uncompressed Source to UnCompressed Destination
        // with mid way compression
        //
        // Case example: BMP -> BMP with -fd compression flag
        //
        //=====================================================
        if ((!SourceFormatIsCompressed) && (DestinationFormatIsCompressed) &&
            (IsDestinationUnCompressed((const char*)g_CmdPrams.DestFile.c_str()) == true))
        {
            MidwayDecompress = true;
            // Prepare for an uncompress request on destination
            p_MipSetIn = &g_MipSetCmp;
            srcFormat  = g_MipSetCmp.m_format;
        }

        //=====================================================
        // Case Compressed Source to UnCompressed Destination
        // Example(s):
        //                DDS - BMP  with no -fd flag
        //                BMP - BMP  with no -fd flag(s)
        //                BMP - BMP  with    -fd flag(s)
        //
        //=====================================================
        if (MidwayDecompress) 
        {
            PrintInfo("\nProcessed image is been decompressed to new target format!\n");
        }
         
        if (((SourceFormatIsCompressed) && (!DestinationFormatIsCompressed)) || (TranscodeBits) || (MidwayDecompress))
        {
            compress_loopStartTime = timeStampsec();

            g_MipSetOut.m_TextureDataType = TDT_ARGB;

            if (SourceFormatIsCompressed)
            {
                // BMP is saved as CMP_FORMAT_ARGB_8888
                // EXR is saved as CMP_FORMAT_ARGB_32F
                switch (srcFormat)
                {
                case CMP_FORMAT_BC6H:
                case CMP_FORMAT_BC6H_SF:
                    destFormat                  = CMP_FORMAT_ARGB_16F;
                    g_MipSetOut.m_ChannelFormat = CF_Float16;
                    break;
                default:
                    destFormat = CMP_FORMAT_ARGB_8888;
                    break;
                }
            }
            else
            {
                if (MidwayDecompress)
                {
                    // Need to determin a target format.
                    // Based on file extension.
                    switch (srcFormat)
                    {
                    case CMP_FORMAT_BC6H:
                    case CMP_FORMAT_BC6H_SF:
                        destFormat                  = CMP_FORMAT_ARGB_16F;
                        g_MipSetOut.m_ChannelFormat = CF_Float16;
                        break;
                    default:
                        destFormat = FormatByFileExtension((const char*)g_CmdPrams.DestFile.c_str(), &g_MipSetOut);
                        break;
                    }
                }
            }

            // Something went wrone in determining destination format
            // Just default it!
            if (destFormat == CMP_FORMAT_Unknown)
                destFormat = CMP_FORMAT_ARGB_8888;

            if (destFormat == CMP_FORMAT_ARGB_32F)
                g_MipSetOut.m_ChannelFormat = CF_Float32;
            else if (destFormat == CMP_FORMAT_ARGB_16F)
                g_MipSetOut.m_ChannelFormat = CF_Float16;

            g_MipSetOut.m_format         = destFormat;
            g_MipSetOut.m_isDeCompressed = srcFormat != CMP_FORMAT_Unknown ? srcFormat : CMP_FORMAT_MAX;

            // Allocate output MipSet
            if (!g_CMIPS->AllocateMipSet(&g_MipSetOut, g_MipSetOut.m_ChannelFormat, g_MipSetOut.m_TextureDataType, p_MipSetIn->m_TextureType,
                                         p_MipSetIn->m_nWidth, p_MipSetIn->m_nHeight, p_MipSetIn->m_nDepth)) // depthsupport, what should nDepth be set as here?
            {
                PrintInfo("Memory Error(2): allocating MIPSet Output buffer\n");
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                return -1;
            }

            g_MipSetOut.m_CubeFaceMask = p_MipSetIn->m_CubeFaceMask;

            MipLevel* pCmpMipLevel    = g_CMIPS->GetMipLevel(p_MipSetIn, 0);
            int       nMaxFaceOrSlice = CMP_MaxFacesOrSlices(p_MipSetIn, 0);
            int       nWidth          = pCmpMipLevel->m_nWidth;
            int       nHeight         = pCmpMipLevel->m_nHeight;
            //
            CMP_BYTE* pMipData = g_CMIPS->GetMipLevel(p_MipSetIn, 0, 0)->m_pbData;

            bool          use_GPUDecode = g_CmdPrams.CompressOptions.bUseGPUDecompress;
            CMP_GPUDecode DecodeWith    = g_CmdPrams.CompressOptions.nGPUDecode;

            decompress_loopStartTime = timeStampsec();

            for (int nFaceOrSlice = 0; nFaceOrSlice < nMaxFaceOrSlice; nFaceOrSlice++)
            {
                int nMipWidth  = nWidth;
                int nMipHeight = nHeight;

                for (int nMipLevel = 0; nMipLevel < p_MipSetIn->m_nMipLevels; nMipLevel++)
                {
                    MipLevel* pInMipLevel = g_CMIPS->GetMipLevel(p_MipSetIn, nMipLevel, nFaceOrSlice);
                    if (!pInMipLevel)
                    {
                        PrintInfo("Memory Error(2): allocating MIPSet Output Cmp level buffer\n");
                        cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                        return -1;
                    }

                    // Valid Mip Level ?
                    if (pInMipLevel->m_pbData)
                        pMipData = pInMipLevel->m_pbData;

                    if (!g_CMIPS->AllocateMipLevelData(g_CMIPS->GetMipLevel(&g_MipSetOut, nMipLevel, nFaceOrSlice), nMipWidth, nMipHeight,
                                                       g_MipSetOut.m_ChannelFormat, g_MipSetOut.m_TextureDataType))
                    {
                        PrintInfo("Memory Error(2): allocating MIPSet Output level buffer\n");
                        cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                        return -1;
                    }

                    //----------------------------
                    // Compressed source
                    //-----------------------------
                    CMP_Texture srcTexture;
                    srcTexture.dwSize       = sizeof(srcTexture);
                    srcTexture.dwWidth      = nMipWidth;
                    srcTexture.dwHeight     = nMipHeight;
                    srcTexture.dwPitch      = 0;
                    srcTexture.nBlockHeight = p_MipSetIn->m_nBlockHeight;
                    srcTexture.nBlockWidth  = p_MipSetIn->m_nBlockWidth;
                    srcTexture.nBlockDepth  = p_MipSetIn->m_nBlockDepth;
                    srcTexture.format       = srcFormat;
                    srcTexture.dwDataSize   = CMP_CalculateBufferSize(&srcTexture);
                    srcTexture.pData        = pMipData;

                    g_CmdPrams.dwHeight   = srcTexture.dwHeight;
                    g_CmdPrams.dwWidth    = srcTexture.dwWidth;
                    g_CmdPrams.dwDataSize = srcTexture.dwDataSize;

                    //-----------------------------
                    // Uncompressed Destination
                    //-----------------------------
                    CMP_Texture destTexture;
                    destTexture.dwSize     = sizeof(destTexture);
                    destTexture.dwWidth    = nMipWidth;
                    destTexture.dwHeight   = nMipHeight;
                    destTexture.dwPitch    = 0;
                    destTexture.format     = destFormat;
                    destTexture.dwDataSize = CMP_CalculateBufferSize(&destTexture);
                    destTexture.pData      = g_CMIPS->GetMipLevel(&g_MipSetOut, nMipLevel, nFaceOrSlice)->m_pbData;
                    if (!g_CmdPrams.silent)
                    {
                        if ((nMipLevel > 1) || (nFaceOrSlice > 1))
                            PrintInfo("\rProcessing destination     MipLevel %2d FaceOrSlice %2d", nMipLevel + 1, nFaceOrSlice);
                        else
                            PrintInfo("\rProcessing destination    ");
                    }
#ifdef _WIN32
                    if (/*(srcTexture.dwDataSize > destTexture.dwDataSize) || */ (IsBadWritePtr(destTexture.pData, destTexture.dwDataSize)))
                    {
                        PrintInfo("Memory Error(2): Destination image must be compatible with source\n");
                        cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                        return -1;
                    }
#else
                    int nullfd = open("/dev/random", O_WRONLY);
                    if (write(nullfd, destTexture.pData, destTexture.dwDataSize) < 0)
                    {
                        PrintInfo("Memory Error(2): Destination image must be compatible with source\n");
                        cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                        return -1;
                    }
                    close(nullfd);
#endif
                    g_fProgress = -1;

                    if (use_GPUDecode)
                    {
#ifdef _WIN32
#ifndef DISABLE_TESTCODE
                        if (srcTexture.format == CMP_FORMAT_ASTC)
                        {
                            PrintInfo(
                                "Decompress Error: ASTC decompressed with GPU is not supported. Please view ASTC compressed images using CPU.\n");
                            cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                            return -1;
                        }

                        if (CMP_DecompressTexture(&srcTexture, &destTexture, DecodeWith) != CMP_OK)
                        {
                            PrintInfo("Error in decompressing source texture\n");
                            cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                            return -1;
                        }
#endif
#else
                        PrintInfo("GPU Decompress is not supported in linux.\n");
                        cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                        return -1;
#endif
                    }
                    else
                    {
                        if (CMP_ConvertTexture(&srcTexture, &destTexture, &g_CmdPrams.CompressOptions, pFeedbackProc) != CMP_OK)
                        {
                            PrintInfo("Error(2) in compressing destination texture\n");
                            cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                            return -1;
                        }
                    }

                    g_CmdPrams.decompress_nIterations++;

                    pMipData += srcTexture.dwDataSize;
                    nMipWidth  = (nMipWidth > 1) ? (nMipWidth >> 1) : 1;
                    nMipHeight = (nMipHeight > 1) ? (nMipHeight >> 1) : 1;
                }
            }

            compress_loopEndTime = timeStampsec();
            decompress_loopEndTime = timeStampsec();

            g_MipSetOut.m_nMipLevels = p_MipSetIn->m_nMipLevels;

            p_MipSetOut = &g_MipSetOut;

            // ===============================================
            // INPUT IMAGE Swizzling options for DXT formats
            // ===============================================
            //if (!use_GPUDecode)
            //    g_MipSetOut.m_swizzle = KeepSwizzle(srcFormat);

            //-----------------------
            // User swizzle overrides
            //-----------------------
            // Did User set no swizzle
            if (g_CmdPrams.noswizzle)
                g_MipSetOut.m_swizzle = false;

            // Did User set do swizzle
            if (g_CmdPrams.doswizzle)
                g_MipSetOut.m_swizzle = true;

            //=================================
            // Save to file destination buffer
            //==================================

            //-------------------------------------------------------------
            // Set user specification for block sizes that was used!
            //-------------------------------------------------------------
            p_MipSetOut->m_nBlockWidth  = g_CmdPrams.BlockWidth;
            p_MipSetOut->m_nBlockHeight = g_CmdPrams.BlockHeight;
            p_MipSetOut->m_nBlockDepth  = g_CmdPrams.BlockDepth;

#ifdef USE_WITH_COMMANDLINE_TOOL
            PrintInfo("\n");
#endif
            if (AMDSaveMIPSTextureImage(g_CmdPrams.DestFile.c_str(), p_MipSetOut, g_CmdPrams.use_OCV_out, g_CmdPrams.CompressOptions) != 0)
            {
                PrintInfo("Error: saving image failed, write permission denied or format is unsupported for the file extension.\n");
                cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                return -1;
            }
        }

#ifdef USE_BASIS
        //============================================================
        // Process Transcode Compress source to new compressed target
        //============================================================
        if (SourceFormatIsCompressed && DestinationFormatIsCompressed && CMP_TranscodeFormat)
        {
            PrintInfo("\nTranscoding %s to %s\n", GetFormatDesc(g_MipSetIn.m_format), GetFormatDesc(g_MipSetCmp.m_format));
            if (GTCTranscode(&g_MipSetIn, &g_MipSetCmp,g_pluginManager))
            {
                g_MipSetOut.m_nMipLevels = p_MipSetIn->m_nMipLevels;

                p_MipSetOut = &g_MipSetCmp;
                //-------------------------------------------------------------
                // Save the new compressed format 
                //-------------------------------------------------------------
                p_MipSetOut->m_nBlockWidth = g_CmdPrams.BlockWidth;
                p_MipSetOut->m_nBlockHeight = g_CmdPrams.BlockHeight;
                p_MipSetOut->m_nBlockDepth = g_CmdPrams.BlockDepth;

                if (AMDSaveMIPSTextureImage(g_CmdPrams.DestFile.c_str(), &g_MipSetCmp, g_CmdPrams.use_OCV_out, g_CmdPrams.CompressOptions) != 0)
                {
                    PrintInfo("Error: saving image failed, write permission denied or format is unsupported for the file extension.\n");
                    cleanup(Delete_gMipSetIn, SwizzledMipSetIn);
                    return -1;
                }
            }
        }
#endif

        // always store these values
        conversion_loopEndTime = timeStampsec();
        g_CmdPrams.compress_fDuration   = compress_loopEndTime   - compress_loopStartTime;
        if (g_CmdPrams.compress_fDuration < 0.001) g_CmdPrams.compress_fDuration = 0.0;

        g_CmdPrams.decompress_fDuration = decompress_loopEndTime - decompress_loopStartTime;
        if (g_CmdPrams.decompress_fDuration < 0.001) g_CmdPrams.decompress_fDuration = 0.0;

        g_CmdPrams.conversion_fDuration = conversion_loopEndTime - conversion_loopStartTime;
        if (g_CmdPrams.conversion_fDuration < 0.001) g_CmdPrams.conversion_fDuration = 0.0;

        if ((!g_CmdPrams.silent) && (g_CmdPrams.showperformance))
        {

#ifdef USE_WITH_COMMANDLINE_TOOL
            PrintInfo("\r");
#endif

            if (g_CmdPrams.compress_nIterations)
                PrintInfo("Compressed to %s with %i iteration(s) in %.3f seconds\n", GetFormatDesc(cmpformat), g_CmdPrams.compress_nIterations,
                          g_CmdPrams.compress_fDuration);

            if (g_CmdPrams.decompress_nIterations)
                PrintInfo("DeCompressed to %s with %i iteration(s) in %.3f seconds\n", GetFormatDesc(destFormat), g_CmdPrams.decompress_nIterations,
                          g_CmdPrams.decompress_fDuration);

            PrintInfo("Total time taken (includes file I/O): %.3f seconds\n", g_CmdPrams.conversion_fDuration);

        }
    }
    else
    {
#ifdef USE_MESH_CLI
        //Mesh Optimization
#ifdef USE_3DMESH_OPTIMIZE
        if (g_CmdPrams.doMeshOptimize)
        {
            if ((fileIsGLTF(g_CmdPrams.SourceFile) && fileIsGLTF(g_CmdPrams.DestFile)) ||
                (fileIsOBJ(g_CmdPrams.SourceFile) && fileIsOBJ(g_CmdPrams.DestFile)))
            {
                if (!(OptimizeMesh(g_CmdPrams.SourceFile, g_CmdPrams.DestFile)))
                {
                    PrintInfo("Error: Mesh Optimization Failed.\n");
                    return -1;
                }
            }
            else
            {
                PrintInfo("Error: Mesh Optimization Failed. Only glTF->glTF, obj->obj are supported in mesh optimization.\n");
                return -1;
            }
        }
#endif

        // Mesh Compression and Decompression
        if (!(g_CmdPrams.doMeshOptimize &&
              !g_CmdPrams
                   .use_Draco_Encode))  // skip mesh decompression for case only meshopt turn on: CompressonatorCLI.exe -meshopt source.gltf/obj dest.gltf/obj
        {
            if ((fileIsGLTF(g_CmdPrams.SourceFile) && fileIsGLTF(g_CmdPrams.DestFile)) 
            #ifdef _WIN32
              || (fileIsOBJ(g_CmdPrams.SourceFile) && fileIsDRC(g_CmdPrams.DestFile)) 
              || (fileIsDRC(g_CmdPrams.SourceFile) && fileIsOBJ(g_CmdPrams.DestFile)) 
              || (fileIsOBJ(g_CmdPrams.SourceFile) && fileIsOBJ(g_CmdPrams.DestFile))
            #endif
                )
            {
                if (!(CompressDecompressMesh(g_CmdPrams.SourceFile, g_CmdPrams.DestFile)))
                {
                    PrintInfo("Error: Mesh Compression Failed.\n");
                    return -1;
                }
            }
            else
            {
                #ifdef _WIN32
                PrintInfo(
                    "Note: Mesh Compression Failed. Only glTF->glTF, obj->drc(compression), drc->obj(decompression), obj->obj(optimize first then "
                    "compress to .drc file) are supported in mesh compression/decompression.\n");
                #else
                PrintInfo(
                    "Note: Mesh Compression Failed. Only glTF->glTF are supported in mesh compression/decompression.\n");
                #endif
                return -1;
            }
        }
#endif
    }

    cleanup(Delete_gMipSetIn, SwizzledMipSetIn);

#ifdef SHOW_PROCESS_MEMORY
    bool result2 = GetProcessMemoryInfo(GetCurrentProcess(), &memCounter2, sizeof(memCounter2));
#endif

    // Warning for log on when not compressing files
    if ((g_CmdPrams.logresults) && (g_CmdPrams.decompress_nIterations > 0))
    {
         PrintInfo("\nWarning: log is only supported for image compression, option is turned off!\n");
         g_CmdPrams.logresults = false;
    }

    if (g_CmdPrams.logresults)
    {
        if (!g_CmdPrams.silent)
        {
    #ifdef USE_WITH_COMMANDLINE_TOOL
            PrintInfo("\rlog results                                           ");
    #endif
        }

        CMP_ANALYSIS_DATA analysisData = {0};
        analysisData.SSIM = -1;     // Set data content is invalid and not processed 

        if (Plugin_Analysis)
        {
            // Diff File
            {
                g_CmdPrams.DiffFile = g_CmdPrams.DestFile;
                int lastindex       = (int)g_CmdPrams.DiffFile.find_last_of(".");
                g_CmdPrams.DiffFile = g_CmdPrams.DiffFile.substr(0, lastindex);
                g_CmdPrams.DiffFile.append("_diff.bmp");
            } // Image Diff

            // Analysis only for when compresing images
            {
                    if (Plugin_Analysis->TC_ImageDiff(
                                                    g_CmdPrams.SourceFile.c_str(), 
                                                    g_CmdPrams.DestFile.c_str(), 
                                                    "", // g_CmdPrams.DiffFile.c_str(),  Skip image diff for now , it takes too long to process
                                                    "",
                                                    &analysisData,
                                                    &g_pluginManager,
                                                    NULL) != 0) 
                                                    { 
                                                        analysisData.SSIM = -2;
                                                    }
            } // Analysis report
        }

        ProcessResults(g_CmdPrams,analysisData);

         if ((analysisData.SSIM != -1) && ((analysisData.SSIM != -2) ))
         {
            psnr_sum         += analysisData.PSNR;
            ssim_sum         += analysisData.SSIM;
            process_time_sum += g_CmdPrams.compress_fDuration;
            total_processed_items++; // used to track number of processed items and used for avg of Process Time, SSIM and PSNR stats.
         }
        // Reset single use items
        g_CmdPrams.compute_setup_fDuration = 0;
    }

    //==============================================================
    // Get a new set of source and destination files for processing.
    //==============================================================
    if (g_CmdPrams.SourceFileList.size() > 0)
    {
        MoreSourceFiles = true;
        // Set the first file in list to SourceFile and delete it from the list
        g_CmdPrams.SourceFile = g_CmdPrams.SourceFileList[0].c_str();
        g_CmdPrams.SourceFileList.erase(g_CmdPrams.SourceFileList.begin());

        std::string destFileName;
        destFileName = DefaultDestination(g_CmdPrams.SourceFile, g_CmdPrams.CompressOptions.DestFormat, g_CmdPrams.FileOutExt);
        g_CmdPrams.DestFile = g_CmdPrams.DestDir + "\\" + destFileName;
    }
    else
        MoreSourceFiles = false;


    if (!g_CmdPrams.silent)
    {
        if ((g_CmdPrams.logresults)&&(!g_CmdPrams.logresultsToFile))
        {
            // check if any data was calculated
            if (g_CmdPrams.SSIM >= 0)
            {
            PrintInfo("MSE %.2f PSRN %.1f SSIM %.4f\n",
                g_CmdPrams.MSE,
                g_CmdPrams.PSNR,
                g_CmdPrams.SSIM
                );
            }
        }

#ifdef USE_WITH_COMMANDLINE_TOOL
        PrintInfo("\rDone Processing                                       \n");
#else
        PrintInfo("Done Processing\n");
#endif
    }


    } while (MoreSourceFiles);

    if (g_CmdPrams.logresults)
    {
        if (Plugin_Analysis)
            delete Plugin_Analysis;

        if (total_processed_items > 1)
        {
            char buff[128];
            snprintf(buff,sizeof(buff),"Average      : PSNR: %.1f  SSIM: %.4f  Time %.3f Sec for %d item(s) \n",
                psnr_sum/total_processed_items,
                ssim_sum/total_processed_items,
                process_time_sum/total_processed_items,
                total_processed_items
                );
            LogToResults(g_CmdPrams,buff);
        }

        LogToResults(g_CmdPrams,"--------------\n");
    }

//printf("--END--\n");
    return processResult;
}

void LogToResults(CCmdLineParamaters &prams, char *str)
{
    if (prams.logresultsToFile)
    {
        #ifdef _WIN32
            FILE*   fp;
            //errno_t err = 
            fopen_s(&fp, prams.LogProcessResultsFile.c_str(), "a");
        #else
            FILE* fp = fopen(prams.LogProcessResultsFile.c_str(), "a");
        #endif
            if (fp)
            {
                fprintf(fp,"%s",str);
                fclose(fp);
            }
    }
}

void ProcessResults(CCmdLineParamaters &prams, CMP_ANALYSIS_DATA &analysisData)
{
    if (prams.logresultsToFile)
    {
        bool newfile = false;
        // Check for file existance first!
        if (!CMP_FileExists(prams.LogProcessResultsFile))
            newfile = true;
    
    #ifdef _WIN32
        FILE*   fp;
        //errno_t err = 
        fopen_s(&fp, prams.LogProcessResultsFile.c_str(), "a");
    #else
        FILE* fp = fopen(prams.LogProcessResultsFile.c_str(), "a");
    #endif
        if (fp)
        {
            // Encode with
            std::string encodewith;
    
            if (prams.CompressOptions.bUseCGCompress)
            {
                if (!prams.CompressOptions.format_support_gpu)
                {
                    encodewith.append("*** CPU used in place of ");
                }
    
                switch (prams.CompressOptions.nEncodeWith)
                {
                case CMP_HPC:                      // Use CPU High Performance Compute to compress textures, full support
                    encodewith.append("HPC");
                    break;
#ifdef USE_GPUEncoders
                case CMP_GPU:                      // Use GPU to compress textures, full support
                    encodewith.append("GPU");
                    break;
                case CMP_GPU_OCL: 
                    encodewith.append("OCL");
                    break;
                case CMP_GPU_VLK:
                    encodewith.append("VLK");
                    break;
                case CMP_GPU_DXC:
                    encodewith.append("DXC");
                    break;
#endif
                case CMP_CPU:
                default:
                    encodewith.append("CPU");
                    break;
                }
            }
            else
            {
                if (prams.CompressOptions.bUseGPUDecompress)
                {
                    switch(prams.CompressOptions.nGPUDecode)
                    {
                        case GPUDecode_DIRECTX:
                            encodewith.append("DirectX");
                            break;
                        case GPUDecode_VULKAN :
                            encodewith.append("Vulkan");
                            break;
                        case GPUDecode_OPENGL :
                        default:
                            encodewith.append("OpenGL");
                            break;
                    }
                }
                else
                    encodewith.append("CPU");
            }
    
            // Write Header info
            if (newfile)
            {
                fprintf(fp,"CompressonatorCLI Performance Log v1.0\n\n");
            }
             fprintf(fp,"Source       : %s, Height %d, Width %d, Size %2.3f MBytes\n", 
                                                prams.SourceFile.c_str(),
                                                prams.dwHeight,
                                                prams.dwWidth,
                                                (float)prams.dwDataSize / 1024000.0f
                                                );
             fprintf(fp,"Destination  : %s\n", prams.DestFile.c_str());
             fprintf(fp,"Using        : %s\n", encodewith.c_str());
    
             fprintf(fp,"Quality      : %.2f\n",g_CmdPrams.CompressOptions.fquality);
             if (prams.compute_setup_fDuration > 0)
             {
                fprintf(fp,"Compute Setup: %.3f seconds\n", g_CmdPrams.compute_setup_fDuration);
             }
             fprintf(fp,"Processed to : %-10s with %2i iteration(s) in %.3f seconds\n", GetFormatDesc(prams.CompressOptions.DestFormat), prams.compress_nIterations,prams.compress_fDuration);
    
             // Log only compression stats
             if (analysisData.PSNR <= 0)
             {
                 analysisData.PSNR = 256;
             }
    
             if (analysisData.SSIM != -1)
             {
                 fprintf(fp,"MSE          : %.2f\n", analysisData.MSE);
                 fprintf(fp,"PSNR         : %.1f\n", analysisData.PSNR);
                 fprintf(fp,"SSIM         : %.4f\n", analysisData.SSIM);
             }
             // There was an error in Analysis
             if (analysisData.SSIM == -2)
               fprintf(fp,"Analysis data: **Failed to report**\n");
    
            fprintf(fp,"Total time   : %.3f seconds\n", prams.conversion_fDuration);
            fprintf(fp,"\n");
            fclose(fp);
        }
   }
   else
   {
       if (analysisData.PSNR <= 0)
       {
           analysisData.PSNR = 256;
       }
    
       if (analysisData.SSIM != -1)
       {
            prams.MSE  = analysisData.MSE;
            prams.PSNR = analysisData.PSNR;
            prams.SSIM = analysisData.SSIM;
       }
   }
}
