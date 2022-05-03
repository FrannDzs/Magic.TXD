/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwimaging.jpeg.cpp
*  PURPOSE:     JPEG image format implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwimaging.hxx"

#include "pluginutil.hxx"

#include "pixelformat.hxx"

#include "streamutil.hxx"

#ifdef RWLIB_INCLUDE_JPEG_IMAGING

#include <jpeglib.h>

#endif //RWLIB_INCLUDE_JPEG_IMAGING

namespace rw
{

#ifdef RWLIB_INCLUDE_JPEG_IMAGING
static const JOCTET FakeEOI[] = { 0xFF, JPEG_EOI };

static const imaging_filename_ext jpeg_ext[] =
{
    { "JPEG", false },
    { "JPG", true }
};

// JPEG compliant serialization library for RenderWare.
struct jpegImagingExtension : public imagingFormatExtension
{
    struct jpeg_block_header
    {
        endian::big_endian <uint8> blockChecksum;
        endian::big_endian <uint8> app0;
        endian::big_endian <uint16> length;
    };

    bool IsStreamCompatible( Interface *engineInterface, Stream *inputStream ) const override
    {
        // This is one of the most ridiculous binary streams I have ever seen.

        // Check the JPEG checksum.
        endian::big_endian <uint16> checksum;

        size_t readCount = inputStream->read( &checksum, sizeof( checksum ) );

        if ( readCount != sizeof( checksum ) )
        {
            return false;
        }

        // Has to be correct.
        if ( checksum != 0xFFD8 )
        {
            return false;
        }

        // Now comes a series of blocks.
        bool imageDataFollows = false;

        while ( true )
        {
            jpeg_block_header header;

            size_t readCount = inputStream->read( &header, sizeof( header ) );

            if ( readCount != sizeof( header ) )
            {
                // Something went horribly wrong.
                return false;
            }

            // Verify that this block is valid.
            if ( header.blockChecksum != 0xFF )
            {
                return false;
            }

            // Check block type.
            uint8 app0 = header.app0;

            // If we find an SOS marker, we are at valid image data.
            if ( app0 == 0xDA )
            {
                imageDataFollows = true;
            }

            // Skip the data.
            uint32 blockLength = ( header.length - 2 );

            if ( blockLength != 0 )
            {
                skipAvailable( inputStream, blockLength );
            }

            if ( imageDataFollows )
            {
                break;
            }
        }

        if ( !imageDataFollows )
        {
            // We do not want JPEGs without image data.
            return false;
        }

        // Alright, now comes the tough part.
        // We have to scan the file for the end of image data...
        int64 startOfImageData = inputStream->tell();

        bool hasFoundEndOfImage = false;

        while ( true )
        {
            uint8 maybe_app0;
            {
                size_t readCount = inputStream->read( &maybe_app0, sizeof( maybe_app0 ) );

                if ( readCount != sizeof( maybe_app0 ) )
                {
                    return false;
                }
            }

            // Are we maybe a block?
            // Otherwise continue scanning.
            if ( maybe_app0 == 0xFF )
            {
                // Check whether we are at the end.
                uint8 possible_end_marker;
                {
                    size_t readCount = inputStream->read( &possible_end_marker, sizeof( possible_end_marker ) );

                    if ( readCount != sizeof( possible_end_marker ) )
                    {
                        return false;
                    }
                }

                if ( possible_end_marker != 0x00 )
                {
                    hasFoundEndOfImage = true;
                    break;
                }

                // We want to search further...
                if ( possible_end_marker == 0xFF )
                {
                    inputStream->seek( -1, rw::RWSEEK_CUR );
                }
            }
        }

        if ( !hasFoundEndOfImage )
        {
            return false;
        }

        // Check whether it makes any sense.
        int64 endOfImageOffset = ( inputStream->tell() - 2 );

        int64 imageDataSize = ( endOfImageOffset - startOfImageData );

        if ( imageDataSize >= 0x100000000 )
        {
            // The image is way too big.
            return false;
        }

        // Yay! We are a valid JPEG!
        return true;
    }

    void GetStorageCapabilities( pixelCapabilities& capsOut ) const override
    {
        capsOut.supportsDXT1 = false;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = false;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = false;
        capsOut.supportsPalette = true;
    }

    struct error_manager : public jpeg_error_mgr
    {
        Interface *engineInterface;
    };

    static noreturn_t jpeg_onFatalError( j_common_ptr cinfo )
    {
        // We encountered some really weird error.
        // Terminate now.
        error_manager *errorMgr = (error_manager*)cinfo->err;
        (void)errorMgr;

        char msgbuf[ JMSG_LENGTH_MAX + 1 ];

        (*cinfo->err->format_message)(cinfo, msgbuf);

        // Some safety stuff.
        msgbuf[ JMSG_LENGTH_MAX ] = '\0';

        throw ImagingUnlocalizedAInternalErrorException( "JPEG", L"JPEG_TEMPL_FATALERR", msgbuf );
    }

    static void jpeg_outputMessage( j_common_ptr cinfo )
    {
        // We do not use the msg_level param.
        error_manager *errorMgr = (error_manager*)cinfo->err;
        (void)errorMgr;

        char msgbuf[ JMSG_LENGTH_MAX + 1 ];

        (*cinfo->err->format_message)(cinfo, msgbuf);

        // Some safety stuff.
        msgbuf[ JMSG_LENGTH_MAX ] = '\0';

        // Need to internationalize it.
        auto wide_msg = CharacterUtil::ConvertStrings <char, wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( msgbuf );

        // Print it as warning.
        errorMgr->engineInterface->PushWarningSingleTemplate( L"JPEG_WARN_TEMPLATE", L"message", wide_msg.GetConstString() );
    }

    struct stream_source_manager : public jpeg_source_mgr
    {
        Interface *engineInterface;
        Stream *theStream;

        void *readbuf;
        size_t rdbufsize;
    };

    static void init_source( j_decompress_ptr cinfo )
    {
        // Nothing to do here.
        return;
    }

    static boolean fill_input_buffer( j_decompress_ptr cinfo )
    {
        // The runtime requested more data.
        stream_source_manager *srcman = (stream_source_manager*)cinfo->src;

        void *rdbuf = srcman->readbuf;
        size_t rdbufsize = srcman->rdbufsize;

        size_t actualReadCount = srcman->theStream->read( rdbuf, rdbufsize );

        if ( actualReadCount == 0 )
        {
            // We are at the end of the file, somehow.
            // Terminate us.
            srcman->next_input_byte = FakeEOI;
            srcman->bytes_in_buffer = 2;
        }
        else
        {
            // Return the buffer to the runtime.
            srcman->next_input_byte = (JOCTET*)rdbuf;
            srcman->bytes_in_buffer = actualReadCount;
        }

        return TRUE;
    }

    static void skip_input_data( j_decompress_ptr cinfo, long num_bytes )
    {
        if ( num_bytes < 0 )
        {
            throw ImagingStructuralErrorException( "JPEG", L"JPEG_STRUCTERR_BYTESKIPCOUNT" );
        }

        size_t num_skip_bytes = (size_t)num_bytes;

        stream_source_manager *srcman = (stream_source_manager*)cinfo->src;

        // Skip some shit.
        size_t currentByteCount = srcman->bytes_in_buffer;
        size_t leftBytes = 0;

        if ( currentByteCount <= num_skip_bytes )
        {
            srcman->bytes_in_buffer = 0;
            srcman->next_input_byte = nullptr;

            leftBytes = ( num_skip_bytes - currentByteCount );
        }
        else
        {
            srcman->bytes_in_buffer -= num_skip_bytes;
            srcman->next_input_byte += num_skip_bytes;

            leftBytes = 0;
        }

        // If we want to skip some data from the actual stream, do so.
        if ( leftBytes != 0 )
        {
            srcman->theStream->skip( leftBytes );
        }
    }

    static boolean resync_to_restart( j_decompress_ptr cinfo, int desired )
    {
        // Use default.
        return jpeg_resync_to_restart( cinfo, desired );
    }

    static void term_source( j_decompress_ptr cinfo )
    {
        // Nothing to do.
        return;
    }

    void DeserializeImage( Interface *engineInterface, Stream *inputStream, imagingLayerTraversal& outputPixels ) const override
    {
        // Alright. We should read this thing now.
        jpeg_decompress_struct decompress_info;

        // Set up the message system.
        error_manager error_man;
        error_man.engineInterface = engineInterface;

        decompress_info.err = jpeg_std_error(&error_man);

        // Customize the error management.
        error_man.error_exit = jpeg_onFatalError;
        error_man.output_message = jpeg_outputMessage;

        // Create the decompression environment.
        jpeg_create_decompress( &decompress_info );

        try
        {
            // We allocate some read buffer that we gonna use to fill pixels.
            const size_t readbuffer_size = 1024;

            void *rdbuf = engineInterface->MemAllocate( readbuffer_size );

            try
            {
                // Create the source manager.
                stream_source_manager srcman;

                srcman.engineInterface = engineInterface;
                srcman.theStream = inputStream;
                srcman.readbuf = rdbuf;
                srcman.rdbufsize = readbuffer_size;

                srcman.init_source = init_source;
                srcman.fill_input_buffer = fill_input_buffer;
                srcman.skip_input_data = skip_input_data;
                srcman.resync_to_restart = resync_to_restart;
                srcman.term_source = term_source;

                // We fill this on our first call to fill the buffer.
                srcman.next_input_byte = nullptr;
                srcman.bytes_in_buffer = 0;

                decompress_info.src = &srcman;

                // Alright, we begin doing the reading.
                jpeg_read_header( &decompress_info, TRUE );

                jpeg_start_decompress( &decompress_info );

                uint32 width = decompress_info.output_width;
                uint32 height = decompress_info.output_height;

                // Decide about the storage format of the pixels.
                eRasterFormat dstRasterFormat;
                uint32 dstDepth;
                uint32 dstRowAlignment = 1;
                eColorOrdering dstColorOrder = COLOR_RGBA;

                unsigned int numComponents = decompress_info.out_color_components;

                J_COLOR_SPACE colorSpace = decompress_info.out_color_space;

                if ( numComponents == 3 && colorSpace == JCS_RGB )
                {
                    dstRasterFormat = RASTER_888;
                    dstDepth = 24;
                }
                else if ( numComponents == 1 && colorSpace == JCS_GRAYSCALE )
                {
                    // We clearly are 8bit LUM here.
                    // If we are 4bit LUM, we should not have a problem either.
                    dstRasterFormat = RASTER_LUM;
                    dstDepth = 8;
                }
                else
                {
                    throw ImagingInternalErrorException( "JPEG", L"JPEG_INTERNERR_UNKFMT" );
                }

                // Create the target buffer for our pixels.
                rasterRowSize dstRowSize = getRasterDataRowSize( width, dstDepth, dstRowAlignment );

                uint32 dstDataSize = getRasterDataSizeByRowSize( dstRowSize, height );

                void *dstTexels = engineInterface->PixelAllocate( dstDataSize );

                try
                {
                    // We need to create a scanline buffer.
                    // Basically, we ask the library to output the entire image in one go.
                    void **scanlineArray = (void**)engineInterface->MemAllocate( height * sizeof(void*), alignof(void*) );

                    try
                    {
                        // Set up the scanline array with proper pointers to our destination buffer.
                        // NOTE: has to be byte aligned.
#ifdef _DEBUG
                        assert( dstRowSize.mode == eRasterDataRowMode::ALIGNED );
#endif //_DEBUG

                        for ( uint32 row = 0; row < height; row++ )
                        {
                            scanlineArray[ row ] = getTexelDataRow( dstTexels, dstRowSize, row ).aligned.aligned_rowPtr;
                        }

                        // Read image data!
                        void **curScanlineArrayPtr = scanlineArray;

                        while ( decompress_info.output_scanline < height )
                        {
                            uint32 curRowIndex = (uint32)( curScanlineArrayPtr - scanlineArray );

                            size_t numReadLines = jpeg_read_scanlines( &decompress_info, (JSAMPARRAY)curScanlineArrayPtr, height - curRowIndex );

                            // Advance the array of scanlines!
                            curScanlineArrayPtr += numReadLines;
                        }

                        // Finish stuff.
                        jpeg_finish_decompress( &decompress_info );

                        // We are successful!
                        // Let's return stuff to the runtime.
                        outputPixels.layerWidth = width;
                        outputPixels.layerHeight = height;
                        outputPixels.mipWidth = width;
                        outputPixels.mipHeight = height;
                        outputPixels.texelSource = dstTexels;
                        outputPixels.dataSize = dstDataSize;
                        outputPixels.rasterFormat = dstRasterFormat;
                        outputPixels.depth = dstDepth;
                        outputPixels.rowAlignment = dstRowAlignment;
                        outputPixels.colorOrder = dstColorOrder;
                        outputPixels.paletteType = PALETTE_NONE;
                        outputPixels.paletteData = nullptr;
                        outputPixels.paletteSize = 0;
                        outputPixels.compressionType = RWCOMPRESS_NONE;
                        outputPixels.hasAlpha = false;  // JPEGs never have alpha.
                    }
                    catch( ... )
                    {
                        engineInterface->MemFree( scanlineArray );

                        throw;
                    }

                    // We should release the scanlines again.
                    engineInterface->MemFree( scanlineArray );
                }
                catch( ... )
                {
                    // Some error occured, so clear all memory.
                    engineInterface->PixelFree( dstTexels );

                    throw;
                }

                // OK.
            }
            catch( ... )
            {
                engineInterface->MemFree( rdbuf );

                throw;
            }

            engineInterface->MemFree( rdbuf );
        }
        catch( ... )
        {
            // Clean up.
            jpeg_destroy_decompress( &decompress_info );

            throw;
        }

        // Clean up.
        jpeg_destroy_decompress( &decompress_info );
    }

    struct stream_destination_manager : public jpeg_destination_mgr
    {
        Interface *engineInterface;
        Stream *theStream;

        void *writebuf;
        size_t writebufsize;
    };

    static void init_destination( j_compress_ptr cinfo )
    {
        return;
    }

    static boolean empty_output_buffer( j_compress_ptr cinfo )
    {
        stream_destination_manager *dstMgr = (stream_destination_manager*)cinfo->dest;

        // If there is stuff pending to be written, do that.
        void *writebuf = dstMgr->writebuf;
        size_t writebufsize = dstMgr->writebufsize;

        dstMgr->theStream->write( writebuf, writebufsize );

        // We need to provide more space to let the guy write.
        dstMgr->next_output_byte = (JOCTET*)writebuf;
        dstMgr->free_in_buffer = writebufsize;

        return TRUE;
    }

    static void term_destination( j_compress_ptr cinfo )
    {
        // We need to finish off the write operation.
        stream_destination_manager *dstMgr = (stream_destination_manager*)cinfo->dest;

        void *writebuf = dstMgr->writebuf;

        size_t actualWriteCount = ( (char*)dstMgr->next_output_byte - (char*)writebuf );

        if ( actualWriteCount != 0 )
        {
            dstMgr->theStream->write( writebuf, actualWriteCount );
        }
    }

    void SerializeImage( Interface *engineInterface, Stream *outputStream, const imagingLayerTraversal& inputPixels ) const override
    {
        // We can only accept uncompressed pixels.
        if ( inputPixels.compressionType != RWCOMPRESS_NONE )
        {
            throw ImagingInvalidConfigurationException( "JPEG", L"JPEG_INVALIDCFG_NOCOMPRINPUT" );
        }

        // Now we do stuff the other way round.
        // We give data to the JPEG library, so the library can compress it!
        jpeg_compress_struct compress_info;

        // Set up the message system.
        error_manager error_man;
        error_man.engineInterface = engineInterface;

        compress_info.err = jpeg_std_error(&error_man);

        // Customize the error management.
        error_man.error_exit = jpeg_onFatalError;
        error_man.output_message = jpeg_outputMessage;

        jpeg_create_compress( &compress_info );

        try
        {
            // Allocate a buffer for file operations.
            const size_t writebuf_size = 1024;

            void *writebuf = engineInterface->MemAllocate( writebuf_size, 1 );

            try
            {
                // Set up the destination manager.
                stream_destination_manager dstman;

                dstman.engineInterface = engineInterface;
                dstman.theStream = outputStream;

                dstman.writebuf = writebuf;
                dstman.writebufsize = writebuf_size;

                dstman.init_destination = init_destination;
                dstman.empty_output_buffer = empty_output_buffer;
                dstman.term_destination = term_destination;

                // We have to initialize this thing.
                dstman.next_output_byte = (JOCTET*)writebuf;
                dstman.free_in_buffer = writebuf_size;

                compress_info.dest = &dstman;

                // We have to decide in what format we should write the destination data.
                // Most of the time we will just write as RGB, but LUM8 can be grayscale encoded.
                eRasterFormat srcRasterFormat = inputPixels.rasterFormat;
                uint32 srcDepth = inputPixels.depth;
                uint32 srcRowAlignment = inputPixels.rowAlignment;
                eColorOrdering srcColorOrder = inputPixels.colorOrder;
                ePaletteType srcPaletteType = inputPixels.paletteType;
                const void *srcPaletteData = inputPixels.paletteData;
                uint32 srcPaletteSize = inputPixels.paletteSize;

                eRasterFormat dstRasterFormat;
                uint32 dstDepth;
                uint32 dstRowAlignment = 1;
                eColorOrdering dstColorOrder = COLOR_RGBA;

                // Those have to be given as part of a handshake agreement.
                uint32 numColorComponents;
                J_COLOR_SPACE colorSpace;

                // We decide by color model.
                eColorModel rasterColorModel = getColorModelFromRasterFormat( srcRasterFormat );

                if ( rasterColorModel == COLORMODEL_RGBA )
                {
                    dstRasterFormat = RASTER_888;
                    dstDepth = 24;

                    numColorComponents = 3;
                    colorSpace = JCS_RGB;
                }
                else if ( rasterColorModel == COLORMODEL_LUMINANCE )
                {
                    // We want to map to 8bit LUM for the JPEG library.
                    dstRasterFormat = RASTER_LUM;
                    dstDepth = 8;

                    numColorComponents = 1;
                    colorSpace = JCS_GRAYSCALE;
                }
                else
                {
                    throw ImagingInvalidConfigurationException( "JPEG", L"JPEG_INVALIDCFG_RASTERFMT" );
                }

                uint32 width = inputPixels.mipWidth;
                uint32 height = inputPixels.mipHeight;
                void *srcTexels = inputPixels.texelSource;

                // Generate destination data that we will directly feed to the compressor.
                // Our strategy is to generate the entire input buffer here.
                void *dstTexels = srcTexels;

                rasterRowSize dstRowSize = getRasterDataRowSize( width, dstDepth, dstRowAlignment );

                bool needsConv =
                    doesRawMipmapBufferNeedFullConversion(
                        width,
                        srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType,
                        dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, PALETTE_NONE
                    );

                if ( needsConv )
                {
                    // Maybe we have to convert texels.
                    uint32 dstDataSize;

                    bool couldConvert =
                        ConvertMipmapLayerNative(
                            engineInterface, width, height, inputPixels.layerWidth, inputPixels.layerHeight, srcTexels, inputPixels.dataSize,
                            srcRasterFormat, srcDepth, srcRowAlignment, srcColorOrder, srcPaletteType, srcPaletteData, srcPaletteSize, RWCOMPRESS_NONE,
                            dstRasterFormat, dstDepth, dstRowAlignment, dstColorOrder, PALETTE_NONE, nullptr, 0, RWCOMPRESS_NONE,
                            false,
                            width, height,
                            dstTexels, dstDataSize
                        );
                    (void)couldConvert;
                }

                try
                {
                    // Allocate a buffer for row pointers that we will pass to the compressor.
                    void **rowpointers = (void**)engineInterface->MemAllocate( sizeof(void*) * height, alignof(void*) );

                    try
                    {
                        // Set up the row pointers.
                        // NOTE: has to be byte-aligned.
#ifdef _DEBUG
                        assert( dstRowSize.mode == eRasterDataRowMode::ALIGNED );
#endif //_DEBUG

                        for ( uint32 row = 0; row < height; row++ )
                        {
                            rowpointers[ row ] = getTexelDataRow( dstTexels, dstRowSize, row ).aligned.aligned_rowPtr;
                        }

                        // Set compression parameters.
                        compress_info.image_width = width;
                        compress_info.image_height = height;
                        compress_info.input_components = numColorComponents;
                        compress_info.in_color_space = colorSpace;

                        // Let the library calculate its internal compression parameters.
                        jpeg_set_defaults( &compress_info );

                        // Customize stuff.
                        jpeg_set_quality( &compress_info, 75, TRUE /* limit to baseline-JPEG values */ );

                        // We are ready to start the compression!
                        jpeg_start_compress( &compress_info, TRUE );

                        // Loop through all scanlines until everything is written.
                        void **currentScanlineArrayPtr = rowpointers;

                        while ( compress_info.next_scanline < height )
                        {
                            uint32 curRowIndex = (uint32)( currentScanlineArrayPtr - rowpointers );

                            uint32 writtenLines = jpeg_write_scanlines( &compress_info, (JSAMPARRAY)currentScanlineArrayPtr, height - curRowIndex );

                            // Advance the row pointers.
                            currentScanlineArrayPtr += writtenLines;
                        }

                        // We are finished, so lets write the end of the image.
                        jpeg_finish_compress( &compress_info );
                    }
                    catch( ... )
                    {
                        engineInterface->MemFree( rowpointers );

                        throw;
                    }

                    engineInterface->MemFree( rowpointers );
                }
                catch( ... )
                {
                    if ( srcTexels != dstTexels )
                    {
                        engineInterface->PixelFree( dstTexels );
                    }

                    throw;
                }

                // Clean up the texel buffer that we do not require anymore.
                if ( srcTexels != dstTexels )
                {
                    engineInterface->PixelFree( dstTexels );
                }
            }
            catch( ... )
            {
                engineInterface->MemFree( writebuf );

                throw;
            }

            // We need to release resources that are not required anymore.
            engineInterface->MemFree( writebuf );
        }
        catch( ... )
        {
            // Some error has happened, so terminate.
            jpeg_destroy_compress( &compress_info );

            throw;
        }

        // Clean up memory.
        jpeg_destroy_compress( &compress_info );
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterImagingFormat( engineInterface, "Joint Photographic Experts Group", IMAGING_COUNT_EXT(jpeg_ext), jpeg_ext, this );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterImagingFormat( engineInterface, this );
    }
};

static optional_struct_space <PluginDependantStructRegister <jpegImagingExtension, RwInterfaceFactory_t>> jpegExtensionStore;

#endif //RWLIB_INCLUDE_JPEG_IMAGING

void registerJPEGImagingExtension( void )
{
#ifdef RWLIB_INCLUDE_JPEG_IMAGING
    // Register the JPEG imaging format parser.
    jpegExtensionStore.Construct( engineFactory );
#endif //RWLIB_INCLUDE_JPEG_IMAGING
}

void unregisterJPEGImagingExtension( void )
{
#ifdef RWLIB_INCLUDE_JPEG_IMAGING
    jpegExtensionStore.Destroy();
#endif //RWLIB_INCLUDE_JPEG_IMAGING
}

};
