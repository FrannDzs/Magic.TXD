/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwprivate.utils.h
*  PURPOSE:
*       RenderWare private global include file containing misc utilities.
*       Those should eventually be included in more specialized headers, provided they are worth it.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_PRIVATE_INTERNAL_UTILITIES_
#define _RENDERWARE_PRIVATE_INTERNAL_UTILITIES_

#include <string.h>

// RW specific stuff.
namespace rw
{

inline bool isRwObjectInheritingFrom( EngineInterface *engineInterface, const RwObject *rwObj, RwTypeSystem::typeInfoBase *baseType )
{
    const GenericRTTI *rtObj = engineInterface->typeSystem.GetTypeStructFromConstAbstractObject( rwObj );

    if ( rtObj )
    {
        // Check whether the type of the dynamic object matches the required type.
        RwTypeSystem::typeInfoBase *objTypeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

        if ( engineInterface->typeSystem.IsTypeInheritingFrom( baseType, objTypeInfo ) )
        {
            return true;
        }
    }

    return false;
}

}

#endif //_RENDERWARE_PRIVATE_INTERNAL_UTILITIES_
