/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.raster.utils.cpp
*  PURPOSE:     Casual utilities related to the Raster object.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.raster.hxx"

#include <sdk/UniChar.h>

namespace rw
{

void Raster::getSizeRules( rasterSizeRules& rulesOut ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    // We need to fetch that from the native texture.
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( (Raster*)this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( (Raster*)this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    nativeTextureSizeRules nativeSizeRules;
    texProvider->GetTextureSizeRules( platformTex, nativeSizeRules );

    // Give opaque data to the runtime.
    rulesOut.powerOfTwo = nativeSizeRules.powerOfTwo;
    rulesOut.squared = nativeSizeRules.squared;
    rulesOut.multipleOf = nativeSizeRules.multipleOf;
    rulesOut.multipleOfValue = nativeSizeRules.multipleOfValue;
    rulesOut.maximum = nativeSizeRules.maximum;
    rulesOut.maxVal = nativeSizeRules.maxVal;
}

bool rasterSizeRules::verifyDimensions( uint32 width, uint32 height ) const
{
    nativeTextureSizeRules nativeSizeRules;
    nativeSizeRules.powerOfTwo = this->powerOfTwo;
    nativeSizeRules.squared = this->squared;
    nativeSizeRules.multipleOf = this->multipleOf;
    nativeSizeRules.multipleOfValue = this->multipleOfValue;
    nativeSizeRules.maximum = this->maximum;
    nativeSizeRules.maxVal = this->maxVal;

    return nativeSizeRules.IsMipmapSizeValid( width, height );
}

void rasterSizeRules::adjustDimensions( uint32 width, uint32 height, uint32& newWidth, uint32& newHeight ) const
{
    nativeTextureSizeRules nativeSizeRules;
    nativeSizeRules.powerOfTwo = this->powerOfTwo;
    nativeSizeRules.squared = this->squared;
    nativeSizeRules.multipleOf = this->multipleOf;
    nativeSizeRules.multipleOfValue = this->multipleOfValue;
    nativeSizeRules.maximum = this->maximum;
    nativeSizeRules.maxVal = this->maxVal;

    nativeSizeRules.CalculateValidLayerDimensions( width, height, newWidth, newHeight );
}

void Raster::getFormatString( char *buf, size_t bufSize, size_t& lengthOut ) const
{
    scoped_rwlock_reader <rwlock> rasterConsistency( GetRasterLock( this ) );

    // Ask the native platform texture to deliver us a format string.
    PlatformTexture *platformTex = this->platformData;

    if ( !platformTex )
    {
        throw RasterNotInitializedException( AcquireRaster( (Raster*)this ), L"NATIVETEX_FRIENDLYNAME" );
    }

    Interface *engineInterface = this->engineInterface;

    const texNativeTypeProvider *texProvider = GetNativeTextureTypeProvider( engineInterface, platformTex );

    if ( !texProvider )
    {
        throw RasterInternalErrorException( AcquireRaster( (Raster*)this ), L"RASTER_REASON_INVNATIVEDATA" );
    }

    texProvider->GetTextureFormatString( engineInterface, platformTex, buf, bufSize, lengthOut );
}

/*
    Raster helper API.
    So we have got standardized names in third-party programs.
*/

const char* GetRasterFormatStandardName( eRasterFormat theFormat )
{
    const char *name = "unknown";

    if ( theFormat == RASTER_DEFAULT )
    {
        name = "unspecified";
    }
    else if ( theFormat == RASTER_1555 )
    {
        name = "RASTER_1555";
    }
    else if ( theFormat == RASTER_565 )
    {
        name = "RASTER_565";
    }
    else if ( theFormat == RASTER_4444 )
    {
        name = "RASTER_4444";
    }
    else if ( theFormat == RASTER_LUM )
    {
        name = "RASTER_LUM";
    }
    else if ( theFormat == RASTER_8888 )
    {
        name = "RASTER_8888";
    }
    else if ( theFormat == RASTER_888 )
    {
        name = "RASTER_888";
    }
    else if ( theFormat == RASTER_16 )
    {
        name = "RASTER_16";
    }
    else if ( theFormat == RASTER_24 )
    {
        name = "RASTER_24";
    }
    else if ( theFormat == RASTER_32 )
    {
        name = "RASTER_32";
    }
    else if ( theFormat == RASTER_555 )
    {
        name = "RASTER_555";
    }
    // NEW FORMATS.
    else if ( theFormat == RASTER_LUM_ALPHA )
    {
        name = "RASTER_LUM_ALPHA";
    }

    return name;
}

// For shorter code.
template <typename leftCharType, typename rightCharType>
AINLINE bool strieq( const leftCharType *left, const rightCharType *right )
{
    return StringEqualToZero( left, right, false );
}

eRasterFormat FindRasterFormatByName( const char *theName )
{
    // TODO: make this as human-friendly as possible!

    if ( strieq( theName, "RASTER_1555" ) ||
         strieq( theName, "1555" ) )
    {
        return RASTER_1555;
    }
    else if ( strieq( theName, "RASTER_565" ) ||
              strieq( theName, "565" ) )
    {
        return RASTER_565;
    }
    else if ( strieq( theName, "RASTER_4444" ) ||
              strieq( theName, "4444" ) )
    {
        return RASTER_4444;
    }
    else if ( strieq( theName, "RASTER_LUM" ) ||
              strieq( theName, "LUM" ) )
    {
        return RASTER_LUM;
    }
    else if ( strieq( theName, "RASTER_8888" ) ||
              strieq( theName, "8888" ) )
    {
        return RASTER_8888;
    }
    else if ( strieq( theName, "RASTER_888" ) ||
              strieq( theName, "888" ) )
    {
        return RASTER_888;
    }
    else if ( strieq( theName, "RASTER_16" ) )
    {
        return RASTER_16;
    }
    else if ( strieq( theName, "RASTER_24" ) )
    {
        return RASTER_24;
    }
    else if ( strieq( theName, "RASTER_32" ) )
    {
        return RASTER_32;
    }
    else if ( strieq( theName, "RASTER_555" ) ||
              strieq( theName, "555" ) )
    {
        return RASTER_555;
    }
    // NEW FORMATS.
    else if ( strieq( theName, "RASTER_LUM_ALPHA" ) ||
              strieq( theName, "LUM_ALPHA" ) )
    {
        return RASTER_LUM_ALPHA;
    }

    // No idea.
    return RASTER_DEFAULT;
}

}
