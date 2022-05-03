#include "StdInc.h"

#include <wrl/client.h>

#include <CFileSystemInterface.h>

#include "rwutils.hxx"

#include <sdk/NumericFormat.h>

using namespace Microsoft::WRL;

RenderWareContextHandlerProvider::RenderWareContextHandlerProvider( void ) : refCount( 1 )
{
    this->isInitialized = false;
    
    module_refCount++;

    int cx_icon = GetSystemMetrics( SM_CXMENUCHECK );
    int cy_icon = GetSystemMetrics( SM_CYMENUCHECK );

    this->rwlogo_bitmap =
        (HBITMAP)LoadImageW( module_instance, MAKEINTRESOURCEW(IDB_RWLOGO), IMAGE_BITMAP, cx_icon, cy_icon, LR_DEFAULTSIZE | LR_LOADTRANSPARENT );
}

RenderWareContextHandlerProvider::~RenderWareContextHandlerProvider( void )
{
    if ( HBITMAP rwbmp = this->rwlogo_bitmap )
    {
        DeleteObject( rwbmp );

        this->rwlogo_bitmap = NULL;
    }

    module_refCount--;
}

IFACEMETHODIMP RenderWareContextHandlerProvider::QueryInterface( REFIID riid, void **ppOut )
{
    if ( !ppOut )
        return E_POINTER;

    if ( riid == __uuidof(IUnknown) )
    {
        this->refCount++;

        IUnknown *comObj = (IShellExtInit*)this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IShellExtInit) )
    {
        this->refCount++;

        IShellExtInit *comObj = this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IContextMenu) )
    {
        this->refCount++;

        IContextMenu *comObj = this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IObjectWithSite) )
    {
        this->refCount++;

        IObjectWithSite *comObj = this;

        *ppOut = comObj;
        return S_OK;
    }
    else if ( riid == __uuidof(IInternetSecurityManager) )
    {
        return E_NOINTERFACE;
    }

    return E_NOINTERFACE;
}

IFACEMETHODIMP_(ULONG) RenderWareContextHandlerProvider::AddRef( void )
{
    return ++this->refCount;
}

IFACEMETHODIMP_(ULONG) RenderWareContextHandlerProvider::Release( void )
{
    unsigned long newRef = --this->refCount;

    if ( newRef == 0 )
    {
        delete this;
    }

    return newRef;
}

IFACEMETHODIMP RenderWareContextHandlerProvider::Initialize( LPCITEMIDLIST idList, LPDATAOBJECT dataObj, HKEY keyProgId )
{
    if ( this->isInitialized )
        return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

    if ( dataObj == NULL )
        return E_INVALIDARG;

    HRESULT res = E_FAIL;

    try
    {
        if ( auto DragQueryFileW = shell32min.DragQueryFileW )
        {
            if ( auto ReleaseStgMedium = ole32min.ReleaseStgMedium )
            {
                try
                {
                    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
                    STGMEDIUM stm;

                    HRESULT getHandleSuccess = dataObj->GetData( &fe, &stm );

                    if ( SUCCEEDED(getHandleSuccess) )
                    {
                        try
                        {
                            HDROP drop = (HDROP)GlobalLock( stm.hGlobal );

                            if ( drop )
                            {
                                try
                                {
                                    UINT numFiles = DragQueryFileW( drop, 0xFFFFFFFF, NULL, 0 );

                                    // For now we want to only support operation on one file.
                                    for ( UINT n = 0; n < numFiles; n++ )
                                    {
                                        // Check what kind of file we got.
                                        std::wstring fileName;

                                        UINT fileNameCharCount = DragQueryFileW( drop, n, NULL, 0 );

                                        fileName.resize( (size_t)fileNameCharCount );

                                        DragQueryFileW( drop, n, (LPWSTR)fileName.data(), fileNameCharCount + 1 );

                                        // We want to extract the extension, because everything depends on it.
                                        const wchar_t *extStart = FileSystem::FindFileNameExtension( fileName.c_str() );

                                        if ( extStart != NULL )
                                        {
                                            // Check for a known extension.
                                            bool hasKnownExt = false;
                                            eContextMenuOptionType optionType;

                                            if ( wcsicmp( extStart, L"TXD" ) == 0 )
                                            {
                                                optionType = CONTEXTOPT_TXD;

                                                hasKnownExt = true;
                                            }
                                            else if ( wcsicmp( extStart, L"RWTEX" ) == 0 )
                                            {
                                                optionType = CONTEXTOPT_TEXTURE;

                                                hasKnownExt = true;
                                            }

                                            if ( hasKnownExt )
                                            {
                                                // Add this context option.
                                                contextOption_t opt;
                                                opt.fileName = std::move( fileName );
                                                opt.optionType = optionType;

                                                this->contextOptions.push_back( std::move( opt ) );
                                            }
                                        }
                                    }

                                    // As long as we processed one valid item, we can show the context menu.
                                    if ( this->contextOptions.empty() == false )
                                    {
                                        res = S_OK;
                                    }
                                }
                                catch( ... )
                                {
                                    GlobalUnlock( stm.hGlobal );

                                    throw;
                                }

                                GlobalUnlock( stm.hGlobal );
                            }
                        }
                        catch( ... )
                        {
                            ReleaseStgMedium( &stm );

                            throw;
                        }

                        ReleaseStgMedium( &stm );
                    }
                }
                catch( ... )
                {
                    // Ignore errors.
                }
            }
        }

        this->isInitialized = true;

        return res;
    }
    catch( ... )
    {
        // Any runtime error? Fuck this.
        return E_FAIL;
    }
}

inline std::wstring RenderWareContextHandlerProvider::getContextDirectory( void ) const
{
    std::wstring dirPath;

    if ( this->contextOptions.empty() == false )
    {
        dirPath = this->contextOptions.front().fileName;
    }

    if ( !dirPath.empty() )
    {
        eir::MultiString <rw::RwStaticMemAllocator> dirOut;

        bool hasDirectory = FileSystem::GetFileNameDirectory( dirPath.c_str(), dirOut );

        if ( hasDirectory )
        {
            dirOut.transform_to <wchar_t> ();

            dirPath = dirOut.to_char <wchar_t> ();
        }
    }

    return dirPath;
}

template <typename resultFunc>
AINLINE void ShellCallWithResultPath( IFileDialog *resDlg, resultFunc cb )
{
    if ( auto CoTaskMemFree = ole32min.CoTaskMemFree )
    {
        ComPtr <IShellItem> resultItem;

        HRESULT resFetchResult = resDlg->GetResult( &resultItem );

        if ( SUCCEEDED(resFetchResult) )
        {
            // We call back the handler with the result.
            // Since we expect a fs result, we convert to a real path.
            LPWSTR strFSPath = NULL;

            HRESULT getFSPath = resultItem->GetDisplayName( SIGDN_FILESYSPATH, &strFSPath );

            if ( SUCCEEDED( getFSPath ) )
            {
                cb( strFSPath );

                CoTaskMemFree( strFSPath );
            }
        }
    }
}

template <typename resultFunc>
static void ShellGetTargetDirectory( const std::wstring& startPath, resultFunc cb )
{
    if ( auto CoCreateInstance = ole32min.CoCreateInstance )
    {
        ComPtr <IFileOpenDialog> dlg;

        HRESULT getDialogSuc = CoCreateInstance( CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &dlg ) );

        if ( SUCCEEDED(getDialogSuc) )
        {
            dlg->SetOptions( FOS_OVERWRITEPROMPT | FOS_NOCHANGEDIR | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST );

            // Set some friendly attributes. :)
            dlg->SetTitle( L"Browse Folder..." );
            dlg->SetOkButtonLabel( L"Select" );
            dlg->SetFileNameLabel( L"Target:" );

            if ( startPath.empty() == false )
            {
                if ( auto SHCreateItemFromParsingName = shell32min.SHCreateItemFromParsingName )
                {
                    ComPtr <IShellItem> dirPathItem;

                    HRESULT resParsePath = SHCreateItemFromParsingName( startPath.c_str(), NULL, IID_PPV_ARGS( &dirPathItem ) );

                    if ( SUCCEEDED(resParsePath) )
                    {
                        dlg->SetFolder( dirPathItem.Get() );
                    }
                }
            }

            // Alrite. Show ourselves.
            HRESULT resShow = dlg->Show( NULL );

            if ( SUCCEEDED(resShow) )
            {
                // Wait for the result.
                ShellCallWithResultPath( dlg.Get(), cb );
            }
        }
    }
}

template <typename callbackType>
AINLINE void ShellGetFileTargetPath(
    const std::wstring& curDirectoryPath,
    UINT numAvailTypes, const COMDLG_FILTERSPEC *availTypes,
    const wchar_t *defaultExt,
    callbackType cb
)
{
    if ( auto CoCreateInstance = ole32min.CoCreateInstance )
    {
        ComPtr <IFileDialog> dlg;

        HRESULT resCreateDialog = CoCreateInstance( CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &dlg ) );

        if ( SUCCEEDED(resCreateDialog) )
        {
            // Before showing the dialog, we set it up.
            dlg->SetOptions(
                FOS_OVERWRITEPROMPT | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM |
                FOS_PATHMUSTEXIST | FOS_NOREADONLYRETURN 
            );

            // Friendly stuff.
            dlg->SetTitle( L"Select Target..." );
            dlg->SetFileTypes( numAvailTypes, availTypes );
            dlg->SetFileTypeIndex( 1 );
            dlg->SetOkButtonLabel( L"Save" );

            // Default extension.
            if ( defaultExt )
            {
                dlg->SetDefaultExtension( defaultExt );
            }

            if ( curDirectoryPath.empty() == false )
            {
                if ( auto SHCreateItemFromParsingName = shell32min.SHCreateItemFromParsingName )
                {
                    ComPtr <IShellItem> dirPathItem;

                    HRESULT resParsePath = SHCreateItemFromParsingName( curDirectoryPath.c_str(), NULL, IID_PPV_ARGS( &dirPathItem ) );

                    if ( SUCCEEDED(resParsePath) )
                    {
                        dlg->SetFolder( dirPathItem.Get() );
                    }
                }
            }

            // OK. Let do this.
            HRESULT resShowDialog = dlg->Show( NULL );

            if ( SUCCEEDED(resShowDialog) )
            {
                // Get the result. There can be only one result.
                // We want it in FS path format.
                ShellCallWithResultPath( dlg.Get(), cb );
            }
        }
    }
}

static rw::RwObject* RwObjectStreamRead( const wchar_t *fileName )
{
    rw::RwObject *resultObj = NULL;

    rw::streamConstructionFileParamW_t fileParam( fileName );

    rw::Stream *theStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_READONLY, &fileParam );

    if ( theStream )
    {
        try
        {
            resultObj = rwEngine->Deserialize( theStream );
        }
        catch( ... )
        {
            rwEngine->DeleteStream( theStream );

            throw;
        }

        rwEngine->DeleteStream( theStream );
    }

    return resultObj;
}

static void RwObjectStreamWrite( const wchar_t *dstPath, rw::RwObject *texObj )
{
    rw::streamConstructionFileParamW_t fileParam( dstPath );

    rw::Stream *rwStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_WRITEONLY, &fileParam );

    if ( rwStream )
    {
        try
        {
            // Just write the dict.
            rwEngine->Serialize( texObj, rwStream );
        }
        catch( ... )
        {
            rwEngine->DeleteStream( rwStream );

            throw;
        }

        rwEngine->DeleteStream( rwStream );
    }
}

template <typename callbackType>
static void RwObj_deepTraverse( rw::RwObject *rwObj, callbackType cb )
{
    if ( rw::TexDictionary *texDict = rw::ToTexDictionary( rwEngine, rwObj ) )
    {
        for ( rw::TexDictionary::texIter_t iter( texDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
        {
            rw::TextureBase *texHandle = iter.Resolve();

            cb( texHandle );
        }
    }

    cb( rwObj );
}

template <typename callbackType>
static void TexObj_forAllTextures( rw::RwObject *texObj, callbackType cb )
{
    RwObj_deepTraverse( texObj,
        [&]( rw::RwObject *rwObj )
    {
        if ( rw::TextureBase *texHandle = rw::ToTexture( rwEngine, rwObj ) )
        {
            cb( texHandle );
        }
    });
}

template <typename callbackType>
static void TexObj_forAllTextures_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::RwObject *texObj = RwObjectStreamRead( txdFileName );

    if ( texObj )
    {
        try
        {
            TexObj_forAllTextures( texObj, cb );
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( texObj );

            throw;
        }

        // Remember to clean up resources.
        rwEngine->DeleteRwObject( texObj );
    }
}

template <typename callbackType>
static void RwObj_deepTraverse_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::RwObject *texObj = RwObjectStreamRead( txdFileName );

    if ( texObj )
    {
        try
        {
            RwObj_deepTraverse( texObj, cb );
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( texObj );

            throw;
        }

        // Remember to clean up resources.
        rwEngine->DeleteRwObject( texObj );
    }
}

static void RwTextureMakeUnique( rw::TextureBase *texHandle )
{
    // Make a copy of the raster, if it is not unique.
    rw::Raster *texRaster = texHandle->GetRaster();

    if ( texRaster->refCount > 1 )
    {
        rw::Raster *newRaster = rw::CloneRaster( texRaster );

        try
        {
            texHandle->SetRaster( newRaster );
        }
        catch( ... )
        {
            rw::DeleteRaster( newRaster );

            throw;
        }

        // Release our reference.
        rw::DeleteRaster( newRaster );
    }
}

static rw::RwObject* RwObjectDeepClone( rw::RwObject *texObj )
{
    rw::RwObject *newObj = rwEngine->CloneRwObject( texObj );

    if ( newObj )
    {
        try
        {
            if ( rw::TexDictionary *newTexDict = rw::ToTexDictionary( rwEngine, newObj ) )
            {
                // Clone the textures with new raster objects.
                for ( rw::TexDictionary::texIter_t iter( newTexDict->GetTextureIterator() ); !iter.IsEnd(); iter.Increment() )
                {
                    rw::TextureBase *texHandle = iter.Resolve();

                    RwTextureMakeUnique( texHandle );
                }
            }
            else if ( rw::TextureBase *newTexture = rw::ToTexture( rwEngine, newObj ) )
            {
                RwTextureMakeUnique( newTexture );
            }
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( newObj );
            throw;
        }
    }

    return newObj;
}

template <typename callbackType>
static void RwObj_transform_ser( const wchar_t *txdFileName, callbackType cb )
{
    rw::RwObject *texObj = RwObjectStreamRead( txdFileName );

    if ( texObj )
    {
        try
        {
            // Make a backup of the original in case something bad happened that causes data loss.
            rw::RwObject *safetyCopy = RwObjectDeepClone( texObj );

            try
            {
                // Process it.
                cb( texObj );

                // Save it.
                try
                {
                    RwObjectStreamWrite( txdFileName, texObj );
                }
                catch( ... )
                {
                    // Well, something fucked up our stuff.
                    // We should restore to the original, because the user does not want data loss!
                    // If we could not make a safety copy... too bad.
                    if ( safetyCopy )
                    {
                        RwObjectStreamWrite( txdFileName, safetyCopy );
                    }

                    // Transfer the error anyway.
                    throw;
                }
            }
            catch( ... )
            {
                if ( safetyCopy )
                {
                    rwEngine->DeleteRwObject( safetyCopy );
                }

                throw;
            }

            // If we have a safety copy, delete it.
            if ( safetyCopy )
            {
                rwEngine->DeleteRwObject( safetyCopy );
            }
        }
        catch( ... )
        {
            rwEngine->DeleteRwObject( texObj );

            throw;
        }

        rwEngine->DeleteRwObject( texObj );
    }
    else
    {
        throw rw::RwException( "invalid RW object" );
    }
}

template <typename callbackType>
static void TexObj_transform_ser( const wchar_t *txdFileName, callbackType cb )
{
    RwObj_transform_ser( txdFileName,
        [&]( rw::RwObject *rwObj )
    {
        TexObj_forAllTextures( rwObj, cb );
    });
}

template <typename exportHandler>
static void TexObj_exportAs( const wchar_t *txdFileName, const wchar_t *ext, const wchar_t *targetDir, exportHandler cb )
{
    TexObj_forAllTextures_ser( txdFileName,
        [=]( rw::TextureBase *texHandle )
    {
        rw::rwStaticString <wchar_t> targetFilePath( targetDir );

        // Make a file path, with the proper extension.
        rw::rwStaticString <char> ansiName = texHandle->GetName();

        auto wideTexName = CharacterUtil::ConvertStrings <char, wchar_t> ( ansiName );

        targetFilePath += L"\\";
        targetFilePath += wideTexName;
        targetFilePath += L".";
        targetFilePath += ext;

        // Do it!
        try
        {
            rw::streamConstructionFileParamW_t fileParam( targetFilePath.GetConstString() );

            rw::Stream *chunkStream = rwEngine->CreateStream( rw::RWSTREAMTYPE_FILE_W, rw::RWSTREAMMODE_WRITEONLY, &fileParam );

            if ( chunkStream )
            {
                try
                {
                    // Alright. We are finally ready :)
                    cb( texHandle, chunkStream );
                }
                catch( ... )
                {
                    rwEngine->DeleteStream( chunkStream );

                    throw;
                }

                rwEngine->DeleteStream( chunkStream );
            }
        }
        catch( rw::RwException& )
        {
            // Whatever.
        }
    });
}

typedef std::list <std::pair <std::wstring, rw::KnownVersions::eGameVersion>> gameVerList_t;

static const gameVerList_t gameVerMap =
{
    { L"GTA 3 (PC)", rw::KnownVersions::GTA3_PC },
    { L"GTA 3 (PS2)", rw::KnownVersions::GTA3_PS2 },
    { L"GTA 3 (XBOX)", rw::KnownVersions::GTA3_XBOX },
    { L"GTA Vice City (PC)", rw::KnownVersions::VC_PC },
    { L"GTA Vice City (PS2)", rw::KnownVersions::VC_PS2 },
    { L"GTA Vice City (XBOX)", rw::KnownVersions::VC_XBOX },
    { L"GTA San Andreas", rw::KnownVersions::SA },
    { L"Manhunt (PC)", rw::KnownVersions::MANHUNT_PC },
    { L"Manhunt (PS2)", rw::KnownVersions::MANHUNT_PS2 },
    { L"Bully", rw::KnownVersions::BULLY },
    { L"Sonic Heroes", rw::KnownVersions::SHEROES_GC }
};

AINLINE std::wstring ansi_to_unicode( const char *ansiStr )
{
    return std::wstring( ansiStr, ansiStr + strlen( ansiStr ) );
}

IFACEMETHODIMP RenderWareContextHandlerProvider::QueryContextMenu(
    HMENU hMenu, UINT indexMenu,
    UINT idCmdFirst, UINT idCmdLast, UINT uFlags
)
{
    struct contextMan
    {
        HMENU rootMenu;
        UINT osContextFlags;

        UINT global_menuIndex;

        inline contextMan( RenderWareContextHandlerProvider *prov, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT osContextFlags, HMENU rootMenu )
        {
            this->provider = prov;
            this->curMenuID = 0;
            this->idCmdFirst = idCmdFirst;
            this->idCmdLast = idCmdLast;
            this->osContextFlags = osContextFlags;
            this->rootMenu = rootMenu;
            this->global_menuIndex = indexMenu;
        }

        struct node
        {
        private:
            friend struct contextMan;

            contextMan *manager;
            HMENU osMenu;
            bool isHandleShared;

            UINT local_menuIndex;

            node *parentNode;

            UINT node_id;

            inline node(
                contextMan *man,
                UINT startIndex,HMENU osMenu, bool isHandleShared,
                node *parent, UINT node_id
            ) : manager( man )
            {
                this->osMenu = osMenu;
                this->isHandleShared = isHandleShared;

                this->local_menuIndex = startIndex;

                this->parentNode = parent;
                this->node_id = node_id;
            }

        public:
            inline ~node( void )
            {
                if ( !this->isHandleShared )
                {
                    DestroyMenu( this->osMenu );
                }
            }

            inline node( node&& right )
            {
                this->manager = right.manager;
                this->osMenu = right.osMenu;
                this->isHandleShared = right.isHandleShared;

                this->local_menuIndex = right.local_menuIndex;
                this->parentNode = right.parentNode;
                this->node_id = right.node_id;

                right.isHandleShared = true;
                right.osMenu = NULL;
            }

            inline node( const node& right ) = delete;
            inline void operator = ( const node& right ) = delete;
            inline void operator = ( node&& right ) = delete;

        private:
            inline UINT& AcquireMenuIndex( void )
            {
                if ( this->manager->osContextFlags & CMF_SYNCCASCADEMENU )
                {
                    return this->manager->global_menuIndex;
                }

                return this->local_menuIndex;
            }

        public:
            inline void AddCommandEntryW( const wchar_t *displayName, rw::rwStaticString <char>&& verbName, std::function <bool ( void )>&& cb, HBITMAP bmpRes = NULL )
            {
                const UINT usedID = this->manager->curMenuID++;

                UINT fMask = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;

                if ( bmpRes != NULL )
                {
                    fMask |= MIIM_BITMAP;
                }

                MENUITEMINFOW itemInfo;
                itemInfo.cbSize = sizeof( itemInfo );
                itemInfo.fMask = fMask;
                itemInfo.dwTypeData = (wchar_t*)displayName;
                itemInfo.fType = MFT_STRING;
                itemInfo.wID = this->manager->idCmdFirst + usedID;
                itemInfo.fState = MFS_ENABLED;

                if ( bmpRes != NULL )
                {
                    itemInfo.hbmpItem = bmpRes;
                }

                UINT& newMenuIndex = AcquireMenuIndex();

                BOOL insertItemSuccess = InsertMenuItemW( this->osMenu, newMenuIndex, TRUE, &itemInfo );

                if ( insertItemSuccess == false )
                {
                    throw std::exception( "failed to insert menu item" );
                }
                else
                {
                    newMenuIndex++;
                }

                // Add the command handler.
                this->manager->provider->cmdMap[ usedID ] = cb;

                // Add the natural verb.
                this->manager->provider->verbMap[ usedID ] = "rwshell." + verbName;
            }

            inline node AddSubmenuEntryW( const wchar_t *displayName, HBITMAP bmpRes = NULL )
            {
                // If we are in sequential mode, simply return a redirect node.
                if ( this->manager->osContextFlags & CMF_SYNCCASCADEMENU )
                {
                    return node( this->manager, 0, this->osMenu, true, NULL, 0 );
                }

                // We want to create a new menu, so be it.
                HMENU submenuHandle = CreateMenu();

                if ( submenuHandle == NULL )
                {
                    throw std::exception( "failed to create Win32 submenu handle" );
                }

                const UINT usedID = this->manager->curMenuID++;
                
                UINT fMask = MIIM_STRING | MIIM_FTYPE | MIIM_SUBMENU | MIIM_ID | MIIM_STATE;

                if ( bmpRes != NULL )
                {
                    fMask |= MIIM_BITMAP;
                }

                MENUITEMINFOW itemInfo;
                itemInfo.cbSize = sizeof( itemInfo );
                itemInfo.fMask = fMask;
                itemInfo.dwTypeData = (wchar_t*)displayName;
                itemInfo.fType = MFT_STRING;
                itemInfo.hSubMenu = submenuHandle;
                itemInfo.wID = this->manager->idCmdFirst + usedID;
                itemInfo.fState = MFS_ENABLED;

                if ( bmpRes != NULL )
                {
                    itemInfo.hbmpItem = bmpRes;
                }

                UINT& newMenuIndex = AcquireMenuIndex();

                BOOL insertItemSuccess = InsertMenuItemW( this->osMenu, newMenuIndex, TRUE, &itemInfo );

                if ( insertItemSuccess == FALSE )
                {
                    throw std::exception( "failed to insert submenu menu item" );
                }
                else
                {
                    newMenuIndex++;
                }

                return node( this->manager, 0, submenuHandle, false, this, usedID );
            }

            inline void Revert( void )
            {
                if ( node *parent = this->parentNode )
                {
                    // Find and remove ourselves.
                    UINT absolute_node_cmd = this->manager->idCmdFirst + this->node_id;

                    BOOL couldDelete = DeleteMenu( parent->osMenu, absolute_node_cmd, MF_BYCOMMAND );

                    assert( couldDelete == TRUE );
                }
            }
        };

        inline node ConstructRoot( void )
        {
            return node( this, this->global_menuIndex, this->rootMenu, true, NULL, 0 );
        }

    private:
        RenderWareContextHandlerProvider *provider;
        
        UINT curMenuID;
        UINT idCmdFirst, idCmdLast;
    };

    // If uFlags include CMF_DEFAULTONLY then we should not do anything.
    if (CMF_DEFAULTONLY & uFlags)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    contextMan manager( this, indexMenu, idCmdFirst, idCmdLast, uFlags, hMenu );

    try
    {
        contextMan::node mainMenu = manager.ConstructRoot();

        // Only make options visible that make sense.
        bool hasTXDOptions = this->hasContextOption( CONTEXTOPT_TXD );
        bool hasTextureOptions = this->hasContextOption( CONTEXTOPT_TEXTURE );

        bool hasOnlyTextureOptions = this->hasOnlyContextOption( CONTEXTOPT_TEXTURE );

        // Do the menu.
        {
            contextMan::node optionsNode = mainMenu.AddSubmenuEntryW( L"RenderWare Options", this->rwlogo_bitmap );

            // Add some cool options.
            try
            {
                // If we have only textures selected and more than one, we can build a TXD out of them!
                if ( hasOnlyTextureOptions && this->contextOptions.size() > 1 )
                {
                    optionsNode.AddCommandEntryW( L"Build TXD", "build_txd",
                        [=]( void )
                    {
                        // We need a target path to write the TXD to.
                        static const COMDLG_FILTERSPEC writeTypes[] =
                        {
                            L"RW Texture Dictionary",
                            L"*.txd"
                        };

                        try
                        {
                            ShellGetFileTargetPath( this->getContextDirectory(), ARRAYSIZE( writeTypes ), writeTypes, L"txd",
                                [=]( const wchar_t *targetPath )
                            {
                                // Create a tex dictionary, add the textures to it and write it to disk.
                                rw::TexDictionary *texDict = rw::CreateTexDictionary( rwEngine );

                                if ( texDict )
                                {
                                    try
                                    {
                                        this->forAllContextItems(
                                            [&]( const wchar_t *filePath )
                                        {
                                            TexObj_forAllTextures_ser( filePath,
                                                [&]( rw::TextureBase *texHandle )
                                            {
                                                // We clone the textures.
                                                // For this we do not have to perform deep cloning.
                                                rw::TextureBase *newTex = (rw::TextureBase*)rwEngine->CloneRwObject( texHandle );

                                                if ( newTex )
                                                {
                                                    newTex->AddToDictionary( texDict );
                                                }
                                            });
                                        });

                                        // Set the version of the TXD to the version of the first texture, if available.
                                        if ( rw::TextureBase *firstTex = GetFirstTexture( texDict ) )
                                        {
                                            texDict->SetEngineVersion( firstTex->GetEngineVersion() );
                                        }

                                        // Write the TXD now.
                                        RwObjectStreamWrite( targetPath, texDict );
                                    }
                                    catch( ... )
                                    {
                                        rwEngine->DeleteRwObject( texDict );

                                        throw;
                                    }

                                    rwEngine->DeleteRwObject( texDict );
                                }
                            });
                        }
                        catch( rw::RwException& except )
                        {
                            rw::rwStaticString <char> errorMsg( "failed to build TXD: " );

                            errorMsg += except.message;

                            MessageBoxA( NULL, errorMsg.GetConstString(), "Build Error", MB_OK );

                            throw;
                        }
                        return true;
                    });
                }
        
                // We can extract things from texture dictionaries (and textures).
                if ( hasTXDOptions || hasTextureOptions )
                {
                    contextMan::node extractNode = optionsNode.AddSubmenuEntryW( L"Extract" );

                    try
                    {
                        // We add the default options that RenderWare supports.

                        // If there are TXD files, it makes sense to allow exporting as texture chunks.
                        if ( hasTXDOptions )
                        {
                            extractNode.AddCommandEntryW( L"Texture Chunks", "extrTexChunks",
                                [=]( void )
                            {
                                ShellGetTargetDirectory( this->getContextDirectory(),
                                    [=]( const wchar_t *targetPath )
                                {
                                    this->forAllContextItems(
                                        [=]( const wchar_t *fileName )
                                    {
                                        // We want to export the contents as raw texture chunks.
                                        TexObj_exportAs( fileName, L"rwtex", targetPath,
                                            [=]( rw::TextureBase *texHandle, rw::Stream *outStream )
                                        {
                                            // We export the texture directly.
                                            rwEngine->Serialize( texHandle, outStream );
                                        });
                                    });
                                });
                                return true;
                            });
                        }

                        try
                        {
                            rw::registered_image_formats_t regFormats;
                            rw::GetRegisteredImageFormats( rwEngine, regFormats );
                            
                            size_t numFormats = regFormats.GetCount();

                            for ( size_t n = 0; n < numFormats; n++ )
                            {
                                const rw::registered_image_format& format = regFormats[ n ];

                                // Make a nice display string.
                                const char *niceExt = rw::GetLongImagingFormatExtension( format.num_ext, format.ext_array );

                                if ( niceExt )
                                {
                                    std::wstring displayString = ansi_to_unicode( niceExt );
                                    displayString += L" (";
                                    displayString += ansi_to_unicode( format.formatName );
                                    displayString += L")";

                                    extractNode.AddCommandEntryW( displayString.c_str(), rw::rwStaticString <char> ( "extr_" ) + niceExt,
                                        [=]( void )
                                    {
                                        // TODO: actually use a good extention instead of the default.
                                        // We want to add support into RW to specify multiple image format extentions.

                                        std::string extention( niceExt );
                                        std::wstring wideExtention( extention.begin(), extention.end() );

                                        std::transform( wideExtention.begin(), wideExtention.end(), wideExtention.begin(), ::tolower );

                                        ShellGetTargetDirectory( this->getContextDirectory(),
                                            [=]( const wchar_t *targetDir )
                                        {
                                            this->forAllContextItems(
                                                [=]( const wchar_t *fileName )
                                            {
                                                TexObj_exportAs( fileName, wideExtention.c_str(), targetDir,
                                                    [=]( rw::TextureBase *texHandle, rw::Stream *outStream )
                                                {
                                                    // Only perform if we have a raster, which should be always anyway.
                                                    if ( rw::Raster *texRaster = texHandle->GetRaster() )
                                                    {
                                                        texRaster->writeImage( outStream, extention.c_str() );
                                                    }
                                                });
                                            });
                                        });
                        
                                        return true;
                                    });
                                }
                            }
                        }
                        catch( rw::RwException& )
                        {
                            // If there was any RW exception, we continue anyway.
                        }
                    }
                    catch( ... )
                    {
                        extractNode.Revert();

                        throw;
                    }
                }

                if ( hasTXDOptions || hasTextureOptions )
                {
                    // We also might want to convert TXD files.
                    contextMan::node convertNode = optionsNode.AddSubmenuEntryW( L"Set Platform" );

                    try
                    {
                        // Fill it with entries of platforms that we can target.
                        rw::platformTypeNameList_t availPlatforms = rw::GetAvailableNativeTextureTypes( rwEngine );

                        UINT cur_index = 0;

                        size_t numPlatforms = availPlatforms.GetCount();

                        for ( size_t n = 0; n < numPlatforms; n++ )
                        {
                            const rw::rwStaticString <char>& name = availPlatforms[ numPlatforms - 1 - n ];

                            std::wstring displayString = ansi_to_unicode( name.GetConstString() );

                            convertNode.AddCommandEntryW( displayString.c_str(), "setPlat_" + name,
                                [=]( void )
                            {
                                try
                                {
                                    this->forAllContextItems(
                                        [=]( const wchar_t *fileName )
                                    {
                                        TexObj_transform_ser( fileName,
                                            [=]( rw::TextureBase *texHandle )
                                        {
                                            // Convert to another platform.
                                            // We only want to suceed with stuff if we actually wrote everything.
                                            if ( rw::Raster *texRaster = texHandle->GetRaster() )
                                            {
                                                rw::ConvertRasterTo( texRaster, name.GetConstString() );
                                            }
                                        });
                                    });
                                }
                                catch( rw::RwException& except )
                                {
                                    // We should display a message box.
                                    rw::rwStaticString <char> errorMessage( "failed to convert to platform: " );

                                    errorMessage += except.message;

                                    MessageBoxA( NULL, errorMessage.GetConstString(), "Conversion Error", MB_OK );

                                    // Throw further.
                                    throw;
                                }

                                return true;
                            });
                        }
                    }
                    catch( ... )
                    {
                        convertNode.Revert();

                        throw;
                    }
                }

                // Every RenderWare object has this ability.
                // We also want the ability to change game version.
                {
                    contextMan::node versionNode = optionsNode.AddSubmenuEntryW( L"Set Version" );

                    try
                    {
                        // Add game versions.
                        for ( const std::pair <std::wstring, rw::KnownVersions::eGameVersion>& verPair : gameVerMap )
                        {
                            versionNode.AddCommandEntryW( verPair.first.c_str(), "gameVer_" + eir::to_string <char, rw::RwStaticMemAllocator> ( (unsigned int)verPair.second ),
                                [=]( void )
                            {
                                try
                                {
                                    // Execute us.
                                    this->forAllContextItems(
                                        [=]( const wchar_t *fileName )
                                    {
                                        RwObj_transform_ser( fileName,
                                            [=]( rw::RwObject *rwObj )
                                        {
                                            RwObj_deepTraverse( rwObj,
                                                [=]( rw::RwObject *rwObj )
                                            {
                                                rw::LibraryVersion newVer = rw::KnownVersions::getGameVersion( verPair.second );

                                                rwObj->SetEngineVersion( newVer );
                                            });
                                        });
                                    });
                                }
                                catch( rw::RwException& except )
                                {
                                    // We want to inform about this aswell.
                                    rw::rwStaticString <char> errorMsg( "failed to set game version: " );

                                    errorMsg += except.message;

                                    MessageBoxA( NULL, errorMsg.GetConstString(), "Error", MB_OK );

                                    // Pass on the error.
                                    throw;
                                }
                                return true;
                            });
                        }
                    }
                    catch( ... )
                    {
                        versionNode.Revert();

                        throw;
                    }
                }

                // TODO: detect whether Magic.TXD has been installed into a location.
                if ( false )
                {
                    // For texture dictionaries, we allow opening with Magic.TXD.
                    if ( hasTXDOptions && this->contextOptions.size() < 4 )
                    {
                        optionsNode.AddCommandEntryW( L"Open with Magic.TXD", "open_with_mgtxd",
                            [=]( void )
                        {
                            //__debugbreak();
                            return true;
                        });
                    }
                }
            }
            catch( ... )
            {
                optionsNode.Revert();

                return HRESULT_FROM_WIN32( GetLastError() );
            }
        }
    }
    catch( ... )
    {
        // We cannot afford having any runtime error, so we must cleanly abort.
    }

    // Determine the highest used ID.
    UINT highestID = 0;

    if ( !this->cmdMap.empty() )
    {
        highestID = this->cmdMap.rbegin()->first;
    }

    return MAKE_HRESULT( SEVERITY_SUCCESS, 0, highestID + 1 );
}

// EVERYTHING in the shell must have fucking IDs. I am not fucking joking.
// Just do it. It will save you some frustration.

// So here is a funny dev story for ya, because you love reading source code.
// I spent an entire day debuggin the Windows(tm) shell(tm) to find out why my IContextHandler broke
// if the user selected more than 16 entries. Turns out that Microsoft expects the root of the submenu
// tree to be marked with ID zero! I bet you cannot read that in the docs. And all over the
// Internet people cry why IContextMenu cannot handle more than friggin 16 entries.
// The easy samples that Microsoft has released are retarded. They do not even feature sub menus,
// and their code really works with them, and does a bad job. Well, I will have to see how far my stuff
// will reach from now on.

IFACEMETHODIMP RenderWareContextHandlerProvider::InvokeCommand( LPCMINVOKECOMMANDINFO pici )
{
    try
    {
        // Check whether we do a unicode request.
        bool isUnicode = false;

        if ( pici->cbSize == sizeof( CMINVOKECOMMANDINFOEX ) )
        {
            CMINVOKECOMMANDINFOEX *infoEx = (CMINVOKECOMMANDINFOEX*)pici;

            if ( HIWORD( infoEx->lpVerbW ) != 0 )
            {
                isUnicode = true;
            }
        }

        // We check by context menu id.
        bool isANSIVerbMode = ( HIWORD( pici->lpVerb ) != 0 );
    
        bool hasFoundCommandID = false;
        UINT cmdID = 0xFFFFFFFF;

        if ( !hasFoundCommandID )
        {
            // Try to handle the unicode version.
            if ( isUnicode )
            {
                CMINVOKECOMMANDINFOEX *infoEx = (CMINVOKECOMMANDINFOEX*)pici;

                hasFoundCommandID = findCommandUnicode( infoEx->lpVerbW, cmdID );
            }
        }

        if ( !hasFoundCommandID ) 
        {
            // Try to handle the ANSI version.
            if ( isANSIVerbMode )
            {
                hasFoundCommandID = findCommandANSI( pici->lpVerb, cmdID );
            }
        }

        if ( !hasFoundCommandID )
        {
            if ( !isANSIVerbMode )
            {
                cmdID = LOWORD( pici->lpVerb );

                hasFoundCommandID = true;
            }
        }

        HRESULT isHandled = E_FAIL;

        if ( hasFoundCommandID )
        {
            // Execute the item.
            menuCmdMap_t::const_iterator iter = this->cmdMap.find( cmdID );

            if ( iter != this->cmdMap.end() )
            {
                try
                {
                    // Do it.
                    bool successful = iter->second();

                    isHandled = ( successful ? S_OK : S_FALSE );
                }
                catch( ... )
                {
                    // If we encountered any exception, just fail the handler.
                }
            }
        }

        return isHandled;
    }
    catch( ... )
    {
        // Any exception really should not be passed on
        return E_FAIL;
    }
}

IFACEMETHODIMP RenderWareContextHandlerProvider::GetCommandString(
    UINT_PTR idCommand, UINT uFlags,
    UINT *pwReserved, LPSTR pszName, UINT cchMax
)
{
    try
    {
        HRESULT res = E_FAIL;

        if ( uFlags == GCS_HELPTEXTA || uFlags == GCS_HELPTEXTW )
        {
            // No thanks.
            res = E_FAIL;
        }
        else if ( uFlags == GCS_VALIDATEA || uFlags == GCS_VALIDATEW )
        {
            // Check by string.
            bool doesExist = false;

            // We check in the map directly.
            UINT cmdID = 0xFFFFFFFF;

            if ( uFlags == GCS_VALIDATEA )
            {
                doesExist = findCommandANSI( pszName, cmdID );
            }
            else if ( uFlags == GCS_VALIDATEW )
            {
                doesExist = findCommandUnicode( (const wchar_t*)pszName, cmdID );
            }

            res = ( doesExist ? S_OK : S_FALSE );
        }
        else if ( uFlags == GCS_VERBA || uFlags == GCS_VERBW )
        {
            const verbMap_t::const_iterator verbIter = this->verbMap.find( (UINT)idCommand );

            if ( verbIter != this->verbMap.end() )
            {
                const auto& theVerb = verbIter->second;

                if ( uFlags == GCS_VERBA )
                {
                    if ( cchMax >= theVerb.GetLength() + 1 )
                    {
                        // Write the string.
                        strcpy( pszName, theVerb.GetConstString() );

                        res = S_OK;
                    }
                }
                else if ( uFlags == GCS_VERBW )
                {
                    auto wideVerb = CharacterUtil::ConvertStrings <char, wchar_t> ( theVerb );

                    if ( cchMax >= wideVerb.GetLength() + 1 )
                    {
                        wcscpy( (wchar_t*)pszName, wideVerb.GetConstString() );

                        res = S_OK;
                    }
                }
            }
        }

        return res;
    }
    catch( ... )
    {
        // Be sure to cleanly terminate!
        return E_FAIL;
    }
}