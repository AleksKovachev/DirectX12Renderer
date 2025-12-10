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
			m_initialClickPos = event->pos();
			m_lastPos = event->pos();
		}

		// Allow propagation if parent needs events.
		QWidget::mousePressEvent( event );
	}

	void mouseMoveEvent( QMouseEvent* event ) override {
		if ( m_LMBDown ) {
			QPoint offset{ event->pos() - m_lastPos };
			m_lastPos = event->pos();

			// Convert to NDC (Normalized Device Coordinates): [-1, 1]
			float ndcX = (static_cast<float>( offset.x() ) / width()) * 2.f;
			float ndcY = -((static_cast<float>( offset.y() ) / height()) * 2.f);

			emit mouseOffsetChanged( ndcX, ndcY );
		}

		// Allow propagation if parent needs events.
		QWidget::mouseMoveEvent( event );
	}

	void mouseReleaseEvent( QMouseEvent* event ) override {
		if ( event->button() == Qt::LeftButton ) {
			m_LMBDown = false;
		}

		// Allow propagation if parent needs events.
		QWidget::mouseReleaseEvent( event );
	}
private:
	QImage m_image;
	QPoint m_initialClickPos;
	bool m_LMBDown{ false };
	QPoint m_lastPos{};

signals:
	void mouseOffsetChanged( float offsetX, float offsetY );

};

#endif // VIEWPORT_WIDGET_H
