#ifndef CAMERA_HPP
#define CAMERA_HPP

#include "d3d12.h"

#include <algorithm> // clamp
#include <cmath> // cosf, sinf
#include <DirectXMath.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace RT {
	struct alignas(16) CameraCB {
		DirectX::XMFLOAT3 cameraPosition;
		float verticalFOV;

		// For the rest, XMFLOAT4 would add an unused w component and remove the _pad.

		DirectX::XMFLOAT3 cameraForward;
		float aspectRatio;

		DirectX::XMFLOAT3 cameraRight;
		int32_t forwardMult;

		DirectX::XMFLOAT3 cameraUp;
		float _pad0;
	};

	struct Camera {
		static constexpr DirectX::XMVECTOR worldUp{ 0.f, 1.f, 0.f };
		static constexpr float maxPitch{ DirectX::XMConvertToRadians( 89.f ) };

		// Position & orientation.
		DirectX::XMFLOAT3 position{ 0.f, 0.f, 35.f }; ///< World space.
		float yaw{ DirectX::XM_PI }; ///< Rotation around world up (Y) in radians.
		float pitch{}; ///< Rotation around local X in radians.

		// Movement.
		float movementSpeed{ 10.f }; ///< Units per second.
		float speedMult{ 3.5f }; ///< Multiplier when speed modifier is active.
		float mouseSensMultiplier{ 0.0005f }; ///< Radians per pixel.

		// Projection.
		float verticalFOV{ DirectX::XMConvertToRadians( 60.f ) }; ///< Radians.
		float aspectRatio{ 1.f };

		// Cached basis vectors.
		DirectX::XMFLOAT3 forward{ 0.f, 0.f, -1.f };
		DirectX::XMFLOAT3 right{ 1.f, 0.f, 0.f };
		DirectX::XMFLOAT3 up{ 0.f, 1.f, 0.f };

		// Constant buffer.
		ComPtr<ID3D12Resource> cb{ nullptr };
		CameraCB cbData{}; ///< Camera constant buffer data for RT mode.
		UINT8* cbMappedPtr{ nullptr };

		/// Sets pitch value, clamping it to avoid gimbal lock.
		void setPitch( float value ) {
			pitch = std::clamp( value, -maxPitch, maxPitch );
		}

		/// Sets vertical field of view in degrees.
		void setVerticalFOVDeg( float value ) {
			verticalFOV = DirectX::XMConvertToRadians( value );
		}

		void ComputeBasisVectors( float zDir ) {
			using namespace DirectX;
			XMVECTOR forwardVec = XMVector3Normalize( XMVectorSet(
				std::cosf( pitch ) * std::sinf( yaw ),
				std::sinf( pitch ),
				zDir * std::cosf( pitch ) * std::cosf( yaw ),
				0.f
			) );

			XMVECTOR rightVec = XMVector3Normalize( XMVector3Cross( forwardVec, worldUp ) );
			XMVECTOR upVec = XMVector3Cross( rightVec, forwardVec );

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
}

namespace Raster {
	enum class CameraCoordinateSystem {
		Local,
		World
	};

	/// Camera-related data for controlling renderer from the GUI.
	struct Camera {
		/// The transform matrix used in the constant buffer to update object position.
		ComPtr<ID3D12Resource> cameraCBRes{ nullptr };
		UINT8* cameraCBMappedPtr{ nullptr };

		/// Shadow map constant buffer for world coordinates.
		ComPtr<ID3D12Resource> shadowCBRes{ nullptr };
		UINT8* shadowCBMappedPtr{ nullptr };

		// Members related to geometry transform with mouse movemet.
		float currOffsetX{}; ///< Camera offset on X axis interpolated to reach targetOffsetX.
		float currOffsetY{}; ///< Camera offset on Y axis interpolated to reach targetOffsetY.
		float targetOffsetX{}; ///< The final destination for the camera offset on the X axis.
		float targetOffsetY{}; ///< The final destination for the camera offset on the Y axis.
		float offsetZ{ 35.f }; ///< Camera offset on the Z axis.

		float dummyObjectRadius{ 0.5f }; ///< Used for offset clamping to viewport bounds.
		float boundsX{}; ///< The current X axis screen boundary the geometry isn't allowed to leave.
		float boundsY{}; ///< The current Y axis screen boundary the geometry isn't allowed to leave.

		float rotSensMultiplier{ 5.0f }; ///< Rotation sensitivity factor.
		float offsetZSens{ 0.5f }; ///< Sensitivity factor for offset on Z axis.
		float FOVSens{ 0.1f }; ///< Sensitivity factor for controlling the FOV.
		float offsetXYSens{ 0.01f }; ///< Sensitivity factor for offset on XY axes.

		float currRotationX{}; ///< Camera rotation in radians on X axis interpolated to reach targetRotationX.
		float currRotationY{}; ///< Camera rotation in radians on Y axis interpolated to reach targetRotationY.
		float targetRotationX{}; ///< The final destination for the camera rotation on the X axis (in radians).
		float targetRotationY{}; ///< The final destination for the camera rotation on the Y axis (in radians).

		// Motion speed and sensitivity.
		float smoothOffsetLerp{ 2.f }; ///< Interpolation factor for camera offset (curr->target).
		float smoothRotationLambda{ 6.f };  ///< Interpolation factor for camera rotation (curr->target)

		float FOVAngle{ DirectX::XMConvertToRadians( 60.f ) }; ///< Vertical Field of View angle (in radians).
		float aspectRatio{ 1.f }; ///< Calculate with render width/height.
		float nearZ{ 0.1f }; ///< Camera near clipping plane.
		float farZ{ 1000.f }; ///< Camera far clipping plane.

		CameraCoordinateSystem coordinateSystem{ CameraCoordinateSystem::World };

		struct alignas(16) CameraDataCB {
			DirectX::XMFLOAT4X4 world;
			DirectX::XMFLOAT4X4 view;
			DirectX::XMFLOAT4X4 projection;
		} cbData;

		struct alignas(16) ShadowMapCamCB {
			DirectX::XMFLOAT4X4 world;
		} cbShadow;

		void SetFOVDeg( float degrees ) {
			FOVAngle = DirectX::XMConvertToRadians( degrees );
		}
	};

	struct alignas(16) ScreenDataCB {
		DirectX::XMFLOAT2 viewportSize;
		float vertSize;
		float _pad0;
	};
}

#endif // CAMERA_HPP
