#include "WolfApp.h"

#include <QMessageBox>
#include <QTimer>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>

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
	float aspectRatio = static_cast<float>(
		m_renderer.scene.settings.renderWidth ) /
		static_cast<float>(m_renderer.scene.settings.renderHeight);
	m_mainWin->resize( m_mainWin->width(), static_cast<int>(m_mainWin->width() / aspectRatio) );
	// Enable transparency.
	m_mainWin->GetUI().overlayBox->setAttribute( Qt::WA_StyledBackground, true );
	// Make click-through.
	m_mainWin->GetUI().overlayBox->setAttribute( Qt::WA_TransparentForMouseEvents, false );


	SetInitialValues();

	connect( m_mainWin->viewport, &WolfViewportWidget::OnCameraPan,
		this, &WolfApp::OnCameraPan
	);
	connect( m_mainWin->viewport, &WolfViewportWidget::OnCameraDolly,
		this, &WolfApp::OnCameraDolly
	);
	connect( m_mainWin->viewport, &WolfViewportWidget::OnCameraFOV,
		this, &WolfApp::OnCameraFOV
	);
	connect( m_mainWin->viewport, &WolfViewportWidget::OnMouseRotationChanged,
		this, &WolfApp::OnMouseRotationChanged
	);
	connect( m_mainWin->viewport, &WolfViewportWidget::OnPositionChangedRT,
		this, &WolfApp::OnPositionChangedRT
	);
	connect( m_mainWin->GetUI().sceneFileBtn, &QPushButton::clicked, this, &WolfApp::OpenSceneBtnClicked );
	connect( m_mainWin->GetUI().loadSceneBtn, &QPushButton::clicked, this, &WolfApp::LoadSceneClicked );
	connect( m_mainWin->GetUI().moveSpeedSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MoveSpeedChangedSpin
	);
	connect( m_mainWin->GetUI().moveSpeedSlider, &QSlider::valueChanged,
		this, &WolfApp::MoveSpeedChangedSlider
	);
	connect( m_mainWin->GetUI().moveSpeedMultSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MoveSpeedMultChanged
	);
	connect( m_mainWin->GetUI().mouseSensitivityRTSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MouseSensitivityRTChanged
	);
	connect( m_mainWin->GetUI().FOVRTSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::VerticalFoVRTChanged
	);
	connect( m_mainWin->GetUI().camPosXSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( m_mainWin->GetUI().camPosYSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( m_mainWin->GetUI().camPosZSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( &m_mainWin->viewport->inputUpdateTimer, &QTimer::timeout,
		this, &WolfApp::OnPositionChangedRT );

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

	std::string scenePath{ m_renderer.scene.GetRenderScenePath() };
	QString fileAbsPath{ QDir::toNativeSeparators( QDir::cleanPath(
		QDir().absoluteFilePath( QString::fromStdString( scenePath ) ) ) ) };

	m_mainWin->GetUI().sceneFileEntry->setText( QDir::toNativeSeparators( fileAbsPath ) );

	return true;
}

void WolfApp::OnRenderModeChanged( bool rayTracingOn ) {
	m_idleTimer->stop();
	m_fpsTimer->stop();

	Core::RenderMode renderMode = rayTracingOn ?
		Core::RenderMode::RayTracing : Core::RenderMode::Rasterization;

	ToggleWidgetVisibility( rayTracingOn );
	SetRenderMode( renderMode );

	m_idleTimer->start( 0 );
	m_fpsTimer->start( 1'000 );
}

void WolfApp::ToggleWidgetVisibility( bool rayTracingOn ) {
	m_mainWin->GetUI().FOVRTSpin->setVisible( rayTracingOn );
	m_mainWin->GetUI().cameraPosBox->setVisible( rayTracingOn );
	m_mainWin->GetUI().moveSpeedLbl->setVisible( rayTracingOn );
	m_mainWin->GetUI().moveSpeedControls->setVisible( rayTracingOn );
	m_mainWin->GetUI().moveSpeedMultLbl->setVisible( rayTracingOn );
	m_mainWin->GetUI().moveSpeedMultSpin->setVisible( rayTracingOn );
	m_mainWin->GetUI().mouseSensitivityRTLbl->setVisible( rayTracingOn );
	m_mainWin->GetUI().mouseSensitivityRTSpin->setVisible( rayTracingOn );
	m_mainWin->GetUI().verticalFOVLbl->setVisible( rayTracingOn );
	m_mainWin->GetUI().FOVRTSpin->setVisible( rayTracingOn );
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

void WolfApp::SetInitialValues() {
	MoveSpeedChangedSpin();
	MoveSpeedMultChanged();
	MouseSensitivityRTChanged();
	VerticalFoVRTChanged();
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

void WolfApp::OnCameraPan( float ndcX, float ndcY ) {
	m_renderer.AddToTargetOffset( ndcX, ndcY );
}

void WolfApp::OnCameraDolly( float offsetZ ) {
	m_renderer.AddToOffsetZ( offsetZ );
}

void WolfApp::OnCameraFOV( float offset ) {
	m_renderer.AddToOffsetFOV( offset );
}

void WolfApp::OnMouseRotationChanged( float deltaAngleX, float deltaAngleY ) {
	m_renderer.AddToTargetRotation( deltaAngleX, deltaAngleY );
}

void WolfApp::OnPositionChangedRT() {
	QSignalBlocker blockX( m_mainWin->GetUI().camPosXSpin );
	QSignalBlocker blockY( m_mainWin->GetUI().camPosYSpin );
	QSignalBlocker blockZ( m_mainWin->GetUI().camPosZSpin );

	m_mainWin->GetUI().camPosXSpin->setValue( m_renderer.cameraRT.position.x );
	m_mainWin->GetUI().camPosYSpin->setValue( m_renderer.cameraRT.position.y );
	m_mainWin->GetUI().camPosZSpin->setValue( -m_renderer.cameraRT.position.z );
}

void WolfApp::OpenSceneBtnClicked() {
	// Open directory selection dialog.
	QDir executableParentDir{ QFileInfo( QCoreApplication::applicationDirPath() ).dir() };
	QString rscDirPath{ executableParentDir.absoluteFilePath( "rsc" ) };

	QString file = QFileDialog::getOpenFileName(
		m_mainWin,
		tr( "Select Directory" ),
		rscDirPath,
		QString( "CRTScene (*.crtscene)" )
	);

	// If user didn't cancel
	if ( !file.isEmpty() )
		m_mainWin->GetUI().sceneFileEntry->setText(QDir::toNativeSeparators(file));
}

void WolfApp::LoadSceneClicked() {
	QString scenePath{ QDir::cleanPath( m_mainWin->GetUI().sceneFileEntry->text().trimmed() ) };
	QFileInfo fileInfo( scenePath );

	if ( !fileInfo.exists() || !fileInfo.isFile() ) {
		QMessageBox::critical(
			m_mainWin,
			"File Not Found",
			QString( "The specified scene file was not found.\nPlease check and try again!" ),
			QMessageBox::Ok
		);
		return;
	}

	std::string scenePathStd{ scenePath.toStdString() };
	m_idleTimer->stop();
	m_fpsTimer->stop();

	m_renderer.ReloadScene( scenePathStd, m_mainWin->viewport->GetNativeWindowHandle());

	m_idleTimer->start( 0 );
	m_fpsTimer->start( 1'000 );
}

void WolfApp::MoveSpeedChangedSpin() {
	m_renderer.cameraRT.movementSpeed =
		m_mainWin->GetUI().moveSpeedSpin->value();

	// Block slider signals so it doesn't call MoveSpeedChangedSlider() and
	// round the value. Unblocks in destructor.
	QSignalBlocker blockSlider( m_mainWin->GetUI().moveSpeedSlider );
	m_mainWin->GetUI().moveSpeedSlider->setValue(
		std::round( m_renderer.cameraRT.movementSpeed ) );
}

void WolfApp::MoveSpeedChangedSlider() {
	m_renderer.cameraRT.movementSpeed =
		m_mainWin->GetUI().moveSpeedSlider->value();
	m_mainWin->GetUI().moveSpeedSpin->setValue(
		m_renderer.cameraRT.movementSpeed );
}

void WolfApp::MoveSpeedMultChanged() {
	m_renderer.cameraRT.speedMult =
		m_mainWin->GetUI().moveSpeedMultSpin->value();
}

void WolfApp::MouseSensitivityRTChanged() {
	m_renderer.cameraRT.mouseSensitivity =
		m_mainWin->GetUI().mouseSensitivityRTSpin->value();
}

void WolfApp::VerticalFoVRTChanged() {
	m_renderer.cameraRT.setVerticalFOVDeg(
		m_mainWin->GetUI().FOVRTSpin->value() );
}

void WolfApp::CameraPositionChangedRT() {
	m_renderer.cameraRT.position.x =
		m_mainWin->GetUI().camPosXSpin->value();
	m_renderer.cameraRT.position.y =
		m_mainWin->GetUI().camPosYSpin->value();
	// Flipping the axis to make the UI more intuitive.
	m_renderer.cameraRT.position.z =
		-m_mainWin->GetUI().camPosZSpin->value();
}
