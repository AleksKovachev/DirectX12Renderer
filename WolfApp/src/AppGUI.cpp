#include "AppGUI.h"

#include <QCloseEvent>

WolfMainWindow::WolfMainWindow( QWidget* parent )
	: QMainWindow( parent ) {
	ui.setupUi( this );
	viewport = ui.viewport;

	// Attach the FPS widgets to the status bar.
	statusBar()->addPermanentWidget( ui.fpsLbl );
	statusBar()->addPermanentWidget( ui.fpsVal );
	statusBar()->setStyleSheet( "QStatusBar::item { border: none; }" );

	// Style the render mode switch to use the system accent color when checked.
	// This won't update on system theme changes while the app is running.
	QColor accentColor = QGuiApplication::palette().color( QPalette::Highlight );
	QString switchStyle = QString(
		"QCheckBox:checked { background-color: %1; }").arg( accentColor.name() );
	ui.renderModeSwitch->setStyleSheet( ui.renderModeSwitch->styleSheet() + switchStyle );

	// Connect the menu action to the render mode switch.
	connect( ui.actionToggleRenderMode, &QAction::triggered, [this]() {
		ui.renderModeSwitch->toggle();
	} );
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

QCheckBox* WolfMainWindow::GetRenderModeSwitch() const {
	return ui.renderModeSwitch;
}

WolfViewportWidget* WolfMainWindow::GetViewport() const {
	return ui.viewport;
}

void WolfMainWindow::SetRenderMode( Core::RenderMode renderMode ) {
	ui.viewport->SetRenderMode( renderMode );
}

QAction* WolfMainWindow::GetActionExit() const {
	return ui.actionExit;
}
