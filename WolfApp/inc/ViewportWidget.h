#ifndef VIEWPORT_WIDGET_H
#define VIEWPORT_WIDGET_H

#include <QImage>
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
		// setAttribute( Qt::WA_PaintOnScreen, true );
	}

	/// Change the viewport image with the given one
	void UpdateImage( const QImage& image ) {
		m_image = image;
		update(); // Trigger a repaint
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

	//! Override and disable Paint Engine when rendering with DirectX directly.
	// QPaintEngine* paintEngine() const override {
	//	return nullptr; // We're handling painting ourselves
	// }

private:
	QImage m_image;
};

#endif // VIEWPORT_WIDGET_H
