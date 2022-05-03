/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/texturebase.cpp
*  PURPOSE:     TextureBase type main source file.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#include "txdread.common.hxx"

#include "txdread.objutil.hxx"

#include "pluginutil.hxx"

#include "rwserialize.hxx"

namespace rw
{

// We need a consistency lock for textures.
struct _fetchTextureTypeStructoid
{
    AINLINE static RwTypeSystem::typeInfoBase* resolveType( EngineInterface *engineInterface )
    {
        return engineInterface->textureTypeInfo;
    }
};

typedef rwobjLockTypeRegister <_fetchTextureTypeStructoid> textureConsistencyEnv;

static optional_struct_space <PluginDependantStructRegister <textureConsistencyEnv, RwInterfaceFactory_t>> textureConsistencyRegister;


static inline rwlock* GetTextureLock( const TextureBase *texHandle )
{
    EngineInterface *engineInterface = (EngineInterface*)texHandle->engineInterface;

    textureConsistencyEnv *lockEnv = textureConsistencyRegister.get().GetPluginStruct( engineInterface );

    if ( lockEnv )
    {
        return lockEnv->GetLock( engineInterface, texHandle );
    }

    return nullptr;
}

/*
 * Texture Base
 */

TextureBase::TextureBase( const TextureBase& right ) :
    RwObject( right ),
    name( eir::constr_with_alloc::DEFAULT, this->engineInterface ),
    maskName( eir::constr_with_alloc::DEFAULT, this->engineInterface )
{
    scoped_rwlock_reader <> ctxCloneTexture( GetTextureLock( &right ) );

    // General cloning business.
    this->name = right.name;
    this->maskName = right.maskName;
    this->texRaster = AcquireRaster( right.texRaster );
    this->filterMode = right.filterMode;
    this->uAddressing = right.uAddressing;
    this->vAddressing = right.vAddressing;

    // We do not want to belong to a TXD by default.
    // Even if the original texture belonged to one.
    this->texDict = nullptr;
}

TextureBase::~TextureBase( void )
{
    // Clear our raster.
    this->SetRaster( nullptr );

    // Make sure we are not in a texture dictionary.
    // SINCE WE HAVE NO LOCK ANYMORE, we must avoid taking it -> native functions.
    this->_RemoveFromDictionaryNative();
}

rwlock* TextureBase::GetLock( void ) const
{
    return GetTextureLock( this );
}

void TextureBase::SetRaster( Raster *texRaster )
{
    scoped_rwlock_writer <> ctxSetRaster( GetTextureLock( this ) );

    // If we had a previous raster, unlink it.
    if ( Raster *prevRaster = this->texRaster )
    {
        DeleteRaster( prevRaster );

        this->texRaster = nullptr;
    }

    if ( texRaster )
    {
        // We get a new reference to the raster.
        this->texRaster = AcquireRaster( texRaster );

        if ( Raster *ourRaster = this->texRaster )
        {
            // Set the virtual version of this texture.
            this->objVersion = ourRaster->GetEngineVersion();
        }
    }
}

void TextureBase::_LinkDictionary( TexDictionary *dict )
{
    // Note: original RenderWare performs an insert, not an append.
    // I switched this around, so that textures stay in the correct order.
    LIST_APPEND( dict->textures.root, texDictNode );

    dict->numTextures++;

    this->texDict = dict;
}

void TextureBase::AddToDictionary( TexDictionary *dict )
{
    scoped_rwlock_writer <> ctxSetDict( GetTextureLock( this ) );

    this->_RemoveFromDictionaryNative();

    // When we add to a dict we assume that a proper dict was passed.
    assert( dict != nullptr );

    scoped_rwlock_writer <> ctxAddToDict( GetTXDLock( dict ) );

    this->_LinkDictionary( dict );
}

void TextureBase::_UnlinkDictionary( TexDictionary *belongingTXD )
{
    LIST_REMOVE( this->texDictNode );

    belongingTXD->numTextures--;

    this->texDict = nullptr;
}

void TextureBase::UnlinkDictionary( void )
{
    scoped_rwlock_writer <> ctxUnlinkDict( GetTextureLock( this ) );

    // Need to check again because lock could have invalidated the assumption
    // that the TXD belongs to someone.
    if ( TexDictionary *belongingTXD = this->texDict )
    {
        this->_UnlinkDictionary( belongingTXD );
    }
}

void TextureBase::_RemoveFromDictionaryNative( void )
{
    TexDictionary *belongingTXD = this->texDict;

    if ( belongingTXD != nullptr )
    {
        scoped_rwlock_writer <> ctxRemoveTexture( GetTXDLock( belongingTXD ) );

        // I have thought long-and-hard and reached the conclusion that this check is safe.
        // * texDict cannot be destroyed because that would violate object-runtime rules anyway
        // * texDict can be changed during this call so we need verification
        // Those are all the points that matter.
        if ( this->texDict == belongingTXD )
        {
            this->_UnlinkDictionary( belongingTXD );
        }
    }
}

void TextureBase::RemoveFromDictionary( void )
{
    scoped_rwlock_writer <> ctxSetDict( GetTextureLock( this ) );

    this->_RemoveFromDictionaryNative();
}

TexDictionary* TextureBase::GetTexDictionary( void ) const
{
    scoped_rwlock_reader <> ctxGetDict( GetTextureLock( this ) );

    return this->texDict;
}

void TextureBase::SetFilterMode( eRasterStageFilterMode filterMode )
{
    scoped_rwlock_writer <> ctxSet( GetTextureLock( this ) );

    this->filterMode = filterMode;
}

void TextureBase::SetUAddressing( eRasterStageAddressMode addrMode )
{
    scoped_rwlock_writer <> ctxSet( GetTextureLock( this ) );

    this->uAddressing = addrMode;
}

void TextureBase::SetVAddressing( eRasterStageAddressMode addrMode )
{
    scoped_rwlock_writer <> ctxSet( GetTextureLock( this ) );

    this->vAddressing = addrMode;
}

eRasterStageFilterMode TextureBase::GetFilterMode( void ) const
{
    scoped_rwlock_reader <> ctxGet( GetTextureLock( this ) );

    return this->filterMode;
}

eRasterStageAddressMode TextureBase::GetUAddressing( void ) const
{
    scoped_rwlock_reader <> ctxGet( GetTextureLock( this ) );

    return this->uAddressing;
}

eRasterStageAddressMode TextureBase::GetVAddressing( void ) const
{
    scoped_rwlock_reader <> ctxGet( GetTextureLock( this ) );

    return this->vAddressing;
}

TextureBase* CreateTexture( Interface *intf, Raster *texRaster )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    TextureBase *textureOut = nullptr;

    if ( RwTypeSystem::typeInfoBase *textureTypeInfo = engineInterface->textureTypeInfo )
    {
        GenericRTTI *rtObj = engineInterface->typeSystem.Construct( engineInterface, textureTypeInfo, nullptr );

        if ( rtObj )
        {
            textureOut = (TextureBase*)RwTypeSystem::GetObjectFromTypeStruct( rtObj );

            // Set the raster into the texture.
            textureOut->SetRaster( texRaster );
        }
    }

    return textureOut;
}

TextureBase* ToTexture( Interface *intf, RwObject *rwObj )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( isRwObjectInheritingFrom( engineInterface, rwObj, engineInterface->textureTypeInfo ) )
    {
        return (TextureBase*)rwObj;
    }

    return nullptr;
}

const TextureBase* ToConstTexture( Interface *intf, const RwObject *rwObj )
{
    EngineInterface *engineInterface = (EngineInterface*)intf;

    if ( isRwObjectInheritingFrom( engineInterface, rwObj, engineInterface->textureTypeInfo ) )
    {
        return (const TextureBase*)rwObj;
    }

    return nullptr;
}

// Filtering helper API.
void TextureBase::improveFiltering(void)
{
    scoped_rwlock_writer <> ctxRenderSettings( GetTextureLock( this ) );

    // This routine scaled up the filtering settings of this texture.
    // When rendered, this texture will appear smoother.

    // Handle stuff depending on our current settings.
    eRasterStageFilterMode currentFilterMode = this->filterMode;

    eRasterStageFilterMode newFilterMode = currentFilterMode;

    if ( currentFilterMode == RWFILTER_POINT )
    {
        newFilterMode = RWFILTER_LINEAR;
    }
    else if ( currentFilterMode == RWFILTER_POINT_POINT )
    {
        newFilterMode = RWFILTER_POINT_LINEAR;
    }

    // Update our texture fields.
    if ( currentFilterMode != newFilterMode )
    {
        this->filterMode = newFilterMode;
    }
}

void TextureBase::fixFiltering(void)
{
    scoped_rwlock_writer <> ctxFixFiltering( GetTextureLock( this ) );

    // Only do things if we have a raster.
    if ( Raster *texRaster = this->texRaster )
    {
        // Adjust filtering mode.
        eRasterStageFilterMode currentFilterMode = this->filterMode;

        uint32 actualNewMipmapCount = texRaster->getMipmapCount();

        // We need to represent a correct filter state, depending on the mipmap count
        // of the native texture. This is required to enable mipmap rendering, when required!
        if ( actualNewMipmapCount > 1 )
        {
            if ( currentFilterMode == RWFILTER_POINT )
            {
                this->filterMode = RWFILTER_POINT_POINT;
            }
            else if ( currentFilterMode == RWFILTER_LINEAR )
            {
                this->filterMode = RWFILTER_LINEAR_LINEAR;
            }
        }
        else
        {
            if ( currentFilterMode == RWFILTER_POINT_POINT ||
                 currentFilterMode == RWFILTER_POINT_LINEAR )
            {
                this->filterMode = RWFILTER_POINT;
            }
            else if ( currentFilterMode == RWFILTER_LINEAR_POINT ||
                      currentFilterMode == RWFILTER_LINEAR_LINEAR )
            {
                this->filterMode = RWFILTER_LINEAR;
            }
        }

        // TODO: if there is a filtering mode that does not make sense at all,
        // how should we handle it?
    }
}

AINLINE bool isValidFilterMode( uint32 binaryFilterMode )
{
    if ( binaryFilterMode == RWFILTER_POINT ||
         binaryFilterMode == RWFILTER_LINEAR ||
         binaryFilterMode == RWFILTER_POINT_POINT ||
         binaryFilterMode == RWFILTER_LINEAR_POINT ||
         binaryFilterMode == RWFILTER_POINT_LINEAR ||
         binaryFilterMode == RWFILTER_LINEAR_LINEAR )
    {
        return true;
    }

    return false;
}

AINLINE bool isValidTexAddressingMode( uint32 binary_addressing )
{
    if ( binary_addressing == RWTEXADDRESS_WRAP ||
         binary_addressing == RWTEXADDRESS_MIRROR ||
         binary_addressing == RWTEXADDRESS_CLAMP ||
         binary_addressing == RWTEXADDRESS_BORDER )
    {
        return true;
    }

    return false;
}

inline void SetFormatInfoToTexture(
    TextureBase& outTex,
    uint32 binaryFilterMode, uint32 binary_uAddressing, uint32 binary_vAddressing,
    bool isLikelyToFail
)
{
    eRasterStageFilterMode rwFilterMode = RWFILTER_LINEAR;

    eRasterStageAddressMode rw_uAddressing = RWTEXADDRESS_WRAP;
    eRasterStageAddressMode rw_vAddressing = RWTEXADDRESS_WRAP;

    // If we are likely to fail, we should check if we should even output warnings.
    bool doOutputWarnings = true;

    if ( isLikelyToFail )
    {
        // If we are likely to fail, we do not want to print as many warnings to the user.
        doOutputWarnings = false;

        const int32 reqWarningLevel = 4;

        if ( outTex.engineInterface->GetWarningLevel() >= reqWarningLevel )
        {
            doOutputWarnings = true;
        }
    }

    // Make sure they are valid.
    if ( isValidFilterMode( binaryFilterMode ) )
    {
        rwFilterMode = (eRasterStageFilterMode)binaryFilterMode;
    }
    else
    {
        if ( doOutputWarnings )
        {
            outTex.engineInterface->PushWarningObject( &outTex, L"TEXTURE_WARN_INVFILTERMODE" );
        }
    }

    if ( isValidTexAddressingMode( binary_uAddressing ) )
    {
        rw_uAddressing = (eRasterStageAddressMode)binary_uAddressing;
    }
    else
    {
        if ( doOutputWarnings )
        {
            outTex.engineInterface->PushWarningObject( &outTex, L"TEXTURE_WARN_INVUADDRMODE" );
        }
    }

    if ( isValidTexAddressingMode( binary_vAddressing ) )
    {
        rw_vAddressing = (eRasterStageAddressMode)binary_vAddressing;
    }
    else
    {
        if ( doOutputWarnings )
        {
            outTex.engineInterface->PushWarningObject( &outTex, L"TEXTURE_WARN_INVVADDRMODE" );
        }
    }

    // Put the fields.
    outTex.SetFilterMode( rwFilterMode );
    outTex.SetUAddressing( rw_uAddressing );
    outTex.SetVAddressing( rw_vAddressing );
}

void texFormatInfo::parse(TextureBase& outTex, bool isLikelyToFail) const
{
    // Read our fields, which are from a binary stream.
    uint32 binaryFilterMode = this->filterMode;

    uint32 binary_uAddressing = this->uAddressing;
    uint32 binary_vAddressing = this->vAddressing;

    SetFormatInfoToTexture(
        outTex,
        binaryFilterMode, binary_uAddressing, binary_vAddressing,
        isLikelyToFail
    );
}

void texFormatInfo::set(const TextureBase& inTex)
{
    this->filterMode = (uint32)inTex.GetFilterMode();
    this->uAddressing = (uint32)inTex.GetUAddressing();
    this->vAddressing = (uint32)inTex.GetVAddressing();
    this->pad1 = 0;
}

void texFormatInfo::writeToBlock(BlockProvider& outputProvider) const
{
    texFormatInfo_serialized <endian::p_little_endian <uint32>> serStruct;
    serStruct.info = *(uint32*)this;

    outputProvider.writeStruct( serStruct );
}

void texFormatInfo::readFromBlock(BlockProvider& inputProvider)
{
    texFormatInfo_serialized <endian::p_little_endian <uint32>> serStruct;

    inputProvider.readStruct( serStruct );

    *(uint32*)this = serStruct.info;
}

void wardrumFormatInfo::parse(TextureBase& outTex) const
{
    // Read our fields, which are from a binary stream.
    uint32 binaryFilterMode = this->filterMode;

    uint32 binary_uAddressing = this->uAddressing;
    uint32 binary_vAddressing = this->vAddressing;

    SetFormatInfoToTexture(
        outTex,
        binaryFilterMode, binary_uAddressing, binary_vAddressing,
        false
    );
}

void wardrumFormatInfo::set(const TextureBase& inTex)
{
    this->filterMode = (uint32)inTex.GetFilterMode();
    this->uAddressing = (uint32)inTex.GetUAddressing();
    this->vAddressing = (uint32)inTex.GetVAddressing();
}

// Main modules.
void registerNativeTexturePlugins( void );
void unregisterNativeTexturePlugins( void );

// Sub modules.
void registerResizeFilteringEnvironment( void );
void unregisterResizeFilteringEnvironment( void );

void registerTextureBasePlugins( void )
{
    // Register the raster content plugins.
    registerNativeTexturePlugins();

    // Important things for the texture handle itself.
    textureConsistencyRegister.Construct( engineFactory );

    // Register pure sub modules.
    registerResizeFilteringEnvironment();
}

void unregisterTextureBasePlugins( void )
{
    unregisterResizeFilteringEnvironment();

    textureConsistencyRegister.Destroy();

    unregisterNativeTexturePlugins();
}

};
