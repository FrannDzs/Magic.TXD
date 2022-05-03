/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwerrorsys.cpp
*  PURPOSE:     Error system implementation.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include <sdk/Templates.h>
#include <sdk/NumericFormat.h>

#include "rwserialize.hxx"

namespace rw
{

// *** Helpers.

static rwStaticString <wchar_t> GetKnownOperationName( EngineInterface *rwEngine, eOperationType opType )
{
    if ( opType == eOperationType::COPY_CONSTRUCT )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_COPY_CONSTRUCT" );
    }
    else if ( opType == eOperationType::ASSIGN )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_ASSIGN" );
    }
    else if ( opType == eOperationType::READ )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_READ" );
    }
    else if ( opType == eOperationType::WRITE )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_WRITE" );
    }
    else if ( opType == eOperationType::SEEK )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_SEEK" );
    }
    else if ( opType == eOperationType::SKIP )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_SKIP" );
    }
    else if ( opType == eOperationType::GETSIZE )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_GETSIZE" );
    }
    else if ( opType == eOperationType::MOVE )
    {
        return GetLanguageItem( rwEngine, L"OPNAME_MOVE" );
    }
    else
    {
        return GetLanguageItem( rwEngine, L"OPNAME_UNK" );
    }
}

// Returns a string message that does fully describe the underlying exception information.
rwStaticString <wchar_t> DescribeException( Interface *engineInterface, const RwException& except )
{
    EngineInterface *rwEngine = (EngineInterface*)engineInterface;

    // First we want to put out subsystem information.
    size_t numSubsystems = except.getNumSubsystemTypes();

    rwStaticString <wchar_t> subsys_str;
    {
        rwStaticString <wchar_t> templ_subsys = GetLanguageItem( rwEngine, L"TEMPL_SUBSYS" );

        for ( size_t n = 0; n < numSubsystems; n++ )
        {
            eSubsystemType subsys = except.getSubsystemType( n );
        
            rwStaticString <wchar_t> subsys_token;

            if ( subsys == eSubsystemType::IMAGING )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_IMAGING" );
            }
            else if ( subsys == eSubsystemType::DRAWING )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_DRAWING" );
            }
            else if ( subsys == eSubsystemType::PIXELCONV )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_PIXELCONV" );
            }
            else if ( subsys == eSubsystemType::NATIVE_TEXTURE )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_NATIVE_TEXTURE" );

                if ( const NativeTextureException *nativetex = dynamic_cast <const NativeTextureException*> ( &except ) )
                {
                    if ( const char *nativeTexName = nativetex->getNativeTextureName() )
                    {
                        rwStaticString <wchar_t> templ_ctxmsg = GetLanguageItem( rwEngine, L"TEMPL_CONTEXT_MSG" );

                        languageTokenMap_t ctxmsg_map;
                        ctxmsg_map[ L"subsysname" ] = std::move( subsys_token );
                        ctxmsg_map[ L"subsys_desc" ] = CharacterUtil::ConvertStrings <char, wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( nativeTexName );

                        subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_ctxmsg.GetConstString(), templ_ctxmsg.GetLength(), ctxmsg_map );
                    }
                }
            }
            else if ( subsys == eSubsystemType::RASTER )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_RASTER" );

                if ( const RasterException *rasterex = dynamic_cast <const RasterException*> ( &except ) )
                {
                    if ( rasterex->getRelevantRaster().is_good() )
                    {
                        const Raster *ras = rasterex->getRelevantRaster();

                        try
                        {
                            const char *rasNativeName = ras->getNativeDataTypeName();

                            rwStaticString <wchar_t> templ_ctxobj = GetLanguageItem( rwEngine, L"TEMPL_CONTEXT_OBJ" );

                            languageTokenMap_t ctxobj_map;
                            ctxobj_map[ L"subsysname" ] = std::move( subsys_token );
                            ctxobj_map[ L"objname" ] = CharacterUtil::ConvertStrings <char, wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( rasNativeName );

                            subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_ctxobj.GetConstString(), templ_ctxobj.GetLength(), ctxobj_map );
                        }
                        catch( NotInitializedException& )
                        {}
                    }
                }
            }
            else if ( subsys == eSubsystemType::DRIVER )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_DRIVER" );

                if ( const DriverException *driverex = dynamic_cast <const DriverException*> ( &except ) )
                {
                    if ( const char *driverName = driverex->getDriverName() )
                    {
                        rwStaticString <wchar_t> templ_ctxmsg = GetLanguageItem( rwEngine, L"TEMPL_CONTEXT_MSG" );

                        languageTokenMap_t ctxmsg_map;
                        ctxmsg_map[ L"subsysname" ] = std::move( subsys_token );
                        ctxmsg_map[ L"subsys_desc" ] = CharacterUtil::ConvertStrings <char, wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( driverName );

                        subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_ctxmsg.GetConstString(), templ_ctxmsg.GetLength(), ctxmsg_map );
                    }
                }
            }
            else if ( subsys == eSubsystemType::DRIVERPROG )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_DRIVERPROG" );

                if ( const DriverProgramException *progex = dynamic_cast <const DriverProgramException*> ( &except ) )
                {
                    if ( const char *progName = progex->getDriverProgramType() )
                    {
                        rwStaticString <wchar_t> templ_ctxmsg = GetLanguageItem( rwEngine, L"TEMPL_CONTEXT_MSG" );

                        languageTokenMap_t ctxmsg_map;
                        ctxmsg_map[ L"subsysname" ] = std::move( subsys_token );
                        ctxmsg_map[ L"subsys_desc" ] = CharacterUtil::ConvertStrings <char, wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( progName );

                        subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_ctxmsg.GetConstString(), templ_ctxmsg.GetLength(), ctxmsg_map );
                    }
                }
            }
            else if ( subsys == eSubsystemType::MEMORY )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_MEMORY" );
            }
            else if ( subsys == eSubsystemType::PLUGIN )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_PLUGIN" );
            }
            else if ( subsys == eSubsystemType::NATIVE_IMAGING )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_NATIVE_IMAGING" );

                if ( const NativeImagingException *natimgex = dynamic_cast <const NativeImagingException*> ( &except ) )
                {
                    if ( const char *fmtname = natimgex->getNativeImageFormatName() )
                    {
                        rwStaticString <wchar_t> templ_ctxmsg = GetLanguageItem( rwEngine, L"TEMPL_CONTEXT_MSG" );

                        languageTokenMap_t ctxmsg_map;
                        ctxmsg_map[ L"subsysname" ] = std::move( subsys_token );
                        ctxmsg_map[ L"subsys_desc" ] = CharacterUtil::ConvertStrings <char, wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( fmtname );

                        subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_ctxmsg.GetConstString(), templ_ctxmsg.GetLength(), ctxmsg_map );
                    }
                }
            }
            else if ( subsys == eSubsystemType::QOL )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_QOL" );
            }
            else if ( subsys == eSubsystemType::FILE )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_FILE" );
            }
            else if ( subsys == eSubsystemType::LOCALIZATION )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_LOCALIZATION" );
            }
            else if ( subsys == eSubsystemType::CONFIG )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_CONFIG" );
            }
            else if ( subsys == eSubsystemType::STREAM )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_STREAM" );
            }
            else if ( subsys == eSubsystemType::PALETTE )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_PALETTE" );
            }
            else if ( subsys == eSubsystemType::RESIZING )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_RESIZING" );
            }
            else if ( subsys == eSubsystemType::TYPESYSTEM )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_TYPESYSTEM" );

                if ( const TypeSystemException *typesysex = dynamic_cast <const TypeSystemException*> ( &except ) )
                {
                    if ( const char *typeName = typesysex->getRelevantTypeName() )
                    {
                        rwStaticString <wchar_t> templ_ctxmsg = GetLanguageItem( rwEngine, L"TEMPL_CONTEXT_MSG" );

                        languageTokenMap_t ctxmsg_map;
                        ctxmsg_map[ L"subsysname" ] = std::move( subsys_token );
                        ctxmsg_map[ L"subsys_desc" ] = CharacterUtil::ConvertStrings <char, wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( typeName );

                        subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_ctxmsg.GetConstString(), templ_ctxmsg.GetLength(), ctxmsg_map );
                    }
                }
            }
            else if ( subsys == eSubsystemType::RWOBJECTS )
            {
                if ( const RwObjectsException *rwobjex = dynamic_cast <const RwObjectsException*> ( &except ) )
                {
                    if ( rwobjex->getRelevantObject().is_good() )
                    {
                        const RwObject *rwobj = rwobjex->getRelevantObject();

                        subsys_token = GetObjectNicePrefixString( rwEngine, rwobj );
                    }
                }
                
                if ( subsys_token.IsEmpty() )
                {
                    subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_RWOBJECTS" );
                }
            }
            else if ( subsys == eSubsystemType::SERIALIZATION )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_SERIALIZATION" );
            }
            else if ( subsys == eSubsystemType::BLOCKAPI )
            {
                if ( const BlockAPIException *blkex = dynamic_cast <const BlockAPIException*> ( &except ) )
                {
                    rwStaticString <wchar_t> templ_blockapi = GetLanguageItem( rwEngine, L"TEMPL_CONTEXT_BLOCKAPI" );

                    uint32 faultBlockID = blkex->getFaultBlockID();
                    uint64 faultBlockOffset = blkex->getFaultBlockOffset();

                    languageTokenMap_t blockapi_map;
                    
                    if ( const wchar_t *token_chunkName = GetTokenNameForChunkID( rwEngine, faultBlockID ) )
                    {
                        blockapi_map[ L"chunkname" ] = GetLanguageItem( rwEngine, token_chunkName );
                    }
                    else
                    {
                        blockapi_map[ L"chunkname" ] = eir::to_string <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( faultBlockID );
                    }

                    blockapi_map[ L"offset" ] = eir::to_string <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( faultBlockOffset );

                    subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_blockapi.GetConstString(), templ_blockapi.GetLength(), blockapi_map );
                }
                else
                {
                    subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_BLOCKAPI" );
                }
            }
            else if ( subsys == eSubsystemType::EVENTSYS )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_EVENTSYSTEM" );
            }
            else if ( subsys == eSubsystemType::UTILITIES )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_UTILS" );
            }
            else if ( subsys == eSubsystemType::WINDOWING )
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_WINDOWING" );
            }
            else
            {
                subsys_token = GetLanguageItem( rwEngine, L"SUBSYS_UNKNOWN" );
            }

            // Create the subsystem enclosed token.
            {
                languageTokenMap_t subsys_map;
                subsys_map[ L"content" ] = std::move( subsys_token );

                subsys_token = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_subsys.GetConstString(), templ_subsys.GetLength(), subsys_map );
            }

            // Combine the tokens.
            if ( subsys_str.IsEmpty() )
            {
                subsys_str = std::move( subsys_token );
            }
            else
            {
                rwStaticString <wchar_t> templ_subsys_combine = GetLanguageItem( rwEngine, L"TEMPL_TWOSUBSYS" );

                languageTokenMap_t combine_map;
                combine_map[ L"first" ] = std::move( subsys_str );
                combine_map[ L"second" ] = std::move( subsys_token );

                subsys_str = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_subsys_combine.GetConstString(), templ_subsys_combine.GetLength(), combine_map );
            }
        }
    }

    // Chose the correct output message template based on the actual exception class.
    eErrorType exceptType = except.getErrorType();

    rwStaticString <wchar_t> exmsg;

    if ( exceptType == eErrorType::OUT_OF_MEMORY )
    {
        // The class must exist in the RwException inheritance tree; else throw fatal exception.
        const OutOfMemoryException& oomex = dynamic_cast <const OutOfMemoryException&> ( except );

        size_t reqSize = oomex.getSizeOfDeniedRequest();

        if ( reqSize > 0 )
        {
            rwStaticString <wchar_t> templ_oom = GetLanguageItem( rwEngine, L"TEMPL_OOM_REQSIZE" );

            languageTokenMap_t oom_map;
            oom_map[ L"reqsize" ] = eir::to_string <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( reqSize );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_oom.GetConstString(), templ_oom.GetLength(), oom_map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_OOM" );
        }
    }
    else if ( exceptType == eErrorType::NOTINIT )
    {
        const NotInitializedException& notinitex = dynamic_cast <const NotInitializedException&> ( except );

        if ( const wchar_t *tokenWhat = notinitex.GetTokenWhat() )
        {
            rwStaticString <wchar_t> templ_notinit = GetLanguageItem( rwEngine, L"TEMPL_NOTINIT_WHAT" );

            languageTokenMap_t notinit_map;
            notinit_map[ L"what" ] = GetLanguageItem( rwEngine, tokenWhat );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_notinit.GetConstString(), templ_notinit.GetLength(), notinit_map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_NOTINIT" );
        }
    }
    else if ( exceptType == eErrorType::UNSUPPOP )
    {
        const UnsupportedOperationException& unsuppex = dynamic_cast <const UnsupportedOperationException&> ( except );

        // Get the operation name, if available.
        rwStaticString <wchar_t> opname;

        eOperationType opType = unsuppex.getOperationType();

        if ( opType == eOperationType::CUSTOM )
        {
            if ( const wchar_t *optoken = unsuppex.getOperationCustomToken() )
            {
                opname = GetLanguageItem( rwEngine, optoken );
            }
        }
        else
        {
            opname = GetKnownOperationName( rwEngine, opType );
        }

        // Get the reason, if available.
        rwStaticString <wchar_t> reason;

        if ( const wchar_t *tokenReason = unsuppex.getTokenReason() )
        {
            reason = GetLanguageItem( rwEngine, tokenReason );
        }

        // Now decide which template to pick.
        if ( reason.IsEmpty() == false && opname.IsEmpty() == false )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_UNSUPPOP_REASON_OP" );

            languageTokenMap_t map;
            map[ L"opname" ] = std::move( opname );
            map[ L"reason" ] = std::move( reason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else if ( reason.IsEmpty() == false )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_UNSUPPOP_REASON" );

            languageTokenMap_t map;
            map[ L"reason" ] = std::move( reason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else if ( opname.IsEmpty() == false )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_UNSUPPOP_OP" );

            languageTokenMap_t map;
            map[ L"opname" ] = std::move( opname );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_UNSUPPOP" );
        }
    }
    else if ( exceptType == eErrorType::INVALIDCFG )
    {
        const InvalidConfigurationException& invcfgex = dynamic_cast <const InvalidConfigurationException&> ( except );

        if ( const wchar_t *tokenReasonWhat = invcfgex.getReasonWhatToken() )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INVALIDCFG_REASON" );

            languageTokenMap_t map;
            map[ L"reason" ] = GetLanguageItem( rwEngine, tokenReasonWhat );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_INVALIDCFG" );
        }
    }
    else if ( exceptType == eErrorType::STRUCTERR )
    {
        const StructuralErrorException& structex = dynamic_cast <const StructuralErrorException&> ( except );

        if ( const wchar_t *tokenReason = structex.getReasonToken() )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_STRUCTERR_REASON" );

            languageTokenMap_t map;
            map[ L"reason" ] = GetLanguageItem( rwEngine, tokenReason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_STRUCTERR" );
        }
    }
    else if ( exceptType == eErrorType::INVALIDOP )
    {
        const InvalidOperationException& invopex = dynamic_cast <const InvalidOperationException&> ( except );

        // Get the operation name, if available.
        rwStaticString <wchar_t> opname;

        eOperationType opType = invopex.getOperationType();

        if ( opType == eOperationType::CUSTOM )
        {
            if ( const wchar_t *optoken = invopex.getOperationCustomToken() )
            {
                opname = GetLanguageItem( rwEngine, optoken );
            }
        }
        else
        {
            opname = GetKnownOperationName( rwEngine, opType );
        }

        // Get the reason, if available.
        rwStaticString <wchar_t> reason;

        if ( const wchar_t *tokenReason = invopex.getTokenReason() )
        {
            reason = GetLanguageItem( rwEngine, tokenReason );
        }

        // Now decide which template to pick.
        if ( reason.IsEmpty() == false && opname.IsEmpty() == false )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INVALIDOP_OP_REASON" );

            languageTokenMap_t map;
            map[ L"opname" ] = std::move( opname );
            map[ L"reason" ] = std::move( reason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else if ( reason.IsEmpty() == false )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INVALIDOP_REASON" );

            languageTokenMap_t map;
            map[ L"reason" ] = std::move( reason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else if ( opname.IsEmpty() == false )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INVALIDOP_OP" );

            languageTokenMap_t map;
            map[ L"opname" ] = std::move( opname );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_INVALIDOP" );
        }
    }
    else if ( exceptType == eErrorType::INVALIDPARAM )
    {
        const InvalidParameterException& invparamex = dynamic_cast <const InvalidParameterException&> ( except );

        const wchar_t *tokenWhat = invparamex.getWhatToken();
        const wchar_t *tokenReason = invparamex.getTokenReason();

        if ( tokenWhat && tokenReason )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INVALIDPARAM_WHAT_REASON" );

            languageTokenMap_t map;
            map[ L"what" ] = GetLanguageItem( rwEngine, tokenWhat );
            map[ L"reason" ] = GetLanguageItem( rwEngine, tokenReason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else if ( tokenWhat )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INVALIDPARAM_WHAT" );

            languageTokenMap_t map;
            map[ L"what" ] = GetLanguageItem( rwEngine, tokenWhat );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else if ( tokenReason )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INVALIDPARAM_REASON" );

            languageTokenMap_t map;
            map[ L"reason" ] = GetLanguageItem( rwEngine, tokenReason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_INVALIDPARAM" );
        }
    }
    else if ( exceptType == eErrorType::RESEXHAUST )
    {
        const ResourcesExhaustedException& exhaustex = dynamic_cast <const ResourcesExhaustedException&> ( except );

        if ( const wchar_t *tokenWhat = exhaustex.getResourceTypeToken() )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_RESEXHAUST_WHAT" );

            languageTokenMap_t map;
            map[ L"what" ] = GetLanguageItem( rwEngine, tokenWhat );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_RESEXHAUST" );
        }
    }
    else if ( exceptType == eErrorType::INTERNERR )
    {
        const InternalErrorException& internex = dynamic_cast <const InternalErrorException&> ( except );

        if ( const wchar_t *tokenReason = internex.getReasonToken() )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_INTERNERR_REASON" );

            languageTokenMap_t map;
            map[ L"reason" ] = GetLanguageItem( rwEngine, tokenReason );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_INTERNERR" );
        }
    }
    else if ( exceptType == eErrorType::NOTFOUND )
    {
        const NotFoundException& notfoundex = dynamic_cast <const NotFoundException&> ( except );

        if ( const wchar_t *tokenWhat = notfoundex.getWhatToken() )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_NOTFOUND" );

            languageTokenMap_t map;
            map[ L"what" ] = GetLanguageItem( rwEngine, tokenWhat );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = GetLanguageItem( rwEngine, L"TEMPL_NOTFOUND" );
        }
    }
    else if ( exceptType == eErrorType::UNLOCALAINTERNERR )
    {
        const UnlocalizedAInternalErrorException& unlocalaex = dynamic_cast <const UnlocalizedAInternalErrorException&> ( except );

        if ( const wchar_t *tokenTemplate = unlocalaex.getTemplateToken() )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, tokenTemplate );

            languageTokenMap_t map;
            map[ L"message" ] = CharacterUtil::ConvertStrings <char, wchar_t> ( unlocalaex.getReason() );

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = CharacterUtil::ConvertStrings <char, wchar_t> ( unlocalaex.getReason() );
        }
    }
    else if ( exceptType == eErrorType::UNLOCALWINTERNERR )
    {
        const UnlocalizedWInternalErrorException& unlocalaex = dynamic_cast <const UnlocalizedWInternalErrorException&> ( except );

        if ( const wchar_t *tokenTemplate = unlocalaex.getTemplateToken() )
        {
            rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, tokenTemplate );

            languageTokenMap_t map;
            map[ L"message" ] = unlocalaex.getReason();

            exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
        }
        else
        {
            exmsg = unlocalaex.getReason();
        }
    }
    else if ( exceptType == eErrorType::UNEXPCHUNK )
    {
        const UnexpectedChunkException& unexpchunk = dynamic_cast <const UnexpectedChunkException&> ( except );

        uint32 expChunkID = unexpchunk.getExpectedChunkID();
        uint32 foundChunkID = unexpchunk.getFoundChunkID();

        rwStaticString <wchar_t> templ = GetLanguageItem( rwEngine, L"TEMPL_UNEXPCHUNK" );

        languageTokenMap_t map;

        if ( const wchar_t *tokenExpectedName = GetTokenNameForChunkID( rwEngine, expChunkID ) )
        {
            map[ L"expected" ] = GetLanguageItem( rwEngine, tokenExpectedName );
        }
        else
        {
            map[ L"expected" ] = eir::to_string <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( expChunkID );
        }

        if ( const wchar_t *tokenFoundName = GetTokenNameForChunkID( rwEngine, foundChunkID ) )
        {
            map[ L"found" ] = GetLanguageItem( rwEngine, tokenFoundName );
        }
        else
        {
            map[ L"found" ] = eir::to_string <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( foundChunkID );
        }

        exmsg = eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ.GetConstString(), templ.GetLength(), map );
    }
    else
    {
        exmsg = GetLanguageItem( rwEngine, L"UNKEXCEPT" );
    }

    // Combine subsystem info and exception message together.
    rwStaticString <wchar_t> templ_genexcept = GetLanguageItem( rwEngine, L"TEMPL_GENEXCEPT" );

    languageTokenMap_t genexcept_map;
    genexcept_map[ L"subsys" ] = std::move( subsys_str );
    genexcept_map[ L"msgcontent" ] = std::move( exmsg );

    return eir::assign_template <wchar_t, RwStaticMemAllocator, rwEirExceptionManager> ( templ_genexcept.GetConstString(), templ_genexcept.GetLength(), genexcept_map );
}

} // namespace rw