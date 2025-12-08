#include "Renderer.hpp"
#include "utils.hpp" // checkHR, wideStrToUTF8

#include "d3dx12_core.h"

#include "ConstColor.hlsl.h"
#include "ConstColorVS.hlsl.h"

#include <cassert>
#include <cmath> // cosf, sinf
#include <format>
#include <fstream>
#include <iostream>
#include <vector>


//!< Simple struct to hold the unique hardware identifier (Vendor ID + Device ID).
struct HardwareID {
	UINT DeviceId;
	UINT VendorId;

	//!< Required for use in std::set
	bool operator==( const HardwareID& other ) const {
		if ( DeviceId == other.DeviceId && VendorId == other.VendorId )
			return true;
		return false;
	}
};

namespace Core {
	WolfRenderer::WolfRenderer( int renderWidth, int renderHeight, UINT bufferCount )
		: m_renderWidth{ renderWidth }, m_renderHeight{ renderHeight }, m_bufferCount{ bufferCount } {
		log( "WolfRenderer instance created." );
#ifdef _DEBUG
//#define D3D12_ENABLE_DEBUG_LAYER 1
	// Enable the D3D12 debug layer.
		ID3D12Debug* debugController;
		if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &debugController ) ) ) ) {
			debugController->EnableDebugLayer();
			debugController->Release();
		}
#endif // _DEBUG
	// Fill the render targets and RTV handles vectors.
		for ( UINT i{}; i < bufferCount; ++i ) {
			m_renderTargets.emplace_back();
			m_rtvHandles.emplace_back();
		}
	}

	WolfRenderer::~WolfRenderer() {
		log( "\n    => Closing application." );
		if ( m_fenceEvent != nullptr )
			CloseHandle( m_fenceEvent );
	}

	void WolfRenderer::SetLoggerMinLevel( LogLevel level ) {
		log.SetMinLevel( level );
	}

	void WolfRenderer::PrepareForRendering( HWND hWnd ) {
		if ( m_isPrepared ) {
			log( "GPU already prepared." );
			return;
		}
		log( "Starting renderer initialization..." );

		CreateDevice(); // Creates Factory, Adapter, Device
		CreateCommandsManagers(); // Creates Queue, Allocator, List (and closes it)
		CreateSwapChain( hWnd );
		CreateDescriptorHeapForSwapChain();
		CreateRenderTargetViewsFromSwapChain();

		CreateFence();

		CreateVertexBuffer();
		CreateRootSignature();
		CreatePipelineState();
		CreateViewport();

		m_isPrepared = true;
	}

	void WolfRenderer::RenderFrame() {
		FrameBegin();

		m_cmdList->SetGraphicsRootSignature( m_rootSignature.Get() );

		m_cmdList->SetGraphicsRoot32BitConstant( 0, m_frameIdx, 0 );

		m_cmdList->SetPipelineState( m_pipelineState.Get() );

		// IA stands for Input Assembler
		m_cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_cmdList->IASetVertexBuffers( 0, 1, &m_vbView );

		m_cmdList->RSSetViewports( 1, &m_viewport );
		m_cmdList->RSSetScissorRects( 1, &m_scissorRect );

		m_cmdList->DrawInstanced( 3, 1, 0, 0 );

		FrameEnd();
	}

	void WolfRenderer::WriteImageToFile( const char* fileName ) {
		void* renderData;
		HRESULT hr = m_readbackBuff->Map( 0, nullptr, &renderData );
		checkHR( "Failed to map GPU data to CPU pointer!", hr, log, LogLevel::Error );

		// renderData now holds the pointer to the texture data!
		std::ofstream fileStream( fileName, std::ios::binary );
		if ( !fileStream.is_open() ) {
			log( "Couldn't open file.", LogLevel::Error );
			return;
		}

		UINT64 textureWidth{ m_textureDesc.Width };
		UINT64 textureHeight{ m_textureDesc.Height };
		UINT rowPitch{ m_renderTargetFootprint.Footprint.RowPitch };

		// Use the RowPitch (footprint.Footprint.RowPitch) when reading the pixel data
		// as it's  larger than the texture width * pixel size due to alignment.
		uint8_t* byteData = reinterpret_cast<uint8_t*>(renderData);
		const uint8_t RGBA_COLOR_CHANNELS_COUNT{ 4 };

		fileStream << "P6 ";
		fileStream << textureWidth << " " << textureHeight << " ";
		fileStream << "255\n";

		for ( UINT rowIdx{}; rowIdx < textureHeight; ++rowIdx ) {
			uint8_t* rowData = byteData + rowIdx * rowPitch;
			for ( UINT64 colIdx{}; colIdx < textureWidth; ++colIdx ) {
				uint8_t* pixelData = rowData + colIdx * RGBA_COLOR_CHANNELS_COUNT;
				unsigned char r = static_cast<unsigned char>( pixelData[0] );
				unsigned char g = static_cast<unsigned char>( pixelData[1] );
				unsigned char b = static_cast<unsigned char>( pixelData[2] );
				// Skip pixelData[3] (alpha channel)
				fileStream.write( reinterpret_cast<const char*>( &r ), 1ll );
				fileStream.write( reinterpret_cast<const char*>( &g ), 1ll );
				fileStream.write( reinterpret_cast<const char*>( &b ), 1ll );
			}
		}

		fileStream.close();

		// Relenquish access to the resource.
		m_readbackBuff->Unmap( 0, nullptr );
	}

	void WolfRenderer::UnmapReadback() {
		log( "Unmapping readback buffer!" );
		m_readbackBuff->Unmap( 0, nullptr );
	}

	void WolfRenderer::StopRendering() {
		log( "Stopping renderer!" );
		WaitForGPURenderFrame();
	}

	void WolfRenderer::CreateDevice() {
		HRESULT hr = CreateDXGIFactory1( IID_PPV_ARGS( &m_dxgiFactory ) );
		checkHR( "Failed to create DXGI Factory.", hr, log );
		log( "Factory created." );

		AssignAdapter();

		hr = D3D12CreateDevice(
			m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( &m_device ) );
		checkHR( "Failed to create D3D12 Device.", hr, log );

		log( "Device created successfully!" );
	}

	void WolfRenderer::AssignAdapter() {
		std::vector<ComPtr<IDXGIAdapter1>> adapters{};
		ComPtr<IDXGIAdapter1> adapter{};
		std::vector<HardwareID> hwIDs{};
		UINT adapterIdx{};

		while ( m_dxgiFactory->EnumAdapters1( adapterIdx, &adapter ) != DXGI_ERROR_NOT_FOUND ) {
			DXGI_ADAPTER_DESC1 desc{};
			HRESULT hr = adapter->GetDesc1( &desc );

			checkHR( std::format( "Failed to get description for adapter index {}", adapterIdx ), hr, log );

			// Skip Microsoft's Basic Render Driver (Software adapter).
			if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) {
				++adapterIdx;
				continue;
			}

			bool exists{ false };
			HardwareID currHWID{ desc.DeviceId, desc.VendorId };
			for ( HardwareID hwID : hwIDs ) {
				if ( currHWID == hwID ) {
					exists = true;
					break;
				}
			}

			if ( exists ) {
				++adapterIdx;
				continue;
			}

			hwIDs.emplace_back( desc.DeviceId, desc.VendorId );
			adapters.push_back( adapter );
			++adapterIdx;
		}

		if ( adapters.size() == 1 ) {
			m_adapter = adapters.at( 0 );
		} else if ( adapters.size() < 1 ) {
			log( "Failed to get description any adapter.", LogLevel::Critical );
			return;
		} else {
			//? Choose the one with most memory? Use DedicatedVideoMemory from desc.
			log( "Multiple adapters found. Choosing the first one." );
			m_adapter = adapters.at( 0 );
		}

		DXGI_ADAPTER_DESC1 desc{};
		HRESULT hr = m_adapter->GetDesc1( &desc );
		checkHR( "Failed to get adapter description.", hr, log );

		log( wideStrToUTF8( std::format( L"Adapter: {}\n", desc.Description ) ) );
		log( std::format( "Dedicated Video Memory: {} MB",
			desc.DedicatedVideoMemory / (1024 * 1024) ) );
		log( std::format( "Device ID: {}", desc.DeviceId ) );
		log( std::format( "Vendor ID: {}", desc.VendorId ) );
	}

	void WolfRenderer::CreateCommandsManagers() {
		HRESULT hr{};

		// Define a command queue descriptor.
		D3D12_COMMAND_QUEUE_DESC queueDesc{};

		// Type: Specifies the kind of work the queue handles. DIRECT is the most general one.
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		// Priority: Usually D3D12_COMMAND_QUEUE_PRIORITY_NORMAL.
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

		// Flags: D3D12_COMMAND_QUEUE_FLAG_NONE for a standard queue. 
		// Other options allow for things like debug or asynchronous execution.
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		// NodeMask: For multi-adapter systems, specifies which GPU node to use. 0 for single GPU.
		queueDesc.NodeMask = 0;

		hr = m_device->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( &m_cmdQueue ) );
		checkHR( "Failed to create Command Queue.", hr, log );

		hr = m_device->CreateCommandAllocator( queueDesc.Type, IID_PPV_ARGS( &m_cmdAllocator ) );
		checkHR( "Failed to create Command Allocator.", hr, log );

		hr = m_device->CreateCommandList(
			0,                    // NodeMask: 0 for single GPU systems.
			queueDesc.Type,       // Type: Must match the Command Queue type (usually DIRECT).
			m_cmdAllocator.Get(),   // Command Allocator: The memory pool to record commands into.
			nullptr, // Initial Pipeline State Object (PSO): Commonly set to nullptr at creation.
			IID_PPV_ARGS( &m_cmdList ) // The Interface ID and output pointer.
		);
		checkHR( "Failed to create Command List.", hr, log );

		// Good practice to close the command list right after creation. Reset() opens it.
		hr = m_cmdList->Close();
		checkHR( "Failed to close the Command List.", hr, log );

		log( "Command List created." );
	}

	void WolfRenderer::CreateFence() {
		// Create a fence for GPU-CPU synchronization
		HRESULT hr = m_device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &m_fence ) );

		checkHR( "Failed creating a Fence.", hr, log );

		// Create an event handle for the fence ( wait )
		m_fenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );

		if ( m_fenceEvent == nullptr ) {
			log( "Failed creating Fence Event.", LogLevel::Critical );
			return;
		}

		log( "Fence and fence event created." );
	}

	void WolfRenderer::CreateGPUTexture() {
		m_textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2D texture.
		m_textureDesc.Width = m_renderWidth;               // Width in pixels.
		m_textureDesc.Height = m_renderHeight;             // Height in pixels.
		m_textureDesc.DepthOrArraySize = 1;                // Single texture (not an array).
		m_textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 32-bit RGBA format (8-bit per channel).
		m_textureDesc.SampleDesc.Count = 1;                // No multisampling
		m_textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // Let the system choose the layout.
		m_textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // No special flags.

		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // Default heap for GPU access.

		HRESULT hr{ m_device->CreateCommittedResource(
			&heapProps,           // Heap properties defining the memory type and location.
			D3D12_HEAP_FLAG_NONE, // Heap flags (none for standard usage).
			&m_textureDesc,        // Resource description (size, format, usage, etc.).
			D3D12_RESOURCE_STATE_RENDER_TARGET, // Initial resource state.
			nullptr,              // Optimized clear value (optional, nullptr if not needed).
			IID_PPV_ARGS( &m_renderTarget ) // The Interface ID and output pointer.
		) };

		checkHR( "Failed to create GPU Resource.", hr, log );
		log( "GPU HEAP and Texture created." );
	}

	void WolfRenderer::CreateReadbackBuffer() {
		UINT64 readbackBufferSize{};

		m_device->GetCopyableFootprints( &m_textureDesc, 0, 1, 0,
			&m_renderTargetFootprint, nullptr, nullptr, &readbackBufferSize );

		D3D12_HEAP_PROPERTIES readbackHeapProps{};
		readbackHeapProps.Type = D3D12_HEAP_TYPE_READBACK;

		D3D12_RESOURCE_DESC readbackDesc{};
		readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // Buffer.
		readbackDesc.Width = readbackBufferSize;
		readbackDesc.Height = 1;
		readbackDesc.DepthOrArraySize = 1;
		readbackDesc.MipLevels = 1;
		readbackDesc.SampleDesc.Count = 1;
		readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		// Polymorphic format as the source format will be used.
		readbackDesc.Format = DXGI_FORMAT_UNKNOWN;

		HRESULT hr{ m_device->CreateCommittedResource(
			&readbackHeapProps,   // Heap properties defining the memory type and location.
			D3D12_HEAP_FLAG_NONE, // Heap flags (none for standard usage).
			&readbackDesc,        // Resource description (size, format, usage, etc.).
			D3D12_RESOURCE_STATE_COPY_DEST, // Initial resource state.
			nullptr,              // Optimized clear value (optional, nullptr if not needed).
			IID_PPV_ARGS( &m_readbackBuff ) // The Interface ID and output pointer.
		) };
		checkHR( "Failed to create GPU Resource.", hr, log );

		log( "Readback buffer created." );
	}

	void WolfRenderer::ResetCommandAllocatorAndList() {
		HRESULT hr = m_cmdAllocator->Reset();
		assert( SUCCEEDED( hr ) );
		//checkHR( "Failed resetting command allocator.", hr, log );

		hr = m_cmdList->Reset( m_cmdAllocator.Get(), nullptr );
		assert( SUCCEEDED( hr ) );
		//checkHR( "Failed resetting command list.", hr, log );
	}

	void WolfRenderer::CopyTexture() {
		// Create a Resource Barrier to transition from Render Target to Copy Source.
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTarget.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		m_cmdList->ResourceBarrier( 1, &barrier );

		// Create a Copy Texture Region GPU command.
		D3D12_TEXTURE_COPY_LOCATION src{};
		src.pResource = m_renderTarget.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		src.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION dst{};
		dst.pResource = m_readbackBuff.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dst.PlacedFootprint = m_renderTargetFootprint;

		m_cmdList->CopyTextureRegion( &dst, 0, 0, 0, &src, nullptr );

		// D3D12_RESOURCE_Barrier::Transition can be used instead.
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_cmdList->ResourceBarrier( 1, &barrier );

		HRESULT hr = m_cmdList->Close();
		checkHR( "Failed to close command list!", hr, log, LogLevel::Error );

		log( "Texture copy commands added. Command list closed." );
	}

	void WolfRenderer::WaitForGPURenderFrame() {
		//log( "Waiting for GPU to render frame." );
		// Wait for the GPU to finish
		if ( m_fence->GetCompletedValue() < m_fenceValue ) {
			HRESULT hr = m_fence->SetEventOnCompletion( m_fenceValue, m_fenceEvent );
			assert( SUCCEEDED( hr ) );
			//checkHR( "Failed setting fence value on GPU competion.", hr, log );
			WaitForSingleObject( m_fenceEvent, INFINITE );
		}
	}

	void WolfRenderer::FrameBegin() {
		ResetCommandAllocatorAndList();

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		m_cmdList->ResourceBarrier( 1, &barrier );

		// Set which Render Target will be used for rendering.
		m_cmdList->OMSetRenderTargets( 1, &m_rtvHandles[m_scFrameIdx], FALSE, nullptr );
		float greenBG[] = { 0.0f, 1.0f, 0.0f, 1.0f };
		m_cmdList->ClearRenderTargetView( m_rtvHandles[m_scFrameIdx], greenBG, 0, nullptr );
	}

	void WolfRenderer::FrameEnd() {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_cmdList->ResourceBarrier( 1, &barrier );

		HRESULT hr{ m_cmdList->Close() };
		assert( SUCCEEDED( hr ) ); // checkHR( "Failed to close command list!", hr, log, LogLevel::Error );

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

		// Present the frame.
		hr = m_swapChain->Present( 0, 0 ); // Sync Interval: 1 to enable VSync.
		assert( SUCCEEDED( hr ) ); // checkHR( "Failed to present the frame!", hr, log, LogLevel::Error );

		// Increment m_fenceValue for every operation
		m_cmdQueue->Signal( m_fence.Get(), ++m_fenceValue );

		WaitForGPURenderFrame();

		++m_frameIdx;
		m_scFrameIdx = m_swapChain->GetCurrentBackBufferIndex();
	}

	void WolfRenderer::CreateSwapChain( HWND hWnd ) {
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
		swapChainDesc.Width = m_renderWidth;
		swapChainDesc.Height = m_renderHeight;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 32-bit color
		swapChainDesc.BufferCount = m_bufferCount; // Double buffering
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1; // No multi-sampling
		//swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Enable Tearing.

		ComPtr<IDXGISwapChain1> swapChain1;
		HRESULT hr = m_dxgiFactory->CreateSwapChainForHwnd(
			m_cmdQueue.Get(), // The command queue to associate with the swap chain
			hWnd,             // The window handle
			&swapChainDesc,   // The swap chain description
			nullptr,          // No full-screen descriptor
			nullptr,          // No restrict to output
			&swapChain1       // The resulting swap chain
		);
		checkHR( "Failed to create a Swap Chain.", hr, log );

		hr = swapChain1->QueryInterface( IID_PPV_ARGS( &m_swapChain ) );
		checkHR( "Failed to convert Swap Chain output to newer version.", hr, log );

		log( "Swap Chain created successfully!" );
	}

	void WolfRenderer::CreateDescriptorHeapForSwapChain() {
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
		rtvHeapDesc.NumDescriptors = m_bufferCount; // Double buffering
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		HRESULT hr = m_device->CreateDescriptorHeap(
			&rtvHeapDesc,
			IID_PPV_ARGS( &m_rtvHeap )
		);
		checkHR( "Failed creating a descriptor heap for the swap chain.", hr, log );
		log( "Descriptor heap created for the swap chain." );

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
	}

	void WolfRenderer::CreateRenderTargetViewsFromSwapChain() {
		for ( UINT scBuffIdx{}; scBuffIdx < m_bufferCount; ++scBuffIdx ) {
			const HRESULT hr{ m_swapChain->GetBuffer(
				scBuffIdx,
				IID_PPV_ARGS( &m_renderTargets[scBuffIdx] )
			) };
			checkHR( "Failed getting a buffer.", hr, log );
			log( "Successfully got a buffer." );

			m_rtvHandles[scBuffIdx] = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
			m_rtvHandles[scBuffIdx].ptr += scBuffIdx * m_rtvDescriptorSize;

			m_device->CreateRenderTargetView(
				m_renderTargets[scBuffIdx].Get(),
				nullptr,
				m_rtvHandles[scBuffIdx]
			);
		}
		log( "Render target views created successfully." );
	}

	void WolfRenderer::CreateVertexBuffer() {
		Vertex triangleVertices[] = {
			{  0.0f,  0.5f },
			{  0.5f, -0.5f },
			{ -0.5f, -0.5f }
		};
		const UINT vertSize{ sizeof( triangleVertices ) };

		// Create the "Intermediate" Upload Buffer (Staging).
		ComPtr<ID3D12Resource> uploadBuffer{ nullptr };

		// Upload heap used so the CPU can write to it.
		D3D12_HEAP_PROPERTIES heapPropsUp{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ) };
		D3D12_RESOURCE_DESC bufferDescUp{ CD3DX12_RESOURCE_DESC::Buffer( vertSize ) };

		HRESULT hr{ m_device->CreateCommittedResource(
			&heapPropsUp,
			D3D12_HEAP_FLAG_NONE,
			&bufferDescUp,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &uploadBuffer )
		) };
		checkHR( "Failed to create upload vertex buffer.", hr, log );

		// Copy data from CPU to Upload Buffer
		void* pVertexData;
		hr = uploadBuffer->Map( 0, nullptr, &pVertexData );
		checkHR( "Failed to map upload buffer.", hr, log );
		memcpy( pVertexData, triangleVertices, vertSize );
		uploadBuffer->Unmap( 0, nullptr );

		// Create the destination Vertex Buffer (Default Heap).
		// Default heap used (GPU VRAM) that CPU can't access directly.
		D3D12_HEAP_PROPERTIES heapPropsDf{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };
		D3D12_RESOURCE_DESC bufferDescDf{ CD3DX12_RESOURCE_DESC::Buffer( vertSize ) };

		hr = m_device->CreateCommittedResource(
			&heapPropsDf,
			D3D12_HEAP_FLAG_NONE,
			&bufferDescDf,
			// Start in COPY_DEST so data can be copied to it immediately.
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS( &m_vertexBuffer )
		);
		checkHR( "Failed to create default vertex buffer.", hr, log );

		// Name the buffer for easier debugging using PIX/NSIGHT.
		m_vertexBuffer->SetName( L"Vertex Buffer Default Resource" );

		ResetCommandAllocatorAndList();

		// Copy data.
		m_cmdList->CopyBufferRegion( m_vertexBuffer.Get(), 0, uploadBuffer.Get(), 0, vertSize );

		// Transition the Default Buffer from COPY_DEST to VERTEX_AND_CONSTANT_BUFFER
		// so the Input Assembled (IA) can read it during rendering.
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_vertexBuffer.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

		m_cmdList->ResourceBarrier( 1, &barrier );

		hr = m_cmdList->Close();
		checkHR( "Failed to close command list for Vertex Buffer upload..", hr, log );

		// Execute the commands
		ID3D12CommandList* ppCommandLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

		// Wait for the GPU to finish the copy BEFORE the 'uploadBuffer' ComPtr
		// goes out of scope. Otherwise, is's destroyed while the GPU is
		// trying to read from it, causing a crash/device removal.
		m_cmdQueue->Signal( m_fence.Get(), ++m_fenceValue );
		WaitForGPURenderFrame();

		m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vbView.StrideInBytes = sizeof( Vertex );
		m_vbView.SizeInBytes = vertSize;

		log( "Vertex Buffer successfully uploaded to GPU Default Heap." );
	}

	void WolfRenderer::CreateRootSignature() {
		D3D12_ROOT_PARAMETER rootParams[1];
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].Constants.ShaderRegister = 0;
		rootParams[0].Constants.RegisterSpace = 0;
		rootParams[0].Constants.Num32BitValues = 1;

		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSignatureDesc.NumParameters = _countof( rootParams );
		rootSignatureDesc.pParameters = rootParams;

		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSignatureDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob
		);

		if ( errorBlob ) {
			std::string msg{ std::format( "Root Signature Error: {}",
				(char*)errorBlob->GetBufferPointer() ) };
			log( msg, LogLevel::Error );
		}
		checkHR( "Failed to serialize root signature.", hr, log );

		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &m_rootSignature )
		);
		checkHR( "CreateRootSignature failed.", hr, log );
		log( "Root Signature created successfully." );
	}

	void WolfRenderer::CreatePipelineState() {
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

		D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.PS = { g_const_color, _countof( g_const_color ) };
		psoDesc.VS = { g_const_color_vs, _countof( g_const_color_vs ) };
		psoDesc.InputLayout = { inputLayout, _countof( inputLayout ) };
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		psoDesc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		HRESULT hr = m_device->CreateGraphicsPipelineState(
			&psoDesc,
			IID_PPV_ARGS( &m_pipelineState )
		);
		checkHR( "Failed to create pipeline state.", hr, log );
	}

	void WolfRenderer::CreateViewport() {
		m_viewport.TopLeftX = 0.0f;
		m_viewport.TopLeftY = 0.0f;
		m_viewport.Width = static_cast<FLOAT>(m_renderWidth);
		m_viewport.Height = static_cast<FLOAT>(m_renderHeight);
		m_viewport.MinDepth = 0.0f;
		m_viewport.MaxDepth = 1.0f;

		m_scissorRect.left = 0;
		m_scissorRect.top = 0;
		m_scissorRect.right = m_renderWidth;
		m_scissorRect.bottom = m_renderHeight;
	}

}