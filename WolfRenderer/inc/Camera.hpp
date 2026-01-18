#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "d3d12.h"

#include <algorithm> // clamp
#include <cmath> // cosf, sinf
#include <DirectXMath.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct alignas(16) RTCameraCB {
	DirectX::XMFLOAT3 cameraPosition;
	float verticalFOV;

	// For the rest, XMFLOAT4 would add an unused w component and remove the _pad.

	DirectX::XMFLOAT3 cameraForward;
	float aspectRatio;

	DirectX::XMFLOAT3 cameraRight;
	float _pad0;

	DirectX::XMFLOAT3 cameraUp;
	float _pad1;
};

struct Camera {
	static constexpr DirectX::XMVECTOR worldUp{ 0.f, 1.f, 0.f };
	static constexpr float maxPitch{ DirectX::XMConvertToRadians( 89.f ) };

	// Position & orientation.
	DirectX::XMFLOAT3 position{ 0.f, 0.f, 20.f }; ///< World space.
	float yaw{ DirectX::XM_PI }; ///< Rotation around world up (Y) in radians.
	float pitch{ 0.f }; ///< Rotation around local X in radians.

	// Movement.
	float movementSpeed{ 5.f }; ///< Units per second.
	float speedMult{ 3.5f }; ///< Multiplier when speed modifier is active.
	float mouseSensitivity{ 0.0005f }; ///< Radians per pixel.

	// Projection.
	float verticalFOV{ DirectX::XMConvertToRadians( 60.f ) }; ///< Radians.
	float aspectRatio{ 1.f };

	// Cached basis vectors.
	DirectX::XMFLOAT3 forward{ 0.f, 0.f, -1.f };
	DirectX::XMFLOAT3 right{ 1.f, 0.f, 0.f };
	DirectX::XMFLOAT3 up{ 0.f, 1.f, 0.f };

	// Constant buffer.
	ComPtr<ID3D12Resource> cb{ nullptr };
	RTCameraCB cbData{}; ///< Camera constant buffer data for RT mode.
	UINT8* cbMappedPtr = nullptr;

	/// Sets pitch value, clamping it to avoid gimbal lock.
	void setPitch( float value ) {
		pitch = std::clamp( value, -maxPitch, maxPitch );
	}

	/// Sets vertical field of view in degrees.
	void setVerticalFOVDeg( float value ) {
		verticalFOV = DirectX::XMConvertToRadians( value );
	}

	void ComputeBasisVectors() {
		using namespace DirectX;
		XMVECTOR forwardVec = XMVector3Normalize( XMVectorSet(
			std::cosf( pitch ) * std::sinf( yaw ),
			std::sinf( pitch ),
			std::cosf( pitch ) * std::cosf( yaw ),
			0.f
		) );

		XMVECTOR rightVec = XMVector3Normalize( XMVector3Cross( forwardVec, worldUp ) );
		XMVECTOR upVec = XMVector3Cross( rightVec , forwardVec );

		XMStoreFloat3( &forward, forwardVec );
		XMStoreFloat3( &right, rightVec );
		XMStoreFloat3( &up, upVec );
	}
};

struct CameraInput {
	float mouseDeltaX;
	float mouseDeltaY;

	bool moveForward;   // W
	bool moveBackward;  // S
	bool moveLeft;      // A
	bool moveRight;     // D
	bool moveUp;        // Q
	bool moveDown;      // E
	bool speedModifier; // Shift
};

#endif // CAMERA_HPP
