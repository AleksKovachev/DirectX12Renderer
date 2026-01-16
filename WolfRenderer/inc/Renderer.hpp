#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <DirectXMath.h>
#include <iostream>
#include <vector>
#include <wrl/client.h>

#include "d3d12.h"
#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")
// #pragma comment(lib, "dxgi.lib d3d12.lib, dxcompiler.lib") is also valid

#include "Camera.hpp"
#include "Logger.hpp"
#include "Scene.hpp"

// Undefine "min" and "max" macros defined in windows.h
// to avoid conflicts with std::min and std::max.
#undef min
#undef max


/* While #pragma comment(lib, ...) is perfectly valid and common, especially in
 * small projects or single source files, the professional standard for large
 * C++ projects is to configure the required libraries in the Visual Studio
 * Project Properties under Linker -> Input -> Additional Dependencies.
 * This allows for more structured project configuration. */

using Microsoft::WRL::ComPtr;
struct IDxcBlob;
struct Transformation;

/// Application-level settings and data.
struct App {
	float deltaTime{};
};

namespace Core {

	/// The mode to use for rendering.
	enum class RenderMode {
		Rasterization,
		RayTracing
	};

	/// The preparation needed before rendering. Use Both to switch between modes.
	enum class RenderPreparation {
		Rasterization,
		RayTracing,
		Both
	};

	enum class TransformCoordinateSystem {
		World,
		Local
	};

	/// Transformation-related data for controlling renderer from the GUI.
	struct Transformation {
		/// The transform matrix used in the constant buffer to update object position.
		ComPtr<ID3D12Resource> transformCB{ nullptr };

		// Members related to geometry transform with mouse movemet.
		float currOffsetX{};
		float currOffsetY{};
		float targetOffsetX{};
		float targetOffsetY{};
		float offsetZ{ 35.f };

		float dummyObjectRadius{ 0.5f }; ///< Used for offset clamping to viewport bounds.
		float boundsX{};
		float boundsY{};

		float rotationSensitivityFactor{ 0.01f };
		float offsetZSensitivityFactor{ 0.5f };
		float FOVSensitivityFactor{ 0.1f };
		float offsetXYSensitivityFactor{ 0.1f };

		float currRotationX{};   // Radians
		float currRotationY{};   // Radians
		float targetRotationX{}; // Radians
		float targetRotationY{}; // Radians

		// Motion speed and sensitivity.
		float smoothOffsetLerp{ 2.f };
		float smoothRotationLambda{ 6.f };

		UINT8* transformCBMappedPtr = nullptr;

		float FOVAngle{ DirectX::XMConvertToRadians( 45.f ) };
		float aspectRatio{ 1.f }; ///< Calculate with render width/height.
		float nearZ{ 0.1f }; ///< Camera near clipping plane.
		float farZ{ 1000.f }; ///< Camera far clipping plane.

		TransformCoordinateSystem coordinateSystem{ TransformCoordinateSystem::Local };

		struct alignas(256) TransformData {
			DirectX::XMFLOAT4X4 mat;
			DirectX::XMFLOAT4X4 projection;
		} transformData;
	};

	// The main Renderer class managing the GPU commands.
	class WolfRenderer {
	public: // Memebrs.
		RenderMode renderMode{ RenderMode::RayTracing }; ///< Current rendering mode.

	public: // Functions.
		/// Constructor
		/// @param[in] renderWidth   Render resolution width.
		/// @param[in] renderHeight  Render resolution height.
		/// @param[in] bufferCount   Number of buffers in the swap chain.
		WolfRenderer( int renderWidth = 800, int renderHeight = 800, UINT bufferCount = 2 );
		~WolfRenderer();

		/// Sets the minimum logging level for the logger.
		/// @param[in] level  Minimum log level to set.
		void SetLoggerMinLevel( LogLevel );

		/// Maps the read-back buffer and writes the image to a file.
		/// @param[in] fileName  Path to the output file.
		void WriteImageToFile( const char* fileName = "output.ppm" );

		/// Unmaps the read-back buffer previously mapped by GetRenderData().
		void UnmapReadback();

		/// Creates the necessary DirectX infrastructure and rendering resources.
		void PrepareForRendering( HWND );

		/// Lets the GPU finihs rendering before closing the application.
		void StopRendering();

		/// Executes the rendering commands and handles GPU-CPU synchronization.
		/// @param[in] cameraInput  Camera input data for the frame. Used in RT mode.
		void RenderFrame( CameraInput& );

		/// Sets the rendering mode to the provided one.
		void SetRenderMode( RenderMode );

		/// Recieves mouse offset coordinates and clamp-adds them to the target offset.
		/// @param[in] dx  The X-axis offset.
		/// @param[in] dy  The Y-axis offset.
		void AddToTargetOffset( float, float );

		/// Recieves mouse offset coordinate and adds it to the Z offset.
		/// @param[in] dz  The Z-axis offset.
		void AddToOffsetZ( float );

		/// Recieves mouse offset coordinate and adds it to the FOV offset.
		/// @param[in] offset  The Z-axis offset.
		void AddToOffsetFOV( float );

		/// Recieves mouse offset coordinates and adds them to the target rotation.
		/// @param[in] deltaAngleX  The X-axis offset.
		/// @param[in] deltaAngleY  The Y-axis offset.
		void AddToTargetRotation( float, float );

		/// Sets the application-level data member.
		/// @param[in] appData  App Pointer to the application data.
		void SetAppData( App* );
	private: // Functions

		//! Ray Tracing specific functions.

		/// Prepares the renderer for ray tracing.
		void PrepareForRayTracing();

		/// Sets up frame-specific data before rendering with Ray Tracing.
		void FrameBeginRayTracing();

		/// Renders a frame using ray tracing.
		void RenderFrameRayTracing();

		/// Finalizes the frame rendering for ray tracing.
		void FrameEndRayTracing();

		/// Creates a global root signature for the ray tracing pipeline.
		void CreateGlobalRootSignature();

		/// Creates the ray tracing pipeline state object.
		void CreateRayTracingPipelineState();

		/// Creates a DXIL library sub-object for the ray generation shader.
		D3D12_STATE_SUBOBJECT CreateRayGenLibSubObject();

		/// Creates a DXIL library sub-object for the closest hit shader.
		D3D12_STATE_SUBOBJECT CreateClosestHitLibSubObject();

		/// Creates a DXIL library sub-object for the miss shader.
		D3D12_STATE_SUBOBJECT CreateMissLibSubObject();

		/// Creates a shader configuration sub-object for the ray tracing pipeline.
		D3D12_STATE_SUBOBJECT CreateShaderConfigSubObject();

		/// Creates a pipeline configuration sub-object for the ray tracing pipeline.
		D3D12_STATE_SUBOBJECT CreatePipelineConfigSubObject();

		/// Creates a root signature sub-object for the ray tracing pipeline.
		D3D12_STATE_SUBOBJECT CreateRootSignatureSubObject();

		/// Creates a hit group sub-object for the ray tracing pipeline.
		D3D12_STATE_SUBOBJECT CreateHitGroupSubObject();

		/// Creates the output texture for the ray tracing shader.
		void CreateRayTracingShaderTexture();

		/// Creates the shader binding table for ray tracing.
		void CreateShaderBindingTable();

		/// Creates an upload heap for the shader binding table.
		/// @param[in] sbtSize  Size of the shader binding table.
		void CreateSBTUploadHeap( UINT );

		/// Creates a default heap for the shader binding table.
		/// @param[in] sbtSize  Size of the shader binding table.
		void CreateSBTDefaultHeap( UINT );

		/// Copies the shader binding table data to the upload heap.
		/// @param[in] rayGenOffset  Offset from the start to the ray generation shader.
		/// @param[in] missOffset  Offset from the start to the miss shader.
		/// @param[in] hitGroupOffset  Offset from the start to the hit group.
		/// @param[in] rayGenShaderID  Pointer to the ray generation shader identifier.
		/// @param[in] missShaderID  Pointer to the miss shader identifier.
		/// @param[in] hitGroupID  Pointer to the hit group identifier.
		void CopySBTDataToUploadHeap( const UINT, const UINT, const UINT, void*, void*, void* );

		/// Copies the shader binding table data from the upload heap to the default heap.
		void CopySBTDataToDefaultHeap();

		/// Prepares the D3D12_DISPATCH_RAYS_DESC structure for dispatching rays.
		/// @param[in] recordSize  Size of any record in the shader binding table.
		/// @param[in] rayGenOffset  Offset from the start to the ray generation shader.
		/// @param[in] missOffset  Offset from the start to the miss shader.
		/// @param[in] hitGroupOffset  Offset from the start to the hit group.
		void PrepareDispatchRayDesc( const UINT, const UINT, const UINT, const UINT );

		/// Creates the vertices that will be rendered by the pipeline for the frame.
		/// Uses an upload heap to store the vertices on the CPU memory, the
		/// GPU will access them using the PCIe.
		void CreateVertexBufferRT();

		/// Compiles a shader from file.
		/// @param[in] filePath    Path to the shader file.
		/// @param[in] entryPoint  Entry point function name in the shader.
		/// @param[in] target      Shader target profile (e.g., "vs_6_0", "ps_6_0", "lib_6_8").
		ComPtr<IDxcBlob> CompileShader(
			const std::wstring&, const std::wstring&, const std::wstring& );

		/// Creates BLAS, TLAS, and TLAS SRV.
		void CreateAccelerationStructures();

		/// Create a Bottom Level Acceleration Structure (BLAS)
		void CreateBLAS();

		/// Create a Top Level Acceleration Structure (TLAS)
		void CreateTLAS();

		/// Creates a Shader Resource View (SRV) for the TLAS.
		void CreateTLASShaderResourceView();

		/// Updates the camera parameters in the constant buffer. Used in RT mode.
		void UpdateRTCamera( CameraInput& );

		/// Creates a constant buffer for the camera parameters used in RT mode.
		void CreateCameraConstantBuffer();

		//! Rasterization specific functions.

		/// Prepares the renderer for rasterization.
		void PrepareForRasterization();

		/// Sets up frame-specific data before rendering with Rasterization.
		void FrameBeginRasterization();

		/// Renders a frame using rasterization.
		void RenderFrameRasterization();

		/// Finalizes the frame rendering for rasterization.
		void FrameEndRasterization();

		/// Creates a root signature, which defines what resources are bound to the pipeline.
		void CreateRootSignature();

		/// Creates the pipeline state object, which holds the rasterization configuration.
		void CreatePipelineState();

		/// Creates the vertices that will be rendered by the pipeline for the frame.
		/// Uses an upload heap to store the vertices on the CPU memory, the
		/// GPU will access them using the PCIe.
		void CreateVertexBuffer();

		/// Creates the viewport and scissor rectangle for rendering.
		void CreateViewport();

		/// Creates a constant buffer for transform matrix.
		void CreateTransformConstantBuffer();

		/// Updates the transform matrix using interpolation from the current
		/// offset and rotation values to the target ones.
		void UpdateSmoothMotion();

		/// Calculates the viewport bounds for clamping the object offset.
		void CalculateViewportBounds();

		/// Creates a depth buffer and DSV Heap.
		void CreateDepthStencil();

		//! Common functions.

		/// Finalizes the frame rendering.
		void FrameEnd();

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

		/// Stall the CPU until the GPU has finished processing the commands.
		void WaitForGPUSync();

		/// Creates a swap chain for double buffering.
		/// @param[in] hWnd  Handle to the window where rendering will be presented.
		void CreateSwapChain( HWND );

		/// Creates a descriptor heap for the swap chain render targets.
		void CreateDescriptorHeapForSwapChain();

		/// Creates a descriptors for the render targets, with which
		/// the texture could be accessed for the next pipeline stages.
		/// Creates a descriptor heap for these descriptors.
		void CreateRenderTargetViewsFromSwapChain();

		/// Resets the command allocator and command list for recording new commands.
		void ResetCommandAllocatorAndList();

		//! Currently unused functions.

		/// Creates ID3D12Resource, D3D12_RESOURCE_DESC and D3D12_HEAP_PROPERTIES.
		/// Describes the 2D buffer, which will be used as a texture, and create its heap.
		void CreateGPUTexture();

		/// Creates a read-back heap and a read-back buffer, based on the
		/// texture for rendering. Stores the memory layout information for the texture.
		void CreateReadbackBuffer();

		/// Prepares texture source, destination and barrier and adds commands
		/// in the command list to copy the texture from GPU.
		void CopyTexture();

	private: // Members
		/// Grants access to the GPUs on the machine.
		ComPtr<IDXGIFactory4> m_dxgiFactory{ nullptr };
		/// Represents the video card used for rendering.
		ComPtr<IDXGIAdapter1> m_adapter{ nullptr };
		/// Allows access to the GPU for the purpose of Direct3D API.
		/// Device5 is the minimum version that supports ray tracing.
		ComPtr<ID3D12Device14> m_device{ nullptr };

		/// Holds the command lists and will be given to the GPU for execution.
		ComPtr<ID3D12CommandQueue> m_cmdQueue{ nullptr };
		/// Manages the GPU memoryfor the commands.
		ComPtr<ID3D12CommandAllocator> m_cmdAllocator{ nullptr };
		/// The actual commands that will be executed by the GPU.
		ComPtr<ID3D12GraphicsCommandList10> m_cmdList{ nullptr };

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
		/// The vertices that will be rendered in RT (Stored in GPU Default Heap).
		ComPtr<ID3D12Resource> m_vertexBufferRT{ nullptr };
		/// The vertex buffer descriptor.
		D3D12_VERTEX_BUFFER_VIEW m_vbView{};
		/// The root signature defining the resources bound to the pipeline.
		ComPtr<ID3D12RootSignature> m_rootSignature{ nullptr };
		/// The pipeline state object holding the pipeline configuration.
		ComPtr<ID3D12PipelineState> m_pipelineState{ nullptr };

		/// Viewport for rendering.
		D3D12_VIEWPORT m_viewport{};
		/// Scissor rectangle for rendering.
		D3D12_RECT m_scissorRect{};

		/// The fence value, which the GPU sets when done.
		UINT64 m_fenceValue{ 0 };
		/// Event handle for fence synchronization, fired when GPU is done.
		HANDLE m_fenceEvent{ nullptr };

		/// Handle to the output texture for ray tracing.
		ComPtr<ID3D12Resource> m_raytracingOutput{ nullptr };

		/// Handle to the descriptor heap of the output texture.
		ComPtr<ID3D12DescriptorHeap> m_uavsrvHeap{ nullptr };

		/// Handle to the descriptor heap of the TLAS.
		ComPtr<ID3D12DescriptorHeap> m_srvHeap{ nullptr };

		/// The global root signature for the ray tracing pipeline.
		ComPtr<ID3D12RootSignature> m_globalRootSignature{ nullptr };

		/// Handle to the ray tracing pipeline state object.
		ComPtr<ID3D12StateObject> m_rtStateObject{ nullptr };

		/* Descriptions for the ray tracing pipeline state sub - objects. */
		D3D12_EXPORT_DESC m_rayGenExportDesc{};
		D3D12_DXIL_LIBRARY_DESC m_rayGenLibDesc{};
		D3D12_EXPORT_DESC m_closestHitExportDesc{};
		D3D12_DXIL_LIBRARY_DESC m_closestHitLibDesc{};
		D3D12_EXPORT_DESC m_missExportDesc{};
		D3D12_DXIL_LIBRARY_DESC m_missLibDesc{};
		D3D12_RAYTRACING_SHADER_CONFIG m_shaderConfig{};
		D3D12_RAYTRACING_PIPELINE_CONFIG m_pipelineConfig{};
		D3D12_GLOBAL_ROOT_SIGNATURE m_globalRootSignatureDesc{};
		D3D12_HIT_GROUP_DESC m_hitGroupDesc{};

		// Shader blobs for ray tracing library sub-objects.
		ComPtr<IDxcBlob> m_rayGenBlob{ nullptr };
		ComPtr<IDxcBlob> m_closestHitBlob{ nullptr };
		ComPtr<IDxcBlob> m_missBlob{ nullptr };

		// Shader Binding Table resources and dispatch description.
		ComPtr<ID3D12Resource> m_sbtUploadBuff{ nullptr };
		ComPtr<ID3D12Resource> m_sbtDefaultBuff{ nullptr };
		D3D12_DISPATCH_RAYS_DESC m_dispatchRaysDesc{};

		/* Acceleration Structures members. */
		ComPtr<ID3D12Resource> m_blasResult{ nullptr };
		ComPtr<ID3D12Resource> m_blasScratch{ nullptr };
		ComPtr<ID3D12Resource> m_tlasResult{ nullptr };

		ComPtr<ID3D12Resource> m_depthStencilBuffer{ nullptr };
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap{ nullptr };
		DXGI_FORMAT m_depthFormat{ DXGI_FORMAT_D32_FLOAT };

		// General members.
		Scene m_scene{ "../rsc/scene1.crtscene" };
		size_t m_frameIdx{};        ///< Current frame index.
		bool m_isPrepared{ false }; ///< Flag indicating if the renderer is prepared.
		Logger log{ std::cout };    ///< Logger instance for logging messages.
		UINT m_bufferCount{};       ///< Number of buffers in the swap chain.
		UINT m_rtvDescriptorSize{}; ///< Size of the RTV descriptor.
		UINT m_scFrameIdx{};        ///< Swap Chain frame index.
		RenderPreparation m_prepMode{ RenderPreparation::Both }; ///< Current preparation mode.
		size_t m_vertexCount{};     ///< Number of vertices to render.
		BOOL m_renderRandomColors{ 1 }; ///< Whether to color each triangle in a random color.
		App* m_app{ nullptr }; ///< Pointer to application-level data.
		Transformation m_transform{}; ///< Camera/object transformation data.
		Camera m_cameraRT{}; ///< Camera used for RT mode.
	};

	/// Calculates the aligned size for a given size and alignment.
	inline UINT alignedSize( UINT size, UINT alignBytes ) {
		return alignBytes * (size / alignBytes + (size % alignBytes ? 1 : 0));
	}
}

#endif // RENDERER_HPP