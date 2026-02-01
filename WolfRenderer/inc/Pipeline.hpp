#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <wrl/client.h>

#include "AppData.hpp"
#include "d3d12.h"


using Microsoft::WRL::ComPtr;

namespace Core {
	class Pipeline {
	public:
		Pipeline( ComPtr<ID3D12Device14>, AppData* );

		/// Creates the pipeline state object, which holds the rasterization configuration.
		void CreatePipelineStates();

		/// Creates a root signature, which defines what resources are bound to the pipeline.
		void CreateRootSignatureDefault();

		/// Creates another root signature for the Edges (Wireframe) pipeline.
		void CreateRootSignatureEdges();

		/// Creates another root signature for the Vertices (Points) pipeline.
		void CreateRootSignatureVertices();

		/// Creates another root signature for the shadow maps pipeline.
		void CreateRootSignatureShadows();

		/// Creates a depth buffer and DSV Heap.
		void CreateDepthStencil();

	public: // Members
		ComPtr<ID3D12DescriptorHeap> dsvHeapDepthStencil{ nullptr };
		/// The root signature defining the resources bound to the default pipeline.
		ComPtr<ID3D12RootSignature> rootSignatureDefault{ nullptr };
		/// The root signature defining the resources bound to the edges (wireframe) pipeline.
		ComPtr<ID3D12RootSignature> rootSignatureEdges{ nullptr };
		/// The root signature defining the resources bound to the vertices(points) pipeline.
		ComPtr<ID3D12RootSignature> rootSignatureVertices{ nullptr };
		/// The root signature defining the resources bound to the shadow map pipeline.
		ComPtr<ID3D12RootSignature> rootSignatureShadows{ nullptr };

		/// The pipeline state object holding the pipeline configuration for rendering faces.
		ComPtr<ID3D12PipelineState> stateFaces{ nullptr };
		/// A pipeline state object without backface culling.
		ComPtr<ID3D12PipelineState> stateNoCull{ nullptr };
		/// A pipeline state object for rendering edges.
		ComPtr<ID3D12PipelineState> stateEdges{ nullptr };
		/// A pipeline state object for rendering vertices.
		ComPtr<ID3D12PipelineState> stateVertices{ nullptr };
		/// A pipeline state object for rendering shadows.
		ComPtr<ID3D12PipelineState> stateShadows{ nullptr };

	private:
		// Depth stencil buffer for 3D depth recognition.
		ComPtr<ID3D12Resource> m_depthStencilBuffer{ nullptr };
		DXGI_FORMAT m_depthFormat{ DXGI_FORMAT_D32_FLOAT };

		ComPtr<ID3D12Device14> m_device{ nullptr };

		AppData* m_app{ nullptr }; ///< Pointer to application-level data.
	};
}

#endif // PIPELINE_HPP