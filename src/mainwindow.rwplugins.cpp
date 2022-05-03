/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/mainwindow.rwplugins.cpp
*  PURPOSE:     RW plugins for interaction with the editor.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "texinfoitem.h"

template <typename structType, bool isDependantStruct = false>
struct rwEnginePluginLink
{
    inline rwEnginePluginLink( rw::Interface *rwEngine ) : reg( rwEngine, structType::type_name() )
    {
        return;
    }

    inline rw::typePluginStructRegister <structType, isDependantStruct>& GetRegister( void )
    {
        return this->reg;
    }

private:
    rw::typePluginStructRegister <structType, isDependantStruct> reg;
};

struct rwtexTexInfoWidget
{
    static const char* type_name() { return "rwobj::texture"; }

    inline ~rwtexTexInfoWidget( void )
    {
        // Make sure to unlink ourselves if we destroy.
        if ( TexInfoWidget *texInfo = this->texInfo )
        {
            texInfo->CleanupOnTextureHandleRemoval();
        }
    }

    TexInfoWidget *texInfo = nullptr;
};

static optional_struct_space <rw::interfacePluginStructRegister <rwEnginePluginLink <rwtexTexInfoWidget>>> rwtexTexInfoWidgetRegister;

void MainWindow::setTextureTexInfoLink( rw::TextureBase *texture, TexInfoWidget *widget )
{
    rwtexTexInfoWidget *widgetInfo = rwtexTexInfoWidgetRegister.get().GetPluginStruct( this->GetEngine() )->GetRegister().GetPluginStruct( texture );

    widgetInfo->texInfo = widget;
}

TexInfoWidget* MainWindow::getTextureTexInfoLink( rw::TextureBase *texture )
{
    rwtexTexInfoWidget *widgetInfo = rwtexTexInfoWidgetRegister.get().GetPluginStruct( this->GetEngine() )->GetRegister().GetPluginStruct( texture );

    return widgetInfo->texInfo;
}

void InitializeMainWindowRenderWarePlugins( void )
{
    rwtexTexInfoWidgetRegister.Construct();
}

void ShutdownMainWindowRenderWarePlugins( void )
{
    rwtexTexInfoWidgetRegister.Destroy();
}