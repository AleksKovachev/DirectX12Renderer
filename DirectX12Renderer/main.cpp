#include <iostream>
#include <vector>

#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")


int main() {
	IDXGIFactory4* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1( IID_PPV_ARGS( &dxgiFactory ) );

	if ( SUCCEEDED( hr ) ) {
		IDXGIAdapter1* adapter;

		std::vector<IDXGIAdapter1*> adapters;

		for ( UINT adapterIdx{};
			dxgiFactory->EnumAdapters1( adapterIdx, &adapter ) != DXGI_ERROR_NOT_FOUND;
			++adapterIdx
		) {
			DXGI_ADAPTER_DESC1 desc;
			hr = adapter->GetDesc1( &desc );

			if ( FAILED( hr ) ) {
				std::cerr << "Failed to get description for adapter index "
					<< adapterIdx << std::endl;
				adapter->Release(); // Cleanup
				adapter = nullptr;
				continue;
			}

			// Skip Microsoft's Basic Render Driver (Software adapter)
			if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) {
				std::cout << "Adapter Index " << adapterIdx
					<< ": Skipping Software Adapter." << std::endl;
				adapter->Release(); // Cleanup
				adapter = nullptr;
				continue;
			}

			// Convert the description string to a usable format
			std::wcout << "Adapter Index " << adapterIdx << ": "
				<< desc.Description << std::endl;
			std::cout << " Dedicated Video Memory: "
				<< desc.DedicatedVideoMemory / (1024 * 1024) << " MB" << std::endl;
			// std::cout << " Dedicated Video Memory: " // Alternative
			//  << desc.DedicatedVideoMemory / 1024 / 1024 << " MB" << std::endl;
			std::wcout << " Device ID: " << desc.DeviceId << std::endl;
			std::wcout << " Vendor ID: " << desc.VendorId << std::endl;

			adapters.push_back( adapter );
		}
		std::cout << "\nFound a total of " << adapters.size()
			<< " adapters." << std::endl;

		// Cleanup
		for ( IDXGIAdapter1* adapter : adapters ) {
			adapter->Release();
			adapter = nullptr;
		}
		adapters.clear();
		dxgiFactory->Release();
		dxgiFactory = nullptr;
	} else {
		std::cerr << "Failed to create DXGI Factory." << std::endl;
		return 1;
	}
	return 0;
}