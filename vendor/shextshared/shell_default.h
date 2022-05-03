#ifndef _SHELL_DEFAULT_DETAILS_
#define _SHELL_DEFAULT_DETAILS_

#include <atomic>

// Implementation of the factory object.
template <typename factoryObjectConstructorType>
struct shellClassFactory : public IClassFactory
{
    // IUnknown
    IFACEMETHODIMP QueryInterface( REFIID riid, void **ppv )
    {
        if ( ppv == NULL )
            return E_POINTER;

        if ( riid == __uuidof(IClassFactory) )
        {
            this->refCount++;

            IClassFactory *comObj = this;

            *ppv = comObj;
            return S_OK;
        }
        else if ( riid == __uuidof(IUnknown) )
        {
            this->refCount++;

            IUnknown *comObj = this;

            *ppv = comObj;
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef( void )
    {
        return ++refCount;
    }

    IFACEMETHODIMP_(ULONG) Release( void )
    {
        unsigned long newRefCount = --refCount;

        if ( newRefCount == 0 )
        {
            // We can clean up after ourselves.
            delete this;
        }

        return newRefCount;
    }

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv)
    {
        if ( pUnkOuter != NULL )
        {
            return CLASS_E_NOAGGREGATION;
        }

        return factoryObjectConstructorType::construct( pUnkOuter, riid, ppv );
    }

    IFACEMETHODIMP LockServer(BOOL fLock)
    {
        if ( fLock )
        {
            module_refCount++;
        }
        else
        {
            module_refCount--;
        }

        return S_OK;
    }

    shellClassFactory( void )
    {
        module_refCount++;
    }

protected:
    ~shellClassFactory( void )
    {
        module_refCount--;
    }

private:
    std::atomic <unsigned long> refCount;
};

extern HMODULE module_instance;
extern std::atomic <unsigned long> module_refCount;

#endif //_SHELL_DEFAULT_DETAILS_