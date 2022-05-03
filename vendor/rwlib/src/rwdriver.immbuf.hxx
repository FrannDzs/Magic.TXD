/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.immbuf.hxx
*  PURPOSE:     Immediate mode buffer implementation
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_DRIVER_IMM_BUFFERS_
#define _RENDERWARE_DRIVER_IMM_BUFFERS_

namespace rw
{

// This buffer is used as a target for memory writes.
// You don't know how much data this buffer has to support, so you want an anonymous buffer that can keep infinite
// and is fast at growing. You also do not care about the implications of high memory usage; just gimme that buffer.
// This is basically what immediate-mode rendering is about!
struct DriverImmediatePushbuffer
{
    void PushMem( const void *srcObj, size_t objSize );

    template <typename objType>
    AINLINE void PushStruct( const objType& mem )
    {
        this->PushMem( &mem, sizeof( objType ) );
    }

    size_t GetMemSize( void ) const;

    void Clear( void );
};

// Resource request API.
DriverImmediatePushbuffer* AllocatePushbuffer( EngineInterface *engineInterface );
void FreePushbuffer( DriverImmediatePushbuffer *buf );

};

#endif //_RENDERWARE_DRIVER_IMM_BUFFERS_