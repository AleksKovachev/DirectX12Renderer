#include "Renderer.hpp"
#include "utils.hpp" // CHECK_HR

#include "d3dx12_core.h"

#include "ConstColor.hlsl.h"
#include "ConstColorVertexPass.hlsl.h"
#include "ConstColorVS.hlsl.h"
#include "ConstColorWireframePass.hlsl.h"
#include "GeometryShader.hlsl.h"

#include <algorithm> // clamp
#include <cmath> // abs, exp, tan
#include <format> // format
#include <string> // string

namespace Core {
	void WolfRenderer::PrepareForRasterization() {
		// Calculate aspect ratio for the transform CB.
		unsigned& width = scene.settings.renderWidth;
		unsigned& height = scene.settings.renderHeight;
		dataRaster.camera.aspectRatio = static_cast<float>(width) / static_cast<float>(height);

		CreateRootSignature();
		CreatePipelineState();
		m_gpuMeshesRaster.clear();
		for ( const Mesh& mesh: scene.GetMeshes() )
			CreateMeshBuffers(mesh);
		CreateTransformConstantBuffer();
		CreateSceneDataConstantBuffer();
		CreateScreenDataConstantBuffer();
		CreateViewport();
		CreateDepthStencil();
		log( "[ Rasterization ] Successful preparation." );
	}

	void WolfRenderer::FrameBeginRasterization() {
		UpdateSmoothMotion();

		ResetCommandAllocatorAndList();

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_cmdList->ResourceBarrier( 1, &barrier );

		// Set which Render Target will be used for rendering.
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
		m_cmdList->OMSetRenderTargets( 1, &m_rtvHandles[m_scFrameIdx], FALSE, &dsvHandle );
		m_cmdList->ClearRenderTargetView( m_rtvHandles[m_scFrameIdx], dataRaster.bgColor, 0, nullptr );
		m_cmdList->ClearDepthStencilView( m_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
			D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr
		);
	}

	void WolfRenderer::RenderFrameRasterization() {
		m_cmdList->SetGraphicsRootSignature( m_rootSignature.Get() );

		// Slot 0: Mask the offset float values as uint values.
		m_cmdList->SetGraphicsRoot32BitConstant( 0, static_cast<UINT>(m_frameIdx), 0 );

		// Slot 1:Transform CBV.
		m_cmdList->SetGraphicsRootConstantBufferView(
			1, dataRaster.camera.const_buffer->GetGPUVirtualAddress() );

		// Slot 2: Scene Data.
		m_cmdList->SetGraphicsRootConstantBufferView( 2, m_sceneDataCB->GetGPUVirtualAddress() );

		memcpy( m_sceneDataCBMappedPtr, &dataRaster.sceneData, sizeof( dataRaster.sceneData ) );

		for ( const Raster::GPUMesh& mesh : m_gpuMeshesRaster ) {
			m_cmdList->IASetVertexBuffers( 0, 1, &mesh.vbView );
			m_cmdList->IASetIndexBuffer( &mesh.ibView );

			m_cmdList->RSSetViewports( 1, &m_viewport );
			m_cmdList->RSSetScissorRects( 1, &m_scissorRect );

			// Slot 3: Screen Data.
			m_cmdList->SetGraphicsRootConstantBufferView( 3, m_screenDataCB->GetGPUVirtualAddress() );

			dataRaster.screenData.viewportSize = { static_cast<float>(scene.settings.renderWidth),
											  static_cast<float>(scene.settings.renderHeight) };
			dataRaster.screenData.vertSize = dataRaster.vertexSize;
			memcpy( m_screenDataCBMappedPtr, &dataRaster.screenData, sizeof( dataRaster.screenData ) );

			if ( dataRaster.renderFaces ) {
				ID3D12PipelineState* state;
				if ( dataRaster.showBackfaces )
					state = m_pipelineStateNoCull.Get();
				else
					state = m_pipelineStateFaces.Get();
				m_cmdList->SetPipelineState( state );
			}

			// Slot 4: Edges color.
			m_cmdList->SetGraphicsRoot32BitConstant( 4, dataRaster.edgeColor, 0 );

			// Slot 5: Vertex color.
			m_cmdList->SetGraphicsRoot32BitConstant( 5, dataRaster.vertexColor, 0 );

			// IA stands for Input Assembler.
			m_cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
			m_cmdList->DrawIndexedInstanced( static_cast<UINT>(mesh.indexCount), 1, 0, 0, 0 );

			if ( dataRaster.renderEdges ) {
				m_cmdList->SetPipelineState( m_pipelineStateEdges.Get() );
				m_cmdList->DrawIndexedInstanced( static_cast<UINT>(mesh.indexCount), 1, 0, 0, 0 );
			}

			if ( dataRaster.renderVerts ) {
				m_cmdList->SetPipelineState( m_pipelineStateVertices.Get() );
				m_cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_POINTLIST );
				// Only 1 point is needed per vertex. Use DrawInstanced, otherwise indices will be ignored.
				m_cmdList->DrawInstanced( static_cast<UINT>(mesh.vertexCount), 1, 0, 0 );
			}
		}
	}

	void WolfRenderer::FrameEndRasterization() {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_cmdList->ResourceBarrier( 1, &barrier );
	}

	void WolfRenderer::CreateRootSignature() {
		D3D12_ROOT_PARAMETER1 rootParams[6]{};
		uint8_t shaderRegisterCBV{};

		// Param 0 - frameIdx.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Constants.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Constants.RegisterSpace = 0;
		rootParams[shaderRegisterCBV].Constants.Num32BitValues = 1;
		shaderRegisterCBV++;

		// Param 1 - transform matrix.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param 2 - Scene data.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param 3 - Screen data.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Descriptor.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Descriptor.RegisterSpace = 0;
		shaderRegisterCBV++;

		// Param 4 - Edges color.
		rootParams[shaderRegisterCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[shaderRegisterCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[shaderRegisterCBV].Constants.ShaderRegister = shaderRegisterCBV;
		rootParams[shaderRegisterCBV].Constants.RegisterSpace = 0;
		rootParams[shaderRegisterCBV].Constants.Num32BitValues = 1;
		shaderRegisterCBV++;

		// Param 5 - Vertex color.
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
			log( std::format( "Root Signature Error: {}", msg ), LogLevel::Error );
		}
		CHECK_HR( "Failed to serialize root signature.", hr, log );

		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &m_rootSignature )
		);
		CHECK_HR( "CreateRootSignature failed.", hr, log );
		log( "[ Rasterization ] Root signature created." );
	}

	void WolfRenderer::CreatePipelineState() {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

		// This tells the rasterizer to take 2D or 3D vertex data.
		// Use DXGI_FORMAT_R32G32_FLOAT for 2D and DXGI_FORMAT_R32G32B32_FLOAT for 3D.
		// VSInput's position parameter type must reflect this type: float2 / float3.
		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

			// Required for normals and lighting.
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.PS = { g_const_color, _countof( g_const_color ) };
		psoDesc.VS = { g_const_color_vs, _countof( g_const_color_vs ) };
		psoDesc.InputLayout = { inputLayout, _countof( inputLayout ) };
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		psoDesc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT ); // DepthEnable = TRUE
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
		psoDesc.DSVFormat = m_depthFormat;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1; // MSAA multiplier. 1x - No MSAA. (2x, 4x, 8x).
		psoDesc.SampleDesc.Quality = 0;

		HRESULT hr = m_device->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS( &m_pipelineStateFaces )
		);
		CHECK_HR( "Failed to create pipeline state for faces.", hr, log );

		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Disable backface culling.

		hr = m_device->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS( &m_pipelineStateNoCull )
		);
		CHECK_HR( "Failed to create pipeline state without backface culling.", hr, log );

		psoDesc.PS = { g_const_color_wire_ps, _countof( g_const_color_wire_ps ) };
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME; // Wireframe.
		hr = m_device->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS( &m_pipelineStateEdges )
		);
		CHECK_HR( "Failed to create pipeline state for edges.", hr, log );

		psoDesc.PS = { g_const_color_verts_ps, _countof( g_const_color_verts_ps ) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		psoDesc.GS = { g_geometry_shader_gs, _countof( g_geometry_shader_gs ) };
		psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

		hr = m_device->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS( &m_pipelineStateVertices )
		);
		CHECK_HR( "Failed to create pipeline state for verteices.", hr, log );

		log( "[ Rasterization ] Pipeline state created." );
	}

	void WolfRenderer::CreateMeshBuffers( const Mesh& mesh ) {
		const size_t vbSize{ sizeof( Vertex ) * mesh.vertices.size() };
		const size_t ibSize{ sizeof( uint32_t ) * mesh.indices.size() };

		// Create the "Intermediate" Upload Buffers (Staging).
		ComPtr<ID3D12Resource> vbUpload{ nullptr };
		ComPtr<ID3D12Resource> ibUpload{ nullptr };

		// Upload heap used so the CPU can write to it.
		D3D12_HEAP_PROPERTIES heapPropsUp{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ) };

		HRESULT hr{};
		// Vertex upload buffer.
		{
			D3D12_RESOURCE_DESC vbDescUp{ CD3DX12_RESOURCE_DESC::Buffer( vbSize ) };

			hr = m_device->CreateCommittedResource(
				&heapPropsUp,
				D3D12_HEAP_FLAG_NONE,
				&vbDescUp,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS( &vbUpload )
			);
			CHECK_HR( "Failed to create upload vertex buffer.", hr, log );

			// Copy data from CPU to Upload Buffer
			void* pVertexData{ nullptr };
			hr = vbUpload->Map( 0, nullptr, &pVertexData );
			CHECK_HR( "Failed to map upload buffer.", hr, log );
			memcpy( pVertexData, mesh.vertices.data(), vbSize );
			vbUpload->Unmap( 0, nullptr );
		}

		// Index upload buffer.
		{
			D3D12_RESOURCE_DESC ibDescUp{ CD3DX12_RESOURCE_DESC::Buffer( ibSize ) };

			hr = m_device->CreateCommittedResource(
				&heapPropsUp,
				D3D12_HEAP_FLAG_NONE,
				&ibDescUp,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS( &ibUpload )
			);
			CHECK_HR( std::string(
				"Failed to create upload index buffer for mesh: " ) + mesh.name, hr, log);

			// Copy data from CPU to Upload Buffer
			void* pIndexData{ nullptr };
			hr = ibUpload->Map( 0, nullptr, &pIndexData );
			CHECK_HR( "Failed to map upload buffer.", hr, log );
			memcpy( pIndexData, mesh.indices.data(), ibSize );
			ibUpload->Unmap( 0, nullptr );
		}

		Raster::GPUMesh gpuMesh;
		gpuMesh.vertexCount = static_cast<UINT>(mesh.vertices.size());
		gpuMesh.indexCount = static_cast<UINT>(mesh.indices.size());

		// Create the destination Vertex Buffer (Default Heap).
		// Default heap used (GPU VRAM) that CPU can't access directly.
		D3D12_HEAP_PROPERTIES heapPropsDf{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };

		// GPU vertex buffer.
		{
			D3D12_RESOURCE_DESC vbDescDf{ CD3DX12_RESOURCE_DESC::Buffer( vbSize ) };

			hr = m_device->CreateCommittedResource(
				&heapPropsDf,
				D3D12_HEAP_FLAG_NONE,
				&vbDescDf,
				// Start in COPY_DEST so data can be copied to it immediately.
				// D3D12_RESOURCE_STATE_COPY_DEST but it's ignored by DirectX. Debug Layer.
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS( &gpuMesh.vertexBuffer )
			);
			CHECK_HR( "Failed to create default vertex buffer.", hr, log );
		}
		//GPU index buffer.
		{
			D3D12_RESOURCE_DESC ibDescDf{ CD3DX12_RESOURCE_DESC::Buffer( ibSize ) };

			hr = m_device->CreateCommittedResource(
				&heapPropsDf,
				D3D12_HEAP_FLAG_NONE,
				&ibDescDf,
				// Start in COPY_DEST so data can be copied to it immediately.
				// D3D12_RESOURCE_STATE_COPY_DEST but it's ignored by DirectX. Debug Layer.
				D3D12_RESOURCE_STATE_COMMON,
				nullptr,
				IID_PPV_ARGS( &gpuMesh.indexBuffer )
			);
			CHECK_HR( "Failed to create default vertex buffer.", hr, log );
		}

		// Name the buffer for easier debugging using PIX/NSIGHT.
		gpuMesh.vertexBuffer->SetName(( std::wstring( L"Vertex Buffer Default Resource for: " )
			+ ConvertStringToWstring(mesh.name) ).c_str() );
		gpuMesh.indexBuffer->SetName( (std::wstring( L"Index Buffer Default Resource for: " )
			+ ConvertStringToWstring( mesh.name )).c_str() );

		ResetCommandAllocatorAndList();

		// Copy data.
		m_cmdList->CopyBufferRegion( gpuMesh.vertexBuffer.Get(), 0, vbUpload.Get(), 0, vbSize );
		m_cmdList->CopyBufferRegion( gpuMesh.indexBuffer.Get(), 0, ibUpload.Get(), 0, ibSize );

		// Transition the Default Buffer from COPY_DEST to VERTEX_AND_CONSTANT_BUFFER
		// so the Input Assembled (IA) can read it during rendering.
		D3D12_RESOURCE_BARRIER barriers[2]{};
		barriers[0].Transition.pResource = gpuMesh.vertexBuffer.Get();
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		barriers[1].Transition.pResource = gpuMesh.indexBuffer.Get();
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;

		m_cmdList->ResourceBarrier( _countof(barriers), barriers );

		hr = m_cmdList->Close();
		CHECK_HR( "Failed to close command list for Vertex Buffer upload..", hr, log );

		// Execute the commands
		ID3D12CommandList* ppCommandLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

		// Wait for the GPU to finish the copy BEFORE the 'uploadBuffer' ComPtr
		// goes out of scope. Otherwise, is's destroyed while the GPU is
		// trying to read from it, causing a crash/device removal.
		WaitForGPUSync();

		gpuMesh.vbView.BufferLocation = gpuMesh.vertexBuffer->GetGPUVirtualAddress();
		gpuMesh.vbView.StrideInBytes = sizeof( Vertex );
		gpuMesh.vbView.SizeInBytes = static_cast<UINT>(vbSize);

		gpuMesh.ibView.BufferLocation = gpuMesh.indexBuffer->GetGPUVirtualAddress();
		gpuMesh.ibView.Format = DXGI_FORMAT_R32_UINT;
		gpuMesh.ibView.SizeInBytes = static_cast<UINT>(ibSize);

		m_gpuMeshesRaster.push_back( gpuMesh );

		log( "[ Rasterization ] Vertex and index buffers uploaded to GPU." );
	}

	void WolfRenderer::CreateViewport() {
		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<FLOAT>(scene.settings.renderWidth);
		m_viewport.Height = static_cast<FLOAT>(scene.settings.renderHeight);
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;

		m_scissorRect.left = 0;
		m_scissorRect.top = 0;
		m_scissorRect.right = scene.settings.renderWidth;
		m_scissorRect.bottom = scene.settings.renderHeight;
		log( "[ Rasterization ] Viewport set up." );
	}

	void WolfRenderer::AddToTargetOffset( float dx, float dy ) {
		// Add mouse offset to target offset.
		// Clamp values to prevent offscreen value accumulation.
		Raster::Transformation& tr = dataRaster.camera;

		CalculateViewportBounds();

		// Predict next target.
		float candidateX = tr.targetOffsetX + dx * tr.offsetXYSens;
		float candidateY = tr.targetOffsetY + dy * tr.offsetXYSens;

		tr.targetOffsetX = std::clamp( candidateX, -tr.boundsX, tr.boundsX );
		tr.targetOffsetY = std::clamp( candidateY, -tr.boundsY, tr.boundsY );
	}

	void WolfRenderer::AddToOffsetZ( float dz ) {
		dataRaster.camera.offsetZ += dz * dataRaster.camera.offsetZSens;
		dataRaster.camera.offsetZ += dz * dataRaster.camera.offsetZSens;
	}

	void WolfRenderer::AddToOffsetFOV( float offset ) {
		float angleRadians{ DirectX::XMConvertToRadians(
			offset * dataRaster.camera.FOVSens ) };
		dataRaster.camera.FOVAngle += angleRadians;

		// A value near 0 causes division by 0 and crashes the application.
		// Clamping the value stops FOV at max zoom. Adding it again makes it
		// go over to the negative numbers, causing inverted projection.
		if ( DirectX::XMScalarNearEqual( dataRaster.camera.FOVAngle, 0.f, 0.00001f * 2.f ) )
			dataRaster.camera.FOVAngle += angleRadians;
	}

	void WolfRenderer::AddToTargetRotation( float deltaAngleX, float deltaAngleY ) {
		dataRaster.camera.targetRotationX += deltaAngleX * dataRaster.camera.rotSens;
		dataRaster.camera.targetRotationY += deltaAngleY * dataRaster.camera.rotSens;
	}

	void WolfRenderer::CreateTransformConstantBuffer() {
		// Constant buffer must be 256-byte aligned. This is a precaution, should already be 256 bytes.
		const UINT cbSize = (sizeof( Raster::Transformation::TransformDataCB ) + 255) & ~255;

		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD );
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer( cbSize );

		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &dataRaster.camera.const_buffer )
		);
		CHECK_HR( "Failed to create Transform constant buffer.", hr, log );

		// Map permanently for CPU writes
		hr = dataRaster.camera.const_buffer->Map(
			0, nullptr, reinterpret_cast<void**>(&dataRaster.camera.transformCBMappedPtr) );
		CHECK_HR( "Failed to map Transform constant buffer.", hr, log );

		// Initialize to identity matrix
		DirectX::XMStoreFloat4x4( &dataRaster.camera.cbData.mat, DirectX::XMMatrixIdentity() );
		memcpy(
			dataRaster.camera.transformCBMappedPtr,
			&dataRaster.camera.cbData,
			sizeof( dataRaster.camera.cbData )
		);
		log( "[ Rasterization ] Transform constant buffer created and mapped." );
	}

	void WolfRenderer::CreateSceneDataConstantBuffer() {
		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD );
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer( sizeof( Raster::SceneDataCB ) );

		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &m_sceneDataCB )
		);
		CHECK_HR( "Failed to create Scene Data constant buffer.", hr, log );

		// Map permanently for CPU writes
		hr = m_sceneDataCB->Map(
			0, nullptr, reinterpret_cast<void**>(&m_sceneDataCBMappedPtr) );
		CHECK_HR( "Failed to map Scene Data constant buffer.", hr, log );

		memcpy( m_sceneDataCBMappedPtr, &dataRaster.sceneData, sizeof( dataRaster.sceneData ) );
		log( "[ Rasterization ] Transform constant buffer created and mapped." );
	}

	void WolfRenderer::CreateScreenDataConstantBuffer() {
		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD );
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer( sizeof( Raster::ScreenConstants ) );

		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &m_screenDataCB )
		);
		CHECK_HR( "Failed to create Screen Data constant buffer.", hr, log );

		// Map permanently for CPU writes
		hr = m_screenDataCB->Map(
			0, nullptr, reinterpret_cast<void**>(&m_screenDataCBMappedPtr) );
		CHECK_HR( "Failed to map Screen Data constant buffer.", hr, log );

		memcpy( m_screenDataCBMappedPtr, &dataRaster.screenData, sizeof( dataRaster.screenData ) );
		log( "[ Rasterization ] Transform constant buffer created and mapped." );
	}

	void WolfRenderer::UpdateSmoothMotion() {
		Raster::Transformation& tr{ dataRaster.camera };

		// Compute smoothing factor for movement (frame-rate independent).
		const float sTransFactor = 1.f - std::exp( -tr.smoothOffsetLerp * m_app->deltaTime );

		// Linear interpolation toward the target.
		tr.currOffsetX += (tr.targetOffsetX - tr.currOffsetX) * sTransFactor;
		tr.currOffsetY += (tr.targetOffsetY - tr.currOffsetY) * sTransFactor;

		// Compute smoothing factor for rotation (frame-rate independent).
		const float sRotFactor = 1.f - std::exp( -tr.smoothRotationLambda * m_app->deltaTime );

		// Linear interpolation toward the target.
		tr.currRotationX += (tr.targetRotationX - tr.currRotationX) * sRotFactor;
		tr.currRotationY += (tr.targetRotationY - tr.currRotationY) * sRotFactor;

		// Don't allow the center of the "screen sphere" to leave the screen
		// when moving around, independent of zoom and FOV.
		// Recalculate bounds to make object stay inside.
		CalculateViewportBounds();

		tr.currOffsetX = std::clamp( tr.currOffsetX, -tr.boundsX, tr.boundsX );
		tr.currOffsetY = std::clamp( tr.currOffsetY, -tr.boundsY, tr.boundsY );

		// Subtracting 0.5 from Y to place camera slightly above ground.
		DirectX::XMMATRIX Trans = DirectX::XMMatrixTranslation(
			tr.currOffsetX, tr.currOffsetY - 0.5f, tr.offsetZ );

		// Using Rotation data for X axis in Qt screen space for MatrixY in
		// WorldView and vice-versa. Negative currRotation inverts rotation direction.
		DirectX::XMMATRIX Rot = DirectX::XMMatrixRotationY( -tr.currRotationX ) *
			DirectX::XMMatrixRotationX( -tr.currRotationY );

		// Create a world-view matrix. Multiplication order matters! Rotation,
		// then translation - transform around world origin. Otherwise - around geometry origin.
		DirectX::XMMATRIX World{};
		if ( tr.coordinateSystem == Raster::TransformCoordinateSystem::Local )
			World = Rot * Trans;
		else if ( tr.coordinateSystem == Raster::TransformCoordinateSystem::World )
			World = Trans * Rot;

		DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(
			DirectX::XMVectorSet( 0.f, 0.f, -1.f, 1.f ), // Camera position.
			DirectX::XMVectorZero(),                     // Look at origin.
			DirectX::XMVectorSet( 0.f, 1.f, 0.f, 0.f )   // Up.
		);

		DirectX::XMMATRIX WorldView = World * View;

		// Create projection matrix (for simulating a camera).
		// Swap nearZ and farZ to use "reverse-Z" for better precision.
		DirectX::XMMATRIX Proj = DirectX::XMMatrixPerspectiveFovLH(
			tr.FOVAngle, tr.aspectRatio, tr.farZ, tr.nearZ );

		XMStoreFloat4x4( &tr.cbData.mat, DirectX::XMMatrixTranspose( WorldView ) );
		XMStoreFloat4x4( &tr.cbData.projection, DirectX::XMMatrixTranspose( Proj ) );

		memcpy( tr.transformCBMappedPtr, &tr.cbData, sizeof( tr.cbData ) );
	}

	void WolfRenderer::CalculateViewportBounds() {
		float depth = std::abs( dataRaster.camera.offsetZ );

		float halfHeight = depth * std::tan( dataRaster.camera.FOVAngle * 0.5f );
		float halfWidth = halfHeight * dataRaster.camera.aspectRatio;

		// If the object does not fit, lock movement instead of exploding.
		dataRaster.camera.boundsX = std::abs( halfWidth - dataRaster.camera.dummyObjectRadius );
		dataRaster.camera.boundsY = std::abs( halfHeight - dataRaster.camera.dummyObjectRadius );
	}

	void WolfRenderer::CreateDepthStencil() {
		// DSV Heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		HRESULT hr{ m_device->CreateDescriptorHeap( &dsvHeapDesc, IID_PPV_ARGS( &m_dsvHeap ) ) };
		CHECK_HR( "Failed to create DSV Heap.", hr, log );

		// Depth texture.
		D3D12_RESOURCE_DESC depthDesc{};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Width = scene.settings.renderWidth;
		depthDesc.Height = scene.settings.renderHeight;
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = m_depthFormat;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = m_depthFormat;
		clearValue.DepthStencil.Depth = 0.f;
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
			m_dsvHeap->GetCPUDescriptorHandleForHeapStart()
		);
		log( "[ Rasterization ] Depth Stencil created." );
	}
}