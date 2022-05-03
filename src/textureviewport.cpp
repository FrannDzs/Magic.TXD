/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/textureviewport.cpp
*  PURPOSE:     Code related to managing the main texture preview.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "textureViewport.h"

#include <QtWidgets/QScroller>
#include <QtGui/QTouchEvent>
#include <QtWidgets/QGestureEvent>
#include <QtGui/QPainter>

static const qreal _min_absolute_scalefactor = 0.01;

TexViewportWidget::TexViewportWidget( MainWindow *MainWnd )
{
    this->mainWnd = MainWnd;
    this->mode = eViewportMode::DISABLED;
    this->showBackground = false;
    this->showFullImage = false;
    this->imgScaleFactor = 1.0;
    this->pad_horizontal = 0;
    this->pad_vertical = 0;

    struct ContentWidget : public QWidget
    {
        inline ContentWidget( TexViewportWidget *viewport )
        {
            this->viewport = viewport;

            // To enable transparency we have to do this:
            QPalette p;
            p.setColor( QPalette::Window, Qt::transparent );
            this->setPalette( p );
        }

        void paintEvent( QPaintEvent *evt ) override
        {
            int pad_horizontal = 0;
            int pad_vertical = 0;

            // Check if padding is enabled.
            if ( viewport->showFullImage == false )
            {
                pad_horizontal = viewport->pad_horizontal;
                pad_vertical = viewport->pad_vertical;
            }

            int pane_width = this->width();
            int pane_height = this->height();

            const QPixmap& contentPixmap = viewport->contentPixmap;

            QRect updateRect = evt->rect();

            QPainter painter( this );

            int scaled_imgwidth = ( pane_width - pad_horizontal * 2 );
            int scaled_imgheight = ( pane_height - pad_vertical * 2 );

            QRect drawRegion( pad_horizontal, pad_vertical, scaled_imgwidth, scaled_imgheight );
            QRect updateRegion = updateRect.intersected( drawRegion );

            if ( updateRegion.isEmpty() == false )
            {
                int zero_draw_off_x = updateRegion.x() - pad_horizontal;
                int zero_draw_off_y = updateRegion.y() - pad_vertical;

                eViewportMode mode = viewport->mode;

                if ( viewport->showBackground && mode == eViewportMode::TEXVIEW )
                {
                    painter.drawTiledPixmap( updateRegion.x(), updateRegion.y(), updateRegion.width(), updateRegion.height(), viewport->backgroundPixmap, zero_draw_off_x, zero_draw_off_y );
                }

                qreal imgScaleFact = viewport->imgScaleFactor;

                int imgsel_x = zero_draw_off_x / imgScaleFact;
                int imgsel_y = zero_draw_off_y / imgScaleFact;
                int imgsel_width = std::max( 1, (int)( updateRegion.width() / imgScaleFact ) );
                int imgsel_height = std::max( 1, (int)( updateRegion.height() / imgScaleFact ) );

                painter.drawPixmap(
                    updateRegion.x(), updateRegion.y(), updateRegion.width(), updateRegion.height(), contentPixmap,
                    imgsel_x, imgsel_y, imgsel_width, imgsel_height
                );
            }

            QWidget::paintEvent( evt );
        }

        void wheelEvent( QWheelEvent *evt ) override
        {
            Qt::KeyboardModifiers keyboardState = QGuiApplication::keyboardModifiers();
            Qt::MouseButtons mousebuttonState = evt->buttons();

            if ( keyboardState & Qt::KeyboardModifier::ControlModifier || mousebuttonState & Qt::MouseButton::RightButton )
            {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
                QPointF centerPoint = evt->globalPosition();
#else
                QPointF centerPoint = evt->globalPosF();
#endif
                QPoint angleDelta = evt->angleDelta();

                int eightdeg_wheelrotate = angleDelta.y();

                if ( eightdeg_wheelrotate != 0 )
                {
                    qreal angle_wheelrotate = (qreal)eightdeg_wheelrotate / 8;

                    this->viewport->zoomBy( centerPoint, 1.0 + angle_wheelrotate * 0.01 );
                }

                evt->accept();
            }
        }

    private:
        TexViewportWidget *viewport;
    };

    // Setup the image widget that will display our texture and system images (loading animation for ex.)
    this->contentWidget = new ContentWidget( this );
    this->contentWidget->setVisible( false );
    this->setWidget( contentWidget );
    this->setAlignment(Qt::AlignCenter);

    this->backgroundPixmap.load( "resources/viewBackground.png" );

    QScrollerProperties props;
    props.setScrollMetric( QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff );
    props.setScrollMetric( QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff );

    QScroller *scroller = QScroller::scroller( this->viewport() );
    scroller->setScrollerProperties( props );

    viewport()->grabGesture( Qt::PinchGesture );

    QScroller::grabGesture( this->viewport(), QScroller::TouchGesture );
}

void TexViewportWidget::setViewportMode( eViewportMode mode )
{
    eViewportMode oldMode = this->mode;

    if ( oldMode == mode )
        return;

    QWidget *contentWidget = this->contentWidget;

    bool showWidget = ( mode != eViewportMode::DISABLED );

    if ( oldMode == eViewportMode::TEXVIEW )
    {
        this->imgScaleFactor = 1.0;
    }

    contentWidget->setVisible( showWidget );

    // TODO: any state to initialize?

    this->mode = mode;
}

eViewportMode TexViewportWidget::getViewportMode( void ) const
{
    return this->mode;
}

static AINLINE qreal calculate_fitting_scale_factor( int fitWidth, int fitHeight, int imgWidth, int imgHeight, bool clamp_into = false )
{
    if ( imgWidth == 0 || imgHeight == 0 )
        return 1.0;

    qreal scaleFactor = std::min((qreal)fitWidth / imgWidth, (qreal)fitHeight / imgHeight);

    if ( clamp_into && scaleFactor > 1.0 )
    {
        scaleFactor = 1.0;
    }

    return scaleFactor;
}

void TexViewportWidget::setTexturePixmap( QPixmap target )
{
    this->setViewportMode( eViewportMode::TEXVIEW );

    if ( this->showFullImage == false )
    {
        QWidget *viewportWidget = viewport();

        this->imgScaleFactor = calculate_fitting_scale_factor( viewportWidget->width(), viewportWidget->height(), target.width(), target.height(), true );
    }
    this->contentPixmap = std::move( target );
    this->updateViewportScale();
    this->centerView( 0.5, 0.5 );
}

void TexViewportWidget::centerView( qreal crx, qreal cry, qreal offx, qreal offy )
{
    if ( this->mode != eViewportMode::TEXVIEW )
        return;

    if ( this->showFullImage )
        return;

    QScrollBar *horScroll = this->horizontalScrollBar();
    QScrollBar *verScroll = this->verticalScrollBar();

    qreal imgScaleFactor = this->imgScaleFactor;

    int scaled_imgwidth = (int)( this->contentPixmap.width()  * imgScaleFactor );
    int scaled_imgheight = (int)( this->contentPixmap.height() * imgScaleFactor );

    // To naturally zoom into a point we must take the center point into account.
    // After zooming, scroll bar representations have changed.

    // Calculate the relative scrollarea view center to gesture center point offsets.
    qreal rel_gesturecp_scrollcp_x;
    qreal rel_gesturecp_scrollcp_y;

    if ( scaled_imgwidth != 0 )
    {
        rel_gesturecp_scrollcp_x = offx / ( scaled_imgwidth );
    }
    else
    {
        rel_gesturecp_scrollcp_x = 0;
    }

    if ( scaled_imgheight != 0 )
    {
        rel_gesturecp_scrollcp_y = offy / ( scaled_imgheight );
    }
    else
    {
        rel_gesturecp_scrollcp_y = 0;
    }

    // Calculate the position we want to center on based on current magnification.
    qreal imgrel_scrollto_x = crx - rel_gesturecp_scrollcp_x;
    qreal imgrel_scrollto_y = cry - rel_gesturecp_scrollcp_y;

    QWidget *viewportWidget = viewport();

    int pix_view_x = this->pad_horizontal + scaled_imgwidth * imgrel_scrollto_x - viewportWidget->width() / 2;
    int pix_view_y = this->pad_vertical + scaled_imgheight * imgrel_scrollto_y - viewportWidget->height() / 2;

    horScroll->setValue( pix_view_x );
    verScroll->setValue( pix_view_y );

    QScroller *scroller = QScroller::scroller( viewportWidget );

    scroller->stop();
}

bool TexViewportWidget::zoomBy( QPointF centerPoint, qreal scaleFactor )
{
    if ( this->mode != eViewportMode::TEXVIEW || this->showFullImage )
    {
        return false;
    }

    qreal imgScaleFactor = this->imgScaleFactor;

    int scaled_imgwidth = this->contentPixmap.width()  * imgScaleFactor;
    int scaled_imgheight = this->contentPixmap.height() * imgScaleFactor;

    // Find the relative coordinate inside of the scaled image pane
    // that the gesture center point resides on.
    qreal gesturecp_img_relx, gesturecp_img_rely;
    {
        QPoint screen_img_pos = contentWidget->mapToGlobal( QPoint( this->pad_horizontal, this->pad_vertical ) );

        gesturecp_img_relx = ( centerPoint.x() - screen_img_pos.x() ) / scaled_imgwidth;
        gesturecp_img_rely = ( centerPoint.y() - screen_img_pos.y() ) / scaled_imgheight;
    }

    // Find the constant pixel offset of the current scrollarea view center
    // to the gesture center point.
    qreal pixoff_gesturecp_scrollcp_x, pixoff_gesturecp_scrollcp_y;
    {
        QWidget *viewportFrame = this->viewport();

        QPoint screen_img_center_pos = viewportFrame->mapToGlobal( QPoint( viewportFrame->width() / 2, viewportFrame->height() / 2 ) );

        pixoff_gesturecp_scrollcp_x = ( centerPoint.x() - screen_img_center_pos.x() );
        pixoff_gesturecp_scrollcp_y = ( centerPoint.y() - screen_img_center_pos.y() );
    }

    this->imgScaleFactor = std::max( _min_absolute_scalefactor, imgScaleFactor * scaleFactor );

    this->updateViewportScale();

    this->centerView( gesturecp_img_relx, gesturecp_img_rely, pixoff_gesturecp_scrollcp_x, pixoff_gesturecp_scrollcp_y );

    return true;
}

void TexViewportWidget::updatePixmap( QPixmap target )
{
    if ( this->mode != eViewportMode::SYSTEM )
        return;

    this->pad_horizontal = 0;
    this->pad_vertical = 0;
    this->imgScaleFactor = calculate_fitting_scale_factor( this->contentWidget->width(), this->contentWidget->height(), target.width(), target.height() );
    this->contentPixmap = std::move( target );
    this->contentWidget->update();
}

void TexViewportWidget::clearContent( void )
{
    this->pad_horizontal = 0;
    this->pad_vertical = 0;
    this->imgScaleFactor = 1.0;
    this->contentPixmap = QPixmap();
    this->contentWidget->update();
    this->contentWidget->setFixedSize( 0, 0 );
}

void TexViewportWidget::setShowBackground( bool show )
{
    this->showBackground = show;
    this->contentWidget->update();
}

bool TexViewportWidget::getShowBackground( void ) const
{
    return this->showBackground;
}

void TexViewportWidget::setShowFullImage( bool fullimg )
{
    this->showFullImage = fullimg;

    if ( this->mode == eViewportMode::TEXVIEW )
    {
        this->updateViewportScale();

        if ( fullimg == false )
        {
            this->centerView( 0.5, 0.5 );
        }
    }
}

bool TexViewportWidget::getShowFullImage( void ) const
{
    return this->showFullImage;
}

void TexViewportWidget::setContentSize( int width, int height, bool scaledContents )
{
    if ( this->mode != eViewportMode::SYSTEM )
        return;

    this->contentWidget->setFixedSize( width, height );
    this->imgScaleFactor = calculate_fitting_scale_factor( width, height, this->contentPixmap.width(), this->contentPixmap.height() );
    this->contentWidget->update();
}

void TexViewportWidget::updateViewportScale( void )
{
    if ( this->mode != eViewportMode::TEXVIEW )
        return;

    const QPixmap& widgetPixMap = this->contentPixmap;

    int imgwidth = widgetPixMap.width();
    int imgheight = widgetPixMap.height();

    QWidget *contentWidget = this->contentWidget;

    if ( this->showFullImage )
    {
        qreal borderScaleFactor = calculate_fitting_scale_factor( this->width(), this->height(), imgwidth, imgheight, true );

        contentWidget->setFixedSize( (int)( borderScaleFactor * imgwidth ), (int)( borderScaleFactor * imgheight ));

        this->imgScaleFactor = borderScaleFactor;

        this->pad_horizontal = 0;
        this->pad_vertical = 0;
    }
    else
    {
        qreal imgScaleFactor = this->imgScaleFactor;

        int scaled_imgwidth = (int)( imgwidth * imgScaleFactor );
        int scaled_imgheight = (int)( imgheight * imgScaleFactor );

        QWidget *viewportWidget = this->viewport();

        this->pad_horizontal = viewportWidget->width() / 2;
        this->pad_vertical = viewportWidget->height() / 2;

        contentWidget->setFixedSize(scaled_imgwidth + this->pad_horizontal * 2, scaled_imgheight + this->pad_vertical * 2);
    }

    contentWidget->update();
}

bool TexViewportWidget::viewportEvent( QEvent *evt )
{
    // First we allow the scroller to reposition ourselves.
    bool wasEventHandled = QAbstractScrollArea::viewportEvent( evt );

    QEvent::Type evtType = evt->type();

    if ( evtType == QEvent::Gesture )
    {
        QGestureEvent *gest_evt = (QGestureEvent*)evt;

        if ( QPinchGesture *pinch = (QPinchGesture*)gest_evt->gesture( Qt::PinchGesture ) )
        {
            bool has_viewport_changed = false;

            if ( pinch->totalChangeFlags() & QPinchGesture::ScaleFactorChanged )
            {
                qreal scalefact = pinch->scaleFactor();
                QPointF centerPoint = pinch->centerPoint();

                has_viewport_changed = this->zoomBy( centerPoint, scalefact );
            }

            if ( has_viewport_changed )
            {
                wasEventHandled = true;
            }
        }
    }

    return wasEventHandled;
}

void TexViewportWidget::resizeEvent( QResizeEvent *evt )
{
    QScrollArea::resizeEvent( evt );

    // Adjust the image to the new viewport size.
    this->updateViewportScale();
}
