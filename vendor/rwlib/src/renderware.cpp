/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/renderware.cpp
*  PURPOSE:     Central implementations.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include <cstdlib>

namespace rw
{

LibraryVersion KnownVersions::getGameVersion( KnownVersions::eGameVersion gameVer )
{
    LibraryVersion outVer;

    outVer.buildNumber = 0xFFFF;
    outVer.rwLibMajor = 3;
    outVer.rwLibMinor = 0;
    outVer.rwRevMajor = 0;
    outVer.rwRevMinor = 0;

    // Modify it for game engines we know.
    if ( gameVer == GTA3_PS2 )
    {
        outVer.rwLibMinor = 1;
    }
    else if ( gameVer == GTA3_PC )
    {
        outVer.rwLibMinor = 1;
        outVer.rwRevMinor = 1;
    }
    else if ( gameVer == GTA3_XBOX )
    {
        outVer.rwLibMinor = 5;
    }
    else if ( gameVer == VC_PS2 )
    {
        outVer.rwLibMinor = 3;
        outVer.rwRevMinor = 2;
    }
    else if ( gameVer == VC_PC )
    {
        outVer.rwLibMinor = 4;
        outVer.rwRevMinor = 3;
    }
    else if ( gameVer == VC_XBOX )
    {
        outVer.rwLibMinor = 5;
    }
    else if ( gameVer == SA ||
              gameVer == MANHUNT_PC )
    {
        outVer.rwLibMinor = 6;
        outVer.rwRevMinor = 3;
    }
    else if ( gameVer == MANHUNT_PS2 )
    {
        outVer.rwLibMinor = 4;
        outVer.rwRevMinor = 5;
    }
    else if ( gameVer == BULLY )
    {
        outVer.rwLibMinor = 7;
        outVer.rwRevMinor = 2;
        outVer.buildNumber = 10;
    }
    else if ( gameVer == LCS_PSP )
    {
        outVer.rwLibMinor = 1;
    }
    else if ( gameVer == SHEROES_GC )
    {
        outVer.rwLibMinor = 5;
    }

    return outVer;
}

void ChunkNotFound(CHUNK_TYPE chunk, uint32 address)
{
	std::cerr << "chunk " << std::hex << chunk << " not found at 0x";
	std::cerr << std::hex << address << std::endl;
	exit(1);
}

uint32 writeInt8(int8 tmp, std::ostream &rw)
{
	rw.write(reinterpret_cast <char *> (&tmp), sizeof(int8));
	return sizeof(int8);
}

uint32 writeUInt8(uint8 tmp, std::ostream &rw)
{
	rw.write(reinterpret_cast <char *> (&tmp), sizeof(uint8));
	return sizeof(uint8);
}

uint32 writeInt16(int16 tmp, std::ostream &rw)
{
	rw.write(reinterpret_cast <char *> (&tmp), sizeof(int16));
	return sizeof(int16);
}

uint32 writeUInt16(uint16 tmp, std::ostream &rw)
{
	rw.write(reinterpret_cast <char *> (&tmp), sizeof(uint16));
	return sizeof(uint16);
}

uint32 writeInt32(int32 tmp, std::ostream &rw)
{
	rw.write(reinterpret_cast <char *> (&tmp), sizeof(int32));
	return sizeof(int32);
}

uint32 writeUInt32(uint32 tmp, std::ostream &rw)
{
	rw.write(reinterpret_cast <char *> (&tmp), sizeof(uint32));
	return sizeof(uint32);
}

uint32 writeFloat32(float32 tmp, std::ostream &rw)
{
	rw.write(reinterpret_cast <char *> (&tmp), sizeof(float32));
	return sizeof(float32);
}

int8 readInt8(std::istream &rw)
{
	int8 tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(int8));
	return tmp;
}

uint8 readUInt8(std::istream &rw)
{
	uint8 tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint8));
	return tmp;
}

int16 readInt16(std::istream &rw)
{
	int16 tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(int16));
	return tmp;
}

uint16 readUInt16(std::istream &rw)
{
	uint16 tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint16));
	return tmp;
}

int32 readInt32(std::istream &rw)
{
	int32 tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(int32));
	return tmp;
}

uint32 readUInt32(std::istream &rw)
{
	uint32 tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(uint32));
	return tmp;
}

float32 readFloat32(std::istream &rw)
{
	float32 tmp;
	rw.read(reinterpret_cast <char *> (&tmp), sizeof(float32));
	return tmp;
}

const char *chunks[] = { "None", "Struct", "String", "Extension", "Unknown",
	"Camera", "Texture", "Material", "Material List", "Atomic Section",
	"Plane Section", "World", "Spline", "Matrix", "Frame List",
	"Geometry", "Clump", "Unknown", "Light", "Unicode String", "Atomic",
	"Texture Native", "Texture Dictionary", "Animation Database",
	"Image", "Skin Animation", "Geometry List", "Anim Animation",
	"Team", "Crowd", "Delta Morph Animation", "Right To Render",
	"MultiTexture Effect Native", "MultiTexture Effect Dictionary",
	"Team Dictionary", "Platform Independet Texture Dictionary",
	"Table of Contents", "Particle Standard Global Data", "AltPipe",
	"Platform Independet Peds", "Patch Mesh", "Chunk Group Start",
	"Chunk Group End", "UV Animation Dictionary", "Coll Tree"
};

/* From 0x0101 through 0x0135 */
const char *toolkitchunks0[] = { "Metrics PLG", "Spline PLG", "Stereo PLG",
	"VRML PLG", "Morph PLG", "PVS PLG", "Memory Leak PLG", "Animation PLG",
	"Gloss PLG", "Logo PLG", "Memory Info PLG", "Random PLG",
	"PNG Image PLG", "Bone PLG", "VRML Anim PLG", "Sky Mipmap Val",
	"MRM PLG", "LOD Atomic PLG", "ME PLG", "Lightmap PLG",
	"Refine PLG", "Skin PLG", "Label PLG", "Particles PLG", "GeomTX PLG",
	"Synth Core PLG", "STQPP PLG",
	"Part PP PLG", "Collision PLG", "HAnim PLG", "User Data PLG",
	"Material Effects PLG", "Particle System PLG", "Delta Morph PLG",
	"Patch PLG", "Team PLG", "Crowd PP PLG", "Mip Split PLG",
	"Anisotrophy PLG", "Not used", "GCN Material PLG", "Geometric PVS PLG",
	"XBOX Material PLG", "Multi Texture PLG", "Chain PLG", "Toon PLG",
	"PTank PLG", "Particle Standard PLG", "PDS PLG", "PrtAdv PLG",
	"Normal Map PLG", "ADC PLG", "UV Animation PLG"
};

/* From 0x0180 through 0x01c1 */
const char *toolkitchunks1[] = {
	"Character Set PLG", "NOHS World PLG", "Import Util PLG",
	"Slerp PLG", "Optim PLG", "TL World PLG", "Database PLG",
	"Raytrace PLG", "Ray PLG", "Library PLG",
	"Not used", "Not used", "Not used", "Not used", "Not used", "Not used",
	"2D PLG", "Tile Render PLG", "JPEG Image PLG", "TGA Image PLG",
	"GIF Image PLG", "Quat PLG", "Spline PVS PLG", "Mipmap PLG",
	"MipmapK PLG", "2D Font", "Intersection PLG", "TIFF Image PLG",
	"Pick PLG", "BMP Image PLG", "RAS Image PLG", "Skin FX PLG",
	"VCAT PLG", "2D Path", "2D Brush", "2D Object", "2D Shape", "2D Scene",
	"2D Pick Region", "2D Object String", "2D Animation PLG",
	"2D Animation",
	"Not used", "Not used", "Not used", "Not used", "Not used", "Not used",
	"2D Keyframe", "2D Maestro", "Barycentric",
	"Platform Independent Texture Dictionary TK", "TOC TK", "TPL TK",
	"AltPipe TK", "Animation TK", "Skin Split Tookit", "Compressed Key TK",
	"Geometry Conditioning PLG", "Wing PLG", "Generic Pipeline TK",
	"Lightmap Conversion TK", "Filesystem PLG", "Dictionary TK",
	"UV Animation Linear", "UV Animation Parameter"
};

const char *RSchunks[] = { "Unused 1", "Unused 2", "Unused 3",
	"Pipeline Set", "Unused 5", "Unused 6", "Specular Material",
	"Unused 8", "2dfx", "Night Vertex Colors", "Collision Model",
	"Unused 12", "Reflection Material", "Mesh Extension", "Frame",
	"Unused 16"
};

std::string getChunkName(uint32 i)
{
	switch (i) {
	case 0x50E:
		return "Bin Mesh PLG";
		break;
	case 0x510:
		return "Native Data PLG";
		break;
	case 0xF21E:
		return "ZModeler Lock";
		break;
	default:
		break;
	}

	if (i <= 45)
		return chunks[i];
	else if (i <= 0x0253F2FF && i >= 0x0253F2F0)
		return RSchunks[i-0x0253F2F0];
	else if (i <= 0x0135 && i >= 0x0101)
		return toolkitchunks0[i-0x0101];
	else if (i <= 0x01C0 && i >= 0x0181)
		return toolkitchunks1[i-0x0181];
	else
		return "Unknown";
}

} // namespace rw

#ifdef RWLIB_INCLUDE_FRAMEWORK_ENTRYPOINTS

#ifdef _WIN32
#include "native.win32.hxx"
#endif

namespace rw
{

// Definition of the framework entry points.
extern LibraryVersion app_version( void );
extern int32 rwmain( Interface *engineInterface );

// The standard RW entry point.
static int _standard_rw_entry_point( void )
{
    using namespace rw;

    // Get the desired engine version.
    LibraryVersion engineVer = app_version();

    Interface *engineInterface = rw::CreateEngine( engineVer );

    if ( engineInterface )
    {
        int32 returnCode = -1;

        try
        {
            // Call into the main application.
            returnCode = rwmain( engineInterface );
        }
        catch( RwException& )
        {
            // We encountered some RenderWare exception.
            // We want to return -1.
#if defined(_DEBUG) && defined(_MSC_VER)
            __debugbreak();
#endif
        }

        // Delete our engine again.
        rw::DeleteEngine( engineInterface );

        if ( returnCode != 0 )
        {
            return -1;
        }
    }
    else
    {
        assert( 0 );
    }

    return 0;
}

#ifdef _WIN32

BOOL WINAPI frameworkEntryPoint_win32( HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow )
{
	return ( _standard_rw_entry_point() != 0 );
}

#else
#error Need framework entry point.
#endif

};

#ifdef RWLIB_APPLICATION_ENTRYPOINT_CRT

int main( int argc, char *argv[] )
{
	return rw::_standard_rw_entry_point();
}

#elif defined(RWLIB_APPLICATION_ENTRYPOINT_WIN32)

BOOL WINAPI WinMain( HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow )
{
	return rw::frameworkEntryPoint_win32( hInst, hPrevInst, lpCmdLine, nCmdShow );
}

#endif //ENTRYPOINT SELECT.

#endif //RWLIB_INCLUDE_FRAMEWORK_ENTRYPOINTS