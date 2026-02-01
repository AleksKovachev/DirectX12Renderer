#include "AppGUI.h"

#include <QCloseEvent>


WolfMainWindow::WolfMainWindow( QWidget* parent )
	: QMainWindow( parent ) {
	m_ui.setupUi( this );

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
	m_ui.randomColorsSwitchRT->setStyleSheet( m_ui.randomColorsSwitchRT->styleSheet() + switchStyle );
	m_ui.showBackfacesSwitchR->setStyleSheet( m_ui.showBackfacesSwitchR->styleSheet() + switchStyle );
	m_ui.renderEdgesSwitchR->setStyleSheet( m_ui.renderEdgesSwitchR->styleSheet() + switchStyle );
	m_ui.randomColorsSwitchR->setStyleSheet( m_ui.randomColorsSwitchR->styleSheet() + switchStyle );
	m_ui.discoModeSwitchR->setStyleSheet( m_ui.discoModeSwitchR->styleSheet() + switchStyle );
	m_ui.renderFacesSwitchR->setStyleSheet( m_ui.renderFacesSwitchR->styleSheet() + switchStyle );
	m_ui.renderVertsSwitchR->setStyleSheet( m_ui.renderVertsSwitchR->styleSheet() + switchStyle );
	m_ui.matchRTCamSwitch->setStyleSheet( m_ui.matchRTCamSwitch->styleSheet() + switchStyle );
	m_ui.shadowOverlaySwitchR->setStyleSheet( m_ui.shadowOverlaySwitchR->styleSheet() + switchStyle );
	m_ui.checkerTextureSwitchR->setStyleSheet( m_ui.checkerTextureSwitchR->styleSheet() + switchStyle );
	m_ui.gridTextureSwitchR->setStyleSheet( m_ui.gridTextureSwitchR->styleSheet() + switchStyle );

	// Connect the menu action to the render mode switch.
	connect( m_ui.actionToggleRenderMode, &QAction::triggered, [this]() {
		m_ui.renderModeSwitch->toggle();
	} );
}

WolfMainWindow::~WolfMainWindow() {}

void WolfMainWindow::closeEvent( QCloseEvent* event ) {
	emit requestQuit();
	event->accept();
}

const Ui::AppGUI& WolfMainWindow::GetUI() const {
	return m_ui;
}
