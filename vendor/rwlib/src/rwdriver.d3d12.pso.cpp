/*****************************************************************************
*
*  PROJECT:     Magic-RW
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        src/rwdriver.d3d12.pso.cpp
*  PURPOSE:     D3D12 pipeline-state-object management.
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/magic-rw/
*
*****************************************************************************/

#include "StdInc.h"

#ifndef _COMPILE_FOR_LEGACY

#ifdef _WIN32

#include "rwdriver.d3d12.hxx"

#include "pluginutil.hxx"

namespace rw
{

inline bool getD3D12RootParameterType( gfxRootParameterType type, D3D12_ROOT_PARAMETER_TYPE& typeOut )
{
    if ( type == gfxRootParameterType::DESCRIPTOR_TABLE )
    {
        typeOut = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    }
    else if ( type == gfxRootParameterType::CONSTANT_BUFFER )
    {
        typeOut = D3D12_ROOT_PARAMETER_TYPE_CBV;
    }
    else if ( type == gfxRootParameterType::SHADER_RESOURCE )
    {
        typeOut = D3D12_ROOT_PARAMETER_TYPE_SRV;
    }
    else if ( type == gfxRootParameterType::DYNAMIC_BUFFER )
    {
        typeOut = D3D12_ROOT_PARAMETER_TYPE_UAV;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12DescriptorRangeType( gfxMappingDeclare type, D3D12_DESCRIPTOR_RANGE_TYPE& typeOut )
{
    if ( type == gfxMappingDeclare::CONSTANT_BUFFER )
    {
        typeOut = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    }
    else if ( type == gfxMappingDeclare::SHADER_RESOURCE )
    {
        typeOut = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }
    else if ( type == gfxMappingDeclare::DYNAMIC_BUFFER )
    {
        typeOut = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12ShaderVisibility( gfxPipelineVisibilityType type, D3D12_SHADER_VISIBILITY& typeOut )
{
    if ( type == gfxPipelineVisibilityType::ALL )
    {
        typeOut = D3D12_SHADER_VISIBILITY_ALL;
    }
    else if ( type == gfxPipelineVisibilityType::VERTEX_SHADER )
    {
        typeOut = D3D12_SHADER_VISIBILITY_VERTEX;
    }
    else if ( type == gfxPipelineVisibilityType::HULL_SHADER )
    {
        typeOut = D3D12_SHADER_VISIBILITY_HULL;
    }
    else if ( type == gfxPipelineVisibilityType::DOMAIN_SHADER )
    {
        typeOut = D3D12_SHADER_VISIBILITY_DOMAIN;
    }
    else if ( type == gfxPipelineVisibilityType::GEOM_SHADER )
    {
        typeOut = D3D12_SHADER_VISIBILITY_GEOMETRY;
    }
    else if ( type == gfxPipelineVisibilityType::PIXEL_SHADER )
    {
        typeOut = D3D12_SHADER_VISIBILITY_PIXEL;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12FilterType( eRasterStageFilterMode filterMode, bool& enableMipmapsOut, D3D12_FILTER& filterOut )
{
    if ( filterMode == RWFILTER_POINT )
    {
        filterOut = D3D12_FILTER_MIN_MAG_MIP_POINT;

        enableMipmapsOut = false;
    }
    else if ( filterMode == RWFILTER_LINEAR )
    {
        filterOut = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

        enableMipmapsOut = false;
    }
    else if ( filterMode == RWFILTER_POINT_POINT )
    {
        filterOut = D3D12_FILTER_MIN_MAG_MIP_POINT;

        enableMipmapsOut = true;
    }
    else if ( filterMode == RWFILTER_LINEAR_POINT )
    {
        filterOut = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;

        enableMipmapsOut = true;
    }
    else if ( filterMode == RWFILTER_POINT_LINEAR )
    {
        filterOut = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;

        enableMipmapsOut = true;
    }
    else if ( filterMode == RWFILTER_LINEAR_LINEAR )
    {
        filterOut = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

        enableMipmapsOut = true;
    }
    else if ( filterMode == RWFILTER_ANISOTROPY )
    {
        filterOut = D3D12_FILTER_ANISOTROPIC;

        enableMipmapsOut = true;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12TextureAddressMode( eRasterStageAddressMode mode, D3D12_TEXTURE_ADDRESS_MODE& modeOut )
{
    if ( mode == RWTEXADDRESS_WRAP )
    {
        modeOut = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
    else if ( mode == RWTEXADDRESS_MIRROR )
    {
        modeOut = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }
    else if ( mode == RWTEXADDRESS_CLAMP )
    {
        modeOut = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
    else if ( mode == RWTEXADDRESS_BORDER )
    {
        modeOut = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12ComparisonFunc( rwCompareOpState cmpOp, D3D12_COMPARISON_FUNC& cmpOpOut )
{
    if ( cmpOp == RWCMP_NEVER )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_NEVER;
    }
    else if ( cmpOp == RWCMP_LESS )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_LESS;
    }
    else if ( cmpOp == RWCMP_EQUAL )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_EQUAL;
    }
    else if ( cmpOp == RWCMP_LESSEQUAL )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }
    else if ( cmpOp == RWCMP_GREATER )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_GREATER;
    }
    else if ( cmpOp == RWCMP_NOTEQUAL )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_NOT_EQUAL;
    }
    else if ( cmpOp == RWCMP_GREATEREQUAL )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    }
    else if ( cmpOp == RWCMP_ALWAYS )
    {
        cmpOpOut = D3D12_COMPARISON_FUNC_ALWAYS;
    }
    else
    {
        return false;
    }

    return true;
}

inline void InternErrorIfFailed( bool success, const wchar_t *errorToken )
{
    if ( !success )
    {
        throw DriverInternalErrorException( "Direct3D12", errorToken );
    }
}

ComPtr <ID3D12RootSignature> createRootSignature( d3d12DriverInterface *env, d3d12DriverInterface::d3d12NativeDriver *driver, const gfxGraphicsState& psoState )
{
    d3d12DriverInterface::D3D12SerializeRootSignature_t rootSigFunc = env->D3D12SerializeRootSignature;

    if ( rootSigFunc == nullptr )
    {
        return nullptr;
    }

    ComPtr <ID3D12RootSignature> rootSigOut = nullptr;

    // We first need to create our root signature.
    uint32 numRootParams = psoState.regMapping.numMappings;
    uint32 numStaticSamplers = psoState.regMapping.numStaticSamplers;

    D3D12_ROOT_PARAMETER *rootParams = nullptr;
    {
        if ( numRootParams != 0 )
        {
            rootParams = new D3D12_ROOT_PARAMETER[ numRootParams ];
        }
    }
    D3D12_STATIC_SAMPLER_DESC *staticSamplers = nullptr;
    {
        if ( numStaticSamplers != 0 )
        {
            staticSamplers = new D3D12_STATIC_SAMPLER_DESC[ numStaticSamplers ];
        }
    }

    // Possible sub allocations.
    rwStaticVector <D3D12_DESCRIPTOR_RANGE*> descriptorRangePtrs;

    if ( ( numRootParams == 0 || rootParams ) && ( numStaticSamplers == 0 || staticSamplers ) )
    {
        try
        {
            // First the root parameters.
            for ( uint32 n = 0; n < numRootParams; n++ )
            {
                const gfxRootParameter& srcParam = psoState.regMapping.mappings[ n ];

                gfxRootParameterType paramType = srcParam.slotType;

                D3D12_ROOT_PARAMETER& param = rootParams[ n ];

                InternErrorIfFailed(
                    getD3D12RootParameterType( paramType, param.ParameterType ),
                    L"D3D12_PSO_INTERNERR_ROOTPARAMMAP"
                );

                if ( paramType == gfxRootParameterType::DESCRIPTOR_TABLE )
                {
                    uint32 numRanges = srcParam.descriptor_table.numRanges;

                    D3D12_DESCRIPTOR_RANGE *descriptorRanges = new D3D12_DESCRIPTOR_RANGE[ numRanges ];

                    for ( uint32 range_iter = 0; range_iter < numRanges; range_iter++ )
                    {
                        const gfxDescriptorRange& srcRange = srcParam.descriptor_table.ranges[ range_iter ];

                        D3D12_DESCRIPTOR_RANGE& curRange = descriptorRanges[ range_iter ];

                        InternErrorIfFailed(
                            getD3D12DescriptorRangeType( srcRange.contentType, curRange.RangeType ),
                            L"D3D12_PSO_INTERNERR_DESCRANGE"
                        );

                        curRange.NumDescriptors = srcRange.numPointers;
                        curRange.BaseShaderRegister = srcRange.baseShaderRegister;
                        curRange.RegisterSpace = 0;
                        curRange.OffsetInDescriptorsFromTableStart = srcRange.heapOffset;
                    }

                    param.DescriptorTable.NumDescriptorRanges = numRanges;
                    param.DescriptorTable.pDescriptorRanges = descriptorRanges;

                    descriptorRangePtrs.AddToBack( descriptorRanges );
                }
                else if ( paramType == gfxRootParameterType::CONSTANT_BUFFER ||
                          paramType == gfxRootParameterType::SHADER_RESOURCE ||
                          paramType == gfxRootParameterType::DYNAMIC_BUFFER )
                {
                    param.Descriptor.ShaderRegister = srcParam.descriptor.shaderRegister;
                    param.Descriptor.RegisterSpace = 0;
                }
                else
                {
                    assert( 0 );
                }

                InternErrorIfFailed(
                    getD3D12ShaderVisibility( srcParam.visibility, param.ShaderVisibility ),
                    L"D3D12_PSO_INTERNERR_ROOTPARAM_SHADERVIS"
                );
            }

            // Now the static samplers.
            for ( uint32 n = 0; n < numStaticSamplers; n++ )
            {
                const gfxStaticSampler& srcSampler = psoState.regMapping.staticSamplers[ n ];

                D3D12_STATIC_SAMPLER_DESC& sampler = staticSamplers[ n ];

                bool enableMipmaps;

                InternErrorIfFailed(
                    getD3D12FilterType( srcSampler.filterMode, enableMipmaps, sampler.Filter ),
                    L"D3D12_PSO_INTERNERR_STATICSAMP_FILTERMODE"
                );

                // Addressing modes.
                InternErrorIfFailed(
                    getD3D12TextureAddressMode( srcSampler.uAddressing, sampler.AddressU ),
                    L"D3D12_PSO_INTERNERR_STATICSAMP_UADDRMODE"
                );
                InternErrorIfFailed(
                    getD3D12TextureAddressMode( srcSampler.vAddressing, sampler.AddressV ),
                    L"D3D12_PSO_INTERNERR_STATICSAMP_VADDRMODE"
                );
                InternErrorIfFailed(
                    getD3D12TextureAddressMode( srcSampler.wAddressing, sampler.AddressW ),
                    L"D3D12_PSO_INTERNERR_STATICSAMP_WADDRMODE"
                );

                sampler.MipLODBias = srcSampler.mipLODBias;
                sampler.MaxAnisotropy = srcSampler.maxAnisotropy;
                sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
                sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
                sampler.MinLOD = srcSampler.minLOD;
                sampler.MaxLOD = srcSampler.maxLOD;
                sampler.ShaderRegister = srcSampler.shaderRegister;
                sampler.RegisterSpace = 0;

                InternErrorIfFailed(
                    getD3D12ShaderVisibility( srcSampler.visibility, sampler.ShaderVisibility ),
                    L"D3D12_PSO_INTERNERR_STATICSAMP_SHADERVIS"
                );
            }

            D3D12_ROOT_SIGNATURE_DESC rootSig;
            rootSig.NumParameters = numRootParams;
            rootSig.pParameters = rootParams;
            rootSig.NumStaticSamplers = numStaticSamplers;
            rootSig.pStaticSamplers = staticSamplers;
            rootSig.Flags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr <ID3DBlob> blob;
            ComPtr <ID3DBlob> errorBlob;

            HRESULT serializeSuccess =
                rootSigFunc(
                    &rootSig, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errorBlob
                );

            if ( !SUCCEEDED( serializeSuccess ) )
            {
                // Try to tell the user about any errors.
                rwStaticString <char> errorMsg;

                if ( ID3DBlob *blob = errorBlob.Get() )
                {
                    errorMsg = rwStaticString <char> ( (const char*)blob->GetBufferPointer(), blob->GetBufferSize() );
                }

                throw DriverUnlocalizedAInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_SERROOTSIGFAIL", std::move( errorMsg ) );
            }

            // We got a winner!
            HRESULT rootSigCreateSuccess = driver->m_device->CreateRootSignature(
                0, blob->GetBufferPointer(), blob->GetBufferSize(),
                IID_PPV_ARGS( &rootSigOut )
            );

            if ( !SUCCEEDED( rootSigCreateSuccess ) )
            {
                throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_ROOTSIGFAIL" );
            }
        }
        catch( ... )
        {
            for ( size_t n = 0; n < descriptorRangePtrs.GetCount(); n++ )
            {
                delete descriptorRangePtrs[ n ];
            }

            delete staticSamplers;

            delete rootParams;

            throw;
        }
    }

    for ( size_t n = 0; n < descriptorRangePtrs.GetCount(); n++ )
    {
        delete descriptorRangePtrs[ n ];
    }

    if ( rootParams )
    {
        delete rootParams;
    }

    if ( staticSamplers )
    {
        delete staticSamplers;
    }

    return rootSigOut;
}

inline bool getD3D12BlendModulator( rwBlendModeState blend, D3D12_BLEND& blendOut )
{
    if ( blend == RWBLEND_ZERO )
    {
        blendOut = D3D12_BLEND_ZERO;
    }
    else if ( blend == RWBLEND_ONE )
    {
        blendOut = D3D12_BLEND_ONE;
    }
    else if ( blend == RWBLEND_SRCCOLOR )
    {
        blendOut = D3D12_BLEND_SRC_COLOR;
    }
    else if ( blend == RWBLEND_INVSRCCOLOR )
    {
        blendOut = D3D12_BLEND_INV_SRC_COLOR;
    }
    else if ( blend == RWBLEND_SRCALPHA )
    {
        blendOut = D3D12_BLEND_SRC_ALPHA;
    }
    else if ( blend == RWBLEND_INVSRCALPHA )
    {
        blendOut = D3D12_BLEND_INV_SRC_ALPHA;
    }
    else if ( blend == RWBLEND_DESTALPHA )
    {
        blendOut = D3D12_BLEND_DEST_ALPHA;
    }
    else if ( blend == RWBLEND_INVDESTALPHA )
    {
        blendOut = D3D12_BLEND_INV_DEST_ALPHA;
    }
    else if ( blend == RWBLEND_DESTCOLOR )
    {
        blendOut = D3D12_BLEND_DEST_COLOR;
    }
    else if ( blend == RWBLEND_INVDESTCOLOR )
    {
        blendOut = D3D12_BLEND_INV_DEST_COLOR;
    }
    else if ( blend == RWBLEND_SRCALPHASAT )
    {
        blendOut = D3D12_BLEND_SRC_ALPHA_SAT;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12BlendOp( rwBlendOp blendOp, D3D12_BLEND_OP& blendOpOut )
{
    if ( blendOp == RWBLENDOP_ADD )
    {
        blendOpOut = D3D12_BLEND_OP_ADD;
    }
    else if ( blendOp == RWBLENDOP_SUBTRACT )
    {
        blendOpOut = D3D12_BLEND_OP_SUBTRACT;
    }
    else if ( blendOp == RWBLENDOP_REV_SUBTRACT )
    {
        blendOpOut = D3D12_BLEND_OP_REV_SUBTRACT;
    }
    else if ( blendOp == RWBLENDOP_MIN )
    {
        blendOpOut = D3D12_BLEND_OP_MIN;
    }
    else if ( blendOp == RWBLENDOP_MAX )
    {
        blendOpOut = D3D12_BLEND_OP_MAX;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12LogicOp( rwLogicOp logicOp, D3D12_LOGIC_OP& logicOpOut )
{
    if ( logicOp == RWLOGICOP_CLEAR )
    {
        logicOpOut = D3D12_LOGIC_OP_CLEAR;
    }
    else if ( logicOp == RWLOGICOP_SET )
    {
        logicOpOut = D3D12_LOGIC_OP_SET;
    }
    else if ( logicOp == RWLOGICOP_COPY )
    {
        logicOpOut = D3D12_LOGIC_OP_COPY;
    }
    else if ( logicOp == RWLOGICOP_COPY_INV )
    {
        logicOpOut = D3D12_LOGIC_OP_COPY_INVERTED;
    }
    else if ( logicOp == RWLOGICOP_NOOP )
    {
        logicOpOut = D3D12_LOGIC_OP_NOOP;
    }
    else if ( logicOp == RWLOGICOP_INVERT )
    {
        logicOpOut = D3D12_LOGIC_OP_INVERT;
    }
    else if ( logicOp == RWLOGICOP_AND )
    {
        logicOpOut = D3D12_LOGIC_OP_AND;
    }
    else if ( logicOp == RWLOGICOP_NAND )
    {
        logicOpOut = D3D12_LOGIC_OP_NAND;
    }
    else if ( logicOp == RWLOGICOP_OR )
    {
        logicOpOut = D3D12_LOGIC_OP_OR;
    }
    else if ( logicOp == RWLOGICOP_NOR )
    {
        logicOpOut = D3D12_LOGIC_OP_NOR;
    }
    else if ( logicOp == RWLOGICOP_XOR )
    {
        logicOpOut = D3D12_LOGIC_OP_XOR;
    }
    else if ( logicOp == RWLOGICOP_EQUIV )
    {
        logicOpOut = D3D12_LOGIC_OP_EQUIV;
    }
    else if ( logicOp == RWLOGICOP_AND_REVERSE )
    {
        logicOpOut = D3D12_LOGIC_OP_AND_REVERSE;
    }
    else if ( logicOp == RWLOGICOP_AND_INV )
    {
        logicOpOut = D3D12_LOGIC_OP_AND_INVERTED;
    }
    else if ( logicOp == RWLOGICOP_OR_REVERSE )
    {
        logicOpOut = D3D12_LOGIC_OP_OR_REVERSE;
    }
    else if ( logicOp == RWLOGICOP_OR_INV )
    {
        logicOpOut = D3D12_LOGIC_OP_OR_INVERTED;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12FillMode( rwRenderFillMode fillMode, D3D12_FILL_MODE& fillModeOut )
{
    if ( fillMode == RWFILLMODE_WIREFRAME )
    {
        fillModeOut = D3D12_FILL_MODE_WIREFRAME;
    }
    else if ( fillMode = RWFILLMODE_SOLID )
    {
        fillModeOut = D3D12_FILL_MODE_SOLID;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12CullMode( rwCullModeState cullMode, bool& frontIsClockwiseOut, D3D12_CULL_MODE& cullModeOut )
{
    frontIsClockwiseOut = true;

    if ( cullMode == RWCULL_CLOCKWISE )
    {
        cullModeOut = D3D12_CULL_MODE_FRONT;
    }
    else if ( cullMode == RWCULL_COUNTERCLOCKWISE )
    {
        cullModeOut = D3D12_CULL_MODE_BACK;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12StencilOp( rwStencilOpState stencilOp, D3D12_STENCIL_OP& stencilOpOut )
{
    if ( stencilOp == RWSTENCIL_KEEP )
    {
        stencilOpOut = D3D12_STENCIL_OP_KEEP;
    }
    else if ( stencilOp == RWSTENCIL_ZERO )
    {
        stencilOpOut = D3D12_STENCIL_OP_ZERO;
    }
    else if ( stencilOp == RWSTENCIL_REPLACE )
    {
        stencilOpOut = D3D12_STENCIL_OP_REPLACE;
    }
    else if ( stencilOp == RWSTENCIL_INCRSAT )
    {
        stencilOpOut = D3D12_STENCIL_OP_INCR_SAT;
    }
    else if ( stencilOp == RWSTENCIL_DECRSAT )
    {
        stencilOpOut = D3D12_STENCIL_OP_DECR_SAT;
    }
    else if ( stencilOp == RWSTENCIL_INVERT )
    {
        stencilOpOut = D3D12_STENCIL_OP_INVERT;
    }
    else if ( stencilOp == RWSTENCIL_INCR )
    {
        stencilOpOut = D3D12_STENCIL_OP_INCR;
    }
    else if ( stencilOp == RWSTENCIL_DECR )
    {
        stencilOpOut = D3D12_STENCIL_OP_DECR;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12VertexAttributeFormat( const vertexAttrib& attrib, DXGI_FORMAT& formatOut )
{
    eVertexAttribValueType valueType = attrib.attribType;
    uint32 valueCount = attrib.count;

    bool hasFormat = false;

    if ( valueType == eVertexAttribValueType::INT8 )
    {
        if ( valueCount == 1 )
        {
            formatOut = DXGI_FORMAT_R8_SINT;

            hasFormat = true;
        }
        else if ( valueCount == 4 )
        {
            formatOut = DXGI_FORMAT_R8G8B8A8_SINT;

            hasFormat = true;
        }
    }
    else if ( valueType == eVertexAttribValueType::UINT8 )
    {
        if ( valueCount == 1 )
        {
            formatOut = DXGI_FORMAT_R8_UINT;

            hasFormat = true;
        }
        else if ( valueCount == 4 )
        {
            formatOut = DXGI_FORMAT_R8G8B8A8_UINT;

            hasFormat = true;
        }
    }
    else if ( valueType == eVertexAttribValueType::INT16 )
    {
        if ( valueCount == 1 )
        {
            formatOut = DXGI_FORMAT_R16_SINT;

            hasFormat = true;
        }
        else if ( valueCount == 2 )
        {
            formatOut = DXGI_FORMAT_R16G16_SINT;

            hasFormat = true;
        }
        else if ( valueCount == 4 )
        {
            formatOut = DXGI_FORMAT_R16G16B16A16_SINT;

            hasFormat = true;
        }
    }
    else if ( valueType == eVertexAttribValueType::UINT16 )
    {
        if ( valueCount == 1 )
        {
            formatOut = DXGI_FORMAT_R16_UINT;

            hasFormat = true;
        }
        else if ( valueCount == 2 )
        {
            formatOut = DXGI_FORMAT_R16G16_UINT;

            hasFormat = true;
        }
        else if ( valueCount == 4 )
        {
            formatOut = DXGI_FORMAT_R16G16B16A16_UINT;

            hasFormat = true;
        }
    }
    else if ( valueType == eVertexAttribValueType::INT32 )
    {
        if ( valueCount == 1 )
        {
            formatOut = DXGI_FORMAT_R32_SINT;

            hasFormat = true;
        }
        else if ( valueCount == 2 )
        {
            formatOut = DXGI_FORMAT_R32G32_SINT;

            hasFormat = true;
        }
        else if ( valueCount == 3 )
        {
            formatOut = DXGI_FORMAT_R32G32B32_SINT;

            hasFormat = true;
        }
        else if ( valueCount == 4 )
        {
            formatOut = DXGI_FORMAT_R32G32B32A32_SINT;

            hasFormat = true;
        }
    }
    else if ( valueType == eVertexAttribValueType::UINT32 )
    {
        if ( valueCount == 1 )
        {
            formatOut = DXGI_FORMAT_R32_UINT;

            hasFormat = true;
        }
        else if ( valueCount == 2 )
        {
            formatOut = DXGI_FORMAT_R32G32_UINT;

            hasFormat = true;
        }
        else if ( valueCount == 3 )
        {
            formatOut = DXGI_FORMAT_R32G32B32_UINT;

            hasFormat = true;
        }
        else if ( valueCount == 4 )
        {
            formatOut = DXGI_FORMAT_R32G32B32A32_UINT;

            hasFormat = true;
        }
    }
    else if ( valueType == eVertexAttribValueType::FLOAT32 )
    {
        if ( valueCount == 1 )
        {
            formatOut = DXGI_FORMAT_R32_FLOAT;

            hasFormat = true;
        }
        else if ( valueCount == 2 )
        {
            formatOut = DXGI_FORMAT_R32G32_FLOAT;

            hasFormat = true;
        }
        else if ( valueCount == 3 )
        {
            formatOut = DXGI_FORMAT_R32G32B32_FLOAT;

            hasFormat = true;
        }
        else if ( valueCount == 4 )
        {
            formatOut = DXGI_FORMAT_R32G32B32A32_FLOAT;

            hasFormat = true;
        }
    }

    return hasFormat;
}

inline const char* getD3D12PipelineAttributeSemantic( eVertexAttribSemanticType semanticType )
{
    if ( semanticType == eVertexAttribSemanticType::POSITION )
    {
        return "POSITION";
    }
    else if ( semanticType == eVertexAttribSemanticType::COLOR )
    {
        return "COLOR";
    }
    else if ( semanticType == eVertexAttribSemanticType::NORMAL )
    {
        return "NORMAL";
    }
    else if ( semanticType == eVertexAttribSemanticType::TEXCOORD )
    {
        return "TEXCOORD";
    }

    return nullptr;
}

inline bool getD3D12PrimitiveTopologyType( ePrimitiveTopologyType topology, D3D12_PRIMITIVE_TOPOLOGY_TYPE& topologyOut )
{
    if ( topology == ePrimitiveTopologyType::POINT )
    {
        topologyOut = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    }
    else if ( topology == ePrimitiveTopologyType::LINE )
    {
        topologyOut = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    }
    else if ( topology == ePrimitiveTopologyType::TRIANGLE )
    {
        topologyOut = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
    else if ( topology == ePrimitiveTopologyType::PATCH )
    {
        topologyOut = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    }
    else
    {
        return false;
    }

    return true;
}

inline bool getD3D12RasterFormat( gfxRasterFormat format, DXGI_FORMAT& fmtOut )
{
    if ( format == gfxRasterFormat::UNSPECIFIED )
    {
        fmtOut = DXGI_FORMAT_UNKNOWN;
    }
    else if ( format == gfxRasterFormat::R32G32B32A32_TYPELESS )
    {
        fmtOut = DXGI_FORMAT_R32G32B32A32_TYPELESS;
    }
    else if ( format == gfxRasterFormat::R32G32B32A32_FLOAT )
    {
        fmtOut = DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    else if ( format == gfxRasterFormat::R32G32B32A32_UINT )
    {
        fmtOut = DXGI_FORMAT_R32G32B32A32_UINT;
    }
    else if ( format == gfxRasterFormat::R32G32B32A32_SINT )
    {
        fmtOut = DXGI_FORMAT_R32G32B32A32_SINT;
    }
    else if ( format == gfxRasterFormat::R32G32B32_TYPELESS )
    {
        fmtOut = DXGI_FORMAT_R32G32B32_TYPELESS;
    }
    else if ( format == gfxRasterFormat::R32G32B32_FLOAT )
    {
        fmtOut = DXGI_FORMAT_R32G32B32_FLOAT;
    }
    else if ( format == gfxRasterFormat::R32G32B32_UINT )
    {
        fmtOut = DXGI_FORMAT_R32G32B32_UINT;
    }
    else if ( format == gfxRasterFormat::R32G32B32_SINT )
    {
        fmtOut = DXGI_FORMAT_R32G32B32_SINT;
    }
    else if ( format == gfxRasterFormat::R8G8B8A8 )
    {
        fmtOut = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else if ( format == gfxRasterFormat::B5G6R5 )
    {
        fmtOut = DXGI_FORMAT_B5G6R5_UNORM;
    }
    else if ( format == gfxRasterFormat::B5G5R5A1 )
    {
        fmtOut = DXGI_FORMAT_B5G5R5A1_UNORM;
    }
    else if ( format == gfxRasterFormat::LUM8 )
    {
        fmtOut = DXGI_FORMAT_R8_UNORM;
    }
    else if ( format == gfxRasterFormat::DEPTH16 )
    {
        fmtOut = DXGI_FORMAT_D16_UNORM;
    }
    else if ( format == gfxRasterFormat::DEPTH24 )
    {
        fmtOut = DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
    else if ( format == gfxRasterFormat::DEPTH32 )
    {
        fmtOut = DXGI_FORMAT_D32_FLOAT;
    }
    else if ( format == gfxRasterFormat::DXT1 )
    {
        fmtOut = DXGI_FORMAT_BC1_UNORM;
    }
    else if ( format == gfxRasterFormat::DXT3 )
    {
        fmtOut = DXGI_FORMAT_BC2_UNORM;
    }
    else if ( format == gfxRasterFormat::DXT5 )
    {
        fmtOut = DXGI_FORMAT_BC3_UNORM;
    }
    else
    {
        return false;
    }

    return true;
}

d3d12DriverInterface::d3d12PipelineStateObject::d3d12PipelineStateObject( d3d12DriverInterface *env, Interface *engineInterface, d3d12NativeDriver *driver, const gfxGraphicsState& psoState )
{
    // Create Direct3D 12 graphics state based on framework parameters.
    // This is the age of bundled state objects, man.

    // We need a root signature.
    this->rootSignature = createRootSignature( env, driver, psoState );

    D3D12_GRAPHICS_PIPELINE_STATE_DESC stateDesc;
    stateDesc.pRootSignature = this->rootSignature.Get();
    stateDesc.VS.pShaderBytecode = psoState.VS.buf;
    stateDesc.VS.BytecodeLength = psoState.VS.memSize;
    stateDesc.PS.pShaderBytecode = psoState.PS.buf;
    stateDesc.PS.BytecodeLength = psoState.PS.memSize;
    stateDesc.DS.pShaderBytecode = psoState.DS.buf;
    stateDesc.DS.BytecodeLength = psoState.DS.memSize;
    stateDesc.HS.pShaderBytecode = psoState.HS.buf;
    stateDesc.HS.BytecodeLength = psoState.HS.memSize;
    stateDesc.GS.pShaderBytecode = psoState.GS.buf;
    stateDesc.GS.BytecodeLength = psoState.GS.memSize;

    // We do not support streaming output stuff.
    stateDesc.StreamOutput.pSODeclaration = nullptr;
    stateDesc.StreamOutput.NumEntries = 0;
    stateDesc.StreamOutput.pBufferStrides = nullptr;
    stateDesc.StreamOutput.NumStrides = 0;
    stateDesc.StreamOutput.RasterizedStream = 0;

    stateDesc.BlendState.AlphaToCoverageEnable = false;
    stateDesc.BlendState.IndependentBlendEnable = false;
    {
        const gfxBlendState& srcRtBlend = psoState.blendState;

        D3D12_RENDER_TARGET_BLEND_DESC& rtBlend = stateDesc.BlendState.RenderTarget[ 0 ];
        rtBlend.BlendEnable = srcRtBlend.enableBlend;
        rtBlend.LogicOpEnable = srcRtBlend.enableLogicOp;

        InternErrorIfFailed(
            getD3D12BlendModulator( srcRtBlend.srcBlend, rtBlend.SrcBlend ),
            L"D3D12_PSO_INTERNERR_SOURCEBLENDMOD"
        );
        InternErrorIfFailed(
            getD3D12BlendModulator( srcRtBlend.dstBlend, rtBlend.DestBlend ),
            L"D3D12_PSO_INTERNERR_DSTBLENDMOD"
        );
        InternErrorIfFailed(
            getD3D12BlendOp( srcRtBlend.blendOp, rtBlend.BlendOp ),
            L"D3D12_PSO_INTERNERR_COLORBLENDOP"
        );

        InternErrorIfFailed(
            getD3D12BlendModulator( srcRtBlend.srcAlphaBlend, rtBlend.SrcBlendAlpha ),
            L"D3D12_PSO_INTERNERR_ALPHASOURCEBLEND"
        );
        InternErrorIfFailed(
            getD3D12BlendModulator( srcRtBlend.dstAlphaBlend, rtBlend.DestBlendAlpha ),
            L"D3D12_PSO_INTERNERR_ALPHADSTBLEND"
        );
        InternErrorIfFailed(
            getD3D12BlendOp( srcRtBlend.alphaBlendOp, rtBlend.BlendOpAlpha ),
            L"D3D12_PSO_INTERNERR_ALPHABLENDOP"
        );

        InternErrorIfFailed(
            getD3D12LogicOp( srcRtBlend.logicOp, rtBlend.LogicOp ),
            L"D3D12_PSO_INTERNERR_LOGICOP"
        );

        rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

    stateDesc.SampleMask = psoState.sampleMask;

    // Rasterizer state.
    {
        const gfxRasterizerState& srcRasterizer = psoState.rasterizerState;

        D3D12_RASTERIZER_DESC& rasterizer = stateDesc.RasterizerState;

        InternErrorIfFailed(
            getD3D12FillMode( srcRasterizer.fillMode, rasterizer.FillMode ),
            L"D3D12_PSO_INTERNERR_RASFILLMODE"
        );

        bool frontCounterClockwise;

        InternErrorIfFailed(
            getD3D12CullMode( srcRasterizer.cullMode, frontCounterClockwise, rasterizer.CullMode ),
            L"D3D12_PSO_INTERNERR_RASCULLMODE"
        );

        rasterizer.FrontCounterClockwise = frontCounterClockwise;
        rasterizer.DepthBias = srcRasterizer.depthBias;
        rasterizer.DepthBiasClamp = srcRasterizer.depthBiasClamp;
        rasterizer.SlopeScaledDepthBias = srcRasterizer.slopeScaledDepthBias;
        rasterizer.DepthClipEnable = true;
        rasterizer.MultisampleEnable = false;
        rasterizer.AntialiasedLineEnable = true;
        rasterizer.ForcedSampleCount = 0;
        rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }

    // Depth stencil state.
    {
        const gfxDepthStencilState& srcDepthStencil = psoState.depthStencilState;

        D3D12_DEPTH_STENCIL_DESC& depthStencil = stateDesc.DepthStencilState;

        depthStencil.DepthEnable = srcDepthStencil.enableDepthTest;
        depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

        InternErrorIfFailed(
            getD3D12ComparisonFunc( srcDepthStencil.depthFunc, depthStencil.DepthFunc ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_DEPTHFUNC"
        );

        depthStencil.StencilEnable = srcDepthStencil.enableStencilTest;
        depthStencil.StencilReadMask = srcDepthStencil.stencilReadMask;
        depthStencil.StencilWriteMask = srcDepthStencil.stencilWriteMask;

        // FRONT FACE
        InternErrorIfFailed(
            getD3D12StencilOp( srcDepthStencil.frontFace.failOp, depthStencil.FrontFace.StencilFailOp ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FFACEFAILOP"
        );
        InternErrorIfFailed(
            getD3D12StencilOp( srcDepthStencil.frontFace.depthFailOp, depthStencil.FrontFace.StencilDepthFailOp ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FFACEDFAILOP"
        );
        InternErrorIfFailed(
            getD3D12StencilOp( srcDepthStencil.frontFace.passOp, depthStencil.FrontFace.StencilPassOp ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FFACEPASSOP"
        );
        InternErrorIfFailed(
            getD3D12ComparisonFunc( srcDepthStencil.frontFace.func, depthStencil.FrontFace.StencilFunc ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FFACEFUNC"
        );

        // BACK FACE.
        InternErrorIfFailed(
            getD3D12StencilOp( srcDepthStencil.backFace.failOp, depthStencil.BackFace.StencilFailOp ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FBACKFAILOP"
        );
        InternErrorIfFailed(
            getD3D12StencilOp( srcDepthStencil.backFace.depthFailOp, depthStencil.BackFace.StencilDepthFailOp ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FBACKDFAILOP"
        );
        InternErrorIfFailed(
            getD3D12StencilOp( srcDepthStencil.backFace.passOp, depthStencil.BackFace.StencilPassOp ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FBACKPASSOP"
        );
        InternErrorIfFailed(
            getD3D12ComparisonFunc( srcDepthStencil.backFace.func, depthStencil.BackFace.StencilFunc ),
            L"D3D12_PSO_INTERNERR_DSTENCIL_FBACKFUNC"
        );
    }

    // Temporary values of input stage.
    D3D12_INPUT_ELEMENT_DESC *inputElemArray = nullptr;

    try
    {
        // Input layout.
        {
            const gfxInputLayout& srcInputLayout = psoState.inputLayout;

            D3D12_INPUT_LAYOUT_DESC& inputLayout = stateDesc.InputLayout;

            uint32 numInputElems = srcInputLayout.paramCount;

            if ( numInputElems != 0 )
            {
                inputElemArray = new D3D12_INPUT_ELEMENT_DESC[ numInputElems ];

                for ( uint32 n = 0; n < numInputElems; n++ )
                {
                    const vertexAttrib& srcVertexAttrib = srcInputLayout.params[ n ];

                    D3D12_INPUT_ELEMENT_DESC& inputElem = inputElemArray[ n ];

                    inputElem.SemanticName =
                        getD3D12PipelineAttributeSemantic( srcVertexAttrib.semanticType );

                    if ( inputElem.SemanticName == nullptr )
                    {
                        throw DriverInternalErrorException( "Direct3D12", L"D3D12_PSO_INTERNERR_MAPVERTEXATTRSEM" );
                    }

                    inputElem.SemanticIndex = srcVertexAttrib.semanticIndex;

                    InternErrorIfFailed(
                        getD3D12VertexAttributeFormat( srcVertexAttrib, inputElem.Format ),
                        L"D3D12_PSO_INTERNERR_VERTEXATTR_MAPDXGI"
                    );

                    inputElem.InputSlot = 0;
                    inputElem.AlignedByteOffset = srcVertexAttrib.alignedByteOffset;
                    inputElem.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                    inputElem.InstanceDataStepRate = 0;
                }
            }

            stateDesc.InputLayout.NumElements = numInputElems;
            stateDesc.InputLayout.pInputElementDescs = inputElemArray;
        }

        stateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        InternErrorIfFailed(
            getD3D12PrimitiveTopologyType( psoState.topologyType, stateDesc.PrimitiveTopologyType ),
            L"D3D12_PSO_INTERNERR_TOPOLOGYMAP"
        );

        stateDesc.NumRenderTargets = psoState.numRenderTargets;

        for ( uint32 n = 0; n < _countof(psoState.renderTargetFormats); n++ )
        {
            InternErrorIfFailed(
                getD3D12RasterFormat( psoState.renderTargetFormats[ n ], stateDesc.RTVFormats[ n ] ),
                L"D3D12_PSO_INTERNERR_RTVIEWFMTMAP"
            );
        }

        InternErrorIfFailed(
            getD3D12RasterFormat( psoState.depthStencilFormat, stateDesc.DSVFormat ),
            L"D3D12_PSO_INTERNERR_DSTENCILVIEWFMTMAP"
        );

        stateDesc.SampleDesc.Count = 1;
        stateDesc.SampleDesc.Quality = 0;
        stateDesc.NodeMask = 0;     // Single GPU :-)
        stateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
        stateDesc.CachedPSO.pCachedBlob = nullptr;
        stateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        // Finally, let us create this thing.
        HRESULT psoCreationSuccess =
            driver->m_device->CreateGraphicsPipelineState( &stateDesc, IID_PPV_ARGS(&psoPtr) );

        if ( !SUCCEEDED( psoCreationSuccess ) )
        {
            throw DriverInternalErrorException( "Direct3D12", L"D3D12_INTERNERR_PSOFAIL" );
        }
    }
    catch( ... )
    {
        if ( inputElemArray )
        {
            delete inputElemArray;
        }

        throw;
    }

    if ( inputElemArray )
    {
        delete inputElemArray;
    }
}

d3d12DriverInterface::d3d12PipelineStateObject::~d3d12PipelineStateObject( void )
{
    // Cleaned up automatically.
}

};

#endif //_WIN32

#endif //_COMPILE_FOR_LEGACY
