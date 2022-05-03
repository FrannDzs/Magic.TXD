/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/texinfoitem.h
*  PURPOSE:     Texture info item visible in the texture list.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include "languages.h"

class TexInfoWidget : public QWidget, public magicTextLocalizationItem
{
public:
    TexInfoWidget(QListWidgetItem *listItem, rw::TextureBase *texItem) : QWidget()
    {
        QLabel *texName = new QLabel(QString());
        texName->setFixedHeight(23);
        texName->setObjectName("label19px");
        QLabel *texInfo = new QLabel(QString());
        texInfo->setObjectName("texInfo");
        QVBoxLayout *layout = new QVBoxLayout();
        layout->setContentsMargins(5, 4, 0, 5);
        layout->addWidget(texName);
        layout->addWidget(texInfo);

        this->texNameLabel = texName;
        this->texInfoLabel = texInfo;
        this->rwTextureHandle = texItem;
        this->listItem = listItem;

        this->updateInfo();

        this->setLayout(layout);

        RegisterTextLocalizationItem( this );
    }

    ~TexInfoWidget( void )
    {
        UnregisterTextLocalizationItem( this );
    }

    inline void SetTextureHandle( rw::TextureBase *texHandle )
    {
        this->rwTextureHandle = texHandle;

        this->updateInfo();
    }

    inline rw::TextureBase* GetTextureHandle( void )
    {
        return this->rwTextureHandle;
    }

    inline void CleanupOnTextureHandleRemoval( void )
    {
        this->rwTextureHandle = nullptr;
    }

    inline static QString getRasterFormatString( const rw::Raster *rasterInfo )
    {
        char platformTexInfoBuf[ 256 + 1 ];

        size_t localPlatformTexInfoLength = 0;
        {
            // Note that the format string is only put into our buffer as much space as we have in it.
            // If the format string is longer, then that does not count as an error.
            // lengthOut will always return the format string length of the real thing, so be careful here.
            const size_t availableStringCharSpace = ( sizeof( platformTexInfoBuf ) - 1 );

            size_t platformTexInfoLength = 0;

            try
            {
                rasterInfo->getFormatString( platformTexInfoBuf, availableStringCharSpace, platformTexInfoLength );
            }
            catch( rw::RwException& )
            {
                // If there was any failure, we just say its unknown.
                static constexpr char error_message[] = "unknown";

                static constexpr size_t error_message_len = ( sizeof( error_message ) - 1 );

                static_assert( sizeof( error_message ) < sizeof( platformTexInfoBuf ), "oh no, somebody does not care about buffer boundaries" );

                memcpy( platformTexInfoBuf, error_message, error_message_len );

                platformTexInfoLength = error_message_len;

                // Just continue the runtime.
            }

            // Get the trimmed string length.
            localPlatformTexInfoLength = std::min( platformTexInfoLength, availableStringCharSpace );
        }

        // Zero terminate it.
        platformTexInfoBuf[ localPlatformTexInfoLength ] = '\0';

        return QString( (const char*)platformTexInfoBuf );
    }

    inline static QString getDefaultRasterInfoString( const rw::Raster *rasterInfo )
    {
        QString textureInfo;

        // First part is the texture size.
        rw::uint32 width, height;
        rasterInfo->getSize( width, height );

        textureInfo += QString::number( width ) + QString( "x" ) + QString::number( height );

        // Then is the platform format info.
        textureInfo += " " + getRasterFormatString( rasterInfo );

        // After that how many mipmap levels we have.
        rw::uint32 mipCount = rasterInfo->getMipmapCount();

        textureInfo += " " + QString::number( mipCount ) + " ";

        if ( mipCount == 1 )
        {
            textureInfo += MAGIC_TEXT( "Main.TexInfo.Level" );
        }
        else
        {
            QString levelsKeyStr = "Main.TexInfo.Lvl" + QString::number(mipCount);
            bool found;
            QString levelsStr = MAGIC_TEXT_CHECK_AVAILABLE( levelsKeyStr, &found );
            if(found)
                textureInfo += levelsStr;
            else
                textureInfo += MAGIC_TEXT( "Main.TexInfo.Levels" );
        }

        return textureInfo;
    }

    inline void updateInfo( void )
    {
        // Construct some information about our texture item.
        if ( rw::TextureBase *texHandle = this->rwTextureHandle )
        {
            QString textureInfo;

            if ( rw::Raster *rasterInfo = texHandle->GetRaster() )
            {
                textureInfo = getDefaultRasterInfoString( rasterInfo );
            }

            this->texNameLabel->setText( ansi_to_qt( texHandle->GetName() ) );
            this->texInfoLabel->setText( textureInfo );
        }
        else
        {
            this->texNameLabel->setText( MAGIC_TEXT( "Main.TexInfo.NoTex" ) );
            this->texInfoLabel->setText( MAGIC_TEXT( "Main.TexInfo.Invalid" ) );
        }
    }

    void updateContent( MainWindow *mainWnd )
    {
        this->updateInfo();
    }

    inline void remove( void )
    {
        delete this->listItem;
    }

private:
    QLabel *texNameLabel;
    QLabel *texInfoLabel;

    rw::TextureBase *rwTextureHandle;

public:
    QListWidgetItem *listItem;
};
