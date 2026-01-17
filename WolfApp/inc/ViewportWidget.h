#ifndef VIEWPORT_WIDGET_H
#define VIEWPORT_WIDGET_H

#include "Camera.hpp" // CameraInput
#include "Renderer.hpp"

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>


class WolfViewportWidget : public QWidget {
	Q_OBJECT

public:
	explicit WolfViewportWidget( QWidget* parent = nullptr ) :QWidget( parent ) {
		// Assure no alpha will be used.
		setAttribute( Qt::WA_OpaquePaintEvent );

		// Reduce unnecessary painting in the background.
		setAttribute( Qt::WA_NoSystemBackground );

		// Use native window handle. HWND could be passed to DirectX for direct rendering.
		setAttribute( Qt::WA_NativeWindow );

		// Disable system for automatic background filling.
		setAutoFillBackground( false );

		// Bypass Qt's paint system if using DirectX interop.
		setAttribute( Qt::WA_PaintOnScreen, true );

		// Sets up the mouse as device to watch events for using Windwos API.
		registerRawMouseInput();
	}

	/// Change the viewport image with the given one
	void UpdateImage( const QImage& image ) {
		m_image = image;
		update(); // Trigger a repaint
	}

	/// Returns the native window handle that represents the underlying window for this widget.
	HWND GetNativeWindowHandle() {
		return reinterpret_cast<HWND>( winId() );
	}

	void SetRenderMode( Core::RenderMode renderMode ) {
		m_renderMode = renderMode;
	}

protected:
	void registerRawMouseInput() {
		RAWINPUTDEVICE device{};
		// usUsagePage identifies a category of HID( Human Interface Device )
		device.usUsagePage = 0x01; // Generic desktop controls (mouse, keyboard, joystick).
		device.usUsage = 0x02; // Exact device within the usage page: mouse.
		// Controls HOW and WHEN raw input is delivered.
		// RIDEV_INPUTSINK: Raw input is sent even when the window is not focused.
		device.dwFlags = 0; // 0: Raw input is sent ONLY when the window has focus.
		// Specifies WHICH window should receive WM_INPUT messages.
		device.hwndTarget = GetNativeWindowHandle();

		RegisterRawInputDevices( &device, 1, sizeof( device ) );
	}

	void mousePressEvent( QMouseEvent* event ) override {
		// Hide the cursor.
		setCursor( Qt::BlankCursor );
		SetCapture( GetNativeWindowHandle() );

		if ( event->button() == Qt::LeftButton ) {
			// Save current mouse coordinates as last mouse position on left click.
			m_LMBDown = true;
			m_lastLMBPos = event->pos();
		}
		if ( event->button() == Qt::RightButton ) {
			// Save current mouse coordinates as last mouse position on right click.
			m_RMBDown = true;
			m_lastRMBPos = event->pos();
		}
		if ( event->button() == Qt::MiddleButton ) {
			// Save current mouse coordinates as last mouse position on right click.
			m_MMBDown = true;
			m_lastMMBPos = event->pos();
		}

		// Allow propagation if parent needs events.
		QWidget::mousePressEvent( event );
	}

	void mouseReleaseEvent( QMouseEvent* event ) override {
		if ( event->button() == Qt::LeftButton ) {
			m_LMBDown = false;
		}
		if ( event->button() == Qt::RightButton ) {
			m_RMBDown = false;
		}
		if ( event->button() == Qt::MiddleButton ) {
			m_MMBDown = false;
		}

		if ( !(m_LMBDown && m_RMBDown && m_MMBDown)) {
			// Show the cursor.
			ReleaseCapture();
			unsetCursor();
		}

		// Allow propagation if parent needs events.
		QWidget::mouseReleaseEvent( event );
	}

	void wheelEvent( QWheelEvent* event ) override {
		// event->pixelDelta(); // Used for high-precision touchpads
		// 120 is a usual value for a single scroll. This is done by
		// Qt, Windows, etc. to allow for high-precision scrolling without
		// using floats. Dividing the number by 8 converts it back to degrees.
		// 15 degree interval is what most mouse wheels use. Dividing the
		// angleDelta by 120 gives +/- 1. Flipping "zoom" direction.
		if ( m_renderMode == Core::RenderMode::Rasterization ) {
			int scrollUpDownVal{ (event->angleDelta() / 120).y() };
			emit onCameraDolly( -static_cast<float>(scrollUpDownVal) );
		}

		// Allow propagation if parent needs events.
		QWidget::wheelEvent( event );
	}

	void keyPressEvent( QKeyEvent* event ) override {
		if ( m_renderMode == Core::RenderMode::RayTracing && m_RMBDown ) {
			switch ( event->key() ) {
				case Qt::Key_W: cameraInput.moveForward = true; break;
				case Qt::Key_S: cameraInput.moveBackward = true; break;
				case Qt::Key_A: cameraInput.moveLeft = true; break;
				case Qt::Key_D: cameraInput.moveRight = true; break;
				case Qt::Key_E: cameraInput.moveUp = true; break;
				case Qt::Key_Q: cameraInput.moveDown = true; break;
				case Qt::Key_Shift: cameraInput.speedModifier = true; break;
				default: QWidget::keyPressEvent( event );
				return; // propagate
			}
		}

		// Allow propagation if parent needs events.
		QWidget::keyPressEvent( event );
	}

	void keyReleaseEvent( QKeyEvent* event ) override {
		if ( m_renderMode == Core::RenderMode::RayTracing ) {
			switch ( event->key() ) {
				case Qt::Key_W: cameraInput.moveForward = false; break;
				case Qt::Key_S: cameraInput.moveBackward = false; break;
				case Qt::Key_A: cameraInput.moveLeft = false; break;
				case Qt::Key_D: cameraInput.moveRight = false; break;
				case Qt::Key_E: cameraInput.moveUp = false; break;
				case Qt::Key_Q: cameraInput.moveDown = false; break;
				case Qt::Key_Shift: cameraInput.speedModifier = false; break;
				default: QWidget::keyPressEvent( event ); return; // propagate
			}
		}

		// Allow propagation if parent needs events.
		QWidget::keyPressEvent( event );
	}

	/// Called when capturing RAW Windows events. Using this instead of Qt's
	/// mouseMoveEvent to avoid calculations and issues when window is resized.
	bool nativeEvent( const QByteArray& eventType, void* message, qintptr* result ) override {
		if ( eventType != "windows_generic_MSG" )
			return false;

		MSG* msg = static_cast<MSG*>(message);

		// WM_INPUT is sent by Windows when a raw input device reports data.
		if ( msg->message == WM_INPUT ) {
			UINT size{};
			// First call tells us how large the RAWINPUT buffer must be.
			GetRawInputData(
				reinterpret_cast<HRAWINPUT>(msg->lParam),
				RID_INPUT,
				nullptr,
				&size,
				sizeof( RAWINPUTHEADER )
			);

			std::vector<BYTE> buffer( size );

			// Second call actually retrieves the raw input data.
			GetRawInputData(
				reinterpret_cast<HRAWINPUT>(msg->lParam),
				RID_INPUT,
				buffer.data(),
				&size,
				sizeof( RAWINPUTHEADER )
			);

			RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer.data());

			// Ignore if not mouse input.
			if ( raw->header.dwType == RIM_TYPEMOUSE ) {

				// These are relative deltas since last event, independent of
				// cursor position, DPI scaling, or window size.
				LONG dx = raw->data.mouse.lLastX;
				LONG dy = raw->data.mouse.lLastY;

				if ( m_renderMode == Core::RenderMode::RayTracing && m_RMBDown ) {
					cameraInput.mouseDeltaX -= dx;
					cameraInput.mouseDeltaY -= dy;
				} else if ( m_renderMode == Core::RenderMode::Rasterization ) {
					if ( m_LMBDown ) {
						emit onCameraPan( static_cast<float>(dx), static_cast<float>(-dy) );
					}
					if ( m_RMBDown ) {
						emit onMouseRotationChanged( static_cast<float>(dx), static_cast<float>(dy) );
					}
					if ( m_MMBDown ) {
						emit onCameraFOV( static_cast<float>(dy) );
					}
				}
			}
		}

		return QWidget::nativeEvent( eventType, message, result );
	}


public: // members
	CameraInput cameraInput{};
private:
	QImage m_image;
	bool m_LMBDown{ false };
	bool m_RMBDown{ false };
	bool m_MMBDown{ false };
	QPoint m_lastLMBPos;
	QPoint m_lastRMBPos;
	QPoint m_lastMMBPos;
	Core::RenderMode m_renderMode;

signals:
	void onCameraPan( float offsetX, float offsetY );
	void onCameraDolly( float offsetZ );
	void onCameraFOV( float offset );
	void onMouseRotationChanged( float deltaAngleX, float deltaAngleY );
};

#endif // VIEWPORT_WIDGET_H
