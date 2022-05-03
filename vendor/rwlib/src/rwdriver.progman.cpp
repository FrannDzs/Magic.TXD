/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.progman.cpp
*  PURPOSE:     Shader program manager
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwdriver.progman.hxx"

namespace rw
{

void driverProgramManager::Initialize( EngineInterface *engineInterface )
{
    LIST_CLEAR( this->programs.root );
    LIST_CLEAR( this->nativeManagers.root );

    // We need a type for GPU programs.
    this->gpuProgTypeInfo = engineInterface->typeSystem.RegisterAbstractType <void*> ( "GPUProgram" );
}

void driverProgramManager::Shutdown( EngineInterface *engineInterface )
{
    // Make sure all programs have deleted themselves.
    assert( LIST_EMPTY( this->programs.root ) == true );
    assert( LIST_EMPTY( this->nativeManagers.root ) == true );

    // Delete the GPU program type.
    if ( RwTypeSystem::typeInfoBase *typeInfo = this->gpuProgTypeInfo )
    {
        engineInterface->typeSystem.DeleteType( typeInfo );

        this->gpuProgTypeInfo = nullptr;
    }
}

optional_struct_space <PluginDependantStructRegister <driverProgramManager, RwInterfaceFactory_t>> driverProgramManagerReg;

// Sub modules.
extern void registerHLSLDriverProgramManager( void );
extern void unregisterHLSLDriverProgramManager( void );

void registerDriverProgramManagerEnv( void )
{
    driverProgramManagerReg.Construct( engineFactory );

    // And now for sub-modules.
    registerHLSLDriverProgramManager();
}

void unregisterDriverProgramManagerEnv( void )
{
    unregisterHLSLDriverProgramManager();

    driverProgramManagerReg.Destroy();
}

struct customNativeProgramTypeInterface final : public RwTypeSystem::typeInterface
{
    AINLINE customNativeProgramTypeInterface( size_t programSize, size_t programAlignment, driverNativeProgramManager *nativeMan )
    {
        this->programSize = programSize;
        this->programAlignment = programAlignment;
        this->nativeMan = nativeMan;
    }

    void Construct( void *mem, EngineInterface *engineInterface, void *construct_params ) const override
    {
        const driverNativeProgramCParams *progParams = (const driverNativeProgramCParams*)construct_params;

        driverProgramHandle *progHandle = new (mem) driverProgramHandle( engineInterface, progParams->progType );

        try
        {
            nativeMan->ConstructProgram( engineInterface, progHandle->GetImplementation(), *progParams );
        }
        catch( ... )
        {
            progHandle->~driverProgramHandle();
            throw;
        }
    }

    void CopyConstruct( void *mem, const void *srcMem ) const override
    {
        const driverProgramHandle *srcObj = (const driverProgramHandle*)srcMem;

        EngineInterface *engineInterface = srcObj->engineInterface;

        driverProgramHandle *copyObj = new (mem) driverProgramHandle( engineInterface, srcObj->programType );

        try
        {
            nativeMan->CopyConstructProgram( copyObj->GetImplementation(), srcObj->GetImplementation() );
        }
        catch( ... )
        {
            copyObj->~driverProgramHandle();
            throw;
        }
    }

    void Destruct( void *mem ) const override
    {
        driverProgramHandle *natProg = (driverProgramHandle*)mem;

        nativeMan->DestructProgram( natProg->GetImplementation() );

        natProg->~driverProgramHandle();
    }

    size_t GetTypeSize( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        return CalculateAlignedTypeSizeWithHeader( sizeof(driverProgramHandle), this->programSize, this->programAlignment );
    }

    size_t GetTypeSizeByObject( EngineInterface *engineInterface, const void *mem ) const override final
    {
        return this->GetTypeSize( engineInterface, nullptr );
    }

    size_t GetTypeAlignment( EngineInterface *engineInterface, void *construct_params ) const override final
    {
        return CalculateAlignmentForObjectWithHeader( alignof(driverProgramHandle), this->programAlignment );
    }

    size_t GetTypeAlignmentByObject( EngineInterface *engineInterface, const void *mem ) const override final
    {
        return this->GetTypeAlignment( engineInterface, nullptr );
    }

    size_t programSize;
    size_t programAlignment;
    driverNativeProgramManager *nativeMan;
};

// Native manager registration API.
bool RegisterNativeProgramManager( EngineInterface *engineInterface, const char *nativeName, driverNativeProgramManager *manager, size_t programSize, size_t programAlignment )
{
    bool success = false;

    if ( driverProgramManager *progMan = driverProgramManagerReg.get().GetPluginStruct( engineInterface ) )
    {
        if ( RwTypeSystem::typeInfoBase *gpuProgTypeInfo = progMan->gpuProgTypeInfo )
        {
            (void)gpuProgTypeInfo;

            // Only register if the native name is not taken already.
            bool isAlreadyTaken = ( progMan->FindNativeManager( nativeName ) != nullptr );

            if ( !isAlreadyTaken )
            {
                if ( manager->nativeManData.isRegistered == false )
                {
                    // We need to create a type for our native program.

                    // Attempt to create the native program type.
                    RwTypeSystem::typeInfoBase *nativeProgType =
                        engineInterface->typeSystem.RegisterCommonTypeInterface <customNativeProgramTypeInterface> (
                            nativeName, progMan->gpuProgTypeInfo, programSize, programAlignment, manager
                        );

                    if ( nativeProgType )
                    {
                        // Time to put us into position :)
                        manager->nativeManData.nativeType = nativeProgType;
                        LIST_INSERT( progMan->nativeManagers.root, manager->nativeManData.node );

                        manager->nativeManData.isRegistered = true;

                        success = true;
                    }
                }
            }
        }
    }

    return success;
}

bool UnregisterNativeProgramManager( EngineInterface *engineInterface, const char *nativeName )
{
    bool success = false;

    if ( driverProgramManager *progMan = driverProgramManagerReg.get().GetPluginStruct( engineInterface ) )
    {
        driverNativeProgramManager *nativeMan = progMan->FindNativeManager( nativeName );

        if ( nativeMan )
        {
            // Delete the type associated with this native program manager.
            engineInterface->typeSystem.DeleteType( nativeMan->nativeManData.nativeType );

            // Well, unregister the thing that the runtime requested us to.
            LIST_REMOVE( nativeMan->nativeManData.node );

            nativeMan->nativeManData.isRegistered = false;

            success = true;
        }
    }

    return success;
}

// Program API :)
DriverProgram* CompileNativeProgram( Interface *intf, const char *nativeName, const char *entryPointName, eDriverProgType progType, const char *shaderSrc, size_t shaderSize )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    driverProgramHandle *handle = nullptr;

    if ( driverProgramManager *progMan = driverProgramManagerReg.get().GetPluginStruct( engineInterface ) )
    {
        // Find the native compiler for this shader code.
        driverNativeProgramManager *nativeMan = progMan->FindNativeManager( nativeName );

        if ( nativeMan )
        {
            // Create our program object and compile it.
            driverProgramHandle *progHandle = nullptr;

            driverNativeProgramCParams cparams;
            cparams.progType = progType;

            GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, nativeMan->nativeManData.nativeType, &cparams );

            if ( rtObj )
            {
                progHandle = (driverProgramHandle*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );
            }

            if ( progHandle )
            {
                // Now the compilation.
                try
                {
                    nativeMan->CompileProgram( progHandle->GetImplementation(), entryPointName, shaderSrc, shaderSize );
                }
                catch( ... )
                {
                    engineInterface->typeSystem.Destroy( engineInterface, rtObj );
                    throw;
                }

                handle = progHandle;
            }
        }
    }

    return handle;
}

void DeleteDriverProgram( DriverProgram *program )
{
    driverProgramHandle *natProg = (driverProgramHandle*)program;

    EngineInterface *engineInterface = natProg->engineInterface;

    // Simply delete the dynamic object.
    GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromObject( natProg );

    if ( rtObj )
    {
        engineInterface->typeSystem.Destroy( engineInterface, rtObj );
    }
}

inline customNativeProgramTypeInterface* GetNativeProgramTypeInterface( const driverProgramHandle *handle )
{
    const GenericRTTI *rtObj = RwTypeSystem::GetTypeStructFromConstObject( handle );

    if ( rtObj )
    {
        RwTypeSystem::typeInfoBase *typeInfo = RwTypeSystem::GetTypeInfoFromTypeStruct( rtObj );

        customNativeProgramTypeInterface *natTypeInfo = dynamic_cast <customNativeProgramTypeInterface*> ( typeInfo->tInterface );

        return natTypeInfo;
    }

    return nullptr;
}

// Get the native program type manager through the object type info.
inline driverNativeProgramManager* GetNativeProgramManager( const driverProgramHandle *handle )
{
    if ( customNativeProgramTypeInterface *natTypeInfo = GetNativeProgramTypeInterface( handle ) )
    {
        return natTypeInfo->nativeMan;
    }

    return nullptr;
}

void* driverProgramHandle::GetImplementation( void )
{
    if ( customNativeProgramTypeInterface *typeInfo = GetNativeProgramTypeInterface( this ) )
    {
        return GetObjectImplementationPointerForObject( this, sizeof(driverProgramHandle), typeInfo->programAlignment );
    }

    return nullptr;
}

const void* driverProgramHandle::GetImplementation( void ) const
{
    if ( customNativeProgramTypeInterface *typeInfo = GetNativeProgramTypeInterface( this ) )
    {
        return GetObjectImplementationPointerForObject( (void*)this, sizeof(driverProgramHandle), typeInfo->programAlignment );
    }

    return nullptr;
}

const void* DriverProgram::GetBytecodeBuffer( void ) const
{
    const driverProgramHandle *natProg = (const driverProgramHandle*)this;

    if ( driverNativeProgramManager *nativeMan = GetNativeProgramManager( natProg ) )
    {
        return nativeMan->ProgramGetBytecodeBuffer( natProg->GetImplementation() );
    }

    return 0;
}

size_t DriverProgram::GetBytecodeSize( void ) const
{
    const driverProgramHandle *natProg = (const driverProgramHandle*)this;

    if ( driverNativeProgramManager *nativeMan = GetNativeProgramManager( natProg ) )
    {
        return nativeMan->ProgramGetBytecodeSize( natProg->GetImplementation() );
    }

    return 0;
}

};
