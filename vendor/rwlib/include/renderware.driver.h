/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        include/renderware.driver.h
*  PURPOSE:
*       RenderWare driver virtualization framework.
*       Drivers are a reconception of the original pipeline design of Criterion.
*       A driver has its own copies of instanced geometry, textures and materials.
*       Those objects have implicit references to instanced handles.
*       
*       If an atomic is asked to be rendered, it is automatically instanced.
*       Instancing can be sheduled asynchronically way before the rendering to improve performance.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

namespace rw
{

// Virtual driver device states.
typedef uint32 rwDeviceValue_t;

enum rwFogModeState : rwDeviceValue_t
{
    RWFOG_DISABLE,
    RWFOG_LINEAR,
    RWFOG_EXP,
    RWFOG_EXP2
};
enum rwShadeModeState : rwDeviceValue_t
{
    RWSHADE_FLAT = 1,
    RWSHADE_GOURAUD
};
enum rwStencilOpState : rwDeviceValue_t
{
    RWSTENCIL_KEEP = 1,
    RWSTENCIL_ZERO,
    RWSTENCIL_REPLACE,
    RWSTENCIL_INCRSAT,
    RWSTENCIL_DECRSAT,
    RWSTENCIL_INVERT,
    RWSTENCIL_INCR,
    RWSTENCIL_DECR
};
enum rwCompareOpState : rwDeviceValue_t
{
    RWCMP_NEVER = 1,
    RWCMP_LESS,
    RWCMP_EQUAL,
    RWCMP_LESSEQUAL,
    RWCMP_GREATER,
    RWCMP_NOTEQUAL,
    RWCMP_GREATEREQUAL,
    RWCMP_ALWAYS
};
enum rwCullModeState : rwDeviceValue_t
{
    RWCULL_NONE = 1,
    RWCULL_CLOCKWISE,
    RWCULL_COUNTERCLOCKWISE
};
enum rwBlendModeState : rwDeviceValue_t
{
    RWBLEND_ZERO = 1,
    RWBLEND_ONE,
    RWBLEND_SRCCOLOR,
    RWBLEND_INVSRCCOLOR,
    RWBLEND_SRCALPHA,
    RWBLEND_INVSRCALPHA,
    RWBLEND_DESTALPHA,
    RWBLEND_INVDESTALPHA,
    RWBLEND_DESTCOLOR,
    RWBLEND_INVDESTCOLOR,
    RWBLEND_SRCALPHASAT
};
enum rwBlendOp : rwDeviceValue_t
{
    RWBLENDOP_ADD,
    RWBLENDOP_SUBTRACT,
    RWBLENDOP_REV_SUBTRACT,
    RWBLENDOP_MIN,
    RWBLENDOP_MAX
};
enum rwLogicOp : rwDeviceValue_t
{
    RWLOGICOP_CLEAR,
    RWLOGICOP_SET,
    RWLOGICOP_COPY,
    RWLOGICOP_COPY_INV,
    RWLOGICOP_NOOP,
    RWLOGICOP_INVERT,
    RWLOGICOP_AND,
    RWLOGICOP_NAND,
    RWLOGICOP_OR,
    RWLOGICOP_NOR,
    RWLOGICOP_XOR,
    RWLOGICOP_EQUIV,
    RWLOGICOP_AND_REVERSE,
    RWLOGICOP_AND_INV,
    RWLOGICOP_OR_REVERSE,
    RWLOGICOP_OR_INV
};
enum rwRenderFillMode : rwDeviceValue_t
{
    RWFILLMODE_WIREFRAME,
    RWFILLMODE_SOLID
};

// The actual primitive topology that is being input.
// This is basically a stronger declaration that the topology type.
enum class ePrimitiveTopology : uint32
{
    POINTLIST,
    LINELIST,
    LINESTRIP,
    TRIANGLELIST,
    TRIANGLESTRIP
};

// Vaguelly says what a graphics state should render.
enum class ePrimitiveTopologyType : uint32
{
    POINT,
    LINE,
    TRIANGLE,
    PATCH
};

// Types of vertex attribute values that are supported by any driver.
enum class eVertexAttribValueType : uint32
{
    INT8,
    INT16,
    INT32,
    UINT8,
    UINT16,
    UINT32,
    FLOAT32
};

// Type of vertex attribute semantic.
// This defines how a vertex attribute should be used in the pipeline.
enum class eVertexAttribSemanticType : uint32
{
    POSITION,
    COLOR,
    NORMAL,
    TEXCOORD
};

// Defines a vertex declaration.
// The driver can create a vertex attrib descriptor out of an array of these.
struct vertexAttrib
{
    eVertexAttribSemanticType semanticType;     // binding point semantic type (required for vertex shader processing)
    uint32 semanticIndex;                       // there are multiple variants of semanticType (tex coord 0, tex coord 1, color target 0, etc)
    eVertexAttribValueType attribType;          // value type
    uint32 count;                               // count of the values of attribType value type
    uint32 alignedByteOffset;                   // byte offset in the vertex data item aligned by the attrib type byte size
};

// Driver graphics state objects.
struct gfxBlendState
{
    inline gfxBlendState( void )
    {
        this->enableBlend = false;
        this->enableLogicOp = false;
        this->srcBlend = RWBLEND_ONE;
        this->dstBlend = RWBLEND_ZERO;
        this->blendOp = RWBLENDOP_ADD;
        this->srcAlphaBlend = RWBLEND_ONE;
        this->dstAlphaBlend = RWBLEND_ZERO;
        this->alphaBlendOp = RWBLENDOP_ADD;
        this->logicOp = RWLOGICOP_NOOP;
    }

    bool enableBlend;
    bool enableLogicOp;
    rwBlendModeState srcBlend;
    rwBlendModeState dstBlend;
    rwBlendOp blendOp;
    rwBlendModeState srcAlphaBlend;
    rwBlendModeState dstAlphaBlend;
    rwBlendOp alphaBlendOp;
    rwLogicOp logicOp;
};

struct gfxRasterizerState
{
    inline gfxRasterizerState( void )
    {
        this->fillMode = RWFILLMODE_SOLID;
        this->cullMode = RWCULL_CLOCKWISE;
        this->depthBias = 0;
        this->depthBiasClamp = 0;
        this->slopeScaledDepthBias = 0.0f;
    }

    rwRenderFillMode fillMode;
    rwCullModeState cullMode;
    int32 depthBias;
    float depthBiasClamp;
    float slopeScaledDepthBias;
};

struct gfxDepthStencilOpState
{
    inline gfxDepthStencilOpState( void )
    {
        this->failOp = RWSTENCIL_KEEP;
        this->depthFailOp = RWSTENCIL_KEEP;
        this->passOp = RWSTENCIL_KEEP;
        this->func = RWCMP_NEVER;
    }

    rwStencilOpState failOp;
    rwStencilOpState depthFailOp;
    rwStencilOpState passOp;
    rwCompareOpState func;
};

struct gfxDepthStencilState
{
    inline gfxDepthStencilState( void )
    {
        this->enableDepthTest = false;
        this->enableDepthWrite = false;
        this->depthFunc = RWCMP_NEVER;
        this->enableStencilTest = false;
        this->stencilReadMask = std::numeric_limits <uint8>::max();
        this->stencilWriteMask = std::numeric_limits <uint8>::max();
    }

    bool enableDepthTest;
    bool enableDepthWrite;
    rwCompareOpState depthFunc;
    bool enableStencilTest;
    uint8 stencilReadMask;
    uint8 stencilWriteMask;
    gfxDepthStencilOpState frontFace;
    gfxDepthStencilOpState backFace;
};

// Driver supported sample value types.
// Hopefully we can map most of them to eRasterFormat!
enum class gfxRasterFormat
{
    UNSPECIFIED,

    // Some very high quality formats for when you really need it.
    R32G32B32A32_TYPELESS,
    R32G32B32A32_FLOAT,
    R32G32B32A32_UINT,
    R32G32B32A32_SINT,
    R32G32B32_TYPELESS,
    R32G32B32_FLOAT,
    R32G32B32_UINT,
    R32G32B32_SINT,

    // RenderWare old-style formats.
    R8G8B8A8,
    B5G6R5,
    B5G5R5A1,
    LUM8,       // actually red component.
    DEPTH16,
    DEPTH24,
    DEPTH32,

    // it goes without saying that any format that is not listed here
    // will have to be software processed into a hardware compatible format.

    // Good old DXT.
    DXT1,
    DXT3,       // same as DXT2
    DXT5,       // same as DXT4
};

enum class gfxMappingDeclare
{
    CONSTANT_BUFFER,    // a buffer that should stay constant across many draws
    SHADER_RESOURCE,    // a buffer that should stay constant across its lifetime
    DYNAMIC_BUFFER,     // a mutable buffer, both by CPU and GPU logic
    SAMPLER,            // a sampler definition.
};

enum class gfxRootParameterType
{
    DESCRIPTOR_TABLE,   // multiple dynamically resolved declarations
    CONSTANT_BUFFER,    // a single constant buffer
    SHADER_RESOURCE,    // a single shader resource
    DYNAMIC_BUFFER,     // a single dynamic buffer
};

struct gfxDescriptorRange
{
    gfxMappingDeclare contentType;
    uint32 numPointers;
    uint32 baseShaderRegister;
    uint32 heapOffset;
};

enum class gfxPipelineVisibilityType
{
    ALL,
    VERTEX_SHADER,
    HULL_SHADER,
    DOMAIN_SHADER,
    GEOM_SHADER,
    PIXEL_SHADER
};

struct gfxRootParameter
{
    gfxRootParameterType slotType;
    union
    {
        struct  // the default way to map resource usage
        {
            uint32 numRanges;
            const gfxDescriptorRange *ranges;
        } descriptor_table;
        struct  // limited mapping using inline 32bit values
        {
            uint32 shaderRegister;
            uint32 numValues;
        } constants;
        struct  // a pointer that is stored in the root signature
        {
            uint32 shaderRegister;
        } descriptor;
    };
    gfxPipelineVisibilityType visibility;
};

struct gfxStaticSampler
{
    inline gfxStaticSampler( void )
    {
        this->filterMode = RWFILTER_POINT;
        this->uAddressing = RWTEXADDRESS_WRAP;
        this->vAddressing = RWTEXADDRESS_WRAP;
        this->wAddressing = RWTEXADDRESS_WRAP;
        this->mipLODBias = 0.0f;
        this->maxAnisotropy = 16;
        this->minLOD = std::numeric_limits <float>::min();
        this->maxLOD = std::numeric_limits <float>::max();
        this->shaderRegister = 0;
        this->visibility = gfxPipelineVisibilityType::ALL;
    }

    eRasterStageFilterMode filterMode;
    eRasterStageAddressMode uAddressing;
    eRasterStageAddressMode vAddressing;
    eRasterStageAddressMode wAddressing;
    float mipLODBias;
    uint32 maxAnisotropy;
    float minLOD;
    float maxLOD;
    uint32 shaderRegister;
    gfxPipelineVisibilityType visibility;
};

enum gfxPipelineAccess : uint32
{
    GFXACCESS_NONE = 0x0,
    GFXACCESS_INPUT_ASSEMBLER = 0x1,
    GFXACCESS_VERTEX_SHADER = 0x2,
    GFXACCESS_HULL_SHADER = 0x4,
    GFXACCESS_DOMAIN_SHADER = 0x8,
    GFXACCESS_GEOMETRY_SHADER = 0x10,
    GFXACCESS_PIXEL_SHADER = 0x20
};

// Register mappings of GPU programs.
struct gfxRegisterMapping
{
    inline gfxRegisterMapping( void )
    {
        this->numMappings = 0;
        this->mappings = nullptr;
        this->numStaticSamplers = 0;
        this->staticSamplers = nullptr;
        this->accessBitfield = GFXACCESS_NONE;
    }

    uint32 numMappings;
    const gfxRootParameter *mappings;
    uint32 numStaticSamplers;
    const gfxStaticSampler *staticSamplers;
    uint32 accessBitfield;  // bitfield combination of gfxPipelineAccess enum
};

struct gfxProgramBytecode
{
    inline gfxProgramBytecode( void )
    {
        this->buf = nullptr;
        this->memSize = 0;
    }

    const void *buf;
    size_t memSize;
};

struct gfxInputLayout
{
    inline gfxInputLayout( void )
    {
        this->paramCount = 0;
        this->params = nullptr;
    }

    uint32 paramCount;
    const vertexAttrib *params;
};

// Struct that defines a compiled graphics state.
struct gfxGraphicsState
{
    inline gfxGraphicsState( void )
    {
        this->sampleMask = std::numeric_limits <uint32>::max();
        this->topologyType = ePrimitiveTopologyType::TRIANGLE;
        this->numRenderTargets = 0;
        
        for ( size_t n = 0; n < 8; n++ )
        {
            this->renderTargetFormats[ n ] = gfxRasterFormat::UNSPECIFIED;
        }

        this->depthStencilFormat = gfxRasterFormat::UNSPECIFIED;
    }

    // Dynamic programmable states.
    gfxRegisterMapping regMapping;
    gfxProgramBytecode VS;
    gfxProgramBytecode PS;
    gfxProgramBytecode DS;
    gfxProgramBytecode HS;
    gfxProgramBytecode GS;

    // Fixed function states.
    gfxBlendState blendState;
    uint32 sampleMask;
    gfxRasterizerState rasterizerState;
    gfxDepthStencilState depthStencilState;
    gfxInputLayout inputLayout;
    ePrimitiveTopologyType topologyType;
    uint32 numRenderTargets;
    gfxRasterFormat renderTargetFormats[8];
    gfxRasterFormat depthStencilFormat;
};

// Instanced object types.
typedef void* DriverRaster;
typedef void* DriverGeometry;
typedef void* DriverMaterial;

struct Driver;

// Native resource pointer managed by CPU or GPU.
typedef rw::uint64 DriverPtr;

struct DriverGraphicsState
{
    inline DriverGraphicsState( Driver *theDriver )
    {
        this->driver = theDriver;
    }

    inline ~DriverGraphicsState( void )
    {
        return;
    }

    void* GetNativeHandle( void );

    void* GetImplementation( void );

private:
    Driver *driver;
};

struct DriverSwapChain
{
    friend struct Driver;

    inline DriverSwapChain( Interface *engineInterface, Driver *theDriver, Window *ownedWindow )
    {
        AcquireObject( ownedWindow );

        this->driver = theDriver;
        this->ownedWindow = ownedWindow;
    }

    inline ~DriverSwapChain( void )
    {
        ReleaseObject( this->ownedWindow );
    }

    Driver* GetDriver( void ) const
    {
        return this->driver;
    }

    // Returns the window that this swap chain is attached to.
    Window* GetAssociatedWindow( void ) const
    {
        return this->ownedWindow;
    }

    // Ability to lock and unlock the back-buffers for usage.
    // Note that this does not exclude multiple-writers.
    rwlock* GetUsageLock( void );

    void Present( bool vsync );
    unsigned int GetBackBufferIndex( void );

    DriverPtr GetBackBufferViewPointer( unsigned int bufIdx );

    void* GetImplementation( void );

private:
    Driver *driver;
    Window *ownedWindow;
};

enum class eCmdAllocType
{
    DIRECT  // made to allocate for graphics, compute and copy command buffers.
};

// Command allocators are not thread-safe.
struct DriverCmdAlloc
{
    friend struct Driver;

    inline DriverCmdAlloc( Driver *theDriver, eCmdAllocType allocType )
    {
        this->theDriver = theDriver;
        this->allocType = allocType;
    }

    inline ~DriverCmdAlloc( void )
    {
        return;
    }

    // Command allocator API.
    bool Reset( void );

    void* GetImplementation( void );

private:
    Driver *theDriver;
    eCmdAllocType allocType;
};

enum class eCmdBufType
{
    GRAPHICS,
    COMPUTE,
    COPY
};

// Command buffers are not thread-safe.
// You can attach them to command allocators.
struct DriverCmdBuffer
{
    friend struct Driver;

    inline DriverCmdBuffer( Driver *theDriver, eCmdBufType bufType )
    {
        this->theDriver = theDriver;
    }

    inline ~DriverCmdBuffer( void )
    {
        return;
    }

    // Typical commands to push.
    //TODO.

    // Maintenance API.
    bool Close( void );
    bool Reset( DriverCmdAlloc *cmdAlloc, DriverGraphicsState *psoObj );
    void* GetNativeHandle( void );

    void* GetImplementation( void );

private:
    Driver *theDriver;
    eCmdBufType bufType;
};

struct DriverFence;

// Command queues for parallel GPU processing of command units.
struct DriverCmdQueue
{
    inline DriverCmdQueue( Driver *theDriver, eCmdBufType queueType )
    {
        this->theDriver = theDriver;
        this->queueType = queueType;
    }

    inline ~DriverCmdQueue( void )
    {
        return;
    }

    //TODO: add API.
    bool Signal( DriverFence *fence, uint64 valueToBe );
    bool ExecuteCmdBuffer( DriverCmdBuffer *cmdBuf );

    void* GetImplementation( void );
    
private:
    Driver *theDriver;
    eCmdBufType queueType;
};

#define RW_DRIVER_FENCE_NO_TIMEOUT          (0xFFFFFFFF)

// Fences for synchronization between GPU and CPU.
struct DriverFence
{
    inline DriverFence( Driver *theDriver )
    {
        this->theDriver = theDriver;
    }

    inline ~DriverFence( void )
    {
        return;
    }

    // Synchronization API.
    void Signal( uint64 newValue );
    uint64 GetCurrentValue( void );
    void SingleWaitForSignal( uint64 waitValue, uint32 timeOut );

    // Management API.
    void* GetNativeHandle( void );

    void* GetImplementation( void );

private:
    Driver *theDriver;
};

enum class eDriverMemType
{
    UPLOAD,
    DISCRETE
};

struct gpuMemProperties
{
    bool canHaveBuffers = true;
    bool canHaveSwapchains = false;
    bool canHaveRenderSurfaces = true;
    bool canHaveNonRenderSurfaces = true;
};

// GPU memory management handle.
struct DriverMemory
{
    inline DriverMemory( Driver *ownerDriver, eDriverMemType memType, const gpuMemProperties& props )
    {
        this->theDriver = ownerDriver;
        this->memType = memType;
        this->props = props;
    }

    inline ~DriverMemory( void )
    {
        return;
    }

    inline eDriverMemType GetMemoryType( void ) const
    {
        return this->memType;
    }

    // TODO: memory handle API.

    void* GetImplementation( void );

private:
    Driver *theDriver;
    eDriverMemType memType;
    gpuMemProperties props;
};

struct Driver
{
    inline Driver( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
    }

    inline Driver( const Driver& right )
    {
        this->engineInterface = right.engineInterface;
    }

    inline Interface* GetEngineInterface( void ) const
    {
        return this->engineInterface;
    }

    // Driver API.
    void* GetNativeHandle( void );

    // Capability API.
    bool SupportsPrimitiveTopology( ePrimitiveTopology topology ) const;
    bool SupportsVertexAttribute( eVertexAttribValueType attribType ) const;

    // Swap chains are used to draw render targets to a window.
    DriverSwapChain* CreateSwapChain( Window *outputWindow, uint32 frameCount, DriverCmdQueue *cmdQueue );
    void DestroySwapChain( DriverSwapChain *swapChain );

    // Graphics state management.
    DriverGraphicsState* CreateGraphicsState( const gfxGraphicsState& psoState );
    void DestroyGraphicsState( DriverGraphicsState *pso );

    // Command allocator management.
    DriverCmdAlloc* CreateCommandAllocator( eCmdAllocType allocType );
    void DestroyCommandAllocator( DriverCmdAlloc *allocMan );

    // Command buffer management.
    DriverCmdBuffer* CreateCommandBuffer( eCmdBufType bufType, DriverCmdAlloc *allocMan );
    void DestroyCommandBuffer( DriverCmdBuffer *cmdBuf );

    // Command queue management.
    DriverCmdQueue* CreateCommandQueue( eCmdBufType queueType, int priority );
    void DestroyCommandQueue( DriverCmdQueue *cmdQueue );

    // Fence management.
    DriverFence* CreateFence( uint64 initValue );
    void DestroyFence( DriverFence *fence );

    // GPU memory management.
    DriverMemory* AllocateMemory( eDriverMemType memType, size_t numBytes, const gpuMemProperties& memProps );
    void ReleaseMemory( DriverMemory *gpumem );

    // Object creation API.
    // This creates instanced objects to use for rendering.
    DriverRaster* CreateInstancedRaster( Raster *sysRaster );

    void* GetImplementation( void );

private:
    Interface *engineInterface;
};

Driver* CreateDriver( Interface *engineInterface, const char *typeName );
void DestroyDriver( Interface *engineInterface, Driver *theDriver );

} // namespace rw