/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/helperruntime.cpp
*  PURPOSE:     Helper windows for user-friendliness.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

// In the editor we want to have help text pop-up on certain occasions.
// Those help texts have properties:
// * show only once
// * do not show anymore when unticked
// The helper runtime is keeping track of the status.

#include "mainwindow.h"

#include <sdk/PluginHelpers.h>

#include "guiserialization.hxx"

#include <sdk/UniChar.h>

struct helper_item
{
    inline helper_item( void )
    {
        this->activeWidget = nullptr;
        this->triggerName = "";
        this->locale_item_name = "";
        this->helperType = eHelperTextType::DIALOG_WITH_TICK;
        this->isAllowedToDisplay = true;
        this->richText = false;
    }

    inline ~helper_item( void )
    {
        if ( QWidget *curWidget = this->activeWidget )
        {
            delete curWidget;

            this->activeWidget = nullptr;
        }
    }

    QWidget *activeWidget;

    const char *triggerName;
    const char *locale_item_name;
    bool richText;

    eHelperTextType helperType;

    bool isAllowedToDisplay;

    RwListEntry <helper_item> node;
};

struct helperRuntimeEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainWnd )
    {
        LIST_CLEAR( helper_items.root );

        this->helperItemCount = 0;

        RegisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_HELPERRUNTIME, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_HELPERRUNTIME );

        // Unregister all remaining helper items.
        while ( !LIST_EMPTY( helper_items.root ) )
        {
            helper_item *item = LIST_GETITEM( helper_item, helper_items.root.next, node );

            LIST_REMOVE( item->node );

            delete item;
        }

        this->helperItemCount = 0;
    }

    struct helper_item_serialized
    {
        bool isAllowedToShow;
    };

    void Load( MainWindow *mainWnd, rw::BlockProvider& configBlock ) override
    {
        // Load all the info.
        rw::uint32 itemCount = configBlock.readUInt32();

        for ( rw::uint32 n = 0; n < itemCount; n++ )
        {
            // Enter a block.
            rw::BlockProvider itemBlock( &configBlock, rw::CHUNK_STRUCT );

            // Read the string.
            rw::rwStaticString <char> triggerName;

            triggerName = RwReadANSIString( itemBlock );

            // Get the related info.
            if ( helper_item *item = this->FindHelperItemByName( triggerName.GetConstString() ) )
            {
                // Alright, we should read on.
                helper_item_serialized ser_info;
                itemBlock.readStruct( ser_info );

                item->isAllowedToDisplay = ser_info.isAllowedToShow;
            }
        }
    }

    void Save( const MainWindow *mainWnd, rw::BlockProvider& configBlock ) const
    {
        // Save each helper item.
        configBlock.writeUInt32( this->helperItemCount );

        LIST_FOREACH_BEGIN( helper_item, this->helper_items.root, node )

            // We create a block for each.
            rw::BlockProvider itemBlock( &configBlock );

            // First write the name.
            RwWriteANSIString( itemBlock, item->triggerName );

            // Now the info struct.
            helper_item_serialized ser_info;
            ser_info.isAllowedToShow = item->isAllowedToDisplay;

            itemBlock.writeStruct( ser_info );

        LIST_FOREACH_END
    }

    helper_item* FindHelperItemByName( const char *name )
    {
        LIST_FOREACH_BEGIN( helper_item, this->helper_items.root, node )

            if ( StringEqualToZero( item->triggerName, name, false ) )
                return item;

        LIST_FOREACH_END

        return nullptr;
    }

    RwList <helper_item> helper_items;

    unsigned int helperItemCount;
};

static optional_struct_space <PluginDependantStructRegister <helperRuntimeEnv, mainWindowFactory_t>> helperRuntimeEnvRegister;

bool RegisterHelperWidget(MainWindow *mainWnd, const char *triggerName, eHelperTextType diagType, const char *locale_item_name, bool richText)
{
    helperRuntimeEnv *helperEnv = helperRuntimeEnvRegister.get().GetPluginStruct( mainWnd );

    bool success = false;

    if ( helperEnv )
    {
        // Make sure no item by that name exists already.
        if ( helperEnv->FindHelperItemByName( triggerName ) == nullptr )
        {
            // Register us.
            helper_item *new_item = new helper_item;

            new_item->triggerName = triggerName;
            new_item->locale_item_name = locale_item_name;
            new_item->helperType = diagType;
            new_item->richText = richText;

            LIST_INSERT( helperEnv->helper_items.root, new_item->node );

            helperEnv->helperItemCount++;

            success = true;
        }
    }

    return success;
}

bool UnregisterHelperWidget( MainWindow *mainWnd, const char *triggerName )
{
    helperRuntimeEnv *helperEnv = helperRuntimeEnvRegister.get().GetPluginStruct( mainWnd );

    bool success = false;

    if ( helperEnv )
    {
        if ( helper_item *the_item = helperEnv->FindHelperItemByName( triggerName ) )
        {
            // Remove it.
            LIST_REMOVE( the_item->node );

            delete the_item;

            helperEnv->helperItemCount--;

            success = true;
        }
    }

    return success;
}

struct HelperWidgetBase abstract : public QDialog, public magicTextLocalizationItem
{
    inline HelperWidgetBase( helper_item *item, QWidget *parent ) : QDialog( parent )
    {
        this->setAttribute( Qt::WA_DeleteOnClose );
        this->setWindowModality( Qt::WindowModality::ApplicationModal );

        this->setWindowFlags( ( this->windowFlags() | Qt::WindowStaysOnTopHint ) & ~Qt::WindowContextHelpButtonHint );

        // Prepare things we are going to need anyway.
        QLabel *helpTextLabel = new QLabel();

        helpTextLabel->setObjectName( "helperTextLabel" );

        this->helpTextLabel = helpTextLabel;

        // We also need a button.
        QPushButton *confirmButton = CreateButtonL( "Helper.Confirm" );

        connect( confirmButton, &QPushButton::clicked, this, &HelperWidgetBase::onClickConfirm );

        this->confirmButton = confirmButton;

        this->helperNode = item;

        item->activeWidget = this;

        RegisterTextLocalizationItem( this );
    }

    ~HelperWidgetBase( void )
    {
        UnregisterTextLocalizationItem( this );

        helperNode->activeWidget = NULL;
    }

    void updateContent( MainWindow *mainWnd ) override
    {
        // We simply update the help text.
        this->setWindowTitle( MAGIC_TEXT( "Helper.Title" ) );

        this->helpTextLabel->setText( MAGIC_TEXT( helperNode->locale_item_name ) );

        if (helperNode->richText) {
            this->helpTextLabel->setTextFormat(Qt::TextFormat::RichText);
            this->helpTextLabel->setOpenExternalLinks(true);
        }
        else {
            this->helpTextLabel->setOpenExternalLinks(false);
            this->helpTextLabel->setTextFormat(Qt::TextFormat::PlainText);
        }
    }

    void onClickConfirm( bool checked )
    {
        // We close ourselves.
        this->close();
    }

protected:
    QLabel *helpTextLabel;
    QPushButton *confirmButton;

    helper_item *helperNode;
};

struct SimpleHelperWidget : public HelperWidgetBase
{
    inline SimpleHelperWidget( helper_item *item, QWidget *parent ) : HelperWidgetBase( item, parent )
    {
        // Make sure that we are always on top.

        // This item consists of the help text and the OK button.
        QVBoxLayout *rootLayout = new QVBoxLayout();

        rootLayout->addWidget( helpTextLabel, 0, Qt::AlignCenter );

        rootLayout->addSpacing( 15 );

        rootLayout->addWidget( confirmButton, 0, Qt::AlignCenter );

        this->setLayout( rootLayout );
    }
};

struct SelectiveHelperWidget : public HelperWidgetBase
{
    inline SelectiveHelperWidget( helper_item *item, QWidget *parent ) : HelperWidgetBase( item, parent )
    {
        // A more advanced widget that contains a checkbox to select if you want to do things.
        QVBoxLayout *rootLayout = new QVBoxLayout();

        rootLayout->addWidget( helpTextLabel, 0, Qt::AlignCenter );

        rootLayout->addSpacing( 5 );

        QCheckBox *checkboxShowAgainState = CreateCheckBoxL( "Helper.NotShowAgain" );

        checkboxShowAgainState->setObjectName( "helperSelecCheckBox" );

        checkboxShowAgainState->setChecked( !item->isAllowedToDisplay );

        connect( checkboxShowAgainState, &QCheckBox::stateChanged, this, &SelectiveHelperWidget::onShowAgainStateChange );

        rootLayout->addWidget( checkboxShowAgainState );

        rootLayout->addSpacing( 15 );

        rootLayout->addWidget( confirmButton, 0, Qt::AlignCenter );

        this->setLayout( rootLayout );
    }

    void onShowAgainStateChange( int checkState )
    {
        if ( checkState == Qt::Checked )
        {
            helperNode->isAllowedToDisplay = false;
        }
        else if ( checkState == Qt::Unchecked )
        {
            helperNode->isAllowedToDisplay = true;
        }
    }
};

void TriggerHelperWidget( MainWindow *mainWnd, const char *triggerName, QWidget *optParent )
{
    helperRuntimeEnv *helperEnv = helperRuntimeEnvRegister.get().GetPluginStruct( mainWnd );

    if ( helperEnv )
    {
        if ( helper_item *item = helperEnv->FindHelperItemByName( triggerName ) )
        {
            // Are we even allowed to trigger?
#ifndef _DEBUG_HELPER_TEXT
            if ( item->isAllowedToDisplay )
#endif //_DEBUG_HELPER_TEXT
            {
                if ( item->activeWidget == nullptr )
                {
                    QWidget *showedWidget = nullptr;

                    if ( item->helperType == eHelperTextType::DIALOG_SHOW_ONCE )
                    {
                        // We trigger this item, meow.
                        SimpleHelperWidget *newWidget = new SimpleHelperWidget( item, optParent );

                        // The item is not allowed to trigger anymore, if we show the simple widget.
                        item->isAllowedToDisplay = false;

                        showedWidget = newWidget;
                    }
                    else if ( item->helperType == eHelperTextType::DIALOG_WITH_TICK )
                    {
                        SelectiveHelperWidget *newWidget = new SelectiveHelperWidget( item, optParent );

                        showedWidget = newWidget;
                    }

                    if ( showedWidget )
                    {
                        // :-)
                        QApplication::beep();

                        showedWidget->setVisible( true );

                        // We want to position ourselves on the cursor.
                        QPoint cursorPos = QCursor::pos();

                        showedWidget->move( cursorPos.x() - 50, cursorPos.y() - 50 );
                    }
                }
            }
        }
    }
}

void InitializeHelperRuntime( void )
{
    helperRuntimeEnvRegister.Construct( mainWindowFactory );
}

void ShutdownHelperRuntime( void )
{
    helperRuntimeEnvRegister.Destroy();
}