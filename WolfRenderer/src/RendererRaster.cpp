#include "Lights.hpp"
#include "Renderer.hpp"
#include "utils.hpp" // CHECK_HR

#include "d3dx12_core.h"

#include <algorithm> // clamp
#include <cmath> // abs, exp, tan
#include <string> // string

namespace Core {
	void WolfRenderer::PrepareForRasterization() {
		// Calculate aspect ratio for the transform CB.
		unsigned& width = m_app->scene.settings.renderWidth;
		unsigned& height = m_app->scene.settings.renderHeight;
		dataR.camera.aspectRatio = static_cast<float>(width) / static_cast<float>(height);

		m_pipeline->CreateRootSignatureDefault();
		m_pipeline->CreateRootSignatureEdges();
		m_pipeline->CreateRootSignatureVertices();
		m_pipeline->CreateRootSignatureShadows();

		m_pipeline->CreatePipelineStates();
		m_gpuMeshesR.clear();
		for ( const Mesh& mesh: m_app->scene.GetMeshes() )
			CreateMeshBuffers(mesh);

		CreateConstantBuffers();

		CreateShadowMap();
		CreateShadowPassSRVAndHeap();
		CreateViewport();
		m_pipeline->CreateDepthStencil();
		m_app->log( "[ Rasterization ] Successful preparation." );
	}

	void WolfRenderer::FrameBeginR() {
		UpdateSmoothMotion();
		UpdateCameraMatricesR();
		ResetCommandAllocatorAndList();

		UpdateDirectionalLight();
		RenderShadowMapPass();

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_cmdList->ResourceBarrier( 1, &barrier );

		barrier.Transition.pResource = m_shadowMapBuffer.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		m_cmdList->ResourceBarrier( 1, &barrier );

		// Set which Render Target will be used for rendering.
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = 
			m_pipeline->dsvHeapDepthStencil->GetCPUDescriptorHandleForHeapStart();
		m_cmdList->OMSetRenderTargets( 1, &m_rtvHandles[m_scFrameIdx], FALSE, &dsvHandle );
		m_cmdList->ClearRenderTargetView( m_rtvHandles[m_scFrameIdx], dataR.bgColor, 0, nullptr );
		m_cmdList->ClearDepthStencilView(
			m_pipeline->dsvHeapDepthStencil->GetCPUDescriptorHandleForHeapStart(),
			D3D12_CLEAR_FLAG_DEPTH, 0.f, 0, 0, nullptr
		);
	}

	void WolfRenderer::RenderFrameR() {
		// Root signatures can't exist all at the same time.
		// Draw calls need to be issued before setting a new root signature.
		if ( dataR.renderFaces ) {
			m_cmdList->SetGraphicsRootSignature( m_pipeline->rootSignatureDefault.Get() );

			// Needs to be set BEFORE settings root parameters when using
			// multiple root signatures and PSOs. Sometimes creates issues otherwise.
			m_cmdList->SetPipelineState( dataR.facesPSO.Get() );

			m_cmdList->RSSetViewports( 1, &m_viewport );
			m_cmdList->RSSetScissorRects( 1, &m_scissorRect );

			// Slot b0: Transform CBV (in Default VS).
			m_cmdList->SetGraphicsRootConstantBufferView(
				0, dataR.camera.cameraCBRes->GetGPUVirtualAddress() );

			// Slot b1: Root constants. Mask the offset float values as uint values (in Default PS).
			m_cmdList->SetGraphicsRoot32BitConstant( 1, static_cast<UINT>(m_frameIdx), 0 );

			// Slot b2: Scene Data (in Default PS).
			m_cmdList->SetGraphicsRootConstantBufferView( 2, m_sceneDataCBRes->GetGPUVirtualAddress() );
			memcpy( m_sceneDataCBMappedPtr, &dataR.sceneData, sizeof( dataR.sceneData ) );

			// Slot b3: Lighting data (in Default PS).
			m_cmdList->SetGraphicsRootConstantBufferView( 3, m_lightDataCBRes->GetGPUVirtualAddress() );
			memcpy( m_lightDataCBMappedPtr, &dataR.directionalLight.cb, sizeof( dataR.directionalLight.cb ) );

			// Slot b4: Light Matrices Data (in Default PS and Shadow Map VS).
			m_cmdList->SetGraphicsRootConstantBufferView( 4, m_lightMatricesCBRes->GetGPUVirtualAddress() );

			// Slot t0: Shadow map.
			ID3D12DescriptorHeap* heaps[] = { m_srvHeapShadowMap.Get() };
			m_cmdList->SetDescriptorHeaps( 1, heaps );
			m_cmdList->SetGraphicsRootDescriptorTable(
				5, m_srvHeapShadowMap->GetGPUDescriptorHandleForHeapStart() );

			for ( const Raster::GPUMesh& mesh : m_gpuMeshesR ) {
				m_cmdList->IASetVertexBuffers( 0, 1, &mesh.vbView );
				m_cmdList->IASetIndexBuffer( &mesh.ibView );

				// IA stands for Input Assembler.
				m_cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
				m_cmdList->DrawIndexedInstanced( static_cast<UINT>(mesh.indexCount), 1, 0, 0, 0 );
			}
		}

		if ( dataR.renderEdges ) {
			m_cmdList->SetGraphicsRootSignature( m_pipeline->rootSignatureEdges.Get() );
			m_cmdList->SetPipelineState( m_pipeline->stateEdges.Get() );

			m_cmdList->RSSetViewports( 1, &m_viewport );
			m_cmdList->RSSetScissorRects( 1, &m_scissorRect );

			// Slot b0:Transform CBV (in Vertex shader).
			m_cmdList->SetGraphicsRootConstantBufferView(
				0, dataR.camera.cameraCBRes->GetGPUVirtualAddress() );

			for ( const Raster::GPUMesh& mesh : m_gpuMeshesR ) {
				m_cmdList->IASetVertexBuffers( 0, 1, &mesh.vbView );
				m_cmdList->IASetIndexBuffer( &mesh.ibView );

				// Slot b1: Edges color (in Edges Pixel shader).
				m_cmdList->SetGraphicsRoot32BitConstant( 1, dataR.edgeColor, 0 );

				m_cmdList->DrawIndexedInstanced( static_cast<UINT>(mesh.indexCount), 1, 0, 0, 0 );
			}
		}

		if ( dataR.renderVerts ) {
			m_cmdList->SetGraphicsRootSignature( m_pipeline->rootSignatureVertices.Get() );
			m_cmdList->SetPipelineState( m_pipeline->stateVertices.Get() );

			m_cmdList->RSSetViewports( 1, &m_viewport );
			m_cmdList->RSSetScissorRects( 1, &m_scissorRect );

			// Slot b0:Transform CBV (in Vertex shader).
			m_cmdList->SetGraphicsRootConstantBufferView(
				0, dataR.camera.cameraCBRes->GetGPUVirtualAddress() );

			for ( const Raster::GPUMesh& mesh : m_gpuMeshesR ) {
				m_cmdList->IASetVertexBuffers( 0, 1, &mesh.vbView );
				m_cmdList->IASetIndexBuffer( &mesh.ibView );

				// Slot b1: Screen Data (in Geometry shader).
				m_cmdList->SetGraphicsRootConstantBufferView( 1, m_screenDataCBRes->GetGPUVirtualAddress() );

				dataR.screenData.viewportSize = {
					static_cast<float>(m_app->scene.settings.renderWidth),
					static_cast<float>(m_app->scene.settings.renderHeight)
				};
				dataR.screenData.vertSize = dataR.vertexSize;
				memcpy( m_screenDataCBMappedPtr, &dataR.screenData, sizeof( dataR.screenData ) );

				// Slot b2: Vertex color (in Vertices Pixel shader).
				m_cmdList->SetGraphicsRoot32BitConstant( 2, dataR.vertexColor, 0 );

				m_cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_POINTLIST );
				// Only 1 point is needed per vertex. Use DrawInstanced, otherwise indices will be ignored.
				m_cmdList->DrawInstanced( static_cast<UINT>(mesh.vertexCount), 1, 0, 0 );
			}
		}
	}

	void WolfRenderer::FrameEndR() {
		// Transition render targets state.
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_cmdList->ResourceBarrier( 1, &barrier );

		// Transition shadow map buffer state.
		barrier.Transition.pResource = m_shadowMapBuffer.Get();
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

		m_cmdList->ResourceBarrier( 1, &barrier );
	}

	void WolfRenderer::CreateMeshBuffers( const Mesh& mesh ) {
		const size_t vbSize{ sizeof( Vertex ) * mesh.vertices.size() };
		const size_t ibSize{ sizeof( uint32_t ) * mesh.indices.size() };

		// Create the "Intermediate" Upload Buffers (Staging).
		ComPtr<ID3D12Resource> vbUpload{ nullptr };
		ComPtr<ID3D12Resource> ibUpload{ nullptr };

		// Upload heap used so the CPU can write to it.
		// https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_heap_properties
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
			CHECK_HR( "Failed to create upload vertex buffer.", hr, m_app->log );

			// Copy data from CPU to Upload Buffer
			void* pVertexData{ nullptr };
			hr = vbUpload->Map( 0, nullptr, &pVertexData );
			CHECK_HR( "Failed to map upload buffer.", hr, m_app->log );
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
			std::string msg{ "Failed to create upload index buffer for mesh: " };
			CHECK_HR( msg + mesh.name, hr, m_app->log);

			// Copy data from CPU to Upload Buffer
			void* pIndexData{ nullptr };
			hr = ibUpload->Map( 0, nullptr, &pIndexData );
			CHECK_HR( "Failed to map upload buffer.", hr, m_app->log );
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
			CHECK_HR( "Failed to create default vertex buffer.", hr, m_app->log );
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
			CHECK_HR( "Failed to create default vertex buffer.", hr, m_app->log );
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
		CHECK_HR( "Failed to close command list for Vertex Buffer upload..", hr, m_app->log );

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

		m_gpuMeshesR.push_back( gpuMesh );

		m_app->log( "[ Rasterization ] Vertex and index buffers uploaded to GPU." );
	}

	void WolfRenderer::CreateViewport() {
		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<FLOAT>(m_app->scene.settings.renderWidth);
		m_viewport.Height = static_cast<FLOAT>(m_app->scene.settings.renderHeight);
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;

		m_scissorRect.left = 0;
		m_scissorRect.top = 0;
		m_scissorRect.right = m_app->scene.settings.renderWidth;
		m_scissorRect.bottom = m_app->scene.settings.renderHeight;
		m_app->log( "[ Rasterization ] Viewport set up." );
	}

	void WolfRenderer::CreateConstantBuffers() {
		// Initialize to identity matrix
		DirectX::XMStoreFloat4x4( &dataR.camera.cbData.world, DirectX::XMMatrixIdentity() );
		DirectX::XMStoreFloat4x4( &dataR.camera.cbData.view, DirectX::XMMatrixIdentity() );
		DirectX::XMStoreFloat4x4( &dataR.camera.cbShadow.world, DirectX::XMMatrixIdentity() );

		Raster::Data& data = dataR;
		Raster::Camera& cam = data.camera;

		size_t cbSize{};

		cbSize =  sizeof( Raster::Camera::CameraDataCB );
		CreateCB( cbSize, cam.cameraCBRes, &cam.cameraCBMappedPtr, &cam.cbData );
		cbSize = sizeof( Raster::SceneDataCB );
		CreateCB( cbSize, m_sceneDataCBRes, &m_sceneDataCBMappedPtr, &data.sceneData );
		cbSize = sizeof( Raster::ScreenDataCB );
		CreateCB( cbSize, m_screenDataCBRes, &m_screenDataCBMappedPtr, &data.screenData );
		cbSize = sizeof( Raster::DirectionalLight::CB );
		CreateCB( cbSize, m_lightDataCBRes, &m_lightDataCBMappedPtr, &data.directionalLight.cb );
		cbSize = sizeof( Raster::LightMatricesCB );
		CreateCB( cbSize, m_lightMatricesCBRes, &m_lightMatricesCBMappedPtr, &data.lightMatrices );
		cbSize = sizeof( Raster::Camera::ShadowMapCamCB );
		CreateCB( cbSize, cam.shadowCBRes, &cam.shadowCBMappedPtr, &cam.cbShadow );
	}

	void WolfRenderer::CreateCB(
		size_t dataSize, ComPtr<ID3D12Resource>& outResource, UINT8** mappedPtr, const void* data
	) {
		// Constant buffer must be 256-byte aligned. This is a precaution, should already be 256 bytes.
		const UINT64 cbAlignedSize = (dataSize + 255) & ~255ULL;
		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD );
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer( cbAlignedSize );

		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &outResource )
		);
		CHECK_HR( "Failed to create constant buffer.", hr, m_app->log );

		// Map permanently for CPU writes
		hr = outResource->Map(
			0, nullptr, reinterpret_cast<void**>(mappedPtr) );
		CHECK_HR( "Failed to map constant buffer.", hr, m_app->log );

		memcpy( *mappedPtr, data, dataSize );
		m_app->log( "[ Rasterization ] Constant buffer created and mapped." );
	}

	void WolfRenderer::AddToTargetOffset( float dx, float dy ) {
		// Add mouse offset to target offset.
		// Clamp values to prevent offscreen value accumulation.
		Raster::Camera& cam = dataR.camera;

		CalculateViewportBounds();

		// Predict next target.
		float candidateX = cam.targetOffsetX + dx * cam.offsetXYSens;
		float candidateY = cam.targetOffsetY + dy * cam.offsetXYSens;

		cam.targetOffsetX = std::clamp( candidateX, -cam.boundsX, cam.boundsX );
		cam.targetOffsetY = std::clamp( candidateY, -cam.boundsY, cam.boundsY );
	}

	void WolfRenderer::AddToOffsetZ( float dz ) {
		dataR.camera.offsetZ += dz * dataR.camera.offsetZSens;
	}

	void WolfRenderer::AddToOffsetFOV( float angleRadians ) {
		dataR.camera.FOVAngle += angleRadians;

		// A value near 0 causes division by 0 and crashes the application.
		// Clamping the value stops FOV at max zoom. Adding it again makes it
		// go over to the negative numbers, causing inverted projection.
		if ( DirectX::XMScalarNearEqual( dataR.camera.FOVAngle, 0.f, 0.00001f * 2.f ) )
			dataR.camera.FOVAngle += angleRadians;
	}

	void WolfRenderer::AddToTargetRotation( float deltaAngleX, float deltaAngleY ) {
		float sensitivity = dataR.camera.rotSensMultiplier * 0.0001f;
		dataR.camera.targetRotationX += deltaAngleX * sensitivity;
		dataR.camera.targetRotationY += deltaAngleY * sensitivity;
	}

	void WolfRenderer::UpdateSmoothMotion() {
		Raster::Camera& cam{ dataR.camera };

		// Compute smoothing factor for movement (frame-rate independent).
		const float sTransFactor = 1.f - std::exp( -cam.smoothOffsetLerp * m_app->deltaTime );

		// Linear interpolation toward the target.
		cam.currOffsetX += (cam.targetOffsetX - cam.currOffsetX) * sTransFactor;
		cam.currOffsetY += (cam.targetOffsetY - cam.currOffsetY) * sTransFactor;

		// Compute smoothing factor for rotation (frame-rate independent).
		const float sRotFactor = 1.f - std::exp( -cam.smoothRotationLambda * m_app->deltaTime );

		// Linear interpolation toward the target.
		cam.currRotationX += (cam.targetRotationX - cam.currRotationX) * sRotFactor;
		cam.currRotationY += (cam.targetRotationY - cam.currRotationY) * sRotFactor;

		// Don't allow the center of the "screen sphere" to leave the screen
		// when moving around, independent of zoom and FOV.
		// Recalculate bounds to make object stay inside.
		CalculateViewportBounds();

		cam.currOffsetX = std::clamp( cam.currOffsetX, -cam.boundsX, cam.boundsX );
		cam.currOffsetY = std::clamp( cam.currOffsetY, -cam.boundsY, cam.boundsY );
	}

	void WolfRenderer::UpdateCameraMatricesR() {
		using namespace DirectX;
		Raster::Camera& cam{ dataR.camera };

		// Subtracting 0.5 from Y to place camera slightly above ground.
		XMMATRIX Trans = XMMatrixTranslation(
			cam.currOffsetX, cam.currOffsetY - 0.5f, cam.offsetZ );

		// Using Rotation data for X axis in Qt screen space for MatrixY in
		// WorldView and vice-versa. Negative currRotation inverts rotation direction.
		XMMATRIX Rot = XMMatrixRotationY( -cam.currRotationX ) *
			XMMatrixRotationX( -cam.currRotationY );

		// Rotation * translation - transform around world origin. Otherwise - around geometry origin.
		// This is NOT true View space. This makes the World rotate while the Camera stays still!
		XMMATRIX View{};
		if ( cam.coordinateSystem == Raster::CameraCoordinateSystem::Local )
			View = Rot * Trans;
		else if ( cam.coordinateSystem == Raster::CameraCoordinateSystem::World )
			View = Trans * Rot;

		XMMATRIX World = XMLoadFloat4x4( &cam.cbData.world );
		// Create a world-view matrix.
		DirectX::XMMATRIX WorldView = World * View;

		// Create projection matrix (for simulating a camera).
		// Swap nearZ and farZ to use "reverse-Z" for better precision.
		DirectX::XMMATRIX Proj = DirectX::XMMatrixPerspectiveFovLH(
			cam.FOVAngle, cam.aspectRatio, cam.farZ, cam.nearZ );

		XMStoreFloat4x4( &cam.cbData.world, World );
		XMStoreFloat4x4( &cam.cbData.view, View );
		XMStoreFloat4x4( &cam.cbData.projection, Proj );

		memcpy( cam.cameraCBMappedPtr, &cam.cbData, sizeof( cam.cbData ) );
	}

	void WolfRenderer::CalculateViewportBounds() {
		float depth = std::abs( dataR.camera.offsetZ );

		float halfHeight = depth * std::tan( dataR.camera.FOVAngle * 0.5f );
		float halfWidth = halfHeight * dataR.camera.aspectRatio;

		// If the object does not fit, lock movement instead of exploding.
		dataR.camera.boundsX = std::abs( halfWidth - dataR.camera.dummyObjectRadius );
		dataR.camera.boundsY = std::abs( halfHeight - dataR.camera.dummyObjectRadius );
	}

	void WolfRenderer::CreateShadowPassSRVAndHeap() {
		// Create SRV heap.
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		HRESULT hr{ m_device->CreateDescriptorHeap(
			&srvHeapDesc, IID_PPV_ARGS( &m_srvHeapShadowMap ) ) };
		CHECK_HR( "Failed to create shadow SRV heap.", hr, m_app->log );

		// Create SRV.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		// The depth buffer is created as D32_FLOAT. When reading a depth-only
		// texture as a shader resource, that single depth channel is usually
		// mapped to the Red (R) channel, hence R32.
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		// If set to TEXTURECUBE, the shader would expect to sample it using a
		// 3D vector (useful for point light shadows).
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;

		m_device->CreateShaderResourceView( m_shadowMapBuffer.Get(), &srvDesc,
			m_srvHeapShadowMap->GetCPUDescriptorHandleForHeapStart() );
	}

	void WolfRenderer::CreateShadowMap() {
		// DSV Heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		HRESULT hr{ m_device->CreateDescriptorHeap(
			&dsvHeapDesc, IID_PPV_ARGS( &m_dsvHeapShadowMap ) ) };
		CHECK_HR( "Failed to create DSV Heap.", hr, m_app->log );

		// Depth texture.
		D3D12_RESOURCE_DESC shadowMapDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R32_TYPELESS, // MUST be typeless depth -> float.
			dataR.lightParams.shadowMapSize,
			dataR.lightParams.shadowMapSize
		)};
		shadowMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = DXGI_FORMAT_D32_FLOAT;;
		clearValue.DepthStencil.Depth = 0.f; // reverse-Z
		clearValue.DepthStencil.Stencil = 0;

		D3D12_HEAP_PROPERTIES heapProps{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };

		hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&shadowMapDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS( &m_shadowMapBuffer )
		);

		// Create DSV.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		m_device->CreateDepthStencilView(
			m_shadowMapBuffer.Get(),
			&dsvDesc,
			m_dsvHeapShadowMap->GetCPUDescriptorHandleForHeapStart()
		);
		m_app->log( "[ Rasterization ] Shadow map created." );
	}

	void WolfRenderer::UpdateDirectionalLight() {
		using namespace DirectX;

		Raster::DirectionalLight& dirLight = dataR.directionalLight;

		// Direction (world-space).
		XMVECTOR dirWS = XMLoadFloat3( &dirLight.directionWS );
		dirWS = XMVector3Normalize( dirWS );

		XMStoreFloat3( &dirLight.cb.directionVS, dirWS );

		// Center shadow frustum around camera/world offset
		XMVECTOR target = XMVectorSet(
			dataR.camera.targetOffsetX,
			dataR.camera.targetOffsetY,
			dataR.camera.offsetZ,
			1.0f
		);

		// Position the light very far so it covers the whole scene.
		XMVECTOR lightDirWS = XMLoadFloat3( &dirLight.directionWS );
		lightDirWS = XMVector3Normalize( lightDirWS );

		XMVECTOR lightPos = target - lightDirWS * 500;

		// Build light view matrix.
		// Flickering starts if lightDir = (0, +-1, 0) and upVec = (0, 1, 0).
		// Dynamically change up vector to avoid this.
		constexpr float parallelThreshold = 0.99f;
		XMVECTOR worldUp = XMVectorSet( 0.f, 1.f, 0.f, 0.f );
		XMVECTOR alternateUp = XMVectorSet( 0.f, 0.f, 1.f, 0.f );

		float dotRes = std::fabsf( XMVectorGetX( XMVector3Dot( lightDirWS, worldUp ) ) );
		XMVECTOR upVec = (dotRes > parallelThreshold) ? alternateUp : worldUp;

		XMMATRIX lightView = XMMatrixLookAtLH( lightPos, target, upVec );

		// Build orthographic projection.
		XMMATRIX lightProj = XMMatrixOrthographicLH(
			dirLight.shadowExtent, dirLight.shadowExtent, dirLight.farZ, dirLight.nearZ );

		// Combine.
		XMMATRIX lightViewProj = lightView * lightProj;

		XMStoreFloat4x4( &dataR.lightMatrices.dirLightViewProjMatrix, lightViewProj );
	}

	void WolfRenderer::RenderShadowMapPass() {
		// Set shadow viewport.
		D3D12_VIEWPORT viewport{};
		viewport.Width  = static_cast<float>(dataR.lightParams.shadowMapSize);
		viewport.Height = static_cast<float>(dataR.lightParams.shadowMapSize);
		viewport.MinDepth = 0.f;
		viewport.MaxDepth = 1.f;

		D3D12_RECT scissor{ 0, 0, (LONG)viewport.Width, (LONG)viewport.Height };

		m_cmdList->RSSetViewports( 1, &viewport );
		m_cmdList->RSSetScissorRects( 1, &scissor );

		// Bind shadow DSV.
		m_dsvHandle = m_dsvHeapShadowMap->GetCPUDescriptorHandleForHeapStart();
		m_cmdList->OMSetRenderTargets( 0, nullptr, FALSE, &m_dsvHandle );

		m_cmdList->ClearDepthStencilView(
			m_dsvHeapShadowMap->GetCPUDescriptorHandleForHeapStart(),
			D3D12_CLEAR_FLAG_DEPTH,
			0.f, // Must match creation of clear value. 0.f because of reverse-Z.
			0,
			0,
			nullptr
		);

		m_cmdList->SetGraphicsRootSignature( m_pipeline->rootSignatureShadows.Get() );
		m_cmdList->SetPipelineState( m_pipeline->stateShadows.Get() );

		// Slot b0: Bind light matrix.
		m_cmdList->SetGraphicsRootConstantBufferView( 0, m_lightMatricesCBRes->GetGPUVirtualAddress() );
		memcpy( m_lightMatricesCBMappedPtr, &dataR.lightMatrices, sizeof( dataR.lightMatrices ) );

		// Slot b1: Bind world matrix.
		m_cmdList->SetGraphicsRootConstantBufferView(
			1, dataR.camera.shadowCBRes->GetGPUVirtualAddress() );

		memcpy(
			dataR.camera.shadowCBMappedPtr,
			&dataR.camera.cbShadow, sizeof( dataR.camera.cbShadow )
		);

		for ( const Raster::GPUMesh& mesh : m_gpuMeshesR ) {
			m_cmdList->IASetVertexBuffers( 0, 1, &mesh.vbView );
			m_cmdList->IASetIndexBuffer( &mesh.ibView );
			m_cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
			m_cmdList->DrawIndexedInstanced( mesh.indexCount, 1, 0, 0, 0 );
		}
	}

	void WolfRenderer::SetFacePassPSO( bool showBackfaces ) {
		dataR.facesPSO = showBackfaces ? m_pipeline->stateNoCull : m_pipeline->stateFaces;
	}
}