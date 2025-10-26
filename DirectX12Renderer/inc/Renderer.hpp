#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <dxgi1_6.h>
#include <d3d12.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
// #pragma comment(lib, "dxgi.lib d3d12.lib") is also valid

#include <iostream>
#include <Windows.h>

#include "Logger.hpp"

/* While #pragma comment(lib, ...) is perfectly valid and common, especially in
 * small projects or single source files, the professional standard for large
 * C++ projects is to configure the required libraries in the Visual Studio
 * Project Properties under Linker -> Input -> Additional Dependencies.
 * This allows for more structured project configuration. */

// The main Renderer class managing the GPU commands.
class Renderer {
public:
	Renderer();

	~Renderer();

	//! @brief Initiate the actual rendering.
	void Render();

private:
	//! @brief Creates the necessary DirectX infrastructure and rendering resources.
	void PrepareForRendering();

	//! @brief Creates a Factory and finds all adapters in the system. Chooses the best one.
	void AssignAdapter();

	//! @brief Create ID3D12Device, an interface which allows access to the GPU
	//! for the purpose of Direct3D API
	void CreateDevice();

	//! @brief Creates ID3D12CommandQueue, ID3D12CommandAllocator and ID3D12GraphicsCommandList
	//! for preparing and submitting GPU commands.
	void CreateCommandsManagers();
private:
	IDXGIFactory4* dxgiFactory{ nullptr }; //!< Grants access to the GPUs on the machine
	IDXGIAdapter1* adapter{ nullptr }; //!< Represents the video card used for rendering.
	ID3D12Device* d3d12Device{ nullptr }; //!< Allows access to the GPU for the purpose of Direct3D API.

	ID3D12CommandQueue* cmdQueue{ nullptr }; //!< Holds the command lists and will be given to the GPU for execution
	ID3D12CommandAllocator* cmdAllocator{ nullptr }; //!< Manages the GPU memoryfor the commands
	ID3D12GraphicsCommandList* cmdList{ nullptr }; //!< The actual commands that will be executed by the GPU

	Logger log{ std::cout }; //!< Logger instance for logging messages
};


// Simple struct to hold the unique hardware identifier (Vendor ID + Device ID)
struct HardwareID {
	UINT DeviceId;
	UINT VendorId;

	// Required for use in std::set
	bool operator==( const HardwareID& other ) const {
		if ( DeviceId == other.DeviceId && VendorId == other.VendorId )
			return true;
		return false;
	}
};


#endif // RENDERER_HPP