/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/mainwindow.loadinganim.h
*  PURPOSE:     Header of the loading animation helpers.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtCore/QTimer>

struct cyclingAnimationStore
{
    friend struct cyclingAnimation;

    void initializeData( MainWindow *mainWnd, const char *img_prefix, const char *img_ext )
    {
        // Remove any previous cached images.
        this->frames.Clear();

        size_t nframe = 1;

        while ( true )
        {
            QString frame_path = mainWnd->makeThemePath( img_prefix );
            frame_path += QString::number( nframe );
            frame_path += img_ext;

            if ( RawGlobalFileExists( qt_to_filePath( frame_path ) ) == false )
                break;

            QPixmap frame;
            frame.load( frame_path );

            this->frames.AddToBack( std::move( frame ) );

            nframe++;
        }
    }

private:
    rw::rwStaticVector <QPixmap> frames;
};

// Helper class to use QLabel for cyclingAnimation.
struct AnimationSimpleUpdateLabel : public QLabel, public pixmapReceiver
{
    using QLabel::QLabel;

protected:
    void updatePixmap( QPixmap pixmap ) override
    {
        this->setPixmap( std::move( pixmap ) );
    }
};

struct cyclingAnimation : public QObject
{
    inline cyclingAnimation( NativeExecutive::CExecutiveManager *execMan, cyclingAnimationStore *frameStore ) : lockAtomic( execMan )
    {
        this->frameStore = frameStore;
        this->targetWidget = nullptr;

        QTimer *statusBarTimer = new QTimer( this );
        statusBarTimer->setInterval( 100 );

        connect( statusBarTimer, &QTimer::timeout, this, &cyclingAnimation::onTimeoutUpdateLoadingAnimation );

        this->animTimer = statusBarTimer;
    }

    void initializeData( MainWindow *mainWnd, const char *idleFrameName )
    {
        this->idleFrame.load( mainWnd->makeThemePath( idleFrameName ) );
    }

    void progress( void )
    {
        if ( this->isRunningAnimation )
        {
            // Only call if the animation is ticking.
            this->animTimer->stop();
            this->onTimeoutUpdateLoadingAnimation();
            this->animTimer->start();
        }
    }

    void setTargetWidget( pixmapReceiver *targetWidget ) noexcept
    {
        this->targetWidget = targetWidget;
    }

    bool isRunning( void ) const noexcept
    {
        return this->isRunningAnimation;
    }

    void refreshPixmap( void ) const
    {
        cyclingAnimationStore *frameStore = this->frameStore;

        if ( this->isRunningAnimation )
        {
            size_t curFrameCount = this->frameStore->frames.GetCount();

            if ( this->currentFrame < curFrameCount )
            {
                this->targetWidget->updatePixmap( frameStore->frames[ this->currentFrame ] );
            }
        }
        else
        {
            this->targetWidget->updatePixmap( this->idleFrame );
        }
    }
   
    void start( void )
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_startAnim( this->lockAtomic );
    
        if ( this->isRunningAnimation == false )
        {
            this->animTimer->start();

            size_t curFrameCount = this->frameStore->frames.GetCount();

            if ( this->currentFrame < curFrameCount )
            {
                this->targetWidget->updatePixmap( this->frameStore->frames[ this->currentFrame ] );
            }

            this->isRunningAnimation = true;
        }
    }

    void stop( void )
    {
        NativeExecutive::CReadWriteWriteContext <> ctx_stopAnim( this->lockAtomic );

        if ( this->isRunningAnimation == true )
        {
            this->animTimer->stop();

            // Reset to idle image.
            this->targetWidget->updatePixmap( this->idleFrame );

            this->isRunningAnimation = false;
        }
    }

private:
    void onTimeoutUpdateLoadingAnimation( void )
    {
        NativeExecutive::CReadWriteReadContext <> ctx_nextAnim( this->lockAtomic );

        if ( this->isRunningAnimation == false )
            return;

        cyclingAnimationStore *frameStore = this->frameStore;

        size_t numFrames = frameStore->frames.GetCount();

        if ( numFrames > 0 )
        {
            size_t nextFrame = ( this->currentFrame + 1 ) % numFrames;

            this->targetWidget->updatePixmap( frameStore->frames[ nextFrame ] );

            this->currentFrame = nextFrame;
        }
    }

    size_t currentFrame = 0;
    bool isRunningAnimation = false;
    NativeExecutive::natRWLock lockAtomic;

    QPixmap idleFrame;
    cyclingAnimationStore *frameStore;
    pixmapReceiver *targetWidget;
    QTimer *animTimer;
};