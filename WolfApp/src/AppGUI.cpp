#include "AppGUI.h"

#include <QCloseEvent>

WolfMainWindow::WolfMainWindow( QWidget* parent )
	: QMainWindow( parent ) {
	ui.setupUi( this );

	statusBar()->addPermanentWidget( ui.fpsLbl );
	statusBar()->addPermanentWidget( ui.fpsVal );
	statusBar()->setStyleSheet( "QStatusBar::item { border: none; }" );
}

WolfMainWindow::~WolfMainWindow() {}

void WolfMainWindow::SetFPS( const int fps ) {
	ui.fpsVal->setText( QString::number( fps ) );
}

void WolfMainWindow::UpdateViewport( const QImage& image ) {
	ui.viewport->UpdateImage( image );
}

void WolfMainWindow::closeEvent( QCloseEvent* event ) {
	emit requestQuit();
	event->accept();
}
