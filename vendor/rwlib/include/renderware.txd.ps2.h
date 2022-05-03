/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.txd.ps2.h
*  PURPOSE:     Public PlayStation2 native texture direct access.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PLAYSTATION2_NATIVETEX_PUBLIC_HEADER_
#define _RENDERWARE_PLAYSTATION2_NATIVETEX_PUBLIC_HEADER_

#include "renderware.h"

namespace rw
{

namespace ps2public
{

enum class eMemoryLayoutType
{
    PSMCT32,
    PSMCT24,
    PSMCT16,
    PSMCT16S,
    PSMT8,
    PSMT4,
    PSMT8H,
    PSMT4HL,
    PSMT4HH,
    PSMZ32,
    PSMZ24,
    PSMZ16,
    PSMZ16S
};

enum class eCLUTMemoryLayoutType
{
    PSMCT32,
    PSMCT16,
    PSMCT16S
};

struct ps2MipmapTransmissionData
{
    uint16 destX, destY;
};

struct ps2TextureMetrics
{
    uint32 transmissionAreaWidth, transmissionAreaHeight;
};

struct ps2NativeTextureDriverInterface
{
    // Performs texture memory allocation and returns the parameters that would be put into the native texture.
    virtual bool CalculateTextureMemoryAllocation(
        // Input values:
        eMemoryLayoutType pixelStorageFmt, eMemoryLayoutType bitbltTransferFmt, eCLUTMemoryLayoutType clutStorageFmt,
        const ps2TextureMetrics *mipmaps, size_t numMipmaps, const ps2TextureMetrics& clut,
        // Output values:
        uint32 mipmapBasePointer[], uint32 mipmapBufferWidth[], ps2MipmapTransmissionData mipmapTransData[],
        uint32& clutBasePointerOut, ps2MipmapTransmissionData& clutTransDataOut,
        uint32& textureBufPageSizeOut
    ) = 0;
};

} // namespace ps2public

} // namespace rw

#endif //_RENDERWARE_PLAYSTATION2_NATIVETEX_PUBLIC_HEADER_