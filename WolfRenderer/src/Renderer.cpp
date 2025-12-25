#include "Renderer.hpp"
#include "utils.hpp" // CHECK_HR, wideStrToUTF8

#include "d3dx12_core.h"
#include "d3dx12_root_signature.h"
#include "dxcapi.use.h"
#include <dxc/dxcapi.h>

#include "ConstColor.hlsl.h"
#include "ConstColorVS.hlsl.h"

#include <cassert>
#include <cmath> // cosf, sinf
#include <filesystem>
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
	dxc::DxcDllSupport gDxcDllHelper{};

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
		log( "Debug layer initialized." );
#endif // _DEBUG
	// Fill the render targets and RTV handles vectors.
		for ( UINT i{}; i < bufferCount; ++i ) {
			m_renderTargets.emplace_back();
			m_rtvHandles.emplace_back();
		}
	}

	WolfRenderer::~WolfRenderer() {
		log( "    => Closing application." );
		if ( m_fenceEvent != nullptr )
			CloseHandle( m_fenceEvent );
	}

	void WolfRenderer::SetLoggerMinLevel( LogLevel level ) {
		log.SetMinLevel( level );
		std::string logLevel{};
		switch (level) {
			case LogLevel::Debug:
				logLevel = "Debug";
				break;
			case LogLevel::Info:
				logLevel = "Info";
				break;
			case LogLevel::Warning:
				logLevel = "Warning";
				break;
			case LogLevel::Error:
				logLevel = "Error";
				break;
			case LogLevel::Critical:
				logLevel = "Critical";
				break;
		}
		log( "Minimum loggig level set to: " + logLevel);
	}

	void WolfRenderer::WriteImageToFile( const char* fileName ) {
		void* renderData;
		HRESULT hr = m_readbackBuff->Map( 0, nullptr, &renderData );
		CHECK_HR( "Failed to map GPU data to CPU pointer!", hr, log );

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

	void WolfRenderer::PrepareForRendering( HWND hWnd ) {
		if ( m_isPrepared ) {
			log( "GPU already prepared." );
			return;
		}
		log( "Starting renderer initialization..." );

		CreateDevice(); // Creates Factory, Adapter, Device
		CreateFence();
		CreateCommandsManagers(); // Creates Queue, Allocator, List (and closes it)
		CreateSwapChain( hWnd );
		CreateDescriptorHeapForSwapChain();
		CreateRenderTargetViewsFromSwapChain();

		if ( m_prepMode == RenderPreparation::Both
			|| m_prepMode == RenderPreparation::Rasterization
		)
			PrepareForRasterization();
		if ( m_prepMode == RenderPreparation::Both
			|| m_prepMode == RenderPreparation::RayTracing
		)
			PrepareForRayTracing();

		m_isPrepared = true;
	}

	void WolfRenderer::StopRendering() {
		log( "Stopping renderer!" );
		WaitForGPUSync();
	}

	void WolfRenderer::RenderFrame( float offsetX, float offsetY ) {
		if ( renderMode == RenderMode::RayTracing ) {
			FrameBeginRayTracing();
			RenderFrameRayTracing();
			FrameEndRayTracing();
		} else if ( renderMode == RenderMode::Rasterization ) {
			FrameBeginRasterization();
			RenderFrameRasterization( offsetX, offsetY );
			FrameEndRasterization();
		}
		FrameEnd();
	}

	void WolfRenderer::SetRenderMode( RenderMode newRenderMode ) {
		WaitForGPUSync();
		renderMode = newRenderMode;
	}

	/*  #####     ###    ##   ##    ######  #####     ###    ######  ######
	 *  ##  ##   ## ##    ## ##       ##    ##  ##   ## ##   ##      ##
	 *  #####   #######    ##         ##    #####   #######  ##      #####
	 *  ## ##   ##   ##    ##         ##    ## ##   ##   ##  ##      ##
	 *  ##  ##  ##   ##    ##         ##    ##  ##  ##   ##  ######  ###### */

	void WolfRenderer::PrepareForRayTracing() {
		CreateGlobalRootSignature();
		CreateRayTracingPipelineState();
		CreateRayTracingShaderTexture();
		CreateShaderBindingTable();
		log( "[ Ray Tracing ] Successful preparation." );
	}

	void WolfRenderer::FrameBeginRayTracing() {
		ResetCommandAllocatorAndList();

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		m_cmdList->ResourceBarrier( 1, &barrier );

		barrier = {};
		barrier.Transition.pResource = m_raytracingOutput.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		m_cmdList->ResourceBarrier( 1, &barrier );
	}

	void WolfRenderer::RenderFrameRayTracing() {
		ID3D12DescriptorHeap* heaps[] = { m_uavHeap.Get() };
		m_cmdList->SetDescriptorHeaps( _countof( heaps ), heaps );
		m_cmdList->SetComputeRootSignature( m_globalRootSignature.Get() );
		m_cmdList->SetComputeRootDescriptorTable(
			0, m_uavHeap->GetGPUDescriptorHandleForHeapStart() );
		m_cmdList->SetPipelineState1( m_rtStateObject.Get() );
		m_cmdList->DispatchRays( &m_dispatchRaysDesc );
	}

	void WolfRenderer::FrameEndRayTracing() {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_raytracingOutput.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		m_cmdList->ResourceBarrier( 1, &barrier );

		m_cmdList->CopyResource( m_renderTargets[m_scFrameIdx].Get(), m_raytracingOutput.Get());

		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_cmdList->ResourceBarrier( 1, &barrier );
	}

	void WolfRenderer::CreateGlobalRootSignature() {
		// Create a Range of type UAV.
		CD3DX12_DESCRIPTOR_RANGE1 uavRange{};
		uavRange.Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
			1, // Number of descriptors
			0, // Base shader register
			0  // Register space
		);

		// Describe the root parameter that will be stored in the Root Signature.
		CD3DX12_ROOT_PARAMETER1 rootParam{};
		rootParam.InitAsDescriptorTable(
			1,                          // Number of descriptor ranges
			&uavRange,                  // Pointer to the range
			D3D12_SHADER_VISIBILITY_ALL // Shader visibility
		);

		// Pass the Root Signature Parameter to the Root Signature Description.
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
		rootSignatureDesc.Init_1_1(
			1,                          // Number of root parameters
			&rootParam,                 // Pointer to the root parameter
			0,                          // No static samplers
			nullptr,                    // No static samplers
			D3D12_ROOT_SIGNATURE_FLAG_NONE
		);

		// Serialize the root signature (similar to compilation process).
		ComPtr<ID3DBlob> signatureBlob{};
		ComPtr<ID3DBlob> errorBlob{};
		HRESULT hr = D3D12SerializeVersionedRootSignature(
			&rootSignatureDesc,
			&signatureBlob,
			&errorBlob
		);

		if ( errorBlob ) {
			const char* msg{ static_cast<char*>(errorBlob->GetBufferPointer()) };
			log( std::format( "Root Signature Error: {}", msg ), LogLevel::Error );
		}
		CHECK_HR( "Failed to serialize root signature.", hr, log );

		// Create the root signature on the device.
		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &m_globalRootSignature )
		);
		CHECK_HR( "CreateRootSignature failed.", hr, log );

		log( "[ Ray Tracing ] Global root signature created." );
	}

	void WolfRenderer::CreateRayTracingPipelineState() {
		D3D12_STATE_SUBOBJECT rayGenLibSubobject{ CreateRayGenLibSubObject() };
		D3D12_STATE_SUBOBJECT missLibSubobject{ CreateMissLibSubObject() };
		D3D12_STATE_SUBOBJECT shaderConfigSubobject{ CreateShaderConfigSubObject() };
		D3D12_STATE_SUBOBJECT pipelineConfigSubobject{ CreatePipelineConfigSubObject() };
		D3D12_STATE_SUBOBJECT rootSignatureSubobject{ CreateRootSignatureSubObject()};

		// Create the actual State Object.
		std::vector<D3D12_STATE_SUBOBJECT> subobjects{
			rayGenLibSubobject,
			missLibSubobject,
			shaderConfigSubobject,
			pipelineConfigSubobject,
			rootSignatureSubobject
		};

		// Describe the ray tracing pipeline state object.
		D3D12_STATE_OBJECT_DESC rtPSODesc{};
		rtPSODesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		rtPSODesc.NumSubobjects = static_cast<UINT>(subobjects.size());
		rtPSODesc.pSubobjects = subobjects.data();

		// Create the ray tracing pipeline state object.
		HRESULT hr = m_device->CreateStateObject(
			&rtPSODesc,
			IID_PPV_ARGS( &m_rtStateObject )
		);
		CHECK_HR( "Failed to create ray tracing pipeline state object.", hr, log );

		log( "[ Ray Tracing ] Pipeline state created." );
	}

	D3D12_STATE_SUBOBJECT WolfRenderer::CreateRayGenLibSubObject() {
		m_rayGenBlob = CompileShader( 
			L"shaders/ray_tracing_shaders.hlsl", L"rayGen", L"lib_6_5" );

		// Define what to export (the shader entry point).
		m_rayGenExportDesc.Name = L"rayGen";
		m_rayGenExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		// Describe the DXIL library.
		m_rayGenLibDesc.DXILLibrary.pShaderBytecode = m_rayGenBlob->GetBufferPointer();
		m_rayGenLibDesc.DXILLibrary.BytecodeLength = m_rayGenBlob->GetBufferSize();
		m_rayGenLibDesc.NumExports = 1;
		m_rayGenLibDesc.pExports = &m_rayGenExportDesc;

		// Compose the actual subobject.
		D3D12_STATE_SUBOBJECT rayGenLibSubobject{};
		rayGenLibSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		rayGenLibSubobject.pDesc = &m_rayGenLibDesc;

		log( "[ Ray Tracing ] Ray generation library pipeline state sub-object created." );

		return rayGenLibSubobject;
	}

	D3D12_STATE_SUBOBJECT WolfRenderer::CreateMissLibSubObject() {
		m_missBlob = CompileShader(
			L"shaders/ray_tracing_shaders.hlsl", L"miss", L"lib_6_5" );

		// Define what to export (the shader entry point).
		m_missExportDesc.Name = L"miss";
		m_missExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		// Describe the DXIL library.
		m_missLibDesc.DXILLibrary.pShaderBytecode = m_missBlob->GetBufferPointer();
		m_missLibDesc.DXILLibrary.BytecodeLength = m_missBlob->GetBufferSize();
		m_missLibDesc.NumExports = 1;
		m_missLibDesc.pExports = &m_missExportDesc;

		// Compose the actual subobject.
		D3D12_STATE_SUBOBJECT missLibSubobject{};
		missLibSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		missLibSubobject.pDesc = &m_missLibDesc;

		log( "[ Ray Tracing ] Miss shader library pipeline state sub-object created." );

		return missLibSubobject;
	}

	D3D12_STATE_SUBOBJECT WolfRenderer::CreateShaderConfigSubObject() {
		// 4 channels (RGBA). The "float color" in RayPayload struct in HLSL.
		m_shaderConfig.MaxPayloadSizeInBytes = 4 * sizeof( float );
		m_shaderConfig.MaxAttributeSizeInBytes =
			D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES; // 8 bytes (barycentrics)

		D3D12_STATE_SUBOBJECT shaderConfigSubobject{};
		shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
		shaderConfigSubobject.pDesc = &m_shaderConfig;

		log( "[ Ray Tracing ] Shader configuration pipeline state sub-object created." );

		return shaderConfigSubobject;
	}

	D3D12_STATE_SUBOBJECT WolfRenderer::CreatePipelineConfigSubObject() {
		// Max recursion depth. 1 means that rays can spawn rays only once.
		m_pipelineConfig.MaxTraceRecursionDepth = 1;

		D3D12_STATE_SUBOBJECT pipelineConfigSubobject{};
		pipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
		pipelineConfigSubobject.pDesc = &m_pipelineConfig;

		log( "[ Ray Tracing ] Pipeline configuration pipeline state sub-object created." );

		return pipelineConfigSubobject;
	}

	D3D12_STATE_SUBOBJECT WolfRenderer::CreateRootSignatureSubObject() {
		m_globalRootSignatureDesc = D3D12_GLOBAL_ROOT_SIGNATURE{ m_globalRootSignature.Get() };

		D3D12_STATE_SUBOBJECT rootSignatureSubobject{};
		rootSignatureSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
		rootSignatureSubobject.pDesc = &m_globalRootSignatureDesc;

		log( "[ Ray Tracing ] Global root signature pipeline state sub-object created." );

		return rootSignatureSubobject;
	}

	void WolfRenderer::CreateRayTracingShaderTexture() {
		// Describe the output texture.
		D3D12_RESOURCE_DESC texDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R8G8B8A8_UNORM, m_renderWidth, m_renderHeight ) };
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		// Describe the heap that will contain the resource.
		D3D12_HEAP_PROPERTIES heapProps{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };

		// Create and allocate the resource in the GPU memory.
		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS( &m_raytracingOutput )
		);
		CHECK_HR( "Failed to create ray tracing output texture.", hr, log );

		// Create a descriptor heap and a descriptor for the output texture.
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 1;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		hr = m_device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS( &m_uavHeap )
		);
		CHECK_HR( "Failed to create UAV descriptor heap.", hr, log );

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = texDesc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		// Create the descriptor inside the descriptor heap.
		m_device->CreateUnorderedAccessView(
			m_raytracingOutput.Get(),
			nullptr,
			&uavDesc,
			m_uavHeap->GetCPUDescriptorHandleForHeapStart()
		);

		log( "[ Ray Tracing ] Shader output texture created." );
	}

	void WolfRenderer::CreateShaderBindingTable() {
		// Access the properties of the ray tracing pipeline state object.
		ComPtr<ID3D12StateObjectProperties> rtStateObjectProps;
		HRESULT hr = m_rtStateObject.As( &rtStateObjectProps );
		CHECK_HR( "Failed to access ray tracing state object properties.", hr, log );

		// Older approach, used in the .As template.
		//HRESULT hr = m_rtStateObject->QueryInterface( IID_PPV_ARGS( &rtStateObjectProps ) );

		// Get the ray generation shader identifier.
		void* rayGenShaderID = rtStateObjectProps->GetShaderIdentifier( L"rayGen" );

		// Calculate the size of the whole table based on the alignment restrictions.
		const UINT shaderIDSize{ D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES };
		const UINT recordSize{ alignedSize( shaderIDSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT ) };
		const UINT sbtSize{ alignedSize( recordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT ) };

		CreateSBTUploadHeap( sbtSize );
		CreateSBTDefaultHeap( sbtSize );
		CopySBTDataToUploadHeap( rayGenShaderID );
		CopySBTDataToDefaultHeap();
		PrepareDispatchRayDesc( sbtSize );

		log( "[ Ray Tracing ] Shader binding table created." );
	}

	void WolfRenderer::CreateSBTUploadHeap( UINT sbtSize ) {
		D3D12_HEAP_PROPERTIES heapProps{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ) };
		D3D12_RESOURCE_DESC sbtDesc{ CD3DX12_RESOURCE_DESC::Buffer( sbtSize ) };
		sbtDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&sbtDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &m_sbtUploadBuff )
		);
		CHECK_HR( "Failed to create SBT upload buffer.", hr, log );

		log( "[ Ray Tracing ] SBT upload heap created." );
	}

	void WolfRenderer::CreateSBTDefaultHeap( UINT sbtSize ) {
		D3D12_HEAP_PROPERTIES heapProps{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };
		D3D12_RESOURCE_DESC sbtDesc{ CD3DX12_RESOURCE_DESC::Buffer( sbtSize ) };
		sbtDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&sbtDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS( &m_sbtDefaultBuff )
		);
		CHECK_HR( "Failed to create SBT upload buffer.", hr, log );

		log( "[ Ray Tracing ] SBT default heap created." );
	}

	void WolfRenderer::CopySBTDataToUploadHeap( void* rayGenShaderID ) {
		UINT8* pData{ nullptr };
		HRESULT hr{ m_sbtUploadBuff->Map( 0, nullptr, reinterpret_cast<void**>(&pData) ) };
		CHECK_HR( "Failed to map SBT upload buffer.", hr, log );

		memcpy( pData, rayGenShaderID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
		m_sbtUploadBuff->Unmap( 0, nullptr );

		log( "[ Ray Tracing ] SBT data copied to upload heap." );
	}

	void WolfRenderer::CopySBTDataToDefaultHeap() {
		ResetCommandAllocatorAndList();

		m_cmdList->CopyResource( m_sbtDefaultBuff.Get(), m_sbtUploadBuff.Get() );

		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		barrier.Transition.pResource = m_sbtDefaultBuff.Get();

		m_cmdList->ResourceBarrier( 1, &barrier );

		HRESULT hr{ m_cmdList->Close() };
		CHECK_HR( "Failed to close command list after copying SBT data.", hr, log );
		ID3D12CommandList* ppCmdLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCmdLists ), ppCmdLists );

		WaitForGPUSync();

		log( "[ Ray Tracing ] SBT data copied from upload heap to default heap." );
	}

	void WolfRenderer::PrepareDispatchRayDesc( UINT sbtSize ) {
		m_dispatchRaysDesc.RayGenerationShaderRecord.StartAddress =
			m_sbtDefaultBuff->GetGPUVirtualAddress(); // GPU virtual address where SBT is stored.
		m_dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = sbtSize;
		m_dispatchRaysDesc.Width = m_renderWidth;
		m_dispatchRaysDesc.Height = m_renderHeight;
		m_dispatchRaysDesc.Depth = 1;
		m_dispatchRaysDesc.MissShaderTable = {};
		m_dispatchRaysDesc.HitGroupTable = {};
		m_dispatchRaysDesc.CallableShaderTable = {};

		log( "[ Ray Tracing ] Dispatch ray description prepared." );
	}

	ComPtr<IDxcBlob> WolfRenderer::CompileShader(
		const std::wstring& filePath,
		const std::wstring& entryPoint,
		const std::wstring& target
	) {
		// Initialize the compiler.
		HRESULT hr{ gDxcDllHelper.Initialize() };
		CHECK_HR( "Failed to initialize DXC DLL.", hr, log );

		// Legacy version. Modern (recommended) approach uses IDxcCompiler3, no
		// IDxcLibrary and no IDxcBlobEncoding.
		// More info at: https://www.notion.so/DirectX-Ray-Tracing-Implementation-2bb01bc2e962804dad4ef2e3fbbc0487?source=copy_link#2cb01bc2e96280f29d7af9ed81ee8dad
		ComPtr<IDxcLibrary> library;
		ComPtr<IDxcCompiler> compiler;
		ComPtr<IDxcBlobEncoding> sourceBlob;

		hr = DxcCreateInstance( CLSID_DxcLibrary, IID_PPV_ARGS( &library ) );
		CHECK_HR( "Failed to create DXC Library instance.", hr, log );

		hr = DxcCreateInstance( CLSID_DxcCompiler, IID_PPV_ARGS( &compiler ) );
		CHECK_HR( "Failed to create DXC Compiler instance.", hr, log );

		hr = library->CreateBlobFromFile( filePath.c_str(), nullptr, &sourceBlob );
		std::string filePathStr{ wideStrToUTF8( filePath ) };
		std::filesystem::path relPath{ filePath };
		std::filesystem::path absPath{ std::filesystem::absolute( relPath ) };
		CHECK_HR( std::format(
			"Failed to create blob from shader file: {}\nAbsolut Path: {}.", filePathStr, absPath.string() ), hr, log );

		// Compile shader.
		LPCWSTR args[] = {
			filePath.c_str(),
			L"-E", entryPoint.c_str(),
			L"-T", target.c_str(),
			L"-Zi",            // Enable debug information
			L"-Qembed_debug",  // Embed debug information in the shader
			L"-Od",            // Disable optimizations for easier debugging
			L"-Zpr"            // Pack matrices in row-major order
		};

		ComPtr<IDxcOperationResult> result{};
		hr = compiler->Compile(
			sourceBlob.Get(),
			filePath.c_str(),
			entryPoint.c_str(),
			target.c_str(),
			args,
			_countof( args ),
			nullptr,
			0,
			nullptr,
			&result
		);
		CHECK_HR( "Failed to compile shader.", hr, log );

		result->GetStatus( &hr );
		if ( FAILED( hr ) ) {
			ComPtr<IDxcBlobEncoding> errorBlob{};
			hr = result->GetErrorBuffer( &errorBlob );
			CHECK_HR( "Failed to get shader compilation error buffer.", hr, log );

			const char* msg{ static_cast<char*>(errorBlob->GetBufferPointer()) };
			log( std::format( "Shader Compilation Error: {}", msg ), LogLevel::Error );
			CHECK_HR( "Shader compilation failed.", hr, log );
		}

		ComPtr<IDxcBlob> shaderBlob{};
		hr = result->GetResult( &shaderBlob );
		CHECK_HR( "Failed to get compiled shader blob.", hr, log );

		log( std::format( "[ Ray Tracing ] Copiled shader: {} with entry point: {}.",
			filePathStr, wideStrToUTF8( entryPoint ) ) );

		return shaderBlob;
	}

	void WolfRenderer::FrameBeginRasterization() {
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

	void WolfRenderer::RenderFrameRasterization( float offsetX, float offsetY ) {
		m_cmdList->SetPipelineState( m_pipelineState.Get() );
		m_cmdList->SetGraphicsRootSignature( m_rootSignature.Get() );

		// IA stands for Input Assembler
		m_cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
		m_cmdList->IASetVertexBuffers( 0, 1, &m_vbView );

		m_cmdList->RSSetViewports( 1, &m_viewport );
		m_cmdList->RSSetScissorRects( 1, &m_scissorRect );

		// Mask the offset float values as uint values.
		m_cmdList->SetGraphicsRoot32BitConstant( 0, static_cast<UINT>(m_frameIdx), 0 );
		m_cmdList->SetGraphicsRoot32BitConstant( 0, *reinterpret_cast<const UINT*>(&offsetX), 1 );
		m_cmdList->SetGraphicsRoot32BitConstant( 0, *reinterpret_cast<const UINT*>(&offsetY), 2 );

		m_cmdList->DrawInstanced( 3, 1, 0, 0 );
	}

	void WolfRenderer::FrameEndRasterization() {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_cmdList->ResourceBarrier( 1, &barrier );
	}

	/*  #####     ###    ######  ######  ######  #####
	 *  ##  ##   ## ##   ##        ##    ##	     ##  ##
	 *  #####   #######  ######    ##    #####   #####
	 *  ## ##   ##   ##      ##    ##    ##	     ## ##
	 *  ##  ##  ##   ##  ######    ##    ######  ##  ## */

	void WolfRenderer::PrepareForRasterization() {
		CreateRootSignature();
		CreatePipelineState();
		CreateVertexBuffer();
		CreateViewport();
		log( "[ Rasterization ] Successful preparation." );
	}

	void WolfRenderer::CreateRootSignature() {
		CD3DX12_ROOT_PARAMETER1 rootParam{};
		rootParam.InitAsConstants( 3, 0, 0, D3D12_SHADER_VISIBILITY_ALL );

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1( 1, &rootParam, 0, nullptr,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT );

		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeVersionedRootSignature(
			&rootSignatureDesc,
			&signatureBlob,
			&errorBlob
		);

		if ( errorBlob ) {
			const char* msg{ static_cast<char*>(errorBlob->GetBufferPointer()) };
			log( std::format( "Root Signature Error: {}", msg ), LogLevel::Error );
		}
		CHECK_HR( "Failed to serialize root signature.", hr, log );

		hr = m_device->CreateRootSignature(
			0,
			signatureBlob->GetBufferPointer(),
			signatureBlob->GetBufferSize(),
			IID_PPV_ARGS( &m_rootSignature )
		);
		CHECK_HR( "CreateRootSignature failed.", hr, log );
		log( "[ Rasterization ] Root signature created." );
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
		CHECK_HR( "Failed to create pipeline state.", hr, log );
		log( "[ Rasterization ] Pipeline state created." );
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
		CHECK_HR( "Failed to create upload vertex buffer.", hr, log );

		// Copy data from CPU to Upload Buffer
		void* pVertexData;
		hr = uploadBuffer->Map( 0, nullptr, &pVertexData );
		CHECK_HR( "Failed to map upload buffer.", hr, log );
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
		CHECK_HR( "Failed to create default vertex buffer.", hr, log );

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
		CHECK_HR( "Failed to close command list for Vertex Buffer upload..", hr, log );

		// Execute the commands
		ID3D12CommandList* ppCommandLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

		// Wait for the GPU to finish the copy BEFORE the 'uploadBuffer' ComPtr
		// goes out of scope. Otherwise, is's destroyed while the GPU is
		// trying to read from it, causing a crash/device removal.
		m_cmdQueue->Signal( m_fence.Get(), ++m_fenceValue );
		WaitForGPUSync();

		m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vbView.StrideInBytes = sizeof( Vertex );
		m_vbView.SizeInBytes = vertSize;

		log( "[ Rasterization ] Vertex buffer successfully uploaded to GPU default heap." );
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
		log( "[ Rasterization ] Viewport set up." );
	}

	/*  ######  ######  ###     ###	 ###     ###  ######  ###     ##
	 *  ##      ##  ##  ## ## ## ##	 ## ## ## ##  ##  ##  ## ##   ##
	 *  ##      ##  ##  ##  ###  ##	 ##  ###  ##  ##  ##  ##  ##  ##
	 *  ##      ##  ##  ##       ##	 ##       ##  ##  ##  ##   ## ##
	 *  ######  ######  ##       ##  ##       ##  ######  ##     ### */

	void WolfRenderer::FrameEnd() {
		HRESULT hr{ m_cmdList->Close() };
		assert( SUCCEEDED( hr ) ); // CHECK_HR( "Failed to close command list!", hr, log, LogLevel::Error );

		// Execute the command list.
		ID3D12CommandList* ppCommandLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

		// Present the frame.
		hr = m_swapChain->Present( 0, 0 ); // Sync Interval: 1 to enable VSync.
		assert( SUCCEEDED( hr ) ); // CHECK_HR( "Failed to present the frame!", hr, log, LogLevel::Error );

		// Increment m_fenceValue for every operation
		m_cmdQueue->Signal( m_fence.Get(), ++m_fenceValue );

		WaitForGPUSync();

		++m_frameIdx;
		m_scFrameIdx = m_swapChain->GetCurrentBackBufferIndex();
	}

	void WolfRenderer::CreateDevice() {
		HRESULT hr = CreateDXGIFactory1( IID_PPV_ARGS( &m_dxgiFactory ) );
		CHECK_HR( "Failed to create DXGI Factory.", hr, log );
		log( "Factory created." );

		AssignAdapter();

		hr = D3D12CreateDevice(
			m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS( &m_device ) );
		CHECK_HR( "Failed to create D3D12 Device.", hr, log );

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

			CHECK_HR( std::format( "Failed to get description for adapter index {}", adapterIdx ), hr, log );

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
		CHECK_HR( "Failed to get adapter description.", hr, log );

		log( wideStrToUTF8( std::format( L"Adapter: {}", desc.Description ) ) );
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
		CHECK_HR( "Failed to create Command Queue.", hr, log );

		hr = m_device->CreateCommandAllocator( queueDesc.Type, IID_PPV_ARGS( &m_cmdAllocator ) );
		CHECK_HR( "Failed to create Command Allocator.", hr, log );

		hr = m_device->CreateCommandList(
			0,                    // NodeMask: 0 for single GPU systems.
			queueDesc.Type,       // Type: Must match the Command Queue type (usually DIRECT).
			m_cmdAllocator.Get(),   // Command Allocator: The memory pool to record commands into.
			nullptr, // Initial Pipeline State Object (PSO): Commonly set to nullptr at creation.
			IID_PPV_ARGS( &m_cmdList ) // The Interface ID and output pointer.
		);
		CHECK_HR( "Failed to create Command List.", hr, log );

		// Good practice to close the command list right after creation. Reset() opens it.
		hr = m_cmdList->Close();
		CHECK_HR( "Failed to close the Command List.", hr, log );

		log( "Command List created." );
	}

	void WolfRenderer::CreateFence() {
		// Create a fence for GPU-CPU synchronization
		HRESULT hr = m_device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &m_fence ) );
		CHECK_HR( "Failed creating a Fence.", hr, log );

		// Create an event handle for the fence ( wait )
		m_fenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );

		if ( m_fenceEvent == nullptr ) {
			log( "Failed creating Fence Event.", LogLevel::Critical );
			return;
		}

		log( "Fence and fence event created." );
	}

	void WolfRenderer::WaitForGPUSync() {
		//log( "Waiting for GPU to render frame." );
		// Wait for the GPU to finish
		if ( m_fence->GetCompletedValue() < m_fenceValue ) {
			HRESULT hr = m_fence->SetEventOnCompletion( m_fenceValue, m_fenceEvent );
			assert( SUCCEEDED( hr ) );
			//CHECK_HR( "Failed setting fence value on GPU competion.", hr, log );
			WaitForSingleObject( m_fenceEvent, INFINITE );
		}
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
		CHECK_HR( "Failed to create a Swap Chain.", hr, log );

		hr = swapChain1->QueryInterface( IID_PPV_ARGS( &m_swapChain ) );
		CHECK_HR( "Failed to convert Swap Chain output to newer version.", hr, log );

		log( "Swap Chain created." );
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
		CHECK_HR( "Failed creating a descriptor heap for the swap chain.", hr, log );
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
			CHECK_HR( "Failed getting a buffer.", hr, log );
			log( "Successfully got a buffer." );

			m_rtvHandles[scBuffIdx] = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
			m_rtvHandles[scBuffIdx].ptr += scBuffIdx * m_rtvDescriptorSize;

			m_device->CreateRenderTargetView(
				m_renderTargets[scBuffIdx].Get(),
				nullptr,
				m_rtvHandles[scBuffIdx]
			);
		}
		log( "Render target views created from swap chain." );
	}

	void WolfRenderer::ResetCommandAllocatorAndList() {
		HRESULT hr = m_cmdAllocator->Reset();
		assert( SUCCEEDED( hr ) );
		//CHECK_HR( "Failed resetting command allocator.", hr, log );

		hr = m_cmdList->Reset( m_cmdAllocator.Get(), nullptr );
		assert( SUCCEEDED( hr ) );
		//CHECK_HR( "Failed resetting command list.", hr, log );
	}

	/*  ##  ##  ###     ##  ##  ##  ######  ######  #####
	 *  ##  ##  ## ##   ##  ##  ##  ##      ##      ##  ##
	 *  ##  ##  ##  ##  ##  ##  ##  ######  #####   ##  ##
	 *  ##  ##  ##   ## ##  ##  ##      ##  ##      ##  ##
	 *  ######  ##     ###  ######  ######  ######  #####  */

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

		CHECK_HR( "Failed to create GPU Resource.", hr, log );
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
		CHECK_HR( "Failed to create GPU Resource.", hr, log );

		log( "Readback buffer created." );
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
		CHECK_HR( "Failed to close command list!", hr, log );

		log( "Texture copy commands added. Command list closed." );
	}
}