/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.progman.hxx
*  PURPOSE:
*       RenderWare driver program manager.
*       Programs are code that run on the GPU to perform a certain action of the pipeline.
*       There are multiple program types supported: vertex, fragment, hull
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_GPU_PROGRAM_MANAGER_
#define _RENDERWARE_GPU_PROGRAM_MANAGER_

#include "pluginutil.hxx"

namespace rw
{

struct driverProgramHandle : public DriverProgram
{
    inline driverProgramHandle( EngineInterface *engineInterface, eDriverProgType progType )
    {
        this->engineInterface = engineInterface;
        this->programType = progType;
        
        // Remember to add ourselves to the list.
    }

    inline void* GetImplementation( void );

    inline const void* GetImplementation( void ) const;

    EngineInterface *engineInterface;

    eDriverProgType programType;

    RwListEntry <driverProgramHandle> node;
};

struct driverNativeProgramCParams
{
    eDriverProgType progType;
};

struct driverNativeProgramManager abstract
{
    inline driverNativeProgramManager( void )
    {
        this->nativeManData.isRegistered = false;
        this->nativeManData.nativeType = nullptr;
    }

    inline ~driverNativeProgramManager( void )
    {
        assert( this->nativeManData.isRegistered == false );
    }

    virtual void ConstructProgram( EngineInterface *engineInterface, void *progMem, const driverNativeProgramCParams& params ) = 0;
    virtual void CopyConstructProgram( void *progMem, const void *srcProgMem ) = 0;
    virtual void DestructProgram( void *progMem ) = 0;

    virtual void CompileProgram( void *progMem, const char *entryPointName, const char *sourceCode, size_t bufSize ) const = 0;

    virtual const void* ProgramGetBytecodeBuffer( const void *progMem ) = 0;
    virtual size_t ProgramGetBytecodeSize( const void *progMem ) = 0;

    // DO NOT ACCESS THOSE FIELD YOURSELF.
    // These fields are subject to internal change.
    struct
    {
        bool isRegistered;

        RwTypeSystem::typeInfoBase *nativeType;
        RwListEntry <driverNativeProgramManager> node;
    } nativeManData;
};

struct driverProgramManager
{
    void Initialize( EngineInterface *engineInterface );
    void Shutdown( EngineInterface *engineInterface );

    inline driverProgramManager& operator = ( const driverProgramManager& right ) = delete;

    inline driverNativeProgramManager* FindNativeManager( const char *nativeName ) const
    {
        LIST_FOREACH_BEGIN( driverNativeProgramManager, this->nativeManagers.root, nativeManData.node )

            if ( strcmp( item->nativeManData.nativeType->name, nativeName ) == 0 )
            {
                return item;
            }

        LIST_FOREACH_END

        return nullptr;
    }

    // We want to keep track of our programs.
    RwList <driverProgramHandle> programs;

    // Native managers are registered in the driverProgramManager.
    RwList <driverNativeProgramManager> nativeManagers;

    // GPUProgram type.
    RwTypeSystem::typeInfoBase *gpuProgTypeInfo;
};

extern optional_struct_space <PluginDependantStructRegister <driverProgramManager, RwInterfaceFactory_t>> driverProgramManagerReg;

// Native program manager registration.
bool RegisterNativeProgramManager( EngineInterface *engineInterface, const char *nativeName, driverNativeProgramManager *manager, size_t programSize, size_t programAlignment );
bool UnregisterNativeProgramManager( EngineInterface *engineInterface, const char *nativeName );

};

#endif //_RENDERWARE_GPU_PROGRAM_MANAGER_