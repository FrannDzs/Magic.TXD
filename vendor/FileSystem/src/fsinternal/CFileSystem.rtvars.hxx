/*****************************************************************************
*
*  PROJECT:     Eir FileSystem
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.random.h
*  PURPOSE:     Runtime local variable storage
*
*  Get Eir FileSystem from https://osdn.net/projects/eirfs/
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYS_RTVAR_STORAGE_
#define _FILESYS_RTVAR_STORAGE_

#include "CFileSystem.internal.h"

#ifdef FILESYS_MULTI_THREADING
#include <CExecutiveManager.h>
#endif //FILESYS_MULTI_THREADING

template <typename structType>
struct rtvarsPluginRegistration
{
    inline rtvarsPluginRegistration( const fs_construction_params& params )
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CExecutiveManager *execMan = (NativeExecutive::CExecutiveManager*)params.nativeExecMan;

        if ( execMan )
        {
            this->thPluginReg.RegisterPlugin( execMan );
            return;
        }
#endif //FILESYS_MULTI_THREADING

        this->sysPluginOff = _fileSysFactory.RegisterStructPlugin <structType> ();
    }
    inline rtvarsPluginRegistration( const rtvarsPluginRegistration& ) = delete;
    inline rtvarsPluginRegistration( rtvarsPluginRegistration&& ) = delete;

    inline ~rtvarsPluginRegistration( void )
    {
        if ( fileSystemFactory_t::IsOffsetValid( this->sysPluginOff ) )
        {
            _fileSysFactory.UnregisterPlugin( this->sysPluginOff );
        }

#ifdef FILESYS_MULTI_THREADING
        this->thPluginReg.UnregisterPlugin();
#endif //FILESYS_MULTI_THREADING
    }

    inline rtvarsPluginRegistration& operator = ( const rtvarsPluginRegistration& ) = delete;
    inline rtvarsPluginRegistration& operator = ( rtvarsPluginRegistration&& ) = delete;

    inline structType* GetPluginStruct( CFileSystemNative *fileSys, bool onlyIfCached = false )
    {
#ifdef FILESYS_MULTI_THREADING
        NativeExecutive::CExecutiveManager *execMan = fileSys->nativeMan;

        if ( execMan )
        {
            return thPluginReg.GetPluginStructCurrent( onlyIfCached );
        }
#endif //FILESYS_MULTI_THREADING

        return fileSystemFactory_t::RESOLVE_STRUCT <structType> ( fileSys, this->sysPluginOff );
    }

private:
#ifdef FILESYS_MULTI_THREADING
    NativeExecutive::execThreadStructPluginRegister <structType> thPluginReg;
#endif //FILESYS_MULTI_THREADING
    fileSystemFactory_t::pluginOffset_t sysPluginOff = fileSystemFactory_t::INVALID_PLUGIN_OFFSET;
};

#endif //_FILESYS_RTVAR_STORAGE_