#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <dxgi1_6.h>
#include <d3d12.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
// #pragma comment(lib, "dxgi.lib d3d12.lib") is also valid

#include <iostream>
#include <wrl/client.h>

#include "Logger.hpp"

/* While #pragma comment(lib, ...) is perfectly valid and common, especially in
 * small projects or single source files, the professional standard for large
 * C++ projects is to configure the required libraries in the Visual Studio
 * Project Properties under Linker -> Input -> Additional Dependencies.
 * This allows for more structured project configuration. */


using Microsoft::WRL::ComPtr;

// The main Renderer class managing the GPU commands.
class Renderer {
public:
	~Renderer();

	//! @brief Initiate the actual rendering.
	void Render();

	//! @brief Sets the minimum logging level for the logger.
	void SetLoggerMinLevel( LogLevel level );

	//! @brief Creates the necessary DirectX infrastructure and rendering resources.
	void PrepareForRendering();

	//! @brief Executes the rendering commands and handles GPU-CPU synchronization.
	void RenderFrame( const float clearColor[4] );

	//! @brief Maps the read-back buffer and writes the image to a file.
	void WriteImageToFile( const char* fileName = "output.ppm" );

private: // Functions

	//! @brief Create ID3D12Device, an interface which allows access to the GPU
	//! for the purpose of Direct3D API
	void CreateDevice();

	//! @brief Creates a Factory and finds all adapters in the system. Chooses the best one.
	void AssignAdapter();

	//! @brief Creates ID3D12CommandQueue, ID3D12CommandAllocator and
	//! ID3D12GraphicsCommandList for preparing and submitting GPU commands.
	void CreateCommandsManagers();

	//! @brief Creates a fence for GPU-CPU synchronization.
	void CreateFence();

	//! @brief Creates ID3D12Resource, D3D12_RESOURCE_DESC and D3D12_HEAP_PROPERTIES.
	//! Describes the 2D buffer, which will be used as a texture, and create its heap.
	void CreateGPUTexture();

	//! @brief Creates a descriptor for the render target, with which
	//! the texture could be accessed for the next pipeline stages.
	//! Creates a descriptor heap for this descriptor.
	void CreateRenderTargetView();

	//! @brief Creates a read-back heap and a read-back buffer, based on the
	//! texture for rendering. Stores the memory layout information for the texture.
	void CreateReadbackBuffer();


	//! @brief Resets the command allocator and command list for recording new commands.
	void ResetCommandAllocatorAndList();

	//! @brief Adds commands in the command list to generate a solid color texture.
	void GenerateConstColorTexture( const float clearColor[4] );

	//! @brief Prepares texture source, destination and barrier and adds commands
	//! in the command list to copy the texture from GPU.
	void CopyTexture();

	//! @brief Stall the CPU until the GPU has finished processing the commands.
	void WaitForGPURenderFrame();

	//! @brief Writes the data to a PPM file.
	void FrameEnd( const char* fileName );

private: // Members
	//!< Grants access to the GPUs on the machine.
	ComPtr<IDXGIFactory4> m_dxgiFactory{ nullptr };
	//!< Represents the video card used for rendering.
	ComPtr<IDXGIAdapter1> m_adapter{ nullptr };
	//!< Allows access to the GPU for the purpose of Direct3D API.
	ComPtr<ID3D12Device> m_device{ nullptr };

	//!< Holds the command lists and will be given to the GPU for execution.
	ComPtr<ID3D12CommandQueue> m_cmdQueue{ nullptr };
	//!< Manages the GPU memoryfor the commands.
	ComPtr<ID3D12CommandAllocator> m_cmdAllocator{ nullptr };
	//!< The actual commands that will be executed by the GPU.
	ComPtr<ID3D12GraphicsCommandList> m_cmdList{ nullptr };

	//!< A GPU resource (like a buffer or texture).
	//!< This is the Render Target used for the texture.
	ComPtr<ID3D12Resource> m_renderTarget{ nullptr };
	//!< Descriptor heap to hold the Render Target Descriptor of the texture.
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap{ nullptr };

	//< Readback buffer to hold the rendered image.
	ComPtr<ID3D12Resource> m_readbackBuff{ nullptr };
	//!< Fence for GPU-CPU synchronization.
	ComPtr<ID3D12Fence> m_fence{ nullptr };

	//!< Hold the texture properties.
	D3D12_RESOURCE_DESC m_textureDesc{};
	//!< Memory layout information for the texture.
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_renderTargetFootprint{};
	// Handle for the descriptor of the texture, with which it could be used in the pipeline.
	D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle{};

	//!< The fence value, which the GPU sets when done.
	UINT64 m_fenceValue{ 0 };
	//!< Event handle for fence synchronization, fired when GPU is done.
	HANDLE m_fenceEvent{ nullptr };

	size_t m_frameIdx{ 0 };
	bool m_isPrepared{ false };
	Logger log{ std::cout }; //!< Logger instance for logging messages.
};


// Simple struct to hold the unique hardware identifier (Vendor ID + Device ID).
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