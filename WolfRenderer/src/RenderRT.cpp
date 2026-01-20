#include "Renderer.hpp"
#include "utils.hpp" // CHECK_HR

#include "d3dx12_barriers.h"
#include "d3dx12_core.h"
#include "d3dx12_root_signature.h"
#include "dxcapi.use.h"
#include <DirectXMath.h>
#include <dxc/dxcapi.h>

#include <filesystem> // path, absolute


namespace Core {
	dxc::DxcDllSupport gDxcDllHelper{};

	void WolfRenderer::PrepareForRayTracing() {
		CreateGlobalRootSignature();
		CreateVertexBufferRT();
		CreateCameraConstantBuffer();
		CreateRayTracingPipelineState();
		CreateRayTracingShaderTexture();
		CreateAccelerationStructures();
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
		ID3D12DescriptorHeap* heaps[] = { m_uavsrvHeap.Get() };
		m_cmdList->SetDescriptorHeaps( _countof( heaps ), heaps );
		m_cmdList->SetComputeRootSignature( m_globalRootSignature.Get() );

		// Slot 0: Description Table
		m_cmdList->SetComputeRootDescriptorTable(
			0, m_uavsrvHeap->GetGPUDescriptorHandleForHeapStart() );

		// Slot 1: Root Constant - random colors.
		m_cmdList->SetComputeRoot32BitConstant( 1, dataRT.bgColorPacked, 0 );
		m_cmdList->SetComputeRoot32BitConstant( 1, dataRT.randomColors, 1 );

		// Slot 2: Camera Data.
		m_cmdList->SetComputeRootConstantBufferView(
			2, dataRT.camera.cb->GetGPUVirtualAddress() );


		// Slot 3: Root Constant - background color.
		m_cmdList->SetComputeRoot32BitConstant( 3, dataRT.bgColorPacked, 0 );

		dataRT.camera.cbData.cameraPosition = dataRT.camera.position;
		dataRT.camera.cbData.cameraForward = dataRT.camera.forward;
		dataRT.camera.cbData.cameraRight = dataRT.camera.right;
		dataRT.camera.cbData.cameraUp = dataRT.camera.up;
		dataRT.camera.cbData.verticalFOV = dataRT.camera.verticalFOV;
		dataRT.camera.cbData.aspectRatio = dataRT.camera.aspectRatio;
		// Convert bool value to either -1 or 1 to avoid if branch and get multiplier.
		dataRT.camera.cbData.forwardMult = dataRT.GetMatchRTCameraToRaster();

		memcpy( dataRT.camera.cbMappedPtr, &dataRT.camera.cbData, sizeof( dataRT.camera.cbData ) );

		m_cmdList->SetPipelineState1( m_rtStateObject.Get() );
		m_cmdList->DispatchRays( &m_dispatchRaysDesc );
	}

	void WolfRenderer::FrameEndRayTracing() {
		D3D12_RESOURCE_BARRIER barrier{};
		barrier.Transition.pResource = m_raytracingOutput.Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		m_cmdList->ResourceBarrier( 1, &barrier );

		m_cmdList->CopyResource( m_renderTargets[m_scFrameIdx].Get(), m_raytracingOutput.Get() );

		barrier.Transition.pResource = m_renderTargets[m_scFrameIdx].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_cmdList->ResourceBarrier( 1, &barrier );
	}

	void WolfRenderer::CreateGlobalRootSignature() {
		D3D12_DESCRIPTOR_RANGE1 ranges[2] = {};

		/* It's very important which range at which position will be placed. This directly
		 * correlates to the order of creation. Here, since UAV is created first in the
		 * CreateRayTracingShaderTexture() with the CreateUnorderedAccessView() call, then
		 * the SRV is created in the CreateTLASShaderResourceView() with the
		 * CreateShaderResourceView() call and the handle which specifically states SRV should
		 * be created with an offset of 1. */

		// Create a Range of type UAV.
		ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		ranges[0].NumDescriptors = 1;
		ranges[0].BaseShaderRegister = 0; // u0
		ranges[0].RegisterSpace = 0;
		ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Create a Range of type SRV.
		ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		ranges[1].NumDescriptors = 1;
		ranges[1].BaseShaderRegister = 0; // t0
		ranges[1].RegisterSpace = 0;
		ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// Describe the root parameter that will be stored in the Root Signature.
		D3D12_ROOT_PARAMETER1 rootParams[3] = {};

		// Param 0 - descriptor table with UAV and SRV.
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 2;
		rootParams[0].DescriptorTable.pDescriptorRanges = ranges;

		// Param 1 - random color and background color root constants.
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[1].Constants.Num32BitValues = 2;
		rootParams[1].Constants.ShaderRegister = 0; // b0
		rootParams[1].Constants.RegisterSpace = 0;

		// Param 2 - camera data.
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[2].Descriptor.ShaderRegister = 1; // b1
		rootParams[2].Descriptor.RegisterSpace = 0;

		// Pass the Root Signature Parameter to the Root Signature Description.
		D3D12_ROOT_SIGNATURE_DESC1 rootSigDesc{};
		rootSigDesc.NumParameters = _countof( rootParams );
		rootSigDesc.pParameters = rootParams;
		rootSigDesc.NumStaticSamplers = 0;
		rootSigDesc.pStaticSamplers = nullptr;
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDescVersioned{};
		rootSigDescVersioned.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootSigDescVersioned.Desc_1_1 = rootSigDesc;

		// Serialize the root signature (similar to compilation process).
		ComPtr<ID3DBlob> signatureBlob{};
		ComPtr<ID3DBlob> errorBlob{};
		HRESULT hr = D3D12SerializeVersionedRootSignature(
			&rootSigDescVersioned,
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
		D3D12_STATE_SUBOBJECT closestHitLibSubobject{ CreateClosestHitLibSubObject() };
		D3D12_STATE_SUBOBJECT missLibSubobject{ CreateMissLibSubObject() };
		D3D12_STATE_SUBOBJECT shaderConfigSubobject{ CreateShaderConfigSubObject() };
		D3D12_STATE_SUBOBJECT pipelineConfigSubobject{ CreatePipelineConfigSubObject() };
		D3D12_STATE_SUBOBJECT rootSignatureSubobject{ CreateRootSignatureSubObject() };
		D3D12_STATE_SUBOBJECT hitGroupSubobject{ CreateHitGroupSubObject() };

		// Create the actual State Object.
		std::vector<D3D12_STATE_SUBOBJECT> subobjects{
			rayGenLibSubobject,
			closestHitLibSubobject,
			missLibSubobject,
			shaderConfigSubobject,
			pipelineConfigSubobject,
			rootSignatureSubobject,
			hitGroupSubobject
		};

		// Describe the ray tracing pipeline state object.
		D3D12_STATE_OBJECT_DESC rtPSODesc{};
		rtPSODesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		rtPSODesc.NumSubobjects = static_cast<UINT>(subobjects.size());
		rtPSODesc.pSubobjects = subobjects.data();

		// Create the ray tracing pipeline state object.
		HRESULT hr = m_device->CreateStateObject(
			&rtPSODesc, IID_PPV_ARGS( &m_rtStateObject ) );
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

	D3D12_STATE_SUBOBJECT WolfRenderer::CreateClosestHitLibSubObject() {
		m_closestHitBlob = CompileShader(
			L"shaders/ray_tracing_shaders.hlsl", L"closestHit", L"lib_6_5" );

		// Define what to export (the shader entry point).
		m_closestHitExportDesc.Name = L"closestHit";
		m_closestHitExportDesc.Flags = D3D12_EXPORT_FLAG_NONE;

		// Describe the DXIL library.
		m_closestHitLibDesc.DXILLibrary.pShaderBytecode = m_closestHitBlob->GetBufferPointer();
		m_closestHitLibDesc.DXILLibrary.BytecodeLength = m_closestHitBlob->GetBufferSize();
		m_closestHitLibDesc.NumExports = 1;
		m_closestHitLibDesc.pExports = &m_closestHitExportDesc;

		// Compose the actual subobject.
		D3D12_STATE_SUBOBJECT closestHitLibSubobject{};
		closestHitLibSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		closestHitLibSubobject.pDesc = &m_closestHitLibDesc;

		log( "[ Ray Tracing ] Closest hit library pipeline state sub-object created." );

		return closestHitLibSubobject;
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

	D3D12_STATE_SUBOBJECT WolfRenderer::CreateHitGroupSubObject() {
		m_hitGroupDesc.HitGroupExport = L"HitGroup";
		m_hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
		m_hitGroupDesc.ClosestHitShaderImport = L"closestHit";

		D3D12_STATE_SUBOBJECT hitGroupSubobject{};
		hitGroupSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
		hitGroupSubobject.pDesc = &m_hitGroupDesc;

		log( "[ Ray Tracing ] Hit group pipeline state sub-object created." );

		return hitGroupSubobject;
	}

	void WolfRenderer::CreateRayTracingShaderTexture() {
		// Describe the output texture.
		D3D12_RESOURCE_DESC texDesc{ CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R8G8B8A8_UNORM,
			scene.settings.renderWidth,
			scene.settings.renderHeight,
			1,
			1
		) };
		texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		// Describe the heap that will contain the resource.
		D3D12_HEAP_PROPERTIES heapProps{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };

		// Create and allocate the resource in the GPU memory.
		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&texDesc,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			nullptr,
			IID_PPV_ARGS( &m_raytracingOutput )
		);
		CHECK_HR( "Failed to create ray tracing output texture.", hr, log );

		// Create a descriptor heap and a descriptor for the output texture.
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.NumDescriptors = 2;
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		hr = m_device->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS( &m_uavsrvHeap )
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
			m_uavsrvHeap->GetCPUDescriptorHandleForHeapStart()
		);

		log( "[ Ray Tracing ] Shader output texture created." );
	}

	void WolfRenderer::CreateShaderBindingTable() {
		// Access the properties of the ray tracing pipeline state object.
		ComPtr<ID3D12StateObjectProperties> rtStateObjectProps;
		HRESULT hr{ m_rtStateObject.As( &rtStateObjectProps ) };
		CHECK_HR( "Failed to access ray tracing state object properties.", hr, log );

		// Older approach, used in the .As template.
		//HRESULT hr = m_rtStateObject->QueryInterface( IID_PPV_ARGS( &rtStateObjectProps ) );

		// Get the ray generation shader identifier.
		void* rayGenShaderID = rtStateObjectProps->GetShaderIdentifier( L"rayGen" );
		void* missShaderID = rtStateObjectProps->GetShaderIdentifier( L"miss" );
		void* hitGroupID = rtStateObjectProps->GetShaderIdentifier( L"HitGroup" );

		// Calculate the size of the whole table based on the alignment restrictions.
		const UINT shaderIDSize{ D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES };
		const UINT recordSize{ alignedSize( shaderIDSize,
			D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT ) };

		UINT rayGenOffset{};
		UINT missOffset{ alignedSize( rayGenOffset + recordSize,
			D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT ) };
		UINT hitGroupOffset{ alignedSize( missOffset + recordSize,
			D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT ) };

		const UINT sbtSize{ hitGroupOffset + recordSize };

		CreateSBTUploadHeap( sbtSize );
		CreateSBTDefaultHeap( sbtSize );
		CopySBTDataToUploadHeap( rayGenOffset, missOffset, hitGroupOffset,
			 rayGenShaderID, missShaderID, hitGroupID );
		CopySBTDataToDefaultHeap();
		PrepareDispatchRayDesc( recordSize, rayGenOffset, missOffset, hitGroupOffset );

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
			// D3D12_RESOURCE_STATE_GENERIC_READ but ignored by DirectX. Debug Layer.
			D3D12_RESOURCE_STATE_COMMON,
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
			// D3D12_RESOURCE_STATE_COPY_DEST but ignored by DirectX. Debug Layer.
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( &m_sbtDefaultBuff )
		);
		CHECK_HR( "Failed to create SBT upload buffer.", hr, log );

		log( "[ Ray Tracing ] SBT default heap created." );
	}

	void WolfRenderer::CopySBTDataToUploadHeap(
		const UINT rayGenOffset, const UINT missOffset, const UINT hitGroupOffset,
		void* rayGenShaderID, void* missShaderID, void* hitGroupID
	) {
		UINT8* pData{ nullptr };
		HRESULT hr{ m_sbtUploadBuff->Map( 0, nullptr, reinterpret_cast<void**>(&pData) ) };
		CHECK_HR( "Failed to map SBT upload buffer.", hr, log );

		memcpy( pData + rayGenOffset, rayGenShaderID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
		memcpy( pData + missOffset, missShaderID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
		memcpy( pData + hitGroupOffset, hitGroupID, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
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

	void WolfRenderer::PrepareDispatchRayDesc(
		const UINT recordSize,
		const UINT rayGenOffset,
		const UINT missOffset,
		const UINT hitGroupOffset
	) {
		m_dispatchRaysDesc.RayGenerationShaderRecord.StartAddress =
			m_sbtDefaultBuff->GetGPUVirtualAddress() + rayGenOffset;
		m_dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = recordSize;

		m_dispatchRaysDesc.MissShaderTable.StartAddress =
			m_sbtDefaultBuff->GetGPUVirtualAddress() + missOffset;
		m_dispatchRaysDesc.MissShaderTable.SizeInBytes = recordSize;
		m_dispatchRaysDesc.MissShaderTable.StrideInBytes = recordSize;

		m_dispatchRaysDesc.HitGroupTable.StartAddress =
			m_sbtDefaultBuff->GetGPUVirtualAddress() + hitGroupOffset;
		m_dispatchRaysDesc.HitGroupTable.SizeInBytes = recordSize;
		m_dispatchRaysDesc.HitGroupTable.StrideInBytes = recordSize;

		m_dispatchRaysDesc.Width = scene.settings.renderWidth;
		m_dispatchRaysDesc.Height = scene.settings.renderHeight;
		m_dispatchRaysDesc.Depth = 1;
		m_dispatchRaysDesc.CallableShaderTable = {};

		log( "[ Ray Tracing ] Dispatch ray description prepared." );
	}

	void WolfRenderer::CreateVertexBufferRT() {
		std::vector<Vertex> rtVertices;
		for ( const Mesh& mesh : scene.GetMeshes() ) {
			for ( uint32_t i : mesh.indices ) {
				const Vertex& v = mesh.vertices[i];
				rtVertices.push_back( Vertex{ v.position, {} } );
			}
		}
		m_vertexCount = rtVertices.size();

		const size_t vertSize{ sizeof( Vertex ) * m_vertexCount };

		// Create the "Intermediate" Upload Buffer (Staging).
		ComPtr<ID3D12Resource> uploadBuffer{ nullptr };

		// Upload heap used so the CPU can write to it.
		D3D12_HEAP_PROPERTIES heapPropsUp{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ) };
		D3D12_RESOURCE_DESC bufferDescUp{ CD3DX12_RESOURCE_DESC::Buffer( vertSize ) };

		HRESULT hr{ m_device->CreateCommittedResource(
			&heapPropsUp,
			D3D12_HEAP_FLAG_NONE,
			&bufferDescUp,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( &uploadBuffer )
		) };
		CHECK_HR( "Failed to create upload vertex buffer.", hr, log );

		// Copy data from CPU to Upload Buffer
		void* pVertexData;
		hr = uploadBuffer->Map( 0, nullptr, &pVertexData );
		CHECK_HR( "Failed to map upload buffer.", hr, log );
		memcpy( pVertexData, rtVertices.data(), vertSize);
		uploadBuffer->Unmap( 0, nullptr );

		// Create the destination Vertex Buffer (Default Heap).
		// Default heap used (GPU VRAM) that CPU can't access directly.
		D3D12_HEAP_PROPERTIES heapPropsDf{ CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT ) };
		D3D12_RESOURCE_DESC bufferDescDf{ CD3DX12_RESOURCE_DESC::Buffer( vertSize ) };

		hr = m_device->CreateCommittedResource(
			&heapPropsDf,
			D3D12_HEAP_FLAG_NONE,
			&bufferDescDf,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( &m_vertexBufferRT )
		);
		CHECK_HR( "Failed to create default vertex buffer.", hr, log );

		// Name the buffer for easier debugging using PIX/NSIGHT.
		m_vertexBufferRT->SetName( L"Vertex Buffer RT Default Resource" );

		ResetCommandAllocatorAndList();

		// Copy data.
		m_cmdList->CopyBufferRegion( m_vertexBufferRT.Get(), 0, uploadBuffer.Get(), 0, vertSize );

		// Transition the Default Buffer from COPY_DEST to VERTEX_AND_CONSTANT_BUFFER
		// so the Input Assembled (IA) can read it during rendering.
		D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
			m_vertexBufferRT.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
		) };

		m_cmdList->ResourceBarrier( 1, &barrier );

		hr = m_cmdList->Close();
		CHECK_HR( "Failed to close command list for Vertex Buffer upload..", hr, log );

		// Execute the commands
		ID3D12CommandList* ppCommandLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

		// Wait for the GPU to finish the copy BEFORE the 'uploadBuffer' ComPtr
		// goes out of scope. Otherwise, is's destroyed while the GPU is
		// trying to read from it, causing a crash/device removal.
		WaitForGPUSync();

		log( "[ Ray Tracing ] Vertex buffer successfully uploaded to GPU default heap." );
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

	void WolfRenderer::CreateAccelerationStructures() {
		CreateBLAS();
		CreateTLAS();
		CreateTLASShaderResourceView();
		log( "[ Ray Tracing ] Acceleration structures created." );
	}

	void WolfRenderer::CreateBLAS() {
		// Describe triangle geometry for BLAS.
		D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC triangleDesc{};
		triangleDesc.VertexBuffer.StartAddress = m_vertexBufferRT->GetGPUVirtualAddress();
		triangleDesc.VertexBuffer.StrideInBytes = sizeof( Vertex );
		triangleDesc.VertexCount = static_cast<UINT>(m_vertexCount);
		triangleDesc.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		D3D12_RAYTRACING_GEOMETRY_DESC geomDesc{};
		geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geomDesc.Triangles = triangleDesc;

		// Build BLAS.
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs{};
		blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		blasInputs.NumDescs = 1;
		blasInputs.pGeometryDescs = &geomDesc;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasPrebuild{};
		m_device->GetRaytracingAccelerationStructurePrebuildInfo(
			&blasInputs, &blasPrebuild );
		assert( blasPrebuild.ResultDataMaxSizeInBytes > 0 );

		// Allocate BLAS and scratch.
		D3D12_RESOURCE_DESC blasDesc{};
		blasDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		blasDesc.Alignment = 0;
		blasDesc.Width = blasPrebuild.ResultDataMaxSizeInBytes;
		blasDesc.Height = 1;
		blasDesc.DepthOrArraySize = 1;
		blasDesc.MipLevels = 1;
		blasDesc.Format = DXGI_FORMAT_UNKNOWN;
		blasDesc.SampleDesc.Count = 1;
		blasDesc.SampleDesc.Quality = 0;
		blasDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		blasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		HRESULT hr{ m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&blasDesc,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS( &m_blasResult )
		) };
		CHECK_HR( "Failed to create BLAS buffer.", hr, log );

		// Scratch buffer.
		D3D12_RESOURCE_DESC scratchDesc{};
		scratchDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		scratchDesc.Alignment = 0;
		scratchDesc.Width = blasPrebuild.ScratchDataSizeInBytes;
		scratchDesc.Height = 1;
		scratchDesc.DepthOrArraySize = 1;
		scratchDesc.MipLevels = 1;
		scratchDesc.Format = DXGI_FORMAT_UNKNOWN;
		scratchDesc.SampleDesc.Count = 1;
		scratchDesc.SampleDesc.Quality = 0;
		scratchDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		scratchDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&scratchDesc,
			// D3D12_RESOURCE_STATE_UNORDERED_ACCESS but it's ignored by DirectX. Debug Layer.
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( &m_blasScratch )
		);
		CHECK_HR( "Failed to create BLAS scratch buffer.", hr, log );

		// Build the BLAS.
		ResetCommandAllocatorAndList();

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuild{};
		blasBuild.Inputs = blasInputs;
		blasBuild.DestAccelerationStructureData = m_blasResult->GetGPUVirtualAddress();
		blasBuild.ScratchAccelerationStructureData = m_blasScratch->GetGPUVirtualAddress();

		m_cmdList->BuildRaytracingAccelerationStructure( &blasBuild, 0, nullptr );

		// Wait for all UAV writes before continuing.
		D3D12_RESOURCE_BARRIER uavBLASBarrier{};
		uavBLASBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavBLASBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		// nullptr blocks all UAV read/writes instead of a specific one.
		uavBLASBarrier.UAV.pResource = nullptr;
		m_cmdList->ResourceBarrier( 1, &uavBLASBarrier );

		hr = m_cmdList->Close();
		CHECK_HR( "Failed to close command list after BLAS build.", hr, log );

		ID3D12CommandList* ppCmdLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppCmdLists ), ppCmdLists );

		WaitForGPUSync();

		log( "[ Ray Tracing ] Bottom-level acceleration structure (BLAS) created." );
	}

	void WolfRenderer::CreateTLAS() {
		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc{};
		DirectX::XMStoreFloat3x4(
			reinterpret_cast<DirectX::XMFLOAT3X4*>(&instanceDesc.Transform),
			DirectX::XMMatrixIdentity()
		);
		instanceDesc.InstanceID = 0;
		instanceDesc.InstanceMask = 1;
		instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
		instanceDesc.InstanceContributionToHitGroupIndex = 0;
		instanceDesc.AccelerationStructure = m_blasResult->GetGPUVirtualAddress();

		ComPtr<ID3D12Resource> instanceBuffer{};
		const UINT instanceSize{ sizeof( instanceDesc ) };

		D3D12_RESOURCE_DESC instanceDescBuff{};
		instanceDescBuff.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		instanceDescBuff.Alignment = 0;
		instanceDescBuff.Width = instanceSize;
		instanceDescBuff.Height = 1;
		instanceDescBuff.DepthOrArraySize = 1;
		instanceDescBuff.MipLevels = 1;
		instanceDescBuff.Format = DXGI_FORMAT_UNKNOWN;
		instanceDescBuff.SampleDesc.Count = 1;
		instanceDescBuff.SampleDesc.Quality = 0;
		instanceDescBuff.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		instanceDescBuff.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heapProps{};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		HRESULT hr{ m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&instanceDescBuff,
			// D3D12_RESOURCE_STATE_GENERIC_READ but it's ignored by DirectX. Debug Layer.
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( &instanceBuffer )
		) };
		CHECK_HR( "Failed to create TLAS instance buffer.", hr, log );

		// Copy instance data to the buffer.
		void* instData{ nullptr };
		hr = instanceBuffer->Map( 0, nullptr, &instData );
		CHECK_HR( "Failed to map TLAS instance buffer.", hr, log );
		memcpy( instData, &instanceDesc, instanceSize );
		instanceBuffer->Unmap( 0, nullptr );

		// Build TLAS.
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs{};
		tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
		tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		tlasInputs.NumDescs = 1;
		tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		tlasInputs.InstanceDescs = instanceBuffer->GetGPUVirtualAddress();

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuild{};
		m_device->GetRaytracingAccelerationStructurePrebuildInfo(
			&tlasInputs, &tlasPrebuild );

		// Allocate TLAS and scratch.
		D3D12_RESOURCE_DESC tlasDesc{};
		tlasDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		tlasDesc.Alignment = 0;
		tlasDesc.Width = tlasPrebuild.ResultDataMaxSizeInBytes;
		tlasDesc.Height = 1;
		tlasDesc.DepthOrArraySize = 1;
		tlasDesc.MipLevels = 1;
		tlasDesc.Format = DXGI_FORMAT_UNKNOWN;
		tlasDesc.SampleDesc.Count = 1;
		tlasDesc.SampleDesc.Quality = 0;
		tlasDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		tlasDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&tlasDesc,
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS( &m_tlasResult )
		);
		CHECK_HR( "Failed to create TLAS buffer.", hr, log );

		// Scratch buffer.
		D3D12_RESOURCE_DESC tlasScratchDesc{};
		tlasScratchDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		tlasScratchDesc.Alignment = 0;
		tlasScratchDesc.Width = tlasPrebuild.ScratchDataSizeInBytes;
		tlasScratchDesc.Height = 1;
		tlasScratchDesc.DepthOrArraySize = 1;
		tlasScratchDesc.MipLevels = 1;
		tlasScratchDesc.Format = DXGI_FORMAT_UNKNOWN;
		tlasScratchDesc.SampleDesc.Count = 1;
		tlasScratchDesc.SampleDesc.Quality = 0;
		tlasScratchDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		tlasScratchDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		ComPtr<ID3D12Resource> tlasScratch{};
		hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&tlasScratchDesc,
			// D3D12_RESOURCE_STATE_UNORDERED_ACCESS but it's ignored by DirectX. Debug Layer.
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( &tlasScratch )
		);
		CHECK_HR( "Failed to create TLAS scratch buffer.", hr, log );

		// Build the TLAS.
		ResetCommandAllocatorAndList();

		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuild{};
		tlasBuild.Inputs = tlasInputs;
		tlasBuild.DestAccelerationStructureData = m_tlasResult->GetGPUVirtualAddress();
		tlasBuild.ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress();

		m_cmdList->BuildRaytracingAccelerationStructure( &tlasBuild, 0, nullptr );

		// Wait for all UAV writes before continuing.
		D3D12_RESOURCE_BARRIER uavTLASBarrier{};
		uavTLASBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		uavTLASBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		uavTLASBarrier.UAV.pResource = nullptr;
		m_cmdList->ResourceBarrier( 1, &uavTLASBarrier );

		hr = m_cmdList->Close();
		CHECK_HR( "Failed to close command list after TLAS build.", hr, log );

		ID3D12CommandList* ppTLASCommandLists[] = { m_cmdList.Get() };
		m_cmdQueue->ExecuteCommandLists( _countof( ppTLASCommandLists ), ppTLASCommandLists );

		WaitForGPUSync();

		log( "[ Ray Tracing ] Top-level acceleration structure (TLAS) created." );
	}

	void WolfRenderer::CreateTLASShaderResourceView() {
		UINT handleSize = m_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

		// Get the handle for the SECOND slot (offset by 1).
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
			m_uavsrvHeap->GetCPUDescriptorHandleForHeapStart(), 1, handleSize );

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		srvDesc.RaytracingAccelerationStructure.Location = m_tlasResult->GetGPUVirtualAddress();
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		m_device->CreateShaderResourceView( nullptr, &srvDesc, srvHandle );

		log( "[ Ray Tracing ] TLAS shader resource view created." );
	}

	void WolfRenderer::UpdateRTCamera( RT::CameraInput& input ) {
		using namespace DirectX;

		float zDir = static_cast<float>(dataRT.GetMatchRTCameraToRaster());
		assert( zDir == 1.f || zDir == -1.f );

		dataRT.camera.yaw += input.mouseDeltaX * dataRT.camera.mouseSensitivity;
		dataRT.camera.setPitch(
			dataRT.camera.pitch + input.mouseDeltaY * dataRT.camera.mouseSensitivity * zDir );

		// Reset mouse delta for next frame.
		input.mouseDeltaX = 0.f;
		input.mouseDeltaY = 0.f;

		dataRT.camera.ComputeBasisVectors( zDir );

		XMVECTOR moveVec = XMVectorZero();

		if ( input.moveForward ) {
			XMVECTOR forwardVec = XMLoadFloat3( &dataRT.camera.forward );
			moveVec = XMVectorAdd( moveVec, XMVectorScale( forwardVec, zDir ) );
		}
		if ( input.moveBackward ) {
			XMVECTOR forwardVec = XMLoadFloat3( &dataRT.camera.forward );
			moveVec = XMVectorSubtract( moveVec, XMVectorScale( forwardVec, zDir ) );
		}
		if ( input.moveRight )
			moveVec = XMVectorAdd( moveVec, XMLoadFloat3( &dataRT.camera.right ) );
		if ( input.moveLeft )
			moveVec = XMVectorSubtract( moveVec, XMLoadFloat3( &dataRT.camera.right ) );
		if ( input.moveUp )
			moveVec = XMVectorAdd( moveVec, dataRT.camera.worldUp );
		if ( input.moveDown )
			moveVec = XMVectorSubtract( moveVec, dataRT.camera.worldUp );


		if ( !XMVector3Equal( moveVec, XMVectorZero() ) ) {
			float effectiveSpeed = dataRT.camera.movementSpeed;
			if ( input.speedModifier )
				effectiveSpeed *= dataRT.camera.speedMult;

			// Normalize for correct diagonal movement.
			moveVec = XMVector3Normalize( moveVec );
			moveVec = XMVectorScale( moveVec, effectiveSpeed * m_app->deltaTime );

			XMVECTOR pos = XMLoadFloat3( &dataRT.camera.position );

			pos = XMVectorAdd( pos, moveVec );
			XMStoreFloat3( &dataRT.camera.position, pos );
		}
	}

	void WolfRenderer::CreateCameraConstantBuffer() {
		D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD );
		D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer( sizeof( RT::CameraCB ) );

		HRESULT hr = m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( &dataRT.camera.cb )
		);
		CHECK_HR( "Failed to create Camera constant buffer", hr, log );

		// Map permanently for CPU writes
		hr = dataRT.camera.cb->Map(
			0, nullptr, reinterpret_cast<void**>(&dataRT.camera.cbMappedPtr) );
		CHECK_HR( "Failed to map Camera constant buffer", hr, log );

		memcpy( dataRT.camera.cbMappedPtr, &dataRT.camera.cbData, sizeof( dataRT.camera.cbData ) );
		log( "[ RayTracing ] Camera constant buffer created and mapped." );
	}
}