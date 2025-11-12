#include "Renderer.hpp"

#include <cassert>
#include <format>
#include <iostream>
#include <vector>

Renderer::Renderer() {
	PrepareForRendering();
}

Renderer::~Renderer() {
	if ( m_fenceEvent != nullptr )
		CloseHandle( m_fenceEvent );
}

void Renderer::Render() {}

void Renderer::PrepareForRendering() {
	CreateDevice();
	CreateCommandsManagers();

	CreateGPUTexture();
	CreateRenderTargetView();
	CreateReadbackBuffer();

	m_cmdAllocator->Reset();
	m_cmdList->Reset( m_cmdAllocator.Get(), nullptr );

	GenerateConstColorTexture();
}

void Renderer::AssignAdapter() {
	std::vector<WRL::ComPtr<IDXGIAdapter1>> adapters{};
	WRL::ComPtr<IDXGIAdapter1> adapter{};
	std::vector<HardwareID> hwIDs{};
	UINT adapterIdx{};

	while ( m_dxgiFactory->EnumAdapters1( adapterIdx, &adapter ) != DXGI_ERROR_NOT_FOUND ) {
		DXGI_ADAPTER_DESC1 desc{};
		HRESULT hr = adapter->GetDesc1( &desc );

		if ( FAILED( hr ) ) {
			log( std::format( "Failed to get description for adapter index {}",
				adapterIdx ) );
			continue;
		}

		// Skip Microsoft's Basic Render Driver (Software adapter)
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
		std::cerr << "Failed to get description for adapter index "
			<< adapterIdx << std::endl;
		return;
	} else {
		//? Choose the one with most memory? Use DedicatedVideoMemory from desc.
		log( "Multiple adapters found. Choosing the first one." );
		m_adapter = adapters.at( 0 );
	}

	DXGI_ADAPTER_DESC1 desc{};
	HRESULT hr = m_adapter->GetDesc1( &desc );
	assert( SUCCEEDED( hr ) );

	//? Make Logger work with wide strings if there are more uses ahead.
	std::wcout << std::format( L"Adapter: {}\n", desc.Description );
	log( std::format( "Dedicated Video Memory: {} MB",
		desc.DedicatedVideoMemory / (1024 * 1024) ) );
	log( std::format( "Device ID: {}", desc.DeviceId ) );
	log( std::format( "Vendor ID: {}", desc.VendorId ) );
}

void Renderer::CreateDevice() {
	HRESULT hr = CreateDXGIFactory1( IID_PPV_ARGS( &m_dxgiFactory ) );
	assert( SUCCEEDED( hr ) );

	AssignAdapter();

	hr = D3D12CreateDevice(
		m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create D3D12 Device. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}
	log( "Device created successfully!" );
}

void Renderer::CreateCommandsManagers() {
	HRESULT hr{};

	// Define a command queue descriptor
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

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create Command Queue. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	hr = m_device->CreateCommandAllocator( queueDesc.Type, IID_PPV_ARGS( &m_cmdAllocator ) );

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create Command Allocator. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	m_device->CreateCommandList(
		0,                    // NodeMask: 0 for single GPU systems.
		queueDesc.Type,       // Type: Must match the Command Queue type (usually DIRECT).
		m_cmdAllocator.Get(),   // Command Allocator: The memory pool to record commands into.
		nullptr, // Initial Pipeline State Object (PSO): Commonly set to nullptr at creation.
		IID_PPV_ARGS( &m_cmdList ) // The Interface ID and output pointer
	);

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create Command List. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	// Create a fence for GPU-CPU synchronization
	hr = m_device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &m_fence ) );

	if ( FAILED( hr ) ) {
		log( std::format( "Failed creating a Fence. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	// Create an event handle for the fence ( wait )
	m_fenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );

	if ( m_fenceEvent == nullptr ) {
		log( "Failed creating Fence Event.", LogLevel::Critical );
		return;
	}

	log( "Command List and Fence created successfully." );
}

void Renderer::CreateGPUTexture() {
	m_textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2D texture
	m_textureDesc.Width = 1920;                        // Width in pixels
	m_textureDesc.Height = 1080;                       // Height in pixels
	m_textureDesc.DepthOrArraySize = 1;                // Single texture (not an array)
	m_textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 32-bit RGBA format (8-bit per channel)
	m_textureDesc.SampleDesc.Count = 1;                // No multisampling
	m_textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // No special flags
	m_textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // Let the system choose the layout

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // Default heap for GPU access

	HRESULT hr{ m_device->CreateCommittedResource(
		&heapProps,           // Heap properties defining the memory type and location.
		D3D12_HEAP_FLAG_NONE, // Heap flags (none for standard usage).
		&m_textureDesc,        // Resource description (size, format, usage, etc.).
		D3D12_RESOURCE_STATE_RENDER_TARGET, // Initial resource state.
		nullptr,              // Optimized clear value (optional, nullptr if not needed).
		IID_PPV_ARGS( &m_renderTarget ) // The Interface ID and output pointer.
	) };

	if ( FAILED(hr) ) {
		log( std::format( "Failed to create GPU Resource. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}
}

void Renderer::CreateRenderTargetView() {
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // Render Target View heap
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // No special flags
	heapDesc.NodeMask = 0; // Single GPU

	HRESULT hr{ m_device->CreateDescriptorHeap(
		&heapDesc,                      // Descriptor heap description
		IID_PPV_ARGS( &m_descriptorHeap ) // The Interface ID and output pointer
	) };

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create Descriptor Heap. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	m_rtvHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

	m_device->CreateRenderTargetView(
		m_renderTarget.Get(), // The resource for which the RTV is created.
		nullptr,              // RTV description (nullptr for default).
		m_rtvHandle           // CPU descriptor handle where the RTV will be stored.
	);
}

void Renderer::GenerateConstColorTexture() {
	m_cmdList->OMSetRenderTargets(
		1,          // Number of render targets
		&m_rtvHandle, // Array of render target handles
		FALSE,      // Whether to use a single handle for all render targets
		nullptr     // Optional depth-stencil view handle
	);

	const float clearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };

	m_cmdList->ClearRenderTargetView(
		m_rtvHandle,  // Render target handle
		clearColor, // Clear color
		0,          // Number of rectangles to clear
		nullptr     // Optional array of rectangles to clear
	);
}

void Renderer::CreateReadbackBuffer() {
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	UINT numRows{};
	UINT64 rowSizeInBytes{};
	UINT64 totalBytes{};

	m_device->GetCopyableFootprints(
		&m_textureDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes );

	D3D12_RESOURCE_DESC readbackDesc{};
	readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // Buffer
	readbackDesc.Width = totalBytes;
	readbackDesc.Height = 1;
	// Polymorphic format as the source format will be used
	readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
	readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	readbackDesc.SampleDesc.Count = 1;

	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_READBACK;

	HRESULT hr{ m_device->CreateCommittedResource(
		&heapProps,           // Heap properties defining the memory type and location.
		D3D12_HEAP_FLAG_NONE, // Heap flags (none for standard usage).
		&readbackDesc,        // Resource description (size, format, usage, etc.).
		D3D12_RESOURCE_STATE_COPY_DEST, // Initial resource state.
		nullptr,              // Optimized clear value (optional, nullptr if not needed).
		IID_PPV_ARGS( &m_readbackBuff ) // The Interface ID and output pointer.
	) };

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create GPU Resource. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	CopyTexture( footprint );
	ExecuteCommandList();
	ReadTextureData( totalBytes, footprint );
}


void Renderer::CopyTexture( D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint ) {
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Transition.pResource = m_renderTarget.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	m_cmdList->ResourceBarrier( 1, &barrier );

	D3D12_TEXTURE_COPY_LOCATION srcLocation{};
	srcLocation.pResource = m_renderTarget.Get();
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dstLocation{};
	dstLocation.pResource = m_readbackBuff.Get();
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dstLocation.PlacedFootprint = footprint;

	m_cmdList->CopyTextureRegion( &dstLocation, 0, 0, 0, &srcLocation, nullptr );

	// D3D12_RESOURCE_Barrier::Transition can be used instead
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	
	m_cmdList->ResourceBarrier( 1, &barrier );
	HRESULT hr = m_cmdList->Close();

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to close command list!  HRESULT: {:#x}", hr ),
			LogLevel::Error );
		// Should be handled here.
		return;
	}
}

void Renderer::ExecuteCommandList() {
	//!< Array of command lists to be executed by the GPU.
	ID3D12CommandList* ppCommandLists[] = { m_cmdList.Get() };

	m_cmdQueue->ExecuteCommandLists(
		_countof( ppCommandLists ), // Number of command lists
		ppCommandLists              // Array of command lists
	);

	// Increment for every operation
	m_cmdQueue->Signal( m_fence.Get(), ++m_fenceValue );

	// Wait for the GPU to finish
	if ( m_fence->GetCompletedValue() < m_fenceValue ) {
		m_fence->SetEventOnCompletion( m_fenceValue, m_fenceEvent );
		WaitForSingleObject( m_fenceEvent, INFINITE );
	}
}

void Renderer::ReadTextureData(
	UINT64 totalBytes, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint ) {
	void* pData;
	D3D12_RANGE readRange{ 0, totalBytes };
	HRESULT hr = m_readbackBuff->Map( 0, &readRange, &pData );

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to map GPU data to CPU pointer!  HRESULT: {:#x}", hr ),
			LogLevel::Error );
		// Should be handled here.
		return;
	}

	// pData now holds the pointer to the texture data!
	// Use the RowPitch (footprint.Footprint.RowPitch) when reading the pixel data
	// as it's  larger than the texture width * pixel size due to alignment.
	const uint8_t* byteData = static_cast<uint8_t*>(pData);

	UINT64 textureWidth{ m_textureDesc.Width };
	UINT64 textureHeight{ m_textureDesc.Height };
	UINT rowPitch{ footprint.Footprint.RowPitch };

	// Create a buffer to hold the final, unpacked image without padding.
	std::vector<uint8_t> finalImage( textureWidth * textureHeight * 4 );

	// The size of one pixel in bytes (RGBA8 = 4 bytes)
	const size_t pixelSize{ 4 };

	// The size of the destination row without padding
	const size_t destRowSize{ textureWidth * pixelSize };

	for ( UINT y{}; y < textureHeight; ++y ) {
		// Source row start in the mapped data
		const uint8_t* p_srcRow = byteData + y * rowPitch;
		// Destination row start in the final image buffer
		uint8_t* destRow = finalImage.data() + y * destRowSize;
		// Copy the row data from source to destination
		std::memcpy( destRow, p_srcRow, destRowSize );
	}

	// Relenquish access to the resource.
	D3D12_RANGE writtenRange{ 0, 0 };
	m_readbackBuff->Unmap( 0, &writtenRange );
}
