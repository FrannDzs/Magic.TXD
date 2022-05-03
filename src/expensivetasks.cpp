/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/expensivetasks.cpp
*  PURPOSE:     Routines that take a longer time to finish.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "qtrwutils.hxx"

#include "tools/imagepipe.hxx"

rw::TexDictionary* ExpensiveTasks::loadTxdFile( QString fileName, bool silent )
{
    MainWindow *mainWnd = this->getMainWindow();
    rw::Interface *rwEngine = mainWnd->GetEngine();

    if (fileName.length() != 0)
    {
        // We got a file name, try to load that TXD file into our editor.
        std::wstring unicodeFileName = fileName.toStdWString();

        FileSystem::filePtr fileStream = OpenGlobalFile( mainWnd, unicodeFileName.c_str(), L"rb" );

        if ( fileStream.is_good() )
        {
            rw::StreamPtr txdFileStream = RwStreamCreateTranslated( rwEngine, fileStream );

            // If the opening succeeded, process things.
            if (txdFileStream.is_good())
            {
                if ( !silent )
                {
                    this->pushLogMessage(MAGIC_TEXT("General.TXDLoad").arg(fileName));
                }

                // Parse the input file.
                rw::RwObject *parsedObject = nullptr;

                try
                {
                    parsedObject = rwEngine->Deserialize(txdFileStream);
                }
                catch (rw::RwException& except)
                {
                    if ( !silent )
                    {
                        // TODO: localize this.
                        this->pushLogMessage(MAGIC_TEXT("General.TXDLoadFail").arg(wide_to_qt(rw::DescribeException(rwEngine, except))), LOGMSG_ERROR);
                    }

                    return nullptr;
                }

                try
                {
                    // Try to cast it to a TXD. If it fails we did not get a TXD.
                    rw::TexDictionary *newTXD = rw::ToTexDictionary(rwEngine, parsedObject);

                    if ( newTXD == nullptr )
                    {
                        const char *objTypeName = rwEngine->GetObjectTypeName(parsedObject);

                        if ( !silent )
                        {
                            this->pushLogMessage(MAGIC_TEXT("General.NotATXD").arg(objTypeName), LOGMSG_WARNING);
                        }

                        // Get rid of the object that is not a TXD.
                        rwEngine->DeleteRwObject(parsedObject);
                    }

                    return newTXD;
                }
                catch( ... )
                {
                    // We failed in some way, so delete the RW object.
                    rwEngine->DeleteRwObject( parsedObject );

                    throw;
                }
            }
        }
    }

    return nullptr;
}

rw::TextureBase* ExpensiveTasks::loadTexture( const wchar_t *widePath, const char *natTexPlatformName, const filePath& extention )
{
    MainWindow *mainWnd = this->getMainWindow();
    rw::Interface *rwEngine = mainWnd->GetEngine();

    rw::streamConstructionFileParamW_t fileParam( widePath );

    rw::StreamPtr imgStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &fileParam );

    if ( !imgStream.is_good() )
        return nullptr;

    struct taskMakeRasterImageImportMethods : public makeRasterImageImportMethods
    {
        inline taskMakeRasterImageImportMethods( rw::Interface *rwEngine, ExpensiveTasks *tasks, const char *natTexPlatformName )
            : makeRasterImageImportMethods( rwEngine ), natTexPlatformName( natTexPlatformName )
        {
            this->tasks = tasks;
        }

        rw::rwStaticString <char> GetNativeTextureName( void ) const override
        {
            return ( this->natTexPlatformName );
        }

        void OnWarning( rw::rwStaticString <wchar_t>&& msg ) const override
        {
            tasks->pushLogMessage( wide_to_qt( msg ), LOGMSG_WARNING );
        }

        void OnWarning( QString&& msg ) const override
        {
            tasks->pushLogMessage( std::move( msg ), LOGMSG_WARNING );
        }

        void OnError( rw::rwStaticString <wchar_t>&& msg ) const override
        {
            tasks->pushLogMessage( wide_to_qt( msg ), LOGMSG_ERROR );
        }

        void OnError( QString&& msg ) const override
        {
            tasks->pushLogMessage( std::move( msg ), LOGMSG_ERROR );
        }

    private:
        ExpensiveTasks *tasks;
        rw::rwStaticString <char> natTexPlatformName;
    };

    // Grab the image properties.
    taskMakeRasterImageImportMethods imp_methods( rwEngine, this, natTexPlatformName );

    return RwMakeTextureFromStream( rwEngine, imgStream, extention, imp_methods );
}

QImage ExpensiveTasks::makePreviewOfRaster( rw::Raster *rasterData, bool drawMipmapLayers )
{
    MainWindow *wnd = this->getMainWindow();
    rw::Interface *rwEngine = wnd->GetEngine();

    NativeExecutive::CExecutiveManager *execMan = (NativeExecutive::CExecutiveManager*)rw::GetThreadingNativeManager( rwEngine );

    try
    {
        // Get a bitmap to the raster.
        // This is a 2D color component surface.
        rw::Bitmap rasterBitmap( rwEngine, 32, rw::RASTER_8888, rw::COLOR_BGRA );

        if ( drawMipmapLayers && rasterData->getMipmapCount() > 1 )
        {
            rasterBitmap.setBgColor( 1.0, 1.0, 1.0, 0.0 );

            rw::DebugDrawMipmaps( rwEngine, rasterData, rasterBitmap );
        }
        else
        {
            rasterBitmap = rasterData->getBitmap();
        }

        return convertRWBitmapToQImage( execMan, rasterBitmap );
    }
    catch( rw::RwException& except )
    {
        this->pushLogMessage(MAGIC_TEXT("General.BitmapFail").arg(wide_to_qt(rw::DescribeException(rwEngine, except))), LOGMSG_WARNING);

        throw;
    }
}

void ExpensiveTasks::changeTXDPlatform( rw::TexDictionary *txd, rw::rwStaticString <char> platform )
{
    // To change the platform of a TXD we have to set all of it's textures platforms.
    for ( rw::TexDictionary::texIter_t iter( txd->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
    {
        rw::TextureBase *texHandle = iter.Resolve();

        rw::Raster *texRaster = texHandle->GetRaster();

        if ( texRaster )
        {
            try
            {
                rw::ConvertRasterTo( texRaster, platform.GetConstString() );
            }
            catch( rw::RwException& except )
            {
                this->pushLogMessage(
                    MAGIC_TEXT("General.PlatformFail")
                    .arg(ansi_to_qt(texHandle->GetName()), wide_to_qt( rw::DescribeException( txd->GetEngine(), except ))), LOGMSG_ERROR
                );

                // Continue changing platform.
            }
        }
    }
}

// Specialization of the MainWindow.
void MainWindow::EditorExpensiveTasks::pushLogMessage( QString msg, eLogMessageType msgType )
{
    MainWindow *mainWnd = LIST_GETITEM( MainWindow, this, expensiveTasks );

    mainWnd->asyncLog( std::move( msg ), msgType );
}

MainWindow* MainWindow::EditorExpensiveTasks::getMainWindow( void )
{
    MainWindow *mainWnd = LIST_GETITEM( MainWindow, this, expensiveTasks );

    return mainWnd;
}
