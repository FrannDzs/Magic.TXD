#pragma once

struct RenderWareThumbnailProvider : public IThumbnailProvider, public IInitializeWithStream
{
    IFACEMETHODIMP QueryInterface( REFIID riid, void **ppOut ) override;
    IFACEMETHODIMP_(ULONG) AddRef( void ) override;
    IFACEMETHODIMP_(ULONG) Release( void ) override;

    IFACEMETHODIMP Initialize( IStream *pStream, DWORD grfMode ) override;
    IFACEMETHODIMP GetThumbnail( UINT cx, HBITMAP *pBitmap, WTS_ALPHATYPE *pAlphaType ) override;

    RenderWareThumbnailProvider( void );

protected:
    ~RenderWareThumbnailProvider( void );

private:
    bool isInitialized;

    std::atomic <unsigned long> refCount;

    rw::RwObject *thumbObj;
};