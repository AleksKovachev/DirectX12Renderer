#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <DirectXMath.h>
#include <iostream>
#include <vector>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
// #pragma comment(lib, "dxgi.lib d3d12.lib") is also valid

#include "Logger.hpp"


/* While #pragma comment(lib, ...) is perfectly valid and common, especially in
 * small projects or single source files, the professional standard for large
 * C++ projects is to configure the required libraries in the Visual Studio
 * Project Properties under Linker -> Input -> Additional Dependencies.
 * This allows for more structured project configuration. */


using Microsoft::WRL::ComPtr;

namespace Core {
	// The main Renderer class managing the GPU commands.
	class WolfRenderer {
	public:
		/// Constructor
		/// @param[in] renderWidth   Render resolution width.
		/// @param[in] renderHeight   Render resolution height.
		WolfRenderer( int renderWidth = 800, int renderHeight = 800, UINT bufferCount = 2 );
		~WolfRenderer();

		/// Sets the minimum logging level for the logger.
		void SetLoggerMinLevel( LogLevel level );

		/// Creates the necessary DirectX infrastructure and rendering resources.
		void PrepareForRendering( HWND hWnd );

		/// Executes the rendering commands and handles GPU-CPU synchronization.
		void RenderFrame();

		/// Maps the read-back buffer and writes the image to a file.
		/// @param[in] fileName  Path to the output file.
		void WriteImageToFile( const char* fileName = "output.ppm" );

		/// Unmaps the read-back buffer previously mapped by GetRenderData().
		void UnmapReadback();

		/// Lets the GPU finihs rendering before closing the application.
		void StopRendering();

		/// Recieves mouse offset coordinates and clamp-adds them to the target offset.
		/// @param[in] dx   The X-axis offset.
		/// @param[in] dy   The Y-axis offset.
		void AddToTargetOffset( float dx, float dy );

		/// Recieves mouse offset coordinates and adds them to the target rotation.
		void AddToTargetRotation( float deltaAngle );
	private: // Functions

		/// Create ID3D12Device, an interface which allows access to the GPU
		/// for the purpose of Direct3D API
		void CreateDevice();

		/// Creates a Factory and finds all adapters in the system. Chooses the best one.
		void AssignAdapter();

		/// Creates ID3D12CommandQueue, ID3D12CommandAllocator and
		/// ID3D12GraphicsCommandList for preparing and submitting GPU commands.
		void CreateCommandsManagers();

		/// Creates a fence for GPU-CPU synchronization.
		void CreateFence();

		/// Creates ID3D12Resource, D3D12_RESOURCE_DESC and D3D12_HEAP_PROPERTIES.
		/// Describes the 2D buffer, which will be used as a texture, and create its heap.
		void CreateGPUTexture();

		/// Creates a read-back heap and a read-back buffer, based on the
		/// texture for rendering. Stores the memory layout information for the texture.
		void CreateReadbackBuffer();

		/// Resets the command allocator and command list for recording new commands.
		void ResetCommandAllocatorAndList();

		/// Prepares texture source, destination and barrier and adds commands
		/// in the command list to copy the texture from GPU.
		void CopyTexture();

		/// Stall the CPU until the GPU has finished processing the commands.
		void WaitForGPURenderFrame();

		/// Sets up frame-specific data before rendering.
		void FrameBegin();

		/// Finalizes the frame rendering.
		void FrameEnd( /* const char* fileName */ );

		/// Creates a swap chain for double buffering.
		void CreateSwapChain( HWND hWnd );

		/// Creates a descriptor heap for the swap chain render targets.
		void CreateDescriptorHeapForSwapChain();

		/// Creates a descriptors for the render targets, with which
		/// the texture could be accessed for the next pipeline stages.
		/// Creates a descriptor heap for these descriptors.
		void CreateRenderTargetViewsFromSwapChain();

		/// Creates the vertices that will be rendered by the pipeline for the frame.
		/// Uses an upload heap to store the vertices on the CPU memory, the
		/// GPU will access them using the PCIe.
		void CreateVertexBuffer();

		/// Creates a root signature, which defines what resources are bound to the pipeline.
		void CreateRootSignature();

		/// Creates the pipeline state object, which holds the configuration
		void CreatePipelineState();

		/// Creates the viewport and scissor rectangle for rendering.
		void CreateViewport();

		void CreateTransformConstantBuffer();

		void UpdateSmoothOffset();
	private: // Members
		/// Grants access to the GPUs on the machine.
		ComPtr<IDXGIFactory4> m_dxgiFactory{ nullptr };
		/// Represents the video card used for rendering.
		ComPtr<IDXGIAdapter1> m_adapter{ nullptr };
		/// Allows access to the GPU for the purpose of Direct3D API.
		ComPtr<ID3D12Device> m_device{ nullptr };

		/// Holds the command lists and will be given to the GPU for execution.
		ComPtr<ID3D12CommandQueue> m_cmdQueue{ nullptr };
		/// Manages the GPU memoryfor the commands.
		ComPtr<ID3D12CommandAllocator> m_cmdAllocator{ nullptr };
		/// The actual commands that will be executed by the GPU.
		ComPtr<ID3D12GraphicsCommandList> m_cmdList{ nullptr };

		/// A GPU resource (like a buffer or texture).
		/// This is the Render Target used for the texture.
		ComPtr<ID3D12Resource> m_renderTarget{ nullptr };

		/// Render Targets from the swap chain.
		std::vector<ComPtr<ID3D12Resource>> m_renderTargets{};
		/// Descriptor heap to hold the Render Target Descriptor of the swap chain.
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap{ nullptr };
		/// CPU descriptor handles for the render targets of the swap chain.
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_rtvHandles{};

		/// Readback buffer to hold the rendered image.
		ComPtr<ID3D12Resource> m_readbackBuff{ nullptr };
		/// Fence for GPU-CPU synchronization.
		ComPtr<ID3D12Fence> m_fence{ nullptr };

		/// The swap chain for buffering (double/tripple/etc.).
		ComPtr<IDXGISwapChain4> m_swapChain{ nullptr };

		/// Hold the texture properties.
		D3D12_RESOURCE_DESC m_textureDesc{};
		/// Memory layout information for the texture.
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_renderTargetFootprint{};

		/// The vertices that will be rendered (Stored in GPU Default Heap).
		ComPtr<ID3D12Resource> m_vertexBuffer{ nullptr };
		/// The vertex buffer descriptor.
		D3D12_VERTEX_BUFFER_VIEW m_vbView{};
		/// The root signature defining the resources bound to the pipeline.
		ComPtr<ID3D12RootSignature> m_rootSignature{ nullptr };
		/// The pipeline state object holding the pipeline configuration.
		ComPtr<ID3D12PipelineState> m_pipelineState{ nullptr };
		/// The transform matrix used in the constant buffer to update object position.
		ComPtr<ID3D12Resource> m_transformCB{ nullptr };

		/// Viewport for rendering.
		D3D12_VIEWPORT m_viewport{};
		/// Scissor rectangle for rendering.
		D3D12_RECT m_scissorRect{};

		/// The fence value, which the GPU sets when done.
		UINT64 m_fenceValue{ 0 };
		/// Event handle for fence synchronization, fired when GPU is done.
		HANDLE m_fenceEvent{ nullptr };

		float m_rendColor[4]{};
		size_t m_frameIdx{ 0 };
		bool m_isPrepared{ false };
		Logger log{ std::cout }; ///< Logger instance for logging messages.
		int m_renderWidth{};
		int m_renderHeight{};
		UINT m_bufferCount{};
		UINT m_rtvDescriptorSize{};
		UINT m_scFrameIdx{ 0 }; ///< Swap Chain frame index.

		// Members related to geometry transform with mouse movemet.
		UINT8* m_transformCBMappedPtr = nullptr;

		float m_currOffsetX{};
		float m_currOffsetY{};
		float m_targetOffsetX{};
		float m_targetOffsetY{};
		float m_currRotation{};   // Radians
		float m_targetRotation{}; // Radians
		float m_rotationSensitivity{ 1.f };
		float m_deltaTime{};

		struct alignas(256) TransformData {
			DirectX::XMFLOAT4X4 mat;
		} m_transformData;
	};

	struct Vertex {
		float x;
		float y;
	};
}

#endif // RENDERER_HPP