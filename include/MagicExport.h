/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/MagicExport.h
*  PURPOSE:     Header of the editor export interface.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <renderware.h>

#define MAGIC_CORE
#include <MagicFormats.h>

enum MAGIC_RASTER_FORMAT;
enum MAGIC_COLOR_ORDERING;
enum MAGIC_PALETTE_TYPE;

struct MagicFormatPluginExports : public MagicFormatPluginInterface
{
    bool PutTexelRGBA(
        void *texelSource, unsigned int texelIndex,
        unsigned int width, unsigned int rowAlignment, unsigned int rowi,
        MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth,
        MAGIC_COLOR_ORDERING colorOrder, unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha
    ) const override;

    bool BrowseTexelRGBA(
        const void *texelSource, unsigned int texelIndex,
        unsigned int width, unsigned int rowAlignment, unsigned int rowi,
        MAGIC_RASTER_FORMAT rasterFormat, unsigned int depth,
        MAGIC_COLOR_ORDERING colorOrder, MAGIC_PALETTE_TYPE paletteType, const void *paletteData, unsigned int paletteSize,
        unsigned char& redOut, unsigned char& greenOut, unsigned char& blueOut, unsigned char& alphaOut
    ) const override;
};