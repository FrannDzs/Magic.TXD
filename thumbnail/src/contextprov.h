#pragma once

#include <wrl/client.h>

using namespace Microsoft::WRL;

#include <OCIdl.h>

#include <map>
#include <functional>

#include <sdk/UniChar.h>

struct RenderWareContextHandlerProvider : public IShellExtInit, public IContextMenu, public IObjectWithSite
{
    // IUnknown
    IFACEMETHODIMP QueryInterface( REFIID riid, void **ppOut ) override;
    IFACEMETHODIMP_(ULONG) AddRef( void ) override;
    IFACEMETHODIMP_(ULONG) Release( void ) override;

    // IShellExtInit
    IFACEMETHODIMP Initialize(
        LPCITEMIDLIST pidlFolder, LPDATAOBJECT
        pDataObj, HKEY hKeyProgID
    ) override;

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(
        HMENU hMenu, UINT indexMenu,
        UINT idCmdFirst, UINT idCmdLast, UINT uFlags
    ) override;
    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici) override;
    IFACEMETHODIMP GetCommandString(
        UINT_PTR idCommand, UINT uFlags,
        UINT *pwReserved, LPSTR pszName, UINT cchMax
    ) override;

    IFACEMETHODIMP SetSite( IUnknown *site ) override
    {
        this->theSite = site;

        return S_OK;
    }

    IFACEMETHODIMP GetSite( REFIID riid, void **ppOut ) override
    {
        if ( IUnknown *site = this->theSite.Get() )
        {
            return site->QueryInterface( riid, ppOut );
        }

        return E_FAIL;
    }

    RenderWareContextHandlerProvider( void );

protected:
    ~RenderWareContextHandlerProvider( void );

private:
    bool isInitialized;

    std::atomic <unsigned long> refCount;

    typedef std::map <UINT, std::function <bool ( void )>> menuCmdMap_t;

    menuCmdMap_t cmdMap;

    typedef std::map <UINT, rw::rwStaticString <char>> verbMap_t;

    verbMap_t verbMap;

    inline bool findCommandANSI( const char *cmdName, UINT& idOut ) const
    {
        for ( const std::pair <UINT, rw::rwStaticString <char>>& verbPair : this->verbMap )
        {
            if ( verbPair.second == cmdName )
            {
                idOut = verbPair.first;
                return true;
            }
        }
        return false;
    }

    inline bool findCommandUnicode( const wchar_t *cmdName, UINT& idOut ) const
    {
        for ( const std::pair <UINT, rw::rwStaticString <char>>& verbPair : this->verbMap )
        {
            auto wideVerb = CharacterUtil::ConvertStrings <char, wchar_t> ( verbPair.second );

            if ( wideVerb == cmdName )
            {
                idOut = verbPair.first;
                return true;
            }
        }
        return false;
    }

    enum eContextMenuOptionType
    {
        CONTEXTOPT_TXD,
        CONTEXTOPT_TEXTURE
    };

    struct contextOption_t
    {
        std::wstring fileName;
        eContextMenuOptionType optionType;
    };

    std::list <contextOption_t> contextOptions;

    inline bool hasContextOption( eContextMenuOptionType type ) const
    {
        for ( const contextOption_t& opt : this->contextOptions )
        {
            if ( opt.optionType == type )
            {
                return true;
            }
        }

        return false;
    }

    inline bool hasOnlyContextOption( eContextMenuOptionType type ) const
    {
        bool hasTheOption = false;

        for ( const contextOption_t& opt : this->contextOptions )
        {
            if ( opt.optionType == type )
            {
                hasTheOption = true;
            }
            else
            {
                return false;
            }
        }

        return hasTheOption;
    }

    inline std::wstring getContextDirectory( void ) const;

    template <typename callbackType>
    inline void forAllContextItems( callbackType cb )
    {
        for ( const contextOption_t& ext : this->contextOptions )
        {
            cb( ext.fileName.c_str() );
        }
    }

    ComPtr <IUnknown> theSite;

    HBITMAP rwlogo_bitmap;
};