/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.qol.h
*  PURPOSE:     Quality-of-Life helpers.
*               Simplifications of object management on the stack.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#ifndef _RENDERWARE_QOL_GLOBALS_
#define _RENDERWARE_QOL_GLOBALS_

// Need to define a special exception type.
#include "renderware.errsys.common.h"

// For inheritance checks and nothrow checks.
#include <type_traits>

namespace rw
{

// Special exceptions thrown in QoL subsystem.
struct QualityOfLifeException : public virtual RwException
{
    inline QualityOfLifeException( const char *qolName ) noexcept
        : RwException( eErrorType::UNSPECIFIED, eSubsystemType::QOL ), qolName( qolName )
    {
        return;
    }

    inline QualityOfLifeException( QualityOfLifeException&& ) = default;
    inline QualityOfLifeException( const QualityOfLifeException& ) = default;

    inline QualityOfLifeException& operator = ( QualityOfLifeException&& ) = default;
    inline QualityOfLifeException& operator = ( const QualityOfLifeException& ) = default;

    inline const char* getQualityOfLifeName( void ) const noexcept
    {
        return this->qolName;
    }

private:
    const char *qolName;
};

// The templates.
#define RWEXCEPT_QOL_ARGUMENTS_DEFINE                   const char *qolName
#define RWEXCEPT_QOL_ARGUMENTS_USE                      qolName

RW_EXCEPTION_TEMPLATE( NotInitialized, QualityOfLife, QOL, NOTINIT );
//RW_EXCEPTION_TEMPLATE( InvalidParameter, QualityOfLife, QOL, INVALIDPARAM );
//RW_EXCEPTION_TEMPLATE( InternalError, QualityOfLife, QOL, INTERNERR );
//RW_EXCEPTION_TEMPLATE( NotFound, QualityOfLife, QOL, NOTFOUND );

// Smart pointer to the RenderWare engine itself.
// Does not support reference count so there cannot be multiple smart pointers to the same engine.
struct InterfacePtr
{
    inline InterfacePtr( void ) noexcept
    {
        this->engine = nullptr;
    }
    inline InterfacePtr( Interface *engine ) noexcept
    {
        this->engine = engine;
    }
    inline InterfacePtr( InterfacePtr&& right ) noexcept
    {
        this->engine = right.engine;

        right.engine = nullptr;
    }
    InterfacePtr( const InterfacePtr& ) = delete;   // NOT SUPPORTED.
    inline ~InterfacePtr( void )
    {
        if ( Interface *engine = this->engine )
        {
            DeleteEngine( engine );
        }
    }

    inline InterfacePtr& operator = ( InterfacePtr&& right ) noexcept
    {
        Interface *old_engine = this->engine;
        Interface *new_engine = right.engine;

        right.engine = nullptr;

        if ( old_engine == new_engine )
        {
            return *this;
        }

        if ( old_engine )
        {
            DeleteEngine( old_engine );
        }

        this->engine = new_engine;

        return *this;
    }
    InterfacePtr& operator = ( const InterfacePtr& ) = delete;  // NOT SUPPORTED.

    inline operator Interface* ( void ) const
    {
        Interface *engine = this->engine;

        if ( engine == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "InterfacePtr", nullptr );
        }

        return engine;
    }

    inline Interface* operator -> ( void ) const
    {
        Interface *engine = this->engine;

        if ( engine == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "InterfacePtr", nullptr );
        }

        return engine;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->engine != nullptr );
    }

    inline Interface* detach( void ) noexcept
    {
        Interface *detached = this->engine;

        this->engine = nullptr;

        return detached;
    }

private:
    Interface *engine;
};

// Smart pointer for any RW object.
template <typename rwobjType>
struct ObjectPtr
{
    static_assert( std::is_base_of <RwObject, rwobjType>::value, "invalid object class, needs to inherit from rw::RwObject" );

    inline ObjectPtr( void ) noexcept
    {
        this->object = nullptr;
    }
    inline ObjectPtr( rwobjType *object ) noexcept : object( object )
    {
        return;
    }
    inline ObjectPtr( ObjectPtr&& right ) noexcept
    {
        this->object = right.object;

        right.object = nullptr;
    }
    inline ObjectPtr( const ObjectPtr& right )
    {
        rwobjType *new_obj = right.object;

        if ( new_obj )
        {
            this->object = (rwobjType*)rw::AcquireObject( new_obj );
        }
        else
        {
            this->object = nullptr;
        }
    }
    inline ~ObjectPtr( void )
    {
        if ( rwobjType *object = this->object )
        {
            rw::ReleaseObject( object );
        }
    }

    inline ObjectPtr& operator = ( ObjectPtr&& right ) noexcept
    {
        rwobjType *old_obj = this->object;
        rwobjType *new_obj = right.object;

        right.object = nullptr;

        if ( old_obj == new_obj )
        {
            return *this;
        }

        if ( old_obj )
        {
            rw::ReleaseObject( old_obj );
        }

        this->object = new_obj;

        return *this;
    }
    inline ObjectPtr& operator = ( const ObjectPtr& right )
    {
        rwobjType *old_obj = this->object;
        rwobjType *new_obj = right.object;

        if ( old_obj == new_obj )
        {
            return *this;
        }

        if ( old_obj )
        {
            rw::ReleaseObject( old_obj );
        }

        if ( new_obj )
        {
            this->object = (rwobjType*)rw::AcquireObject( new_obj );
        }
        else
        {
            this->object = nullptr;
        }

        return *this;
    }

    inline operator rwobjType* ( void ) const
    {
        rwobjType *object = this->object;

        if ( object == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "ObjectPtr", nullptr );
        }

        return object;
    }

    inline rwobjType* operator -> ( void ) const
    {
        rwobjType *object = this->object;

        if ( object == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "ObjectPtr", nullptr );
        }

        return object;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->object != nullptr );
    }

    inline rwobjType* detach( void ) noexcept
    {
        rwobjType *detached = this->object;

        this->object = nullptr;

        return detached;
    }

private:
    rwobjType *object;
};

// Raster smart pointer.
struct RasterPtr
{
    inline RasterPtr( void ) noexcept
    {
        this->ptr = nullptr;
    }
    inline RasterPtr( Raster *ptr ) noexcept : ptr( ptr )
    {
        return;
    }
    inline RasterPtr( RasterPtr&& right ) noexcept
    {
        this->ptr = right.ptr;

        right.ptr = nullptr;
    }
    inline RasterPtr( const RasterPtr& right )
    {
        Raster *new_ptr = right.ptr;

        if ( new_ptr )
        {
            this->ptr = AcquireRaster( new_ptr );
        }
        else
        {
            this->ptr = nullptr;
        }
    }
    inline ~RasterPtr( void )
    {
        if ( Raster *ptr = this->ptr )
        {
            DeleteRaster( ptr );
        }
    }

    inline RasterPtr& operator = ( RasterPtr&& right ) noexcept
    {
        Raster *old_ptr = this->ptr;
        Raster *new_ptr = right.ptr;

        right.ptr = nullptr;

        if ( old_ptr == new_ptr )
        {
            return *this;
        }

        if ( old_ptr )
        {
            DeleteRaster( old_ptr );
        }

        this->ptr = new_ptr;

        return *this;
    }
    inline RasterPtr& operator = ( const RasterPtr& right )
    {
        Raster *old_ptr = this->ptr;
        Raster *new_ptr = right.ptr;

        if ( old_ptr == new_ptr )
        {
            return *this;
        }

        if ( old_ptr )
        {
            DeleteRaster( old_ptr );
        }

        if ( new_ptr )
        {
            this->ptr = AcquireRaster( new_ptr );
        }
        else
        {
            this->ptr = nullptr;
        }

        return *this;
    }

    inline operator Raster* ( void ) const
    {
        Raster *ptr = this->ptr;

        if ( ptr == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "RasterPtr", nullptr );
        }

        return ptr;
    }

    inline Raster* operator -> ( void ) const
    {
        Raster *ptr = this->ptr;

        if ( ptr == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "RasterPtr", nullptr );
        }
        
        return ptr;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->ptr != nullptr );
    }

    inline Raster* detach( void ) noexcept
    {
        Raster *detached = this->ptr;

        this->ptr = nullptr;

        return detached;
    }

private:
    Raster *ptr;
};

struct StreamPtr
{
    inline StreamPtr( void ) noexcept
    {
        this->stream = nullptr;
    }
    inline StreamPtr( Stream *stream ) noexcept
    {
        this->stream = stream;
    }

    inline StreamPtr( StreamPtr&& right ) noexcept
    {
        this->stream = right.stream;

        right.stream = nullptr;
    }
    inline StreamPtr( const StreamPtr& ) = delete;

    inline ~StreamPtr( void )
    {
        if ( Stream *stream = this->stream )
        {
            Interface *engine = stream->getEngine();

            engine->DeleteStream( stream );
        }
    }

    inline StreamPtr& operator = ( StreamPtr&& right ) noexcept
    {
        Stream *oldStream = this->stream;
        Stream *newStream = right.stream;

        right.stream = nullptr;

        if ( oldStream == newStream )
            return *this;

        if ( oldStream )
        {
            Interface *engine = oldStream->getEngine();

            engine->DeleteStream( oldStream );
        }

        this->stream = newStream;

        return *this;
    }
    inline StreamPtr& operator = ( const StreamPtr& ) = delete;

    inline operator Stream* ( void ) const
    {
        Stream *stream = this->stream;

        if ( stream == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "StreamPtr", nullptr );
        }

        return stream;
    }

    inline Stream* operator -> ( void ) const
    {
        return *this;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->stream != nullptr );
    }

    inline Stream* detach( void ) noexcept
    {
        Stream *detached = this->stream;

        this->stream = nullptr;

        return detached;
    }

private:
    Stream *stream;
};

struct RwlockInst
{
    inline RwlockInst( void ) noexcept
    {
        this->engine = nullptr;
        this->lock = nullptr;
    }
    inline RwlockInst( Interface *engine )
    {
        this->engine = engine;
        this->lock = CreateReadWriteLock( engine );
    }

    inline RwlockInst( RwlockInst&& right ) noexcept
    {
        this->engine = right.engine;
        this->lock = right.lock;

        right.lock = nullptr;
    }
    inline RwlockInst( const RwlockInst& ) = delete;

    inline ~RwlockInst( void )
    {
        if ( rwlock *lock = this->lock )
        {
            CloseReadWriteLock( this->engine, lock );
        }
    }

    inline RwlockInst& operator = ( RwlockInst&& right ) noexcept
    {
        rwlock *oldLock = this->lock;
        rwlock *newLock = right.lock;

        right.lock = nullptr;

        if ( oldLock == newLock )
            return *this;

        if ( oldLock )
        {
            CloseReadWriteLock( this->engine, oldLock );
        }

        this->engine = right.engine;
        this->lock = newLock;

        return *this;
    }
    inline RwlockInst& operator = ( const RwlockInst& ) = delete;

    inline operator rwlock* ( void ) const
    {
        rwlock *lock = this->lock;

        if ( lock == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "RwlockInst", nullptr );
        }

        return lock;
    }

    inline rwlock* operator -> ( void ) const
    {
        rwlock *lock = this->lock;

        if ( lock == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "RwlockInst", nullptr );
        }

        return lock;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->lock != nullptr );
    }

private:
    Interface *engine;
    rwlock *lock;
};

struct ReentrantRwlockInst
{
    inline ReentrantRwlockInst( void ) noexcept
    {
        this->engine = nullptr;
        this->lock = nullptr;
    }
    inline ReentrantRwlockInst( Interface *engine )
    {
        this->engine = engine;
        this->lock = CreateReentrantReadWriteLock( engine );
    }

    inline ReentrantRwlockInst( ReentrantRwlockInst&& right ) noexcept
    {
        this->engine = right.engine;
        this->lock = right.lock;

        right.lock = nullptr;
    }
    inline ReentrantRwlockInst( const ReentrantRwlockInst& ) = delete;

    inline ~ReentrantRwlockInst( void )
    {
        if ( reentrant_rwlock *lock = this->lock )
        {
            CloseReentrantReadWriteLock( this->engine, lock );
        }
    }

    inline ReentrantRwlockInst& operator = ( ReentrantRwlockInst&& right ) noexcept
    {
        reentrant_rwlock *oldLock = this->lock;
        reentrant_rwlock *newLock = right.lock;

        right.lock = nullptr;

        if ( oldLock == newLock )
            return *this;

        if ( oldLock )
        {
            CloseReentrantReadWriteLock( this->engine, oldLock );
        }

        this->engine = right.engine;
        this->lock = newLock;

        return *this;
    }
    inline ReentrantRwlockInst& operator = ( const ReentrantRwlockInst& ) = delete;

    inline operator reentrant_rwlock* ( void ) const
    {
        reentrant_rwlock *lock = this->lock;

        if ( lock == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "ReentrantRwlockInst", nullptr );
        }

        return lock;
    }

    inline reentrant_rwlock* operator -> ( void ) const
    {
        reentrant_rwlock *lock = this->lock;

        if ( lock == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "ReentrantRwlockInst", nullptr );
        }

        return lock;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->lock != nullptr );
    }

private:
    Interface *engine;
    reentrant_rwlock *lock;
};

struct UnfairMutexInst
{
    inline UnfairMutexInst( void ) noexcept
    {
        this->engine = nullptr;
        this->mutex = nullptr;
    }
    inline UnfairMutexInst( Interface *engine )
    {
        this->engine = engine;
        this->mutex = CreateUnfairMutex( engine );
    }

    inline UnfairMutexInst( UnfairMutexInst&& right ) noexcept
    {
        this->engine = right.engine;
        this->mutex = right.mutex;

        right.mutex = nullptr;
    }
    inline UnfairMutexInst( const UnfairMutexInst& ) = delete;

    inline ~UnfairMutexInst( void )
    {
        if ( unfair_mutex *mtx = this->mutex )
        {
            CloseUnfairMutex( this->engine, mtx );
        }
    }

    inline UnfairMutexInst& operator = ( UnfairMutexInst&& right ) noexcept
    {
        unfair_mutex *oldMtx = this->mutex;
        unfair_mutex *newMtx = right.mutex;

        right.mutex = nullptr;

        if ( oldMtx == newMtx )
            return *this;

        if ( oldMtx )
        {
            CloseUnfairMutex( this->engine, oldMtx );
        }

        this->engine = right.engine;
        this->mutex = newMtx;

        return *this;
    }
    inline UnfairMutexInst& operator = ( const UnfairMutexInst& ) = delete;

    inline operator unfair_mutex* ( void ) const
    {
        unfair_mutex *mtx = this->mutex;

        if ( mtx == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "UnfairMutexInst", nullptr );
        }

        return mtx;
    }

    inline unfair_mutex* operator -> ( void ) const
    {
        unfair_mutex *mtx = this->mutex;

        if ( mtx == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "UnfairMutexInst", nullptr );
        }

        return mtx;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->mutex != nullptr );
    }

private:
    Interface *engine;
    unfair_mutex *mutex;
};

struct SyncEventInst
{
    inline SyncEventInst( void ) noexcept
    {
        this->engine = nullptr;
        this->evt = nullptr;
    }
    inline SyncEventInst( Interface *engine )
    {
        this->engine = engine;
        this->evt = CreateSyncEvent( engine );
    }

    inline SyncEventInst( SyncEventInst&& right ) noexcept
    {
        this->engine = right.engine;
        this->evt = right.evt;

        right.evt = nullptr;
    }
    inline SyncEventInst( const SyncEventInst& ) = delete;

    inline ~SyncEventInst( void )
    {
        if ( sync_event *evt = this->evt )
        {
            CloseSyncEvent( this->engine, evt );
        }
    }

    inline SyncEventInst& operator = ( SyncEventInst&& right ) noexcept
    {
        sync_event *oldEvt = this->evt;
        sync_event *newEvt = right.evt;

        right.evt = nullptr;

        if ( oldEvt == newEvt )
            return *this;

        if ( oldEvt )
        {
            CloseSyncEvent( this->engine, oldEvt );
        }

        this->engine = right.engine;
        this->evt = newEvt;

        return *this;
    }
    inline SyncEventInst& operator = ( const SyncEventInst& ) = delete;

    inline operator sync_event* ( void ) const
    {
        sync_event *evt = this->evt;

        if ( evt == nullptr )
        {
            throw QualityOfLifeNotInitializedException( "SyncEventInst", nullptr );
        }

        return evt;
    }

    inline sync_event* operator -> ( void ) const
    {
        return *this;
    }

    inline bool is_good( void ) const noexcept
    {
        return ( this->evt != nullptr );
    }

private:
    Interface *engine;
    sync_event *evt;
};

// *** HELPERS FOR INTERFACE CONFIGURATION PROPERTIES ON THE STACK ***
struct StackedConfig_WarningManager
{
    inline StackedConfig_WarningManager( Interface *engine, WarningManagerInterface *intf )
    {
        this->old_intf = engine->GetWarningManager();
        this->engine = engine;

        engine->SetWarningManager( intf );
    }
    inline StackedConfig_WarningManager( StackedConfig_WarningManager&& ) = delete;
    inline StackedConfig_WarningManager( const StackedConfig_WarningManager& ) = delete;

    inline ~StackedConfig_WarningManager( void )
    {
        this->engine->SetWarningManager( this->old_intf );
    }

    inline StackedConfig_WarningManager& operator = ( StackedConfig_WarningManager&& ) = delete;
    inline StackedConfig_WarningManager& operator = ( const StackedConfig_WarningManager& ) = delete;

private:
    Interface *engine;
    WarningManagerInterface *old_intf;
};

struct StackedConfig_WarningLevel
{
    inline StackedConfig_WarningLevel( Interface *engine, int level )
    {
        this->old_level = engine->GetWarningLevel();
        this->engine = engine;

        engine->SetWarningLevel( level );
    }
    inline StackedConfig_WarningLevel( StackedConfig_WarningLevel&& ) = delete;
    inline StackedConfig_WarningLevel( const StackedConfig_WarningLevel& ) = delete;

    inline ~StackedConfig_WarningLevel( void )
    {
        this->engine->SetWarningLevel( this->old_level );
    }

    inline StackedConfig_WarningLevel& operator = ( StackedConfig_WarningLevel&& ) = delete;
    inline StackedConfig_WarningLevel& operator = ( const StackedConfig_WarningLevel& ) = delete;

private:
    Interface *engine;
    int old_level;
};

// NOTE: There must be only one of such entry per each thread stack.
struct StackedConfig_ApplyThreadConfig
{
    inline StackedConfig_ApplyThreadConfig( Interface *engine )
    {
        this->engine = engine;

        AssignThreadedRuntimeConfig( engine );
    }

    inline StackedConfig_ApplyThreadConfig( StackedConfig_ApplyThreadConfig&& ) = delete;
    inline StackedConfig_ApplyThreadConfig( const StackedConfig_ApplyThreadConfig& ) = delete;

    inline ~StackedConfig_ApplyThreadConfig( void )
    {
        ReleaseThreadedRuntimeConfig( engine );
    }

    inline StackedConfig_ApplyThreadConfig& operator = ( StackedConfig_ApplyThreadConfig&& ) = delete;
    inline StackedConfig_ApplyThreadConfig& operator = ( const StackedConfig_ApplyThreadConfig& ) = delete;

private:
    Interface *engine;
};

// *** THREADING HELPERS ***
template <typename callbackType>
inline thread_t MakeThreadL( Interface *rwEngine, callbackType&& cb )
{
    static_assert( std::is_nothrow_move_constructible <callbackType>::value == true );

    RwDynMemAllocator memAlloc( rwEngine );

    callbackType *rem_cb = eir::dyn_new_struct <callbackType, rwEirExceptionManager> ( memAlloc, nullptr, std::move( cb ) );

    try
    {
        thread_t resThread = MakeThread(
            rwEngine,
            [] ( thread_t threadHandle, Interface *rwEngine, void *ud )
            {
                callbackType *rem_cb = (callbackType*)ud;

                callbackType cb = std::move( *rem_cb );
                {
                    RwDynMemAllocator memAlloc( rwEngine );

                    eir::dyn_del_struct <callbackType> ( memAlloc, nullptr, rem_cb );
                }

                cb( threadHandle );
            },
            rem_cb
        );

        if ( resThread == nullptr )
        {
            eir::dyn_del_struct <callbackType> ( memAlloc, nullptr, rem_cb );
        }

        return resThread;
    }
    catch( ... )
    {
        eir::dyn_del_struct <callbackType> ( memAlloc, nullptr, rem_cb );

        throw;
    }
}

// Scoped lock objects.
struct scoped_unfair_mutex
{
    inline scoped_unfair_mutex( unfair_mutex *mtx )
    {
        if ( mtx )
        {
            mtx->enter();
        }

        this->mtx = mtx;
    }
    
    inline scoped_unfair_mutex( scoped_unfair_mutex&& right ) noexcept
    {
        this->mtx = right.mtx;

        right.mtx = nullptr;
    }
    inline scoped_unfair_mutex( const scoped_unfair_mutex& ) = delete;

    inline ~scoped_unfair_mutex( void )
    {
        if ( unfair_mutex *mtx = this->mtx )
        {
            mtx->leave();
        }
    }

    inline scoped_unfair_mutex& operator = ( scoped_unfair_mutex&& right ) noexcept
    {
        this->mtx = right.mtx;

        right.mtx = nullptr;

        return *this;
    }
    inline scoped_unfair_mutex& operator = ( const scoped_unfair_mutex& ) = delete;

private:
    unfair_mutex *mtx;
};

} // namespace rw

#endif //_RENDERWARE_QOL_GLOBALS_