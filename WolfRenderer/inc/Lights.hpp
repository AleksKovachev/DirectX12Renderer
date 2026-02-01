#ifndef LIGHTS_HPP
#define LIGHTS_HPP

#include "DirectXMath.h"

namespace Raster {
	struct LightParams {
		uint32_t shadowMapSize{ 8192 };
	};

	struct DirectionalLight {
		struct alignas(16) CB {
			DirectX::XMFLOAT4 color{ 1.f, 1.f, 1.f, 1.f };
			DirectX::XMFLOAT3 directionVS{ -0.9f, -1.f, -0.4f }; // normalized, view-space

			float intensity{ 1.f };
			float specularStrength{ 32.f };
			float shadowBias{ 0.f };
			float ambientIntensity{ 0.15f };
			float _pad;
		} cb;
		DirectX::XMFLOAT3 directionWS{ -0.3f, -1.f, -0.2f };

		/* Use cascades to better handle the correlation betwen shadows in the
		 distance and shadow map size. This divides the dir light's view frustum
		 into multiple parts and rendering a separate shadow. Needs transitions. */
		// https://learnopengl.com/Guest-Articles/2021/CSM
		// https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
		float shadowExtent{ 200.f }; // Range: [50-2000]
		float nearZ{ 0.01f };
		float farZ{ 1000.f };
	};

	struct alignas(16) LightMatricesCB {
		/// A projection matrix from the light's point of view. Used for shadow maps;
		DirectX::XMFLOAT4X4 dirLightViewProjMatrix;
	};
}

#endif // LIGHTS_HPP