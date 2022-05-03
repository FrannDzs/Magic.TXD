/*****************************************************************************
*
*  PROJECT:     Magic.TXD
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/exportallwindow.cpp
*  PURPOSE:     Components of the Export All window.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-txd/
*
*****************************************************************************/

#include "mainwindow.h"

#include "guiserialization.hxx"

struct exportAllWindowSerializationEnv : public magicSerializationProvider
{
    inline void Initialize( MainWindow *mainWnd )
    {
        RegisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_EXPORTALLWINDOW, this );
    }

    inline void Shutdown( MainWindow *mainWnd )
    {
        UnregisterMainWindowSerialization( mainWnd, MAGICSERIALIZE_EXPORTALLWINDOW );
    }

    void Load( MainWindow *mainwnd, rw::BlockProvider& exportAllBlock ) override
    {
        mainwnd->lastUsedAllExportFormat = RwReadANSIString( exportAllBlock );
        mainwnd->lastAllExportTarget = RwReadUnicodeString( exportAllBlock );
    }

    void Save( const MainWindow *mainwnd, rw::BlockProvider& exportAllBlock ) const override
    {
        RwWriteANSIString( exportAllBlock, mainwnd->lastUsedAllExportFormat );
        RwWriteUnicodeString( exportAllBlock, mainwnd->lastAllExportTarget );
    }
};

static optional_struct_space <PluginDependantStructRegister <exportAllWindowSerializationEnv, mainWindowFactory_t>> exportAllWindowSerializationEnvRegister;

void InitializeExportAllWindowSerialization( void )
{
    exportAllWindowSerializationEnvRegister.Construct( mainWindowFactory );
}

void ShutdownExportAllWindowSerialization( void )
{
    exportAllWindowSerializationEnvRegister.Destroy();
}
