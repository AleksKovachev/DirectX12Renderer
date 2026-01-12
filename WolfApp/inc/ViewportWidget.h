#ifndef VIEWPORT_WIDGET_H
#define VIEWPORT_WIDGET_H

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

		// Allow propagation if parent needs events.
		QWidget::mousePressEvent( event );
	}

	void mouseMoveEvent( QMouseEvent* event ) override {
		if ( m_LMBDown ) {
			// Calculate the offset of current mouse position from last one saved.
			QPoint offset{ event->pos() - m_lastLMBPos };
			m_lastLMBPos = event->pos();

			// Convert to NDC (Normalized Device Coordinates): [-1, 1].
			// Don't emit in RT mode as the data is only sent to Rasterization.
			if ( m_renderMode == Core::RenderMode::Rasterization )
				emit onCameraPan(
					static_cast<float>(offset.x()) / width() * 2.f,
					-(static_cast<float>(offset.y()) / height() * 2.f)
				);
		}
		if ( m_RMBDown ) {
			// Calculate the offset of current mouse position from last one saved.
			QPoint delta{ event->pos() - m_lastRMBPos };
			m_lastRMBPos = event->pos();

			// Horizontal mouse movement controls Z rotation (Z = from-towards screen).
			float deltaAngle{ static_cast<float>(delta.x()) * 0.01f };

			emit onMouseRotationChanged( deltaAngle );
		}

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

		// Allow propagation if parent needs events.
		QWidget::mouseReleaseEvent( event );
	}

private:
	QImage m_image;
	bool m_LMBDown{ false };
	bool m_RMBDown{ false };
	QPoint m_initialLMBPos;
	QPoint m_lastLMBPos;
	QPoint m_lastRMBPos;
	Core::RenderMode m_renderMode;

signals:
	void onCameraPan( float offsetX, float offsetY );
	void onMouseRotationChanged( float deltaAngle );
};

#endif // VIEWPORT_WIDGET_H
