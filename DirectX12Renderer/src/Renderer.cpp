#include "Renderer.hpp"

#include <cassert>
#include <format>
#include <iostream>
#include <vector>

Renderer::Renderer() {
	PrepareForRendering();
}

Renderer::~Renderer() {
	cmdList->Release();
	cmdList = nullptr;
	cmdAllocator->Release();
	cmdAllocator = nullptr;
	cmdQueue->Release();
	cmdQueue = nullptr;
	d3d12Device->Release();
	d3d12Device = nullptr;
	dxgiFactory->Release();
	dxgiFactory = nullptr;
}

void Renderer::Render() {}

void Renderer::PrepareForRendering() {
	CreateDevice();
	CreateCommandsManagers();
}

void Renderer::AssignAdapter() {
	std::vector<IDXGIAdapter1*> adapters{};
	IDXGIAdapter1* adapter{};
	std::vector<HardwareID> hwIDs{};
	UINT adapterIdx{};

	while ( dxgiFactory->EnumAdapters1( adapterIdx, &adapter ) != DXGI_ERROR_NOT_FOUND ) {
		DXGI_ADAPTER_DESC1 desc{};
		HRESULT hr = adapter->GetDesc1( &desc );

		if ( FAILED( hr ) ) {
			log( std::format( "Failed to get description for adapter index {}",
				adapterIdx ) );
			adapter->Release(); // Cleanup
			adapter = nullptr;
			continue;
		}

		// Skip Microsoft's Basic Render Driver (Software adapter)
		if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) {
			adapter->Release(); // Cleanup
			adapter = nullptr;
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
			adapter->Release(); // Cleanup
			adapter = nullptr;
			++adapterIdx;
			continue;
		}

		hwIDs.emplace_back( desc.DeviceId, desc.VendorId );
		adapters.push_back( adapter );
		++adapterIdx;
	}

	if ( adapters.size() == 1 ) {
		this->adapter = adapters.at( 0 );
	} else if ( adapters.size() < 1 ) {
		std::cerr << "Failed to get description for adapter index "
			<< adapterIdx << std::endl;
		return;
	} else {
		//? Choose the one with most memory? Better criteria?
		log( "Multiple adapters found. Choosing the first one." );
		this->adapter = adapters.at( 0 );
	}

	DXGI_ADAPTER_DESC1 desc{};
	HRESULT hr = this->adapter->GetDesc1( &desc );
	assert( SUCCEEDED( hr ) );

	//? Make Logger work with wide strings if there are more uses ahead.
	std::wcout << std::format( L"Adapter: {}\n", desc.Description );
	log( std::format( "Dedicated Video Memory: {} MB",
		desc.DedicatedVideoMemory / (1024 * 1024) ) );
	log( std::format( "Device ID: {}", desc.DeviceId ) );
	log( std::format( "Vendor ID: {}", desc.VendorId ) );
}

void Renderer::CreateDevice() {
	HRESULT hr = CreateDXGIFactory1( IID_PPV_ARGS( &dxgiFactory ) );
	assert( SUCCEEDED( hr ) );

	AssignAdapter();

	hr = D3D12CreateDevice(
		adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( &d3d12Device ) );

	// Adapter is no longer needed and needs to be released.
	// Device holds its own reference to it.
	adapter->Release();
	adapter = nullptr;

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

	hr = d3d12Device->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( &cmdQueue ) );

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create Command Queue. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	hr = d3d12Device->CreateCommandAllocator( queueDesc.Type, IID_PPV_ARGS( &cmdAllocator ) );

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create Command Allocator. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	d3d12Device->CreateCommandList(
		0,              // NodeMask: 0 for single GPU systems.
		queueDesc.Type, // Type: Must match the Command Queue type (usually DIRECT).
		cmdAllocator,   // Command Allocator: The memory pool to record commands into.
		nullptr,        // Initial Pipeline State Object (PSO): Commonly set to nullptr at creation.
		IID_PPV_ARGS( &cmdList ) // The Interface ID and output pointer
	);

	if ( FAILED( hr ) ) {
		log( std::format( "Failed to create Command List. HRESULT: {:#x}", hr ),
			LogLevel::Critical );
		return;
	}

	// Initial State: Close the Command List.
	// Command lists are created in the "recording" state. Since there isn't
	// anything recorded yet, it's best practice to close it immediately.
	hr = cmdList->Close();
	if ( FAILED( hr ) ) {
		std::cerr << "Error: Failed to close Command List." << std::endl;
		log( "Failed to close Command List.", LogLevel::Error );
		// Should be handled here.
	}
	log( "Command List closed for initial state setup." );
}
