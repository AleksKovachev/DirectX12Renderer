#include "WolfApp.h"

#include <QTimer>


WolfApp::WolfApp()
	: m_mainWin{ nullptr }, m_idleTimer( nullptr ), m_fpsTimer( nullptr ) {
}

WolfApp::~WolfApp() {
	delete m_idleTimer;
	delete m_fpsTimer;
}

bool WolfApp::init( App* appData ) {
	if ( InitWindow() == false )
		return false;

	m_renderer.SetAppData( appData );

	connect( m_mainWin->viewport, &WolfViewportWidget::onCameraPan,
		this, &WolfApp::onCameraPan
	);
	connect( m_mainWin->viewport, &WolfViewportWidget::onCameraDolly,
		this, &WolfApp::onCameraDolly
	);
	connect( m_mainWin->viewport, &WolfViewportWidget::onCameraFOV,
		this, &WolfApp::onCameraFOV
	);
	connect( m_mainWin->viewport, &WolfViewportWidget::onMouseRotationChanged,
		this, &WolfApp::onMouseRotationChanged
	);

	m_mainWin->show();

	m_renderer.PrepareForRendering( m_mainWin->viewport->GetNativeWindowHandle() );

	m_idleTimer = new QTimer( m_mainWin );
	connect( m_idleTimer, &QTimer::timeout, this, &WolfApp::OnIdleTick );
	m_idleTimer->start( 0 ); // Continuous rendering.

	m_fpsTimer = new QTimer( m_mainWin );
	connect( m_fpsTimer, &QTimer::timeout, this, &WolfApp::UpdateRenderStats );
	m_fpsTimer->start( 1'000 ); // Update FPS every second.

	connect( m_mainWin->GetRenderModeSwitch(), &QCheckBox::toggled, this,
		&WolfApp::OnRenderModeChanged );

	return true;
}

void WolfApp::OnRenderModeChanged( bool rayTracingEnabled ) {
	m_idleTimer->stop();
	m_fpsTimer->stop();

	SetRenderMode( rayTracingEnabled
		? Core::RenderMode::RayTracing : Core::RenderMode::Rasterization );

	m_idleTimer->start( 0 );
	m_fpsTimer->start( 1'000 );
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

void WolfApp::SetRenderMode( Core::RenderMode renderMode ) {
	m_renderer.SetRenderMode( renderMode );
	m_mainWin->SetRenderMode( renderMode );
}

bool WolfApp::InitWindow() {
	m_mainWin = new WolfMainWindow;
	connect( m_mainWin, &WolfMainWindow::requestQuit, this, &WolfApp::OnQuit );
	connect( m_mainWin->GetActionExit(), &QAction::triggered, this, [this]() {
		OnQuit(); QApplication::quit(); } );
	m_mainWin->SetRenderMode( m_renderer.renderMode );
	return true;
}

void WolfApp::RenderFrame() {
	m_renderer.RenderFrame( m_mainWin->viewport->cameraInput );

	++m_frameIdxAtLastFPSCalc;
}

void WolfApp::onCameraPan( float ndcX, float ndcY ) {
	m_renderer.AddToTargetOffset( ndcX, ndcY );
}

void WolfApp::onCameraDolly( float offsetZ ) {
	m_renderer.AddToOffsetZ( offsetZ );
}

void WolfApp::onCameraFOV( float offset ) {
	m_renderer.AddToOffsetFOV( offset );
}

void WolfApp::onMouseRotationChanged( float deltaAngleX, float deltaAngleY ) {
	m_renderer.AddToTargetRotation( deltaAngleX, deltaAngleY );
}
