/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/languages.common.cpp
*  PURPOSE:     Localized UI item implementations.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "languages.h"

// Common language components that the whole Magic.TXD application has to know about.
// We basically use those components in the entire GUI interface.

struct SimpleLocalizedButton : public QPushButton, public simpleLocalizationItem
{
    inline SimpleLocalizedButton( QString systemToken ) : simpleLocalizationItem( std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedButton( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QPushButton* CreateButtonL( QString systemToken )
{
    SimpleLocalizedButton *newButton = new SimpleLocalizedButton( std::move( systemToken ) );
    newButton->setMinimumWidth( 90 );
    return newButton;
}

struct FixedWidthLocalizedButton : public QPushButton, public simpleLocalizationItem
{
    inline FixedWidthLocalizedButton( QString systemToken, int fontSize ) : simpleLocalizationItem( systemToken ), fontSize( fontSize )
    {
        Init();
    }

    inline ~FixedWidthLocalizedButton( void )
    {
        Shutdown();
    }

    void doText( QString theText )
    {
        this->setText( theText );
        this->setFixedWidth( GetTextWidthInPixels( theText, this->fontSize ) );
    }

    int fontSize;
};

QPushButton* CreateFixedWidthButtonL( QString systemToken, int fontSize )
{
    FixedWidthLocalizedButton *newButton = new FixedWidthLocalizedButton( std::move( systemToken ), fontSize );

    return newButton;
}

struct SimpleLocalizedLabel : public QLabel, public simpleLocalizationItem
{
    inline SimpleLocalizedLabel( QString systemToken ) : simpleLocalizationItem( std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedLabel( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QLabel* CreateLabelL( QString systemToken )
{
    SimpleLocalizedLabel *newLabel = new SimpleLocalizedLabel( std::move( systemToken ) );

    return newLabel;
}

struct FixedWidthLocalizedLabel : public QLabel, public simpleLocalizationItem
{
    inline FixedWidthLocalizedLabel( QString systemToken, int fontSize ) : simpleLocalizationItem( std::move( systemToken ) ), font_size( fontSize )
    {
        Init();
    }

    inline ~FixedWidthLocalizedLabel( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
        this->setFixedWidth( GetTextWidthInPixels( text, this->font_size ) );
    }

    int font_size;
};

QLabel* CreateFixedWidthLabelL( QString systemToken, int fontSize )
{
    FixedWidthLocalizedLabel *newLabel = new FixedWidthLocalizedLabel( systemToken, fontSize );

    return newLabel;
}

struct SimpleLocalizedCheckBox : public QCheckBox, public simpleLocalizationItem
{
    inline SimpleLocalizedCheckBox( QString systemToken ) : simpleLocalizationItem( std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedCheckBox( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QCheckBox* CreateCheckBoxL( QString systemToken )
{
    SimpleLocalizedCheckBox *newCB = new SimpleLocalizedCheckBox( std::move( systemToken ) );

    return newCB;
}

struct SimpleLocalizedRadioButton : public QRadioButton, public simpleLocalizationItem
{
    inline SimpleLocalizedRadioButton( QString systemToken ) : simpleLocalizationItem( std::move( systemToken ) )
    {
        Init();
    }

    inline ~SimpleLocalizedRadioButton( void )
    {
        Shutdown();
    }

    void doText( QString text ) override
    {
        this->setText( text );
    }
};

QRadioButton* CreateRadioButtonL( QString systemToken )
{
    SimpleLocalizedRadioButton *radioButton = new SimpleLocalizedRadioButton( systemToken );

    return radioButton;
}

struct MnemonicAction : public QAction, public magicTextLocalizationItem
{
    inline MnemonicAction( QString systemToken, QObject *parent ) : QAction( parent )
    {
        this->systemToken = std::move( systemToken );

        RegisterTextLocalizationItem( this );
    }

    inline ~MnemonicAction( void )
    {
        UnregisterTextLocalizationItem( this );
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        QString descText = MAGIC_TEXT( this->systemToken );

        this->setText( "&" + descText );
    }

private:
    QString systemToken;
};

QAction* CreateMnemonicActionL( QString systemToken, QObject *parent )
{
    MnemonicAction *newAction = new MnemonicAction( systemToken, parent );

    return newAction;
}
