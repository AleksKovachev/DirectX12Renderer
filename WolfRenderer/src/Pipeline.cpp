#include "d3dx12_core.h"
#include "Logger.hpp"
#include "Pipeline.hpp"
#include "utils.hpp"

#include "DefaultPS.hlsl.h"
#include "DefaultVS.hlsl.h"
#include "EdgesPassPS.hlsl.h"
#include "ShadowPassVS.hlsl.h"
#include "VertexPassGS.hlsl.h"
#include "VertexPassPS.hlsl.h"



namespace Core {
	Pipeline::Pipeline( ComPtr<ID3D12Device14> device, AppData* appData )
		: m_device{ device }, m_app{ appData } {}

	void Pipeline::CreatePipelineStates() {
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_graphics_pipeline_state_desc
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_rasterizer_desc
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_depth_stencil_desc
			// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_input_layout_desc

			// This tells the rasterizer to take 2D or 3D vertex data.
			// Use DXGI_FORMAT_R32G32_FLOAT for 2D and DXGI_FORMAT_R32G32B32_FLOAT for 3D.
			// VSInput's position parameter type must reflect this type: float2 / float3.
		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

			// Required for normals and lighting. APPEND... calculates offset
			// based on previous element. In this case, it's 12 (R+G+B) = (4+4+4) = 12bytes.
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		/////////////////////////////////////////////////////////////////
		//                  Default (Faces) + No Cull                  //
		/////////////////////////////////////////////////////////////////

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = rootSignatureDefault.Get();
		psoDesc.PS = { g_default_ps, _countof( g_default_ps ) };
		psoDesc.VS = { g_default_vs, _countof( g_default_vs ) };
		psoDesc.InputLayout = { inputLayout, _countof( inputLayout ) };
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		psoDesc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT ); // DepthEnable = TRUE
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		psoDesc.DSVFormat = m_depthFormat;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		/* Number of color buffers.Use more for Deferred Rendering or G - Buffers.
		Allows for color output ar index 0, normals at index 1, world positions
		at index 2 in a single draw call. */
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1; // MSAA multiplier. 1x - No MSAA. (2x, 4x, 8x).
		psoDesc.SampleDesc.Quality = 0;

		HRESULT hr = m_device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &stateFaces ) );
		CHECK_HR( "Failed to create pipeline state for faces.", hr, m_app->log );

		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Disable backface culling.

		hr = m_device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &stateNoCull ) );
		CHECK_HR( "Failed to create pipeline state without backface culling.", hr, m_app->log );

		/////////////////////////////////////////////////////////////////
		//                          Edges Pass                         //
		/////////////////////////////////////////////////////////////////

		psoDesc.pRootSignature = rootSignatureEdges.Get();
		psoDesc.PS = { g_edges_pass_ps, _countof( g_edges_pass_ps ) };
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // Wireframe.
		// Sample count doesn't work for non-triangle geometry (LINELIST) or WIREFRAME.
		psoDesc.SampleDesc.Count = 1;
		hr = m_device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &stateEdges ) );
		CHECK_HR( "Failed to create pipeline state for edges.", hr, m_app->log );

		/////////////////////////////////////////////////////////////////
		//                         Vertex Pass                         //
		/////////////////////////////////////////////////////////////////

		psoDesc.pRootSignature = rootSignatureVertices.Get();
		psoDesc.PS = { g_vertex_pass_ps, _countof( g_vertex_pass_ps ) };
		psoDesc.GS = { g_vertex_pass_gs, _countof( g_vertex_pass_gs ) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

		hr = m_device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &stateVertices ) );
		CHECK_HR( "Failed to create pipeline state for verteices.", hr, m_app->log );

		/////////////////////////////////////////////////////////////////
		//                         Shadow Pass                         //
		/////////////////////////////////////////////////////////////////

		psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{};
		psoDesc.pRootSignature = rootSignatureShadows.Get();
		psoDesc.VS = { g_shadow_pass_vs, _countof( g_shadow_pass_vs ) };
		psoDesc.PS = { nullptr, 0 };
		psoDesc.InputLayout = { inputLayout, _countof( inputLayout ) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		// Fixes "Shadow Acne", providing natural buffer. Commonly used for shadow maps.
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		/* A constant value added to every pixel to "push" the shadow map away from the camera.
		Usual range (reverse-Z): [-100, -500]. Lower = Shadow Acne, higher = Peter panning. */
		// https://learn.microsoft.com/en-us/windows/win32/direct3d11/d3d10-graphics-programming-guide-output-merger-stage-depth-bias
		psoDesc.RasterizerState.DepthBias = -10;
		// Multiplier for nearly parallel triangles to the light direction. Range: [-1.0, -5.0].
		psoDesc.RasterizerState.SlopeScaledDepthBias = -1.f;
		psoDesc.RasterizerState.DepthBiasClamp = 0.f;
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.NumRenderTargets = 0; // Number of color buffers. 0 = Depth-only.
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // Standard for shadow maps for Depth Stencil View (DSV).
		psoDesc.SampleDesc.Count = 1; // MSAA multiplier. 1x - No MSAA. (2x, 4x, 8x).

		hr = m_device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &stateShadows ) );
		CHECK_HR( "Failed to create pipeline state for shadows.", hr, m_app->log );

		m_app->log( "[ Rasterization ] All pipeline states created." );
	}

	void Pipeline::CreateRootSignatureDefault() {
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_root_parameter1
		D3D12_ROOT_PARAMETER1 rootParams[6]{};
		uint8_t shaderRegisterCBV{};

		// Param b0 - transform matrix.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param b1 - Root constants: frameIdx, specStrength.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Constants.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Constants.RegisterSpace = 0;
		rootParams[shaderRegisterCBV].Constants.Num32BitValues = 2;
		shaderRegisterCBV++;

		// Param b2 - Scene data.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param b3 - Directional Light Data.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param b4 - Light MatricesData.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// SRV t0 - Shadow map SRV.
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_descriptor_range1
		D3D12_DESCRIPTOR_RANGE1 range{};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.NumDescriptors = 1;
		range.BaseShaderRegister = 0;
		range.RegisterSpace = 0;
		range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams[shaderRegisterCBV].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[shaderRegisterCBV].DescriptorTable.pDescriptorRanges = &range;
		shaderRegisterCBV++;

		// Sampler s0 - shadow sampler.
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_static_sampler_desc
		D3D12_STATIC_SAMPLER_DESC shadowSampler{};
		// MIN_MAG_LIN: Use lerp when texture is resized to blur edges instead of being blocky.
		shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		// If pixel's texture range is outside [0, 1], use border color.
		shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		// White = 1.0, meaning "Lit". Prevents shadow streaks.
		shadowSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		// If current pixel's depth is closer to the light (larger value) than
		// the shadow map value - it's lit. Default: LESS_EQUAL. Changes because Reverse-Z is used.
		shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		shadowSampler.ShaderRegister = 0;
		shadowSampler.RegisterSpace = 0;
		// Optimize by limiting to pixel shader visibility, as it's only needed there.
		shadowSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_root_signature_desc1
		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc{};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.NumParameters = _countof( rootParams );
		rootSignatureDesc.pParameters = rootParams;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &shadowSampler;

		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_versioned_root_signature_desc
		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDescVersioned{};
		rootSigDescVersioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSigDescVersioned.Desc_1_1 = rootSignatureDesc;

		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeVersionedRootSignature(
			&rootSigDescVersioned,
			&signatureBlob,
			&errorBlob
		);

		if ( errorBlob ) {
			const char* msg{ static_cast<char*>(errorBlob->GetBufferPointer()) };
			m_app->log( std::format( "Main Root Signature Error: {}", msg ), LogLevel::Error );
		}
		CHECK_HR( "Failed to serialize main root signature.", hr, m_app->log );

		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &rootSignatureDefault )
		);
		CHECK_HR( "CreateRootSignature failed.", hr, m_app->log );
		m_app->log( "[ Rasterization ] Main root signature created." );
	}

	void Pipeline::CreateRootSignatureEdges() {
		D3D12_ROOT_PARAMETER1 rootParams[2]{};
		uint8_t shaderRegisterCBV{};

		// Param b0 - transform matrix.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param b1 - Edges color.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Constants.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Constants.RegisterSpace = 0;
		rootParams[shaderRegisterCBV].Constants.Num32BitValues = 1;
		shaderRegisterCBV++;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc{};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.NumParameters = _countof( rootParams );
		rootSignatureDesc.pParameters = rootParams;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDescVersioned{};
		rootSigDescVersioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSigDescVersioned.Desc_1_1 = rootSignatureDesc;

		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeVersionedRootSignature(
			&rootSigDescVersioned,
			&signatureBlob,
			&errorBlob
		);

		if ( errorBlob ) {
			const char* msg{ static_cast<char*>(errorBlob->GetBufferPointer()) };
			m_app->log( std::format( "Edges Root Signature Error: {}", msg ), LogLevel::Error );
		}
		CHECK_HR( "Failed to serialize root signature for edges.", hr, m_app->log );

		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &rootSignatureEdges )
		);
		CHECK_HR( "CreateRootSignature failed.", hr, m_app->log );
		m_app->log( "[ Rasterization ] Root signature for edges created." );
	}

	void Pipeline::CreateRootSignatureVertices() {
		D3D12_ROOT_PARAMETER1 rootParams[3]{};
		uint8_t shaderRegisterCBV{};

		// Param b0 - transform matrix.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param b1 - Screen data.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param b2 - Vertex color.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Constants.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Constants.RegisterSpace = 0;
		rootParams[shaderRegisterCBV].Constants.Num32BitValues = 1;
		shaderRegisterCBV++;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc{};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.NumParameters = _countof( rootParams );
		rootSignatureDesc.pParameters = rootParams;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDescVersioned{};
		rootSigDescVersioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSigDescVersioned.Desc_1_1 = rootSignatureDesc;

		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeVersionedRootSignature(
			&rootSigDescVersioned,
			&signatureBlob,
			&errorBlob
		);

		if ( errorBlob ) {
			const char* msg{ static_cast<char*>(errorBlob->GetBufferPointer()) };
			m_app->log( std::format( "Vertices Root Signature Error: {}", msg ), LogLevel::Error );
		}
		CHECK_HR( "Failed to serialize root signature for vertices.", hr, m_app->log );

		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &rootSignatureVertices )
		);
		CHECK_HR( "CreateRootSignature failed.", hr, m_app->log );
		m_app->log( "[ Rasterization ] Root signature for vertices created." );
	}

	void Pipeline::CreateRootSignatureShadows() {
		D3D12_ROOT_PARAMETER1 rootParams[2]{};

		// Param b0 - Light matrices.
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[0].Descriptor.ShaderRegister = 0;
		rootParams[0].Descriptor.RegisterSpace = 0;

		// Param b1 - Shadow world matrix.
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParams[1].Descriptor.ShaderRegister = 1;
		rootParams[1].Descriptor.RegisterSpace = 0;

		D3D12_ROOT_SIGNATURE_DESC1 rootSignatureDesc{};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.NumParameters = _countof( rootParams );
		rootSignatureDesc.pParameters = rootParams;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDescVersioned{};
		rootSigDescVersioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSigDescVersioned.Desc_1_1 = rootSignatureDesc;

		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeVersionedRootSignature(
			&rootSigDescVersioned,
			&signatureBlob,
			&errorBlob
		);

		if ( errorBlob ) {
			const char* msg{ static_cast<char*>(errorBlob->GetBufferPointer()) };
			m_app->log( std::format( "Shadows Root Signature Error: {}", msg ), LogLevel::Error );
		}
		CHECK_HR( "Failed to serialize root signature for shadows.", hr, m_app->log );

		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &rootSignatureShadows )
		);
		CHECK_HR( "CreateRootSignature failed.", hr, m_app->log );
		m_app->log( "[ Rasterization ] Root signature for shadows created." );
	}

	void Pipeline::CreateDepthStencil() {
		// DSV Heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		HRESULT hr{ m_device->CreateDescriptorHeap(
			&dsvHeapDesc, IID_PPV_ARGS( &dsvHeapDepthStencil ) ) };
		CHECK_HR( "Failed to create DSV Heap.", hr, m_app->log );

		// Depth texture.
		D3D12_RESOURCE_DESC depthDesc{};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = m_app->scene.settings.renderWidth;
		depthDesc.Height = m_app->scene.settings.renderHeight;
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = m_depthFormat;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = m_depthFormat;
		clearValue.DepthStencil.Depth = 0.f; // Usually 1.f. Set to 0.f because of reverse-Z.
		clearValue.DepthStencil.Stencil = 0;

		D3D12_HEAP_PROPERTIES heapProps{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };

		hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS( &m_depthStencilBuffer )
		);

		// Create DSV.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = m_depthFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		m_device->CreateDepthStencilView(
			m_depthStencilBuffer.Get(),
			&dsvDesc,
			dsvHeapDepthStencil->GetCPUDescriptorHandleForHeapStart()
		);
		m_app->log( "[ Rasterization ] Depth Stencil created." );
	}
}