/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/qtsharedlogic.h
*  PURPOSE:     Header with Qt utilities.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QFormLayout>
#include "qtutils.h"

#include "languages.h"

// Put very generic shared things in this file.

namespace qtshared
{
    struct PathBrowseButton : public QPushButton, public simpleLocalizationItem
    {
        inline PathBrowseButton(MagicLineEdit *lineEdit ) : QPushButton(), simpleLocalizationItem( "Tools.Browse" )
        {
            this->theEdit = lineEdit;

            connect( this, &QPushButton::clicked, this, &PathBrowseButton::OnBrowsePath );

            Init();
        }

        inline ~PathBrowseButton( void )
        {
            Shutdown();
        }

        void doText( QString text ) override
        {
            this->setText( text );
        }

    public slots:
        void OnBrowsePath( bool checked )
        {
            QString selPath =
                QFileDialog::getExistingDirectory(
                    this, MAGIC_TEXT( "Tools.BrwDesc" ),
                    theEdit->text()
                );

            if ( selPath.isEmpty() == false )
            {
                this->theEdit->setText( selPath );
            }
        }

    private:
        MagicLineEdit *theEdit;
    };

    inline QLayout* createPathSelectGroup( QString begStr, MagicLineEdit*& editOut )
    {
        QHBoxLayout *pathRow = new QHBoxLayout();

        MagicLineEdit *pathEdit = new MagicLineEdit( begStr );

        pathRow->addWidget( pathEdit );

        QPushButton *pathBrowseButton = new PathBrowseButton( pathEdit );

        pathRow->addWidget( pathBrowseButton );

        editOut = pathEdit;

        return pathRow;
    }

    inline QLayout* createMipmapGenerationGroup( QObject *parent, bool isEnabled, int curMipMax, QCheckBox*& propGenMipmapsOut, MagicLineEdit*& editMaxMipLevelOut )
    {
        QHBoxLayout *genMipGroup = new QHBoxLayout();

        QCheckBox *propGenMipmaps = CreateCheckBoxL( "Tools.GenMips" );

        propGenMipmaps->setChecked( isEnabled );

        propGenMipmapsOut = propGenMipmaps;

        genMipGroup->addWidget( propGenMipmaps );

        QHBoxLayout *mipMaxLevelGroup = new QHBoxLayout();

        mipMaxLevelGroup->setAlignment( Qt::AlignRight );

        mipMaxLevelGroup->addWidget( CreateLabelL( "Tools.MaxMips" ), 0, Qt::AlignRight );

        MagicLineEdit *maxMipLevelEdit = new MagicLineEdit( QString( "%1" ).arg( curMipMax ) );

        QIntValidator *maxMipLevelVal = new QIntValidator( 0, 32, parent );

        maxMipLevelEdit->setValidator( maxMipLevelVal );

        editMaxMipLevelOut = maxMipLevelEdit;

        maxMipLevelEdit->setMaximumWidth( 40 );

        mipMaxLevelGroup->addWidget( maxMipLevelEdit );

        genMipGroup->addLayout( mipMaxLevelGroup );

        return genMipGroup;
    }

    inline QLayout* createGameRootInputOutputForm( const rw::rwStaticString <wchar_t>& curGameRoot, const rw::rwStaticString <wchar_t>& curOutputRoot, MagicLineEdit*& editGameRootOut, MagicLineEdit*& editOutputRootOut )
    {
        QFormLayout *basicPathForm = new QFormLayout();

        QLayout *gameRootLayout = qtshared::createPathSelectGroup( wide_to_qt( curGameRoot ), editGameRootOut );
        QLayout *outputRootLayout = qtshared::createPathSelectGroup( wide_to_qt( curOutputRoot ), editOutputRootOut );

        basicPathForm->addRow( CreateLabelL("Tools.GameRt"), gameRootLayout );
        basicPathForm->addRow( CreateLabelL("Tools.Output"), outputRootLayout );

        return basicPathForm;
    }
};
