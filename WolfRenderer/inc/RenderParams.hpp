#ifndef RENDERPARAMS_HPP
#define RENDERPARAMS_HPP

#include <cstdint> // uint32_t
#include <windows.h> // BOOL

#include "Camera.hpp" // Camera, ScreenConstants
#include "Scene.hpp" // SceneData

namespace Core {
	/// The mode to use for rendering.
	enum class RenderMode {
		Rasterization,
		RayTracing
	};
}

namespace RT {
	struct Data {
		RT::Camera camera{}; ///< Camera used in the scene.

		BOOL randomColors{ true }; ///< Whether to color each triangle in a random color.
		uint32_t bgColorPacked{ 0xFF2D2D2D }; ///< Scene background color.

		void SetMatchRTCameraToRaster( bool match ) {
			m_matchRTCamToRaster = 1 - (2 * match);
		}

		int GetMatchRTCameraToRaster() {
			return m_matchRTCamToRaster;
		}
	private:
		int m_matchRTCamToRaster{ 1 }; ///< Match RT camera to Raster camera. Default 1 = false.
	};
}

namespace Raster {
	struct Data {
		Transformation camera{}; ///< Camera/object transformation data.
		ScreenConstants screenData{};
		SceneDataCB sceneData{}; ///< Scene data for Raster mode.
		bool renderFaces{ true }; ///< Whether to render faces.
		bool renderEdges{ false }; ///< Whether to render edges.
		bool renderVerts{ false }; ///< Whether to render vertices.
		bool showBackfaces{ false }; ///< Whether to render backfaces.
		float vertexSize{ 2.5f }; ///< Size in pixels of the displayed vertices.
		uint32_t edgeColor{}; ///< Default color for rendered edges.
		uint32_t vertexColor{ 0xFFFF7224 }; ///< Default color for rendered vertices.
		float bgColor[4] = { 0.1764f, 0.1764f, 0.1764f, 1.f }; ///< Scene background color.
	};
}

#endif // RENDERPARAMS_HPP