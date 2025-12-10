#include "WolfApp.h"

#include <QTimer>


WolfApp::WolfApp()
	: m_mainWin{ nullptr }, m_idleTimer( nullptr ), m_fpsTimer( nullptr ) {
}

WolfApp::~WolfApp() {
	delete m_idleTimer;
	delete m_fpsTimer;
}

bool WolfApp::init() {
	if ( InitWindow() == false )
		return false;

	connect( m_mainWin->viewport, &WolfViewportWidget::mouseOffsetChanged,
		this, &WolfApp::OnViewportOffsetChanged
	);

	m_mainWin->show();

	m_renderer.PrepareForRendering( m_mainWin->viewport->GetNativeWindowHandle() );

	m_idleTimer = new QTimer( m_mainWin );
	connect( m_idleTimer, &QTimer::timeout, this, &WolfApp::OnIdleTick );
	m_idleTimer->start( 0 ); // Continuous rendering.

	m_fpsTimer = new QTimer( m_mainWin );
	connect( m_fpsTimer, &QTimer::timeout, this, &WolfApp::UpdateRenderStats );
	m_fpsTimer->start( 1'000 ); // Update FPS every second.

	return true;
}

void WolfApp::OnQuit() {
	m_idleTimer->stop();
	m_fpsTimer->stop();
	m_renderer.StopRendering();
}

void WolfApp::OnIdleTick() {
	RenderFrame();
}

void WolfApp::UpdateRenderStats() {
	m_mainWin->SetFPS( m_frameIdxAtLastFPSCalc );
	m_frameIdxAtLastFPSCalc = 0;
}

bool WolfApp::InitWindow() {
	m_mainWin = new WolfMainWindow;
	connect( m_mainWin, &WolfMainWindow::requestQuit, this, &WolfApp::OnQuit );
	return true;
}

void WolfApp::RenderFrame() {
	m_renderer.RenderFrame();

	++m_frameIdxAtLastFPSCalc;
}

void WolfApp::OnViewportOffsetChanged( float ndcX, float ndcY ) {
	m_renderer.AddToTargetOffset( ndcX, ndcY );
}
