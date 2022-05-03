/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.qol.h
*  PURPOSE:     Quality-of-life helpers for library simple usage
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NATIVE_EXECUTIVE_QUALITY_OF_LIFE_
#define _NATIVE_EXECUTIVE_QUALITY_OF_LIFE_

#include <type_traits>
#include <tuple>

#include "CExecutiveManager.qol.rwlock.h"

BEGIN_NATIVE_EXECUTIVE

// Main NativeExecutive instance container object.
struct natExecInst
{
    inline natExecInst( void )
    {
        CExecutiveManager *manager = CExecutiveManager::Create();

        if ( !manager )
        {
            throw native_executive_exception();
        }

        this->manager = manager;
    }
    inline natExecInst( natExecInst&& right ) noexcept
    {
        this->manager = right.manager;

        right.manager = nullptr;
    }
    inline natExecInst( const natExecInst&& right ) = delete;
    inline ~natExecInst( void )
    {
        if ( CExecutiveManager *manager = this->manager )
        {
            CExecutiveManager::Delete( manager );
        }
    }

    inline natExecInst& operator = ( natExecInst&& right ) noexcept
    {
        this->~natExecInst();

        return *new (this) natExecInst( std::move( right ) );
    }
    inline natExecInst& operator = ( const natExecInst& right ) = delete;

    inline bool is_good( void ) const
    {
        return ( this->manager != nullptr );
    }

    inline CExecutiveManager& inst( void ) const
    {
        CExecutiveManager *natMan = this->manager;

        if ( natMan == nullptr )
        {
            throw native_executive_exception();
        }

        return *natMan;
    }

    inline CExecutiveManager* operator -> ( void ) const
    {
        return &inst();
    }

    inline operator CExecutiveManager* ( void ) const
    {
        return &inst();
    }

private:
    CExecutiveManager *manager;
};

// Handle for a NativeExecutive thread.
struct natThreadInst
{
    inline natThreadInst( CExecThread *thread )
    {
        this->thread = thread;
    }
    inline natThreadInst( natThreadInst&& right ) noexcept
    {
        this->thread = right.thread;

        right.thread = nullptr;
    }
    // TODO: actually make this work.
    inline natThreadInst( const natThreadInst& right ) = delete;
    inline ~natThreadInst( void )
    {
        if ( CExecThread *thread = this->thread )
        {
            CExecutiveManager *manager = thread->GetManager();

            manager->CloseThread( thread );
        }
    }

    inline natThreadInst& operator = ( natThreadInst&& right ) noexcept
    {
        this->~natThreadInst();

        return *new (this) natThreadInst( std::move( right ) );
    }
    inline natThreadInst& operator = ( const natThreadInst& right ) = delete;

    inline CExecThread& inst( void ) const
    {
        CExecThread *thread = this->thread;

        if ( thread == nullptr )
        {
            throw native_executive_exception();
        }

        return *thread;
    }

    inline CExecThread* operator -> ( void ) const
    {
        return &inst();
    }

    inline operator CExecThread* ( void ) const
    {
        return &inst();
    }

private:
    CExecThread *thread;
};

struct natEventInst
{
    inline natEventInst( CExecutiveManager *manager, bool shouldWait = false ) : manager( manager )
    {
        this->evtHandle = manager->CreateEvent( shouldWait );
    }
    inline natEventInst( const natEventInst& ) = delete;
    inline natEventInst( natEventInst&& right ) noexcept
    {
        this->manager = right.manager;
        this->evtHandle = right.evtHandle;

        right.manager = nullptr;
        right.evtHandle = nullptr;
    }

private:
    AINLINE void _clear_entity( void )
    {
        if ( CEvent *evtHandle = this->evtHandle )
        {
            this->manager->CloseEvent( evtHandle );
        }
    }

public:
    inline ~natEventInst( void )
    {
        this->_clear_entity();
    }

    inline natEventInst& operator = ( const natEventInst& ) = delete;
    inline natEventInst& operator = ( natEventInst&& right ) noexcept
    {
        _clear_entity();

        this->manager = right.manager;
        this->evtHandle = right.evtHandle;

        right.manager = nullptr;
        right.evtHandle = nullptr;

        return *this;
    }

    inline CEvent& inst( void ) const
    {
        CEvent *evtHandle = this->evtHandle;

        if ( evtHandle == nullptr )
        {
            throw native_executive_exception();
        }

        return *evtHandle;
    }

    inline CEvent* operator -> ( void ) const
    {
        return &inst();
    }

    inline operator CEvent* ( void ) const
    {
        return &inst();
    }

private:
    CExecutiveManager *manager;
    CEvent *evtHandle;
};

struct natCondInst
{
    inline natCondInst( CCondVar *var ) : condVar( var )
    {
        return;
    }
    inline natCondInst( CExecutiveManager *manager )
    {
        this->condVar = manager->CreateConditionVariable();   
    }
    inline natCondInst( const natCondInst& ) = delete;
    inline natCondInst( natCondInst&& right ) noexcept
    {
        this->condVar = right.condVar;

        right.condVar = nullptr;
    }

private:
    AINLINE void _clear_entity( void )
    {
        if ( CCondVar *var = this->condVar )
        {
            CExecutiveManager *manager = var->GetManager();

            manager->CloseConditionVariable( var );
        }
    }

public:
    inline ~natCondInst( void )
    {
        _clear_entity();
    }

    inline natCondInst& operator = ( const natCondInst& ) = delete;
    inline natCondInst& operator = ( natCondInst&& right ) noexcept
    {
        _clear_entity();
    
        this->condVar = right.condVar;

        right.condVar = nullptr;
        return *this;
    }

    inline CCondVar& inst( void ) const
    {
        CCondVar *var = this->condVar;

        if ( var == nullptr )
        {
            throw native_executive_exception();
        }

        return *var;
    }

    inline CCondVar* operator -> ( void ) const
    {
        return &inst();
    }

    inline operator CCondVar* ( void ) const
    {
        return &inst();
    }

private:
    CCondVar *condVar;
};

// Well-forced Read/Write lock container.
struct natRWLock
{
    inline natRWLock( CExecutiveManager *manager )
    {
        this->manager = manager;
        this->rwlock = manager->CreateReadWriteLock();

        if ( this->rwlock == nullptr )
        {
            throw native_executive_exception();
        }
    }
    inline natRWLock( CExecutiveManager *manager, CReadWriteLock *lock ) noexcept
    {
        this->manager = manager;
        this->rwlock = lock;
    }
    inline natRWLock( const natRWLock& ) = delete;
    inline natRWLock( natRWLock&& right ) noexcept
    {
        this->rwlock = right.rwlock;
        this->manager = right.manager;

        right.rwlock = nullptr;
    }

private:
    AINLINE void _clear_entity( void )
    {
        if ( CReadWriteLock *rwlock = this->rwlock )
        {
            this->manager->CloseReadWriteLock( rwlock );
        }
    }

public:
    inline ~natRWLock( void )
    {
        _clear_entity();
    }

    inline natRWLock& operator = ( const natRWLock& ) = delete;
    inline natRWLock& operator = ( natRWLock&& right ) noexcept
    {
        _clear_entity();

        this->rwlock = right.rwlock;
        this->manager = right.manager;

        right.rwlock = nullptr;
        return *this;
    }

    inline CReadWriteLock& inst( void ) const
    {
        CReadWriteLock *rwlock = this->rwlock;

        if ( rwlock == nullptr )
        {
            throw native_executive_exception();
        }

        return *rwlock;
    }

    inline CReadWriteLock* operator -> ( void ) const
    {
        return &inst();
    }

    inline operator CReadWriteLock* ( void ) const
    {
        return &inst();
    }

private:
    CExecutiveManager *manager;
    CReadWriteLock *rwlock;
};

// Optional Read/Write lock container.
struct natOptRWLock
{
    inline natOptRWLock( CExecutiveManager *manager )
    {
        this->manager = manager;

        if ( manager )
        {
            this->rwlock = manager->CreateReadWriteLock();
        }
        else
        {
            this->rwlock = nullptr;
        }
    }
    inline natOptRWLock( CExecutiveManager *manager, CReadWriteLock *lock ) noexcept
    {
        this->manager = manager;
        this->rwlock = lock;
    }
    inline natOptRWLock( const natOptRWLock& ) = delete;
    inline natOptRWLock( natOptRWLock&& right ) noexcept
    {
        this->rwlock = right.rwlock;
        this->manager = right.manager;

        right.rwlock = nullptr;
    }

private:
    AINLINE void _clear_entity( void )
    {
        if ( CReadWriteLock *rwlock = this->rwlock )
        {
            this->manager->CloseReadWriteLock( rwlock );
        }
    }

public:
    inline ~natOptRWLock( void )
    {
        _clear_entity();
    }

    inline natOptRWLock& operator = ( const natOptRWLock& ) = delete;
    inline natOptRWLock& operator = ( natOptRWLock&& right ) noexcept
    {
        _clear_entity();

        this->rwlock = right.rwlock;
        this->manager = right.manager;

        right.rwlock = nullptr;
        return *this;
    }

    inline bool is_good( void ) const
    {
        return ( this->rwlock != nullptr );
    }

    inline CReadWriteLock* operator -> ( void ) const
    {
        if ( this->rwlock == nullptr )
        {
            throw native_executive_exception();
        }

        return this->rwlock;
    }

    inline operator CReadWriteLock* ( void ) const
    {
        return this->rwlock;
    }

    inline CReadWriteLock* get( void ) const
    {
        return this->rwlock;
    }

private:
    CExecutiveManager *manager;
    CReadWriteLock *rwlock;
};

// Helper for the unfair mutex.
struct CUnfairMutexContext
{
    inline CUnfairMutexContext( CUnfairMutex *mtx ) noexcept
    {
        if ( mtx )
        {
            mtx->lock();
        }

        this->mtx = mtx;
    }
    inline CUnfairMutexContext( CUnfairMutex& mtx ) noexcept
    {
        mtx.lock();

        this->mtx = &mtx;
    }
    inline CUnfairMutexContext( const CUnfairMutexContext& ) = delete;
    inline CUnfairMutexContext( CUnfairMutexContext&& right ) noexcept
    {
        this->mtx = right.mtx;

        right.mtx = nullptr;
    }

    inline ~CUnfairMutexContext( void )
    {
        if ( CUnfairMutex *mtx = this->mtx )
        {
            mtx->unlock();
        }
    }

    inline CUnfairMutexContext& operator = ( const CUnfairMutexContext& ) = delete;
    inline CUnfairMutexContext& operator = ( CUnfairMutexContext&& right ) noexcept
    {
        this->release();

        this->mtx = right.mtx;

        right.mtx = nullptr;
        return *this;
    }

    inline CUnfairMutex* release( void ) noexcept
    {
        if ( CUnfairMutex *mtx = this->mtx )
        {
            mtx->unlock();

            this->mtx = nullptr;

            return mtx;
        }

        return nullptr;
    }

private:
    CUnfairMutex *mtx;
};

// Helper for unfair mutex.
struct CSpinLockContext
{
    inline CSpinLockContext( CSpinLock *lock )
    {
        if ( lock )
        {
            lock->lock();
        }

        this->lock = lock;
    }

    inline CSpinLockContext( CSpinLock& lock )
    {
        lock.lock();

        this->lock = &lock;
    }

    inline CSpinLockContext( const CSpinLockContext& lock ) = delete;
    inline CSpinLockContext( CSpinLockContext&& right ) noexcept
    {
        this->lock = right.lock;

        right.lock = nullptr;
    }

private:
    inline void _release_lock( void )
    {
        CSpinLock *lock = this->lock;

        if ( lock )
        {
            lock->unlock();
        }
    }

public:
    inline ~CSpinLockContext( void )
    {
        _release_lock();
    }

    inline CSpinLockContext& operator = ( const CSpinLockContext& ) = delete;
    inline CSpinLockContext& operator = ( CSpinLockContext&& right ) noexcept
    {
        this->_release_lock();

        CSpinLock *lock = right.lock;

        if ( lock )
        {
            lock->lock();
        }

        this->lock = lock;

        return *this;
    }

    inline void Suspend( void )
    {
        this->_release_lock();

        this->lock = nullptr;
    }

    inline CSpinLock* GetCurrentLock( void )
    {
        return this->lock;
    }

private:
    CSpinLock *lock;
};

// Takes multiple locks in order and keeps them aquired while under a master lock to ensure atomicity of lock taking.
template <typename masterLockCtxType, typename... lockContextTypes>
struct AtomicSequentialLockAcquisition
{
private:
    template <size_t iter, size_t max, typename selectedConstructorLambda, typename... constructorLambdas>
    AINLINE void ConstructContexts( selectedConstructorLambda&& cb, constructorLambdas&&... cbs )
    {
        std::get <iter> ( contexts ).Construct( cb() );

        if constexpr ( iter + 1 < max )
        {
            try
            {
                ConstructContexts <iter+1, max> ( std::move( cbs )... );
            }
            catch( ... )
            {
                std::get <iter> ( contexts ).Destroy();

                throw;
            }
        }
    }

    template <size_t iter>
    AINLINE void DestroyContexts( void ) noexcept
    {
        std::get <iter> ( contexts ).Destroy();

        if constexpr ( iter > 0 )
        {
            DestroyContexts <iter - 1> ();
        }
    }

public:
    template <typename masterLockPtr, typename... constructorLambdas>
    AINLINE AtomicSequentialLockAcquisition( masterLockPtr *masterLock, constructorLambdas&&... cbs )
    {
        masterLockCtxType masterCtx( masterLock );

        ConstructContexts <0, sizeof...( lockContextTypes )> ( cbs... );
    }
    AINLINE AtomicSequentialLockAcquisition( AtomicSequentialLockAcquisition&& ) = delete;
    AINLINE AtomicSequentialLockAcquisition( const AtomicSequentialLockAcquisition& ) = delete;

    AINLINE ~AtomicSequentialLockAcquisition( void )
    {
        DestroyContexts <sizeof...( lockContextTypes ) - 1> ();
    }

    AINLINE AtomicSequentialLockAcquisition& operator = ( AtomicSequentialLockAcquisition&& ) = delete;
    AINLINE AtomicSequentialLockAcquisition& operator = ( const AtomicSequentialLockAcquisition& ) = delete;

private:
    std::tuple <optional_struct_space <lockContextTypes>...> contexts;
};

// Helper for lambdas in thread creation.
template <typename callbackType>
AINLINE CExecThread* CreateThreadL( CExecutiveManager *nativeMan, callbackType&& cb, size_t stackSize = 0, const char *hint_thread_name = nullptr )
{
    static_assert( std::is_nothrow_move_constructible <callbackType>::value == true );

    NatExecStandardObjectAllocator memAlloc( nativeMan );

    callbackType *storedLambda = eir::dyn_new_struct <callbackType> ( memAlloc, nullptr, std::move( cb ) );

    CExecThread *theThread = nativeMan->CreateThread(
        []( CExecThread *thread, void *ud )
        {
            callbackType *cb_dyn = (callbackType*)ud;

            callbackType cb_stored = std::move( *cb_dyn );

            // Free the dynamic memory.
            {
                NatExecStandardObjectAllocator memAlloc( thread->GetManager() );

                eir::dyn_del_struct <callbackType> ( memAlloc, nullptr, cb_dyn );
            }

            cb_stored( thread );
        },
        storedLambda, stackSize, hint_thread_name
    );

    if ( theThread == nullptr )
    {
        eir::dyn_del_struct <callbackType> ( memAlloc, nullptr, storedLambda );
    }

    return theThread;
}

// Helper for fiber creation using lambda.
template <typename callbackType>
AINLINE CFiber* CreateFiberL( CExecutiveManager *execMan, callbackType&& cb, size_t stackSize = 0 )
{
    static_assert( std::is_nothrow_move_constructible <callbackType>::value == true );

    struct helper
    {
        static void lambda_runtime( CFiber *fiber, void *ud )
        {
            callbackType cb( std::move( *(callbackType*)ud ) );

            fiber->yield();

            // Now execute our callback.
            cb( fiber );
        }
    };

    CFiber *fib = execMan->CreateFiber( helper::lambda_runtime, &cb, stackSize );

    if ( fib == nullptr )
    {
        return nullptr;
    }

    // Initialize it so that it can put its own stuff onto the stack.
    fib->resume();

    // It now has initialized by putting the callback on its own stack.
    // Thus we can give back control to the runtime.
    return fib;
}

END_NATIVE_EXECUTIVE

#endif //_NATIVE_EXECUTIVE_QUALITY_OF_LIFE_