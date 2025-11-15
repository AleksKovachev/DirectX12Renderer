#include "Renderer.hpp"

#include <cassert>
#include <format>
#include <iostream>
#include <vector>

Renderer::Renderer() {
	PrepareForRendering();
}

void Renderer::Render() {}

void Renderer::PrepareForRendering() {
	CreateDevice();
	CreateCommandsManagers();
}

void Renderer::AssignAdapter() {
	std::vector<ComPtr<IDXGIAdapter1>> adapters{};
	ComPtr<IDXGIAdapter1> m_adapter{};
	std::vector<HardwareID> hwIDs{};
	UINT adapterIdx{};

	while ( m_dxgiFactory->EnumAdapters1( adapterIdx, &m_adapter ) != DXGI_ERROR_NOT_FOUND ) {
		DXGI_ADAPTER_DESC1 desc{};
		HRESULT hr = m_adapter->GetDesc1( &desc );

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
		adapters.push_back( m_adapter );
		++adapterIdx;
	}

	if ( adapters.size() == 1 ) {
		this->m_adapter = adapters.at( 0 );
	} else if ( adapters.size() < 1 ) {
		std::cerr << "Failed to get description for adapter index "
			<< adapterIdx << std::endl;
		return;
	} else {
		//? Choose the one with most memory? Better criteria?
		log( "Multiple adapters found. Choosing the first one." );
		this->m_adapter = adapters.at( 0 );
	}

	DXGI_ADAPTER_DESC1 desc{};
	HRESULT hr = this->m_adapter->GetDesc1( &desc );
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

	// Initial State: Close the Command List.
	// Command lists are created in the "recording" state. Since there isn't
	// anything recorded yet, it's best practice to close it immediately.
	hr = m_cmdList->Close();
	if ( FAILED( hr ) ) {
		std::cerr << "Error: Failed to close Command List." << std::endl;
		log( "Failed to close Command List.", LogLevel::Error );
		// Should be handled here.
	}
	log( "Command List closed for initial state setup." );
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

	if ( FAILED( hr ) ) {
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
