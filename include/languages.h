/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/languages.h
*  PURPOSE:     Header of the editor language system.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QVector>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qlabel.h>

#include <QtCore/QDirIterator>

#define DEFAULT_LANGUAGE "English"

#define CURRENT_MAGL_VERSION 2
#define MINIMUM_SUPPORTED_MAGL_VERSION 1

struct magicTextLocalizationItem abstract
{
    // Called by the language loader when a language has been loaded into the application.
    // This will update all text in Magic.TXD!
    virtual void updateContent( MainWindow *mainWnd ) = 0;
};

// API to register localization items.
bool RegisterTextLocalizationItem( magicTextLocalizationItem *provider );
bool UnregisterTextLocalizationItem( magicTextLocalizationItem *provider );

typedef std::list <magicTextLocalizationItem*> localizations_t;

struct LanguageInfo {
    // basic parameters
    unsigned int version;
    QString name;
    QString nameInOriginal;
    QString abbr;
    QString authors;
    QString rwlangkey;
    QString parentlangname;
};

localizations_t GetTextLocalizationItems( void );

// Main Query function to request current localized text strings.
QString getLanguageItemByKey( QString token, bool *found = nullptr );

// If you want to you can use those colorful macros instead!
#define MAGIC_TEXT( key )                       getLanguageItemByKey( key )

#define MAGIC_TEXT_CHECK_AVAILABLE( key, b )    getLanguageItemByKey( key, b )

// Helper struct.
struct simpleLocalizationItem : public magicTextLocalizationItem
{
    inline simpleLocalizationItem( QString systemToken )
    {
        this->systemToken = std::move( systemToken );
    }

    inline ~simpleLocalizationItem( void )
    {
        return;
    }

    void Init( void )
    {
        // Register ourselves.
        RegisterTextLocalizationItem( this );
    }

    void Shutdown( void )
    {
        // Unregister again.
        UnregisterTextLocalizationItem( this );
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        QString newText = MAGIC_TEXT( this->systemToken );

        this->doText( std::move( newText ) );
    }

    virtual void doText( QString text ) = 0;

    QString systemToken;
};

// Common GUI components that are linked to localized text.
QPushButton* CreateButtonL( QString systemToken );
QLabel* CreateLabelL( QString systemToken );
QLabel* CreateFixedWidthLabelL( QString systemToken, int fontSize );
QCheckBox* CreateCheckBoxL( QString systemToken );
QRadioButton* CreateRadioButtonL( QString systemToken );

QAction* CreateMnemonicActionL( QString systemToken, QObject *parent = nullptr );

// TODO: Maybe move it somewhere
unsigned int GetTextWidthInPixels(QString &text, unsigned int fontSize);
