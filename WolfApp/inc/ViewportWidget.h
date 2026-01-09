#ifndef VIEWPORT_WIDGET_H
#define VIEWPORT_WIDGET_H

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

		// Reduce unnecessary painting in the background
		setAttribute( Qt::WA_NoSystemBackground );

		// Use native window handle. HWND could be passed to DirectX for direct rendering.
		setAttribute( Qt::WA_NativeWindow );
		// HWND hwnd = (HWND)winId(); Get the actual handle.

		// Disable system for automatic background filling.
		setAutoFillBackground( false );

		// Bypass Qt's paint system if using DirectX interop later
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

protected:
	void paintEvent( QPaintEvent* event ) override {
		QPainter painter( this );

		// Faster scaling
		painter.setRenderHint( QPainter::SmoothPixmapTransform, false );

		// If no image, fill with a solid red color to indicate an issue.
		if ( m_image.isNull() ) {
			painter.fillRect( rect(), Qt::red );
			return;
		}

		painter.drawImage( rect(), m_image );
	}

	void mousePressEvent( QMouseEvent* event ) override {
		if ( event->button() == Qt::LeftButton ) {
			m_LMBDown = true;
			m_initialPos = event->pos();
		}

		// Allow propagation if parent needs events.
		QWidget::mousePressEvent( event );
	}

	void mouseMoveEvent( QMouseEvent* event ) override {
		if ( m_LMBDown ) {
			// Calculate the offset of current mouse position from last one saved.
			QPoint offset{ event->pos() - m_initialPos };

			// Convert to NDC (Normalized Device Coordinates): [-1, 1]
			emit onCameraPan(
				static_cast<float>(offset.x() + m_lastPos.x()) / width() * 2.f,
				static_cast<float>(offset.y() + m_lastPos.y()) / height() * 2.f
			);
		}

		// Allow propagation if parent needs events.
		QWidget::mouseMoveEvent( event );
	}

	void mouseReleaseEvent( QMouseEvent* event ) override {
		if ( event->button() == Qt::LeftButton ) {
			m_LMBDown = false;
			m_lastPos = m_lastPos + event->pos() - m_initialPos;
		}

		// Allow propagation if parent needs events.
		QWidget::mouseReleaseEvent( event );
	}

private:
	QImage m_image;
	QPoint m_initialPos;
	QPoint m_lastPos;
	bool m_LMBDown{ false };

signals:
	void onCameraPan( float offsetX, float offsetY );
};

#endif // VIEWPORT_WIDGET_H
