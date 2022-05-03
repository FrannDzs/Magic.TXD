/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.progman.hlsl.cpp
*  PURPOSE:     HLSL shader code management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwdriver.progman.hxx"

#ifndef _COMPILE_FOR_LEGACY

#ifdef _WIN32

#include <sdk/UniChar.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <wrl/client.h>
#include <d3dcompiler.h>

#define D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES (1 << 20)

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY

namespace rw
{

#ifndef _COMPILE_FOR_LEGACY

#ifdef _WIN32

struct hlslProgram
{
    inline hlslProgram( EngineInterface *engineInterface, eDriverProgType progType )
    {
        this->engineInterface = engineInterface;
        this->progType = progType;
    }

    inline hlslProgram( const hlslProgram& right )
    {
        this->engineInterface = right.engineInterface;
        this->progType = right.progType;
        this->progData = right.progData;
    }

    inline ~hlslProgram( void )
    {
        // We automatically release data.
    }

    EngineInterface *engineInterface;
    eDriverProgType progType;

    // Immutable shader data.
    Microsoft::WRL::ComPtr <ID3DBlob> progData;
};

struct hlslDriverProgramManager : public driverNativeProgramManager
{
    void ConstructProgram( EngineInterface *engineInterface, void *progMem, const driverNativeProgramCParams& params ) override
    {
        new (progMem) hlslProgram( engineInterface, params.progType );
    }

    void CopyConstructProgram( void *progMem, const void *srcProgMem ) override
    {
        new (progMem) hlslProgram( *(const hlslProgram*)srcProgMem );
    }

    void DestructProgram( void *progMem ) override
    {
        ((hlslProgram*)progMem)->~hlslProgram();
    }

    void CompileProgram( void *progMem, const char *entryPointName, const char *sourceCode, size_t dataSize ) const override
    {
        hlslProgram *program = (hlslProgram*)progMem;

        Microsoft::WRL::ComPtr <ID3DBlob> shaderBlob;

        // Translate the virtual program type to Direct3D semantics.
        eDriverProgType natProgType = program->progType;

        const char *targetType = nullptr;

        if ( natProgType == eDriverProgType::DRIVER_PROG_FRAGMENT )
        {
            targetType = "ps_5_0";
        }
        else if ( natProgType == eDriverProgType::DRIVER_PROG_VERTEX )
        {
            targetType = "vs_5_0";
        }
        else if ( natProgType == eDriverProgType::DRIVER_PROG_HULL )
        {
            targetType = "hs_5_0";
        }
        else
        {
            throw DriverProgramInvalidParameterException( "HLSL", L"DRIVER_PROGTYPE_FRIENDLYNAME", nullptr );
        }

        UINT compFlags = D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;

#ifdef _DEBUG
        // Insert debug information into the code.
        compFlags |= D3DCOMPILE_DEBUG;

        // Skip optimizations.
        compFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        // Compile it.
        Microsoft::WRL::ComPtr <ID3DBlob> errorMsgData;

        HRESULT compResult =
            this->procCompile2(
                sourceCode, dataSize, nullptr, nullptr, nullptr,
                entryPointName, targetType,
                compFlags, 0, 0,
                nullptr, 0,
                &shaderBlob, &errorMsgData
            );

        bool couldCompile = ( compResult == S_OK );

        if ( !couldCompile )
        {
            // Output some strange error message.
            rwStaticString <char> errorMsg = "unknown";

            if ( errorMsgData )
            {
                const char *rawErrorMsg = (const char*)errorMsgData->GetBufferPointer();

                errorMsg = rawErrorMsg;
            }

            throw DriverProgramUnlocalizedAInternalErrorException( "HLSL", L"DRIVER_INTERNERR_COMPILATIONFAIL", std::move( errorMsg ) );
        }

        // Embed into our program.
        // This automatically frees the previous data.
        program->progData = shaderBlob;
    }

    const void* ProgramGetBytecodeBuffer( const void *progMem ) override
    {
        const hlslProgram *prog = (const hlslProgram*)progMem;

        return prog->progData->GetBufferPointer();
    }

    size_t ProgramGetBytecodeSize( const void *progMem ) override
    {
        const hlslProgram *prog = (const hlslProgram*)progMem;

        return prog->progData->GetBufferSize();
    }

    inline void Initialize( EngineInterface *engineInterface )
    {
        bool hasRegistered = false;

        // Attempt to initialize the shader compiler.
        this->compilerModule = LoadLibraryA( "D3DCompiler_47.dll" );
        this->procCompileFromFile = nullptr;
        this->procCompile2 = nullptr;

        if ( HMODULE compModule = this->compilerModule )
        {
            this->procCompileFromFile = (decltype(&D3DCompileFromFile))GetProcAddress( compModule, "D3DCompileFromFile" );
            this->procCompile2 = (decltype(&D3DCompile2))GetProcAddress( compModule, "D3DCompile2" );
        }

        if ( this->procCompileFromFile != nullptr &&
             this->procCompile2 != nullptr )
        {
            hasRegistered = RegisterNativeProgramManager( engineInterface, "HLSL", this, sizeof( hlslProgram ), alignof( hlslProgram ) );
        }

        this->hasRegistered = hasRegistered;
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        if ( HMODULE compModule = this->compilerModule )
        {
            FreeLibrary( compModule );

            this->compilerModule = nullptr;
        }

        if ( this->hasRegistered )
        {
            UnregisterNativeProgramManager( engineInterface, "HLSL" );
        }
    }

    bool hasRegistered;

    // Need to store information about the shader compiler we will use.
    HMODULE compilerModule;

    decltype( &D3DCompileFromFile ) procCompileFromFile;
    decltype( &D3DCompile2 ) procCompile2;
};

static optional_struct_space <PluginDependantStructRegister <hlslDriverProgramManager, RwInterfaceFactory_t>> hlslDriverProgramManagerReg;

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY

void registerHLSLDriverProgramManager( void )
{
#ifndef _COMPILE_FOR_LEGACY
#ifdef _WIN32
    hlslDriverProgramManagerReg.Construct( engineFactory );
#endif //_WIN32
#endif //_COMPILE_FOR_LEGACY
}

void unregisterHLSLDriverProgramManager( void )
{
#ifndef _COMPILE_FOR_LEGACY
#ifdef _WIN32
    hlslDriverProgramManagerReg.Destroy();
#endif //_WIN32
#endif //_COMPILE_FOR_LEGACY
}

};
