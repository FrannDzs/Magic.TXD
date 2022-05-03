#include "StdInc.h"

#include "rwutils.hxx"

RenderWareThumbnailProvider::RenderWareThumbnailProvider( void ) : refCount( 1 )
{
    this->isInitialized = false;
    this->thumbObj = NULL;

    module_refCount++;
}

RenderWareThumbnailProvider::~RenderWareThumbnailProvider( void )
{
    // If we have a RenderWare object, free it.
    if ( rw::RwObject *rwObj = this->thumbObj )
    {
        rwEngine->DeleteRwObject( rwObj );
    }

    // TODO: this is actually crap, because after this operation we also have to execute code, anyway.
    // Have to think about this.
    module_refCount--;
}

IFACEMETHODIMP RenderWareThumbnailProvider::QueryInterface( REFIID riid, void **ppOut )
{
    if ( ppOut == NULL )
        return E_POINTER;

    if ( riid == __uuidof(IThumbnailProvider) )
    {
        this->refCount++;

        IThumbnailProvider *comObj = this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IInitializeWithStream) )
    {
        this->refCount++;
        
        IInitializeWithStream *comObj = this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IUnknown) )
    {
        this->refCount++;

        IUnknown *comObj = (IThumbnailProvider*)this;

        *ppOut = comObj;
        return S_OK;
    }

    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) RenderWareThumbnailProvider::AddRef( void )
{
    return ++this->refCount;
}

IFACEMETHODIMP_(ULONG) RenderWareThumbnailProvider::Release( void )
{
    unsigned long newRefCount = --this->refCount;

    if ( newRefCount == 0 )
    {
        delete this;
    }

    return newRefCount;
}

IFACEMETHODIMP RenderWareThumbnailProvider::Initialize( IStream *pStream, DWORD grfMode )
{
    if ( this->isInitialized )
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

    try
    {
        // We decide to handle the thing over here.
        // So we want to read the RenderWare object.
        rw::Stream *rwStream = RwStreamCreateFromWin32( rwEngine, pStream );

        if ( rwStream )
        {
            try
            {
                this->thumbObj = rwEngine->Deserialize( rwStream );
            }
            catch( ... )
            {
                // We failed for some reason.
            }

            rwEngine->DeleteStream( rwStream );
        }

        this->isInitialized = true;

        return S_OK;
    }
    catch( ... )
    {
        // Shit.
        return E_FAIL;
    }
}

static void adjustToMaxDimm( rw::uint32 width, rw::uint32 height, rw::uint32 maxDimm, rw::uint32& recWidth, rw::uint32& recHeight )
{
    if ( width > maxDimm )
    {
        double aspectRatio = (double)height / (double)width;

        width = maxDimm;
        height = (rw::uint32)( (double)width * aspectRatio );
    }
    
    if ( height > maxDimm )
    {
        double aspectRatio = (double)width / (double)height;

        height = maxDimm;
        width = (rw::uint32)( (double)height * aspectRatio );
    }

    recWidth = width;
    recHeight = height;
}

struct bitmapPixel
{
    unsigned char b, g, r, a;
};

static HRESULT rasterToHBITMAP( rw::Raster *raster, rw::uint32 bmpWidth, rw::uint32 bmpHeight, HBITMAP *pBitmap, bool& hasAlphaOut )
{
    rw::Bitmap pixelData = raster->getBitmap();

    // We should scale the bitmap.
    pixelData.scale( rwEngine, bmpWidth, bmpHeight );

    // We want to encode as 32bit HBITMAP.
    rw::uint32 width, height;
    pixelData.getSize( width, height );

    const size_t memSize = sizeof( bitmapPixel ) * width * height;

    void *mem = malloc( memSize );

    if ( !mem )
        return E_OUTOFMEMORY;

    BITMAPINFO bmi;
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -(LONG)height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = 0;
    bmi.bmiHeader.biSizeImage = 0;
    bmi.bmiHeader.biXPelsPerMeter = 0;
    bmi.bmiHeader.biYPelsPerMeter = 0;
    bmi.bmiHeader.biClrUsed = 0;
    bmi.bmiHeader.biClrImportant = 0;

    // Fill colors.
    bool hasAlpha = false;

    void *colorStart = mem;

    try
    {
        for ( rw::uint32 y = 0; y < height; y++ )
        {
            for ( rw::uint32 x = 0; x < width; x++ )
            {
                bitmapPixel *pix = (bitmapPixel*)colorStart + y * width + x;

                rw::uint8 r, g, b, a;

                bool gotColor = pixelData.browsecolor( x, y, r, g, b, a );

                if ( !gotColor )
                {
                    r = 0;
                    g = 0;
                    b = 0;
                    a = 255;
                }

                if ( a != 255 )
                {
                    hasAlpha = true;
                }

                pix->r = r;
                pix->g = g;
                pix->b = b;
                pix->a = a;
            }
        }
    }
    catch( ... )
    {
        free( mem );

        throw;
    }

    // Create the bitmap!
    void *ppv;

    HBITMAP resBmp = CreateDIBSection( NULL, &bmi, DIB_RGB_COLORS, &ppv, NULL, 0 );

    if ( resBmp )
    {
        SetDIBits( NULL, resBmp, 0, height, colorStart, &bmi, DIB_RGB_COLORS );
    }

    free( mem );

    if ( !resBmp )
        return S_FALSE;

    // Give the result HBITMAP to the runtime.
    *pBitmap = resBmp;
    hasAlphaOut = hasAlpha;

    // We do not know about alpha.

    return S_OK;
}

static HRESULT generateRasterPreviewHBITMAP( UINT cx, rw::Raster *texRaster, HBITMAP *pBitmap, WTS_ALPHATYPE *pAlphaType )
{
    // So we want to turn this into an experimental HBITMAP.
    // Lets do it!
    // First determine the final size that it should have.
    rw::uint32 width, height;
    texRaster->getSize( width, height );

    rw::uint32 recWidth, recHeight;
    {
        adjustToMaxDimm( width, height, cx, recWidth, recHeight );
    }

    rw::Raster *srcRaster = rw::AcquireRaster( texRaster );

    HRESULT res;

    try
    {
        // Do stuff.
        bool hasAlpha = false;
        HBITMAP bmp;

        res = rasterToHBITMAP( srcRaster, recWidth, recHeight, &bmp, hasAlpha );

        if ( res == S_OK )
        {
            *pBitmap = bmp;
            *pAlphaType = ( hasAlpha ? WTSAT_ARGB : WTSAT_RGB );
        }
    }
    catch( ... )
    {
        rw::DeleteRaster( srcRaster );

        throw;
    }

    // Clean up.
    rw::DeleteRaster( srcRaster );

    return res;
}

IFACEMETHODIMP RenderWareThumbnailProvider::GetThumbnail( UINT cx, HBITMAP *pBitmap, WTS_ALPHATYPE *pAlphaType )
{
    if ( !this->isInitialized )
        return S_FALSE;

    // We can always have no TXD available.
    if ( rw::RwObject *thumbObj = this->thumbObj )
    {
        try
        {
            // It all depends on the kind of object we have!
            if ( rw::TexDictionary *txd = rw::ToTexDictionary( rwEngine, thumbObj ) )
            {
                // As a test, return the first raster of the TXD.
                if ( rw::TextureBase *texHandle = GetFirstTexture( txd ) )
                {
                    if ( rw::Raster *texRaster = texHandle->GetRaster() )
                    {
                        return generateRasterPreviewHBITMAP( cx, texRaster, pBitmap, pAlphaType );
                    }
                }
            }
            else if ( rw::TextureBase *texHandle = rw::ToTexture( rwEngine, this->thumbObj ) )
            {
                if ( rw::Raster *texRaster = texHandle->GetRaster() )
                {
                    return generateRasterPreviewHBITMAP( cx, texRaster, pBitmap, pAlphaType );
                }
            }
        }
        catch( ... )
        {
            // Ignore any kind of runtime error we could encounter.
        }
    }

    return S_FALSE;
}