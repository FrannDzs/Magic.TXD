/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/textureviewport.h
*  PURPOSE:     Header of the main texture preview code.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QScrollArea>

// Receiver class of pixmaps.
struct pixmapReceiver
{
    friend struct cyclingAnimation;

protected:
    virtual void updatePixmap( QPixmap pixmap ) = 0;
};

class MainWindow;

enum class eViewportMode
{
    DISABLED,
    SYSTEM,
    TEXVIEW
};

class TexViewportWidget : public QScrollArea, public pixmapReceiver
{
public:
    TexViewportWidget( MainWindow *MainWnd );

    void setViewportMode( eViewportMode mode );
    eViewportMode getViewportMode( void ) const;

    // Swap the viewport to TEXVIEW mode and sets the pixmap as display image.
    void setTexturePixmap( QPixmap pixmap );
    void clearContent( void );

    void centerView( qreal crx, qreal cry, qreal offx = 0, qreal offy = 0 );
    bool zoomBy( QPointF centerPoint, qreal scaleFactor );

    void setShowBackground( bool background );
    bool getShowBackground( void ) const;

    void setShowFullImage( bool fullimg );
    bool getShowFullImage( void ) const;

    void setContentSize( int width, int height, bool scaledContents = false );

protected:
    bool viewportEvent( QEvent *evt ) override;
    void resizeEvent( QResizeEvent *evt ) override;

    // Called by the loading animation helper.
    void updatePixmap( QPixmap pixmap ) override;

private:
    void updateViewportScale( void );

    MainWindow *mainWnd;

    QWidget *contentWidget;

    eViewportMode mode;

    QPixmap contentPixmap;
    QPixmap backgroundPixmap;

    qreal imgScaleFactor;

    bool showFullImage;
    bool showBackground;

    int pad_horizontal;
    int pad_vertical;
};
