// AMD AMDUtils code
// 
// Copyright(c) 2017 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stdafx.h"
#include "GltfFeatures.h"
#include "ModelTexture.h"
#include "dxgi.h"
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "Misc.h"
#include "UtilFuncs.h"

//--------------------------------------------------------------------------------------
// Constructor of the Texture class
// initializes all members
//--------------------------------------------------------------------------------------
ModelTexture::ModelTexture()
{
    pMipSet = NULL;
    m_CMIPS = NULL;
}

//--------------------------------------------------------------------------------------
// Destructor of the Texture class
//--------------------------------------------------------------------------------------
ModelTexture::~ModelTexture()
{
    CleanMipSet();
}

void ModelTexture::setCMIPS(CMIPS *cmips)
{
    m_CMIPS = cmips;
}

void ModelTexture::CleanMipSet()
{
    if (pMipSet)
    {
        if (pMipSet->m_pMipLevelTable)
        {
            l_cmips.FreeMipSet(pMipSet);
            pMipSet->m_pMipLevelTable = NULL;
        }

        delete pMipSet;
        pMipSet = NULL;
    }
}

//--------------------------------------------------------------------------------------
// entry function to initialize an image from a .DDS texture
//--------------------------------------------------------------------------------------
INT32 ModelTexture::LoadImageMipSetFromFile(const WCHAR *pFilename, void *pluginManager)
{
    CleanMipSet();

    try
    {
        // get the ext and load image with amd compressonator image plugin
        char *fileExt;
        wstring ws(pFilename);
        string sFilename(ws.begin(), ws.end());
        size_t dot = sFilename.find_last_of('.');
        std::string temp;

        if (dot != std::string::npos) {
            temp = (sFilename.substr(dot + 1, sFilename.size() - dot));
            std::transform(temp.begin(), temp.end(), temp.begin(), toupper);
            fileExt = (char*)temp.data();
        }

        pMipSet = new MipSet();

        if (pMipSet == NULL)
        {
            CleanMipSet();
            if (m_CMIPS)
            {
                m_CMIPS->Print("Error loading file: Out of memory for MipSet data");
            }
            return -1;
        }

        memset(pMipSet, 0, sizeof(MipSet));

        if (m_CMIPS)
        {
            char fname[_MAX_FNAME];
            getFileNameExt(sFilename.c_str(), fname, _MAX_FNAME);
            m_CMIPS->SetProgress(0);
            if (m_CMIPS->m_canceled)
                return -1;
            m_CMIPS->Print("Loading: %s", fname);
        }

        if (AMDLoadMIPSTextureImage(sFilename.c_str(), pMipSet, false, pluginManager) != 0)
        {
            CleanMipSet();
            if (m_CMIPS)
            {
                m_CMIPS->Print("Error: reading image, data type not supported");
            }
            return -1;
        }

        if (pMipSet)
        {
            if (pMipSet->m_format == CMP_FORMAT_Unknown)
            {
                pMipSet->m_format = GetFormat(pMipSet);
            }

            pMipSet->m_swizzle = KeepSwizzle(pMipSet->m_format);

            if (pMipSet->m_compressed || (pMipSet->m_ChannelFormat == CF_Compressed))
            {
                pMipSet->m_compressed = true;
                Config configsetting;
                configsetting.swizzle = pMipSet->m_swizzle;
                pMipSet = DecompressMIPSet(pMipSet, GPUDecode_INVALID, &configsetting, NULL);
                if (pMipSet == NULL)
                {
                    if (m_CMIPS)
                    {
                        m_CMIPS->Print("Error: reading compressed image");
                    }
                    return -1;
                }
            }
            if (pMipSet->m_swizzle)
                SwizzleMipMap(pMipSet);
        }
        else
        {
            CleanMipSet();
            if (m_CMIPS)
            {
                m_CMIPS->Print("Error: reading image, data type not supported");
            }
            return -1;
        }

    }
    catch (std::bad_alloc)
    {
        if (m_CMIPS)
        {
            CleanMipSet();
            m_CMIPS->m_canceled = true;
            m_CMIPS->Print("Error: Out of Memory while loading textures!");
        }
        return -1;
    }

    return 0;
}


