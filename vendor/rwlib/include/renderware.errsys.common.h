/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.errsys.common.h
*  PURPOSE:     Error system exception definitions.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_ERROR_SYSTEM_COMMON_HEADER_
#define _RENDERWARE_ERROR_SYSTEM_COMMON_HEADER_

namespace rw
{

// Enumeration definitons of all available subsystems/components.
enum class eSubsystemType
{
    IMAGING,
    DRAWING,
    PIXELCONV,
    NATIVE_TEXTURE,
    RASTER,
    DRIVER,
    DRIVERPROG,
    MEMORY,
    PLUGIN,
    NATIVE_IMAGING,
    QOL,
    FILE,
    LOCALIZATION,
    CONFIG,
    STREAM,
    PALETTE,
    RESIZING,
    TYPESYSTEM,
    RWOBJECTS,
    SERIALIZATION,
    BLOCKAPI,
    EVENTSYS,
    UTILITIES,
    WINDOWING
};

// Commonly known error types.
enum class eErrorType
{
    UNSPECIFIED,
    OUT_OF_MEMORY,                  // OutOfMemoryException
    NOTINIT,                        // NotInitializedException
    UNSUPPOP,                       // UnsupportedOperationException
    INVALIDCFG,                     // InvalidConfigurationException
    STRUCTERR,                      // StructuralErrorException
    INVALIDOP,                      // InvalidOperationException
    INVALIDPARAM,                   // InvalidParameterException
    RESEXHAUST,                     // ResourcesExhaustedException
    INTERNERR,                      // InternalErrorException
    NOTFOUND,                       // NotFoundException
    UNLOCALAINTERNERR,              // UnlocalizedAInternalErrorException
    UNLOCALWINTERNERR,              // UnlocalizedWInternalErrorException
    UNEXPCHUNK                      // UnexpectedChunkException
};

// Commonly known operation types.
enum class eOperationType
{
    CUSTOM,
    COPY_CONSTRUCT,
    ASSIGN,
    READ,
    WRITE,
    SEEK,
    SKIP,
    GETSIZE,
    MOVE
};

//================================
// *** Main exception classes ***
//================================

// Main exception base class of this RenderWare framework.
// If you want to catch it, please make sure that you compile your project with the same compiler as this framework
// (a.k.a. ABI compatibility).
struct RwException
{
    inline RwException( eErrorType errorType, eSubsystemType subsysType ) noexcept
    {
        this->errorType = errorType;
        this->subsysTypes[0] = subsysType;
        this->numSubsysTypes = 1;
    }
    inline RwException( eErrorType errorType, eSubsystemType one, eSubsystemType two ) noexcept
    {
        this->errorType = errorType;
        this->subsysTypes[0] = one;
        this->subsysTypes[1] = two;
        this->numSubsysTypes = 2;
    }
    inline RwException( RwException&& ) = default;
    inline RwException( const RwException& ) = default;

    virtual ~RwException( void )        {}

    inline RwException& operator = ( RwException&& ) = default;
    inline RwException& operator = ( const RwException& ) = default;

    // Returns the actual exception class type that this exception can be cast to.
    inline eErrorType getErrorType( void ) const noexcept
    {
        return this->errorType;
    }

    // Returns the subsystem that this exception originated from.
    // Gives you a hint of what data can be attached to this exception.
    inline eSubsystemType getSubsystemType( size_t idx ) const noexcept
    {
        return this->subsysTypes[idx];
    }

    inline size_t getNumSubsystemTypes( void ) const noexcept
    {
        return this->numSubsysTypes;
    }

private:
    eErrorType errorType;
    eSubsystemType subsysTypes[4];
    size_t numSubsysTypes;
};

// Thrown if an out-of-system memory condition was found. Since this is a very serious error case
// it deserves its' own exception class.
struct OutOfMemoryException : public RwException
{
    // It is discouraged to inherit from OutOfMemoryException in any way. When this exception is thrown the system
    // is considered in a vulnerable state. To rescue the application you can consider releasing memory
    // of very big objects, like textures or geometries, and load them on-demand only.

    inline OutOfMemoryException( eSubsystemType subsys, size_t sizeOfDeniedRequest ) noexcept
        : RwException( eErrorType::OUT_OF_MEMORY, subsys ), sizeOfDeniedRequest( sizeOfDeniedRequest )
    {
        return;
    }

    inline size_t getSizeOfDeniedRequest( void ) const noexcept
    {
        return this->sizeOfDeniedRequest;
    }

private:
    size_t sizeOfDeniedRequest;
};

// Thrown if an object was accessed in a certain property that required initialization first.
// An example is access to an uninitialized QoL type like StreamPtr.
struct NotInitializedException : public virtual RwException
{
    inline NotInitializedException( eSubsystemType subsys, const wchar_t *descTokenWhat ) noexcept
        : RwException( eErrorType::NOTINIT, subsys ), descTokenWhat( descTokenWhat )
    {
        return;
    }
    inline NotInitializedException( NotInitializedException&& ) = default;
    inline NotInitializedException( const NotInitializedException& ) = default;

    inline NotInitializedException& operator = ( NotInitializedException&& ) = default;
    inline NotInitializedException& operator = ( const NotInitializedException& ) = default;

    inline const wchar_t* GetTokenWhat( void ) const noexcept
    {
        return this->descTokenWhat;
    }

private:
    const wchar_t *descTokenWhat;
};

// Thrown if an operation was attempted that is not supported in a certain framework configuration.
struct UnsupportedOperationException : public virtual RwException
{
    inline UnsupportedOperationException( eSubsystemType subsys, eOperationType opType, const wchar_t *descTokenReason ) noexcept
        : RwException( eErrorType::UNSUPPOP, subsys ), opType( opType ), descTokenCustom( nullptr ), descTokenReason( descTokenReason )
    {
        return;
    }
    inline UnsupportedOperationException( eSubsystemType subsys, const wchar_t *descTokenOp, const wchar_t *descTokenReason ) noexcept
        : RwException( eErrorType::UNSUPPOP, subsys ), opType( eOperationType::CUSTOM ), descTokenCustom( descTokenOp ), descTokenReason( descTokenReason )
    {
        return;
    }

    inline UnsupportedOperationException( UnsupportedOperationException&& ) = default;
    inline UnsupportedOperationException( const UnsupportedOperationException& ) = default;

    inline UnsupportedOperationException& operator = ( UnsupportedOperationException&& ) = default;
    inline UnsupportedOperationException& operator = ( const UnsupportedOperationException& ) = default;

    inline eOperationType getOperationType( void ) const noexcept
    {
        return this->opType;
    }

    inline const wchar_t* getOperationCustomToken( void ) const noexcept
    {
        return this->descTokenCustom;
    }

    inline const wchar_t* getTokenReason( void ) const noexcept
    {
        return this->descTokenReason;
    }

private:
    eOperationType opType;
    const wchar_t *descTokenCustom;
    const wchar_t *descTokenReason;
};

// Thrown if an invalid configuration was detected during runtime. This could happen
// if multiple parameters to assemble a configuration were detected but put together
// they make no sense.
struct InvalidConfigurationException : public virtual RwException
{
    inline InvalidConfigurationException( eSubsystemType subsys, const wchar_t *descTokenReasonWhat ) noexcept
        : RwException( eErrorType::INVALIDCFG, subsys ), descTokenReasonWhat( descTokenReasonWhat )
    {
        return;
    }

    inline InvalidConfigurationException( InvalidConfigurationException&& ) = default;
    inline InvalidConfigurationException( const InvalidConfigurationException& ) = default;

    inline InvalidConfigurationException& operator = ( InvalidConfigurationException&& ) = default;
    inline InvalidConfigurationException& operator = ( const InvalidConfigurationException& ) = default;

    inline const wchar_t* getReasonWhatToken( void ) const noexcept
    {
        return this->descTokenReasonWhat;
    }

private:
    const wchar_t *descTokenReasonWhat;
};

// Thrown if an invalid (file) structure was detected during parsing. The difference to
// InvalidConfigurationException is that we throw for external configuration reasons, like
// files and not parameters passed to routines by the user program.
struct StructuralErrorException : public virtual RwException
{
    inline StructuralErrorException( eSubsystemType subsys, const wchar_t *descTokenReason ) noexcept
        : RwException( eErrorType::STRUCTERR, subsys ), descTokenReason( descTokenReason )
    {
        return;
    }

    inline StructuralErrorException( StructuralErrorException&& ) = default;
    inline StructuralErrorException( const StructuralErrorException& ) = default;

    inline StructuralErrorException& operator = ( StructuralErrorException&& ) = default;
    inline StructuralErrorException& operator = ( const StructuralErrorException& ) = default;

    inline const wchar_t* getReasonToken( void ) const noexcept
    {
        return this->descTokenReason;
    }

private:
    const wchar_t *descTokenReason;
};

// Thrown if an invalid operation was attempted to be performed. While unsupported operations
// can be enabled in the future, this exception is thrown in cases which should be undisputably
// illegal.
struct InvalidOperationException : public virtual RwException
{
    inline InvalidOperationException( eSubsystemType subsys, eOperationType opType, const wchar_t *descTokenReason ) noexcept
        : RwException( eErrorType::INVALIDOP, subsys ), opType( opType ), descTokenReason( descTokenReason )
    {
        return;
    }
    inline InvalidOperationException( eSubsystemType subsys, const wchar_t *descTokenOp, const wchar_t *descTokenReason ) noexcept
        : RwException( eErrorType::INVALIDOP, subsys ), descTokenCustom( descTokenOp ), descTokenReason( descTokenReason )
    {
        return;
    }

    inline InvalidOperationException( InvalidOperationException&& ) = default;
    inline InvalidOperationException( const InvalidOperationException& ) = default;

    inline InvalidOperationException& operator = ( InvalidOperationException&& ) = default;
    inline InvalidOperationException& operator = ( const InvalidOperationException& ) = default;

    inline eOperationType getOperationType( void ) const noexcept
    {
        return this->opType;
    }

    inline const wchar_t* getOperationCustomToken( void ) const noexcept
    {
        return this->descTokenCustom;
    }

    inline const wchar_t* getTokenReason( void ) const noexcept
    {
        return this->descTokenReason;
    }

private:
    eOperationType opType;
    const wchar_t *descTokenCustom;
    const wchar_t *descTokenReason;
};

// Thrown if an invalid parameter was passed to a routine. While multiple parameters make
// up a configuration, a single parameter at face value could be invalid.
struct InvalidParameterException : public virtual RwException
{
    inline InvalidParameterException( eSubsystemType subsys, const wchar_t *descTokenWhat, const wchar_t *descTokenReason ) noexcept
        : RwException( eErrorType::INVALIDPARAM, subsys ), descTokenWhat( descTokenWhat ), descTokenReason( descTokenReason )
    {
        return;
    }

    inline InvalidParameterException( InvalidParameterException&& ) = default;
    inline InvalidParameterException( const InvalidParameterException& ) = default;

    inline InvalidParameterException& operator = ( InvalidParameterException&& ) = default;
    inline InvalidParameterException& operator = ( const InvalidParameterException& ) = default;

    inline const wchar_t* getWhatToken( void ) const noexcept
    {
        return this->descTokenWhat;
    }

    inline const wchar_t* getTokenReason( void ) const noexcept
    {
        return this->descTokenReason;
    }

private:
    const wchar_t *descTokenWhat;
    const wchar_t *descTokenReason;
};

// Thrown if a resource request that is not memory has failed during runtime.
struct ResourcesExhaustedException : public virtual RwException
{
    inline ResourcesExhaustedException( eSubsystemType subsys, const wchar_t *descTokenResourceType ) noexcept
        : RwException( eErrorType::RESEXHAUST, subsys ), descTokenResourceType( descTokenResourceType )
    {
        return;
    }

    inline ResourcesExhaustedException( ResourcesExhaustedException&& ) = default;
    inline ResourcesExhaustedException( const ResourcesExhaustedException& ) = default;

    inline ResourcesExhaustedException& operator = ( ResourcesExhaustedException&& ) = default;
    inline ResourcesExhaustedException& operator = ( const ResourcesExhaustedException& ) = default;

    inline const wchar_t* getResourceTypeToken( void ) const noexcept
    {
        return this->descTokenResourceType;
    }

private:
    const wchar_t *descTokenResourceType;
};

// Thrown if an error was detected that is of internal nature and should not be usually
// encountered. Errors of this category should be reported with reproducible steps to
// the official bug tracker/forums.
struct InternalErrorException : public virtual RwException
{
    inline InternalErrorException( eSubsystemType subsys, const wchar_t *descTokenReason ) noexcept
        : RwException( eErrorType::INTERNERR, subsys ), descTokenReason( descTokenReason )
    {
        return;
    }

    inline InternalErrorException( InternalErrorException&& ) = default;
    inline InternalErrorException( const InternalErrorException& ) = default;

    inline InternalErrorException& operator = ( InternalErrorException&& ) = default;
    inline InternalErrorException& operator = ( const InternalErrorException& ) = default;

    inline const wchar_t* getReasonToken( void ) const noexcept
    {
        return this->descTokenReason;
    }

private:
    const wchar_t *descTokenReason;
};

// Thrown if the requested resource/item was not found. It could still exist at different
// engine configurations or runtime states.
struct NotFoundException : public virtual RwException
{
    inline NotFoundException( eSubsystemType subsys, const wchar_t *descTokenWhat ) noexcept
        : RwException( eErrorType::NOTFOUND, subsys ), descTokenWhat( descTokenWhat )
    {
        return;
    }

    inline NotFoundException( NotFoundException&& ) = default;
    inline NotFoundException( const NotFoundException& ) = default;

    inline NotFoundException& operator = ( NotFoundException&& ) = default;
    inline NotFoundException& operator = ( const NotFoundException& ) = default;

    inline const wchar_t* getWhatToken( void ) const noexcept
    {
        return this->descTokenWhat;
    }

private:
    const wchar_t *descTokenWhat;
};

//===============================================================
// *** Specialized exception helpers for context attachment ***
//===============================================================

// OoM is missing on purpose here: don't inherit from it!

#define RWEXCEPT_NOTINIT_ARGUMENTS_DEFINE           const wchar_t *ni_descTokenWhat
#define RWEXCEPT_NOTINIT_ARGUMENTS_USE              ni_descTokenWhat
#define RWEXCEPT_UNSUPPOP_FIRST_ARGUMENTS_DEFINE        eOperationType uo_opType, const wchar_t *uo_descTokenReason
#define RWEXCEPT_UNSUPPOP_SECOND_ARGUMENTS_DEFINE       const wchar_t *uo_descTokenOp, const wchar_t *uo_descTokenReason
#define RWEXCEPT_UNSUPPOP_FIRST_ARGUMENTS_USE           uo_opType, uo_descTokenReason
#define RWEXCEPT_UNSUPPOP_SECOND_ARGUMENTS_USE          uo_descTokenOp, uo_descTokenReason
#define RWEXCEPT_INVALIDCFG_ARGUMENTS_DEFINE        const wchar_t *ic_descTokenReasonWhat
#define RWEXCEPT_INVALIDCFG_ARGUMENTS_USE           ic_descTokenReasonWhat
#define RWEXCEPT_STRUCTERR_ARGUMENTS_DEFINE         const wchar_t *se_descTokenReason
#define RWEXCEPT_STRUCTERR_ARGUMENTS_USE            se_descTokenReason
#define RWEXCEPT_INVALIDOP_FIRST_ARGUMENTS_DEFINE       RWEXCEPT_UNSUPPOP_FIRST_ARGUMENTS_DEFINE
#define RWEXCEPT_INVALIDOP_SECOND_ARGUMENTS_DEFINE      RWEXCEPT_UNSUPPOP_SECOND_ARGUMENTS_DEFINE
#define RWEXCEPT_INVALIDOP_FIRST_ARGUMENTS_USE          RWEXCEPT_UNSUPPOP_FIRST_ARGUMENTS_USE
#define RWEXCEPT_INVALIDOP_SECOND_ARGUMENTS_USE         RWEXCEPT_UNSUPPOP_SECOND_ARGUMENTS_USE
#define RWEXCEPT_INVALIDPARAM_ARGUMENTS_DEFINE      const wchar_t *ip_descTokenWhat, const wchar_t *ip_descTokenReason
#define RWEXCEPT_INVALIDPARAM_ARGUMENTS_USE         ip_descTokenWhat, ip_descTokenReason
#define RWEXCEPT_RESEXHAUST_ARGUMENTS_DEFINE        const wchar_t *re_descTokenResourceType
#define RWEXCEPT_RESEXHAUST_ARGUMENTS_USE           re_descTokenResourceType
#define RWEXCEPT_INTERNERR_ARGUMENTS_DEFINE         const wchar_t *ie_descTokenReason
#define RWEXCEPT_INTERNERR_ARGUMENTS_USE            ie_descTokenReason
#define RWEXCEPT_NOTFOUND_ARGUMENTS_DEFINE          const wchar_t *nf_descTokenWhat
#define RWEXCEPT_NOTFOUND_ARGUMENTS_USE             nf_descTokenWhat

#define RW_EXCEPTION_TEMPLATE( ExceptionName, SubSystemClassName, SubSystemEnumName, ExceptionClassMacroName ) \
    struct SubSystemClassName##ExceptionName##Exception : public ExceptionName##Exception, public SubSystemClassName##Exception \
    { \
        inline SubSystemClassName##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName, RWEXCEPT_##ExceptionClassMacroName##_ARGUMENTS_USE ), \
              SubSystemClassName##Exception( RWEXCEPT_##SubSystemEnumName##_ARGUMENTS_USE ) \
        { \
        } \
    }
#define RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS( ExceptionName, SubSystemName1, SubSystemEnumName1, SubSystemClassName2, SubSystemEnumName2, ExceptionClassMacroName ) \
    struct SubSystemName1##SubSystemClassName2##ExceptionName##Exception : public ExceptionName##Exception, public SubSystemClassName2##Exception \
    { \
        inline SubSystemName1##SubSystemClassName2##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName1, eSubsystemType::SubSystemEnumName2 ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName1, RWEXCEPT_##ExceptionClassMacroName##_ARGUMENTS_USE ), \
              SubSystemClassName2##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_USE ) \
        { \
        } \
    }
#define RW_EXCEPTION_TEMPLATE_2( ExceptionName, SubSystemClassName, SubSystemEnumName, ExceptionClassMacroName ) \
    struct SubSystemClassName##ExceptionName##Exception : public ExceptionName##Exception, public SubSystemClassName##Exception \
    { \
        inline SubSystemClassName##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_FIRST_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName, RWEXCEPT_##ExceptionClassMacroName##_FIRST_ARGUMENTS_USE ), \
              SubSystemClassName##Exception( RWEXCEPT_##SubSystemEnumName##_ARGUMENTS_USE ) \
        { \
        } \
        inline SubSystemClassName##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_SECOND_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName, RWEXCEPT_##ExceptionClassMacroName##_SECOND_ARGUMENTS_USE ), \
              SubSystemClassName##Exception( RWEXCEPT_##SubSystemEnumName##_ARGUMENTS_USE ) \
        { \
        } \
    }
#define RW_EXCEPTION_BARE_TEMPLATE_WITHCLASS_2( ExceptionName, SubSystemName1, SubSystemEnumName1, SubSystemClassName2, SubSystemEnumName2, ExceptionClassMacroName ) \
    struct SubSystemName1##SubSystemClassName2##ExceptionName##Exception : public ExceptionName##Exception, public SubSystemClassName2##Exception \
    { \
        inline SubSystemName1##SubSystemClassName2##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_FIRST_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName1, eSubsystemType::SubSystemEnumName2 ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName1, RWEXCEPT_##ExceptionClassMacroName##_FIRST_ARGUMENTS_USE ), \
              SubSystemClassName2##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_USE ) \
        { \
        } \
        inline SubSystemName1##SubSystemClassName2##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_SECOND_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName1, eSubsystemType::SubSystemEnumName2 ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName1, RWEXCEPT_##ExceptionClassMacroName##_SECOND_ARGUMENTS_USE ), \
              SubSystemClassName2##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_USE ) \
        { \
        } \
    }

#define RW_EXCEPTION_2BASE_TEMPLATE( ExceptionName, SubSystemClassName1, SubSystemClassName2, SubSystemEnumName1, SubSystemEnumName2, ExceptionClassMacroName ) \
    struct SubSystemClassName1##SubSystemClassName2##ExceptionName##Exception : public ExceptionName##Exception, public SubSystemClassName1##Exception, public SubSystemClassName2##Exception \
    { \
        inline SubSystemClassName1##SubSystemClassName2##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName1##_ARGUMENTS_DEFINE, RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName1, eSubsystemType::SubSystemEnumName2 ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName1, RWEXCEPT_##ExceptionClassMacroName##_ARGUMENTS_USE ), \
              SubSystemClassName1##Exception( RWEXCEPT_##SubSystemEnumName1##_ARGUMENTS_USE ), \
              SubSystemClassName2##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_USE ) \
        { \
        } \
    }

#define RW_EXCEPTION_2BASE_TEMPLATE_2( ExceptionName, SubSystemClassName1, SubSystemClassName2, SubSystemEnumName1, SubSystemEnumName2, ExceptionClassMacroName ) \
    struct SubSystemClassName1##SubSystemClassName2##ExceptionName##Exception : public ExceptionName##Exception, public SubSystemClassName1##Exception, public SubSystemClassName2##Exception \
    { \
        inline SubSystemClassName1##SubSystemClassName2##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName1##_ARGUMENTS_DEFINE, RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_FIRST_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName1, eSubsystemType::SubSystemEnumName2 ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName1, RWEXCEPT_##ExceptionClassMacroName##_FIRST_ARGUMENTS_USE ), \
              SubSystemClassName1##Exception( RWEXCEPT_##SubSystemEnumName1##_ARGUMENTS_USE ), \
              SubSystemClassName2##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_USE ) \
        { \
        } \
        inline SubSystemClassName1##SubSystemClassName2##ExceptionName##Exception( RWEXCEPT_##SubSystemEnumName1##_ARGUMENTS_DEFINE, RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_DEFINE, RWEXCEPT_##ExceptionClassMacroName##_SECOND_ARGUMENTS_DEFINE ) \
            : RwException( eErrorType::ExceptionClassMacroName, eSubsystemType::SubSystemEnumName1, eSubsystemType::SubSystemEnumName2 ), \
              ExceptionName##Exception( eSubsystemType::SubSystemEnumName1, RWEXCEPT_##ExceptionClassMacroName##_SECOND_ARGUMENTS_USE ), \
              SubSystemClassName1##Exception( RWEXCEPT_##SubSystemEnumName1##_ARGUMENTS_USE ), \
              SubSystemClassName2##Exception( RWEXCEPT_##SubSystemEnumName2##_ARGUMENTS_USE ) \
        { \
        } \
    }

} // namespace rw

#endif //_RENDERWARE_ERROR_SYSTEM_COMMON_HEADER_
