#pragma once

#include "modrelink.h"

#include <ShlObj.h>

struct shell32_minlink
{
    GLOB_REDIR( SHChangeNotify );
    GLOB_REDIR( SHCreateItemFromParsingName );
    GLOB_REDIR( DragQueryFileW );

    HMODULE module;

    inline shell32_minlink( void )
    {
        HMODULE module = LoadLibraryW( L"shell32.dll" );

        this->module = module;

        if ( module )
        {
            GLOB_REDIR_L( module, SHChangeNotify );
            GLOB_REDIR_L( module, SHCreateItemFromParsingName );
            GLOB_REDIR_L( module, DragQueryFileW );
        }
    }

    inline ~shell32_minlink( void )
    {
        if ( HMODULE module = this->module )
        {
            FreeLibrary( module );
        }
    }
};

extern shell32_minlink shell32min;

struct ole32_minlink
{
    GLOB_REDIR( ReleaseStgMedium );
    GLOB_REDIR( CoCreateInstance );
    GLOB_REDIR( CoTaskMemFree );
    GLOB_REDIR( StringFromGUID2 );

    HMODULE module;

    inline ole32_minlink( void )
    {
        HMODULE module = LoadLibraryW( L"ole32.dll" );
        
        this->module = module;

        if ( module )
        {
            GLOB_REDIR_L( module, ReleaseStgMedium );
            GLOB_REDIR_L( module, CoCreateInstance );
            GLOB_REDIR_L( module, CoTaskMemFree );
            GLOB_REDIR_L( module, StringFromGUID2 );
        }
    }
    
    inline ~ole32_minlink( void )
    {
        if ( HMODULE module = this->module )
        {
            FreeLibrary( module );
        }
    }
};

extern ole32_minlink ole32min;