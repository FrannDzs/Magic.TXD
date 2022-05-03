/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/expensivetasks.h
*  PURPOSE:     Header of long executing tasks codebase.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#ifndef _MAGICTXD_EXPENSIVE_TASKS_HEADER_
#define _MAGICTXD_EXPENSIVE_TASKS_HEADER_

#include "txdlog.h"

// Forward declaration.
struct MainWindow;

struct ExpensiveTasks abstract
{
    rw::TexDictionary* loadTxdFile( QString fileName, bool silent = false );
    rw::TextureBase* loadTexture( const wchar_t *widePath, const char *natTexPlatformName, const filePath& extention );
    QImage makePreviewOfRaster( rw::Raster *rasterData, bool drawMipmapLayers );
    void changeTXDPlatform( rw::TexDictionary *txd, rw::rwStaticString <char> platform );

protected:
    virtual void pushLogMessage( QString msg, eLogMessageType msgType = LOGMSG_INFO ) = 0;
    virtual MainWindow* getMainWindow( void ) = 0;
};

#endif //_MAGICTXD_EXPENSIVE_TASKS_HEADER_