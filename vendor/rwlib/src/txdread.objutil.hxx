/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/txdread.objutil.hxx
*  PURPOSE:     TexDictionary consistency definitions
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_TEXDICT_OBJECT_UTILS_
#define _RENDERWARE_TEXDICT_OBJECT_UTILS_

#include "txdread.common.hxx"

namespace rw
{

// We need a TXD consistency lock.
struct _fetchTXDTypeStructoid
{
    AINLINE static RwTypeSystem::typeInfoBase* resolveType( EngineInterface *engineInterface )
    {
        texDictionaryStreamPlugin *txdEnv = texDictionaryStreamStore.get().GetPluginStruct( engineInterface );

        if ( txdEnv )
        {
            return txdEnv->txdTypeInfo;
        }

        return nullptr;
    }
};

typedef rwobjLockTypeRegister <_fetchTXDTypeStructoid> txdConsistencyLockEnv;

extern optional_struct_space <PluginDependantStructRegister <txdConsistencyLockEnv, RwInterfaceFactory_t>> txdConsistencyLockRegister;


inline rwlock* GetTXDLock( const rw::TexDictionary *txdHandle )
{
    EngineInterface *engineInterface = (EngineInterface*)txdHandle->engineInterface;

    txdConsistencyLockEnv *txdEnv = txdConsistencyLockRegister.get().GetPluginStruct( engineInterface );

    if ( txdEnv )
    {
        return txdEnv->GetLock( engineInterface, txdHandle );
    }

    return nullptr;
}

};

#endif //_RENDERWARE_TEXDICT_OBJECT_UTILS_