/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.immbuf.cpp
*  PURPOSE:     Immediate mode buffer.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "rwcommon.hxx"

#include "rwdriver.hxx"

#include "pluginutil.hxx"

namespace rw
{

// Fast memory buffers for pushing all kinds of data to the GPU.
// Structs here are meant to solve starvation/performance problems, so please use these instead of your own stuff.
// We know what we are doing.
struct DriverImmediatePushbufferImpl : public DriverImmediatePushbuffer
{
    inline DriverImmediatePushbufferImpl( EngineInterface *engineInterface )
    {
        this->engineInterface = engineInterface;

        this->mem = nullptr;
        this->usedMemSize = 0;
        this->reservedMemSize = 0;
    }

    inline ~DriverImmediatePushbufferImpl( void )
    {
        EngineInterface *engineInterface = this->engineInterface;

        if ( void *mem = this->mem )
        {
            engineInterface->MemFree( mem );
        }
    }

    EngineInterface *engineInterface;

    void *mem;

    size_t usedMemSize;
    size_t reservedMemSize;
};

// Pushbuffer API implementation.
void DriverImmediatePushbuffer::PushMem( const void *mem, size_t memSize )
{
    DriverImmediatePushbufferImpl *bufImpl = (DriverImmediatePushbufferImpl*)this;

    EngineInterface *engineInterface = bufImpl->engineInterface;

    // Add this data to our (extensible) buffer.
    // We want to grow logarithmically if we are not big enough.
    size_t currentMemSize = bufImpl->usedMemSize;
    size_t reservedMemSize = bufImpl->reservedMemSize;

    void *curmem = bufImpl->mem;

    size_t newMemSize = ( currentMemSize + memSize );

    if ( reservedMemSize < newMemSize )
    {
        // We have to grow.
        if ( reservedMemSize == 0 )
        {
            reservedMemSize = 1;
        }

        while ( reservedMemSize < newMemSize )
        {
            reservedMemSize *= 2;
        }

        void *newmem = engineInterface->MemAllocate( reservedMemSize );

        // Reallocate the buffer.
        if ( curmem )
        {
            engineInterface->MemFree( curmem );
        }

        curmem = newmem;

        bufImpl->reservedMemSize = reservedMemSize;
        bufImpl->mem = curmem;
    }

    // Put down our item.
    memcpy( (char*)curmem + currentMemSize, mem, memSize );

    currentMemSize += memSize;

    bufImpl->usedMemSize = currentMemSize;
}

size_t DriverImmediatePushbuffer::GetMemSize( void ) const
{
    DriverImmediatePushbufferImpl *bufImpl = (DriverImmediatePushbufferImpl*)this;

    return bufImpl->usedMemSize;
}

void DriverImmediatePushbuffer::Clear( void )
{
    DriverImmediatePushbufferImpl *bufImpl = (DriverImmediatePushbufferImpl*)this;

    bufImpl->usedMemSize = 0;
}

struct driverResourceEnv
{
    inline void Initialize( EngineInterface *engineInterface )
    {
        // Initialize resource allocators.
        this->pushbuffers.SummonEntries( engineInterface, 32, engineInterface );
    }

    inline void Shutdown( EngineInterface *engineInterface )
    {
        this->pushbuffers.Shutdown( engineInterface );
    }

    inline driverResourceEnv& operator = ( const driverResourceEnv& right ) = delete;

    CachedConstructedClassAllocator <DriverImmediatePushbufferImpl> pushbuffers;
};

static optional_struct_space <PluginDependantStructRegister <driverResourceEnv, RwInterfaceFactory_t>> driverResEnvRegister;

// Driver resource management API.
DriverImmediatePushbuffer* AllocatePushbuffer( EngineInterface *engineInterface )
{
    if ( driverResourceEnv *env = driverResEnvRegister.get().GetPluginStruct( engineInterface ) )
    {
        return env->pushbuffers.Allocate( engineInterface, engineInterface );
    }

    return nullptr;
}

void FreePushbuffer( DriverImmediatePushbuffer *buf )
{
    DriverImmediatePushbufferImpl *bufImpl = (DriverImmediatePushbufferImpl*)buf;

    EngineInterface *engineInterface = bufImpl->engineInterface;

    if ( driverResourceEnv *env = driverResEnvRegister.get().GetPluginStruct( engineInterface ) )
    {
        env->pushbuffers.Free( engineInterface, bufImpl );
    }
}

void registerDriverResourceEnvironment( void )
{
    driverResEnvRegister.Construct( engineFactory );
}

void unregisterDriverResourceEnvironment( void )
{
    driverResEnvRegister.Destroy();
}

};