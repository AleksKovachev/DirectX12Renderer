#include "AppGUI.h"

#include <QCloseEvent>


WolfMainWindow::WolfMainWindow( QWidget* parent )
	: QMainWindow( parent ) {
	m_ui.setupUi( this );
	viewport = m_ui.viewport;

	// Attach the FPS widgets to the status bar.
	statusBar()->addPermanentWidget( m_ui.fpsLbl );
	statusBar()->addPermanentWidget( m_ui.fpsVal );
	statusBar()->setStyleSheet( "QStatusBar::item { border: none; }" );

	// Style the render mode switch to use the system accent color when checked.
	// This won't update on system theme changes while the app is running.
	QColor accentColor = QGuiApplication::palette().color( QPalette::Highlight );
	QString switchStyle = QString(
		"QCheckBox:checked { background-color: %1; }").arg( accentColor.name() );
	m_ui.renderModeSwitch->setStyleSheet( m_ui.renderModeSwitch->styleSheet() + switchStyle );

	// Connect the menu action to the render mode switch.
	connect( m_ui.actionToggleRenderMode, &QAction::triggered, [this]() {
		m_ui.renderModeSwitch->toggle();
	} );
}

WolfMainWindow::~WolfMainWindow() {}

void WolfMainWindow::SetFPS( const int fps ) {
	m_ui.fpsVal->setText( QString::number( fps ) );
}

void WolfMainWindow::UpdateViewport( const QImage& image ) {
	viewport->UpdateImage( image );
}

void WolfMainWindow::closeEvent( QCloseEvent* event ) {
	emit requestQuit();
	event->accept();
}

QCheckBox* WolfMainWindow::GetRenderModeSwitch() const {
	return m_ui.renderModeSwitch;
}

WolfViewportWidget* WolfMainWindow::GetViewport() const {
	return viewport;
}

void WolfMainWindow::SetRenderMode( Core::RenderMode renderMode ) {
	viewport->SetRenderMode( renderMode );
}

QAction* WolfMainWindow::GetActionExit() const {
	return m_ui.actionExit;
}

const Ui::AppGUI& WolfMainWindow::GetUI() {
	return m_ui;
}
