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
	void mousePressEvent( QMouseEvent* event ) override {
		// Hide the cursor.
		setCursor( Qt::BlankCursor );
		ignoreNextMouseMove = true;

		if ( event->button() == Qt::LeftButton ) {
			// Save current mouse coordinates as last mouse position on left click.
			m_LMBDown = true;
			m_lastLMBPos = event->pos();
		}
		if ( event->button() == Qt::RightButton ) {
			// Save current mouse coordinates as last mouse position on right click.
			m_RMBDown = true;
			m_lastRMBPos = event->pos();
			m_lastRMBPosRT = event->pos();
		}
		if ( event->button() == Qt::MiddleButton ) {
			// Save current mouse coordinates as last mouse position on right click.
			m_MMBDown = true;
			m_lastMMBPos = event->pos();
		}

		// Allow propagation if parent needs events.
		QWidget::mousePressEvent( event );
	}

	void mouseMoveEvent( QMouseEvent* event ) override {
		if ( !(m_LMBDown || m_RMBDown || m_MMBDown) ) {
			// No buttons pressed, ignore.
			QWidget::mouseMoveEvent( event );
			return;
		}

		// Compute screen center.
		QPoint screenCenter{ mapToGlobal( QPoint( width() / 2, height() / 2 ) ) };

		// Mouse deltas relative to center.
		QPointF delta = mapFromGlobal(
			event->globalPosition() ) - QPointF( width() / 2.f, height() / 2.f );

		// Avoid frame jumps because of mouse position reset.
		if ( ignoreNextMouseMove ) {
			delta = QPointF( 0.f, 0.f );
			ignoreNextMouseMove = false;
		}

		// Send the delta from the last position.
		if ( m_LMBDown ) {
			if ( m_renderMode == Core::RenderMode::Rasterization ) {
				emit onCameraPan( delta.x(), -delta.y() );
			}
		}
		if ( m_RMBDown ) {
			if ( m_renderMode == Core::RenderMode::Rasterization ) {
				emit onMouseRotationChanged( delta.x(), delta.y() );
			}
			else if ( m_renderMode == Core::RenderMode::RayTracing ) {
				cameraInput.mouseDeltaX = delta.x();
				cameraInput.mouseDeltaY = -delta.y();
			}
		}
		if ( m_MMBDown ) {
			emit onCameraFOV( delta.y() );
		}

		// Reset cursor to screen center for next event.
		QCursor::setPos( screenCenter );

		// Update all "last positions" to match the center, so deltas are correct next frame.
		if ( m_LMBDown )
			m_lastLMBPos = mapFromGlobal( screenCenter );
		if ( m_RMBDown )
			m_lastRMBPos = mapFromGlobal( screenCenter );
		if ( m_MMBDown )
			m_lastMMBPos = mapFromGlobal( screenCenter );

		// Allow propagation if parent needs events.
		QWidget::mouseMoveEvent( event );
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

public: // members
	CameraInput cameraInput{};
private:
	QImage m_image;
	bool m_LMBDown{ false };
	bool m_RMBDown{ false };
	bool m_MMBDown{ false };
	QPoint m_initialRMBPosRT;
	QPoint m_lastRMBPosRT;
	QPoint m_lastLMBPos;
	QPoint m_lastRMBPos;
	QPoint m_lastMMBPos;
	Core::RenderMode m_renderMode;
	bool ignoreNextMouseMove{ false };

signals:
	void onCameraPan( float offsetX, float offsetY );
	void onCameraDolly( float offsetZ );
	void onCameraFOV( float offset );
	void onMouseRotationChanged( float deltaAngleX, float deltaAngleY );
};

#endif // VIEWPORT_WIDGET_H
