#ifndef LIGHTS_HPP
#define LIGHTS_HPP

#include "DirectXMath.h"

namespace Raster {
	struct DirectionalLight {
		struct alignas(16) CB {
			DirectX::XMFLOAT3 directionVS{ -0.9f, -1.f, -0.4f }; // normalized, view-space
			uint32_t pckedColor{ 0xFFFFFFFF };

			//DirectX::XMFLOAT3 color;
			float intensity{ 1.f };
			float specularStrength{ 32.f };
			float _pad0;
			float _pad1;
		} cb;
		DirectX::XMFLOAT3 directionWS{ -0.3f, -1.f, -0.2f };
	};
}

#endif // LIGHTS_HPP66