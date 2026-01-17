#include "WolfApp.h"

#include <QMessageBox>
#include <QTimer>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>

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
	float aspectRatio = static_cast<float>( m_renderer.scene.settings.renderWidth ) /
		static_cast<float>(m_renderer.scene.settings.renderHeight);
	// Make the window 80% of the screen size and then correct the aspect ratio.
	m_mainWin->resize( m_mainWin->screen()->size() * 0.8 );
	m_mainWin->resize( m_mainWin->width(), static_cast<int>(m_mainWin->width() / aspectRatio) );

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
	connect( m_ui->sceneFileBtn, &QPushButton::clicked, this, &WolfApp::OpenSceneBtnClicked );
	connect( m_ui->loadSceneBtn, &QPushButton::clicked, this, &WolfApp::LoadSceneClicked );
	connect( m_ui->moveSpeedSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MoveSpeedChangedSpin
	);
	connect( m_ui->moveSpeedSlider, &QSlider::valueChanged,
		this, &WolfApp::MoveSpeedChangedSlider
	);
	connect( m_ui->moveSpeedMultSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MoveSpeedMultChanged
	);
	connect( m_ui->mouseSensitivityRTSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MouseSensitivityRTChanged
	);
	connect( m_ui->FOVRTSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::VerticalFoVRTChanged
	);
	connect( m_ui->camPosXSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( m_ui->camPosYSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( m_ui->camPosZSpin, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( &m_mainWin->viewport->inputUpdateTimer, &QTimer::timeout,
		this, &WolfApp::OnPositionChangedRT
	);
	connect( m_ui->viewport, &WolfViewportWidget::OnChangeSpeedMult,
		this, &WolfApp::OnChangeSpeedMult
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

	std::string scenePath{ m_renderer.scene.GetRenderScenePath() };
	QString fileAbsPath{ QDir::toNativeSeparators( QDir::cleanPath(
		QDir().absoluteFilePath( QString::fromStdString( scenePath ) ) ) ) };

	m_ui->sceneFileEntry->setText( QDir::toNativeSeparators( fileAbsPath ) );

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
	m_ui->FOVRTSpin->setVisible( rayTracingOn );
	m_ui->cameraPosBox->setVisible( rayTracingOn );
	m_ui->moveSpeedLbl->setVisible( rayTracingOn );
	m_ui->moveSpeedControls->setVisible( rayTracingOn );
	m_ui->moveSpeedMultLbl->setVisible( rayTracingOn );
	m_ui->moveSpeedMultSpin->setVisible( rayTracingOn );
	m_ui->mouseSensitivityRTLbl->setVisible( rayTracingOn );
	m_ui->mouseSensitivityRTSpin->setVisible( rayTracingOn );
	m_ui->verticalFOVLbl->setVisible( rayTracingOn );
	m_ui->FOVRTSpin->setVisible( rayTracingOn );
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
	m_ui = &m_mainWin->GetUI();
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
	QSignalBlocker blockX( m_ui->camPosXSpin );
	QSignalBlocker blockY( m_ui->camPosYSpin );
	QSignalBlocker blockZ( m_ui->camPosZSpin );

	m_ui->camPosXSpin->setValue( m_renderer.cameraRT.position.x );
	m_ui->camPosYSpin->setValue( m_renderer.cameraRT.position.y );
	m_ui->camPosZSpin->setValue( -m_renderer.cameraRT.position.z );
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
		m_ui->sceneFileEntry->setText(QDir::toNativeSeparators(file));
}

void WolfApp::LoadSceneClicked() {
	QString scenePath{ QDir::cleanPath( m_ui->sceneFileEntry->text().trimmed() ) };
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
		m_ui->moveSpeedSpin->value();

	// Block slider signals so it doesn't call MoveSpeedChangedSlider() and
	// round the value. Unblocks in destructor.
	QSignalBlocker blockSlider( m_ui->moveSpeedSlider );
	m_ui->moveSpeedSlider->setValue( std::round( m_renderer.cameraRT.movementSpeed ) );
}

void WolfApp::MoveSpeedChangedSlider() {
	m_renderer.cameraRT.movementSpeed = m_ui->moveSpeedSlider->value();
	m_ui->moveSpeedSpin->setValue( m_renderer.cameraRT.movementSpeed );
}

void WolfApp::MoveSpeedMultChanged() {
	m_renderer.cameraRT.speedMult = m_ui->moveSpeedMultSpin->value();
}

void WolfApp::MouseSensitivityRTChanged() {
	m_renderer.cameraRT.mouseSensitivity = m_ui->mouseSensitivityRTSpin->value();
}

void WolfApp::VerticalFoVRTChanged() {
	m_renderer.cameraRT.setVerticalFOVDeg( m_ui->FOVRTSpin->value() );
}

void WolfApp::CameraPositionChangedRT() {
	m_renderer.cameraRT.position.x = m_ui->camPosXSpin->value();
	m_renderer.cameraRT.position.y = m_ui->camPosYSpin->value();
	// Flipping the axis to make the UI more intuitive.
	m_renderer.cameraRT.position.z = -m_ui->camPosZSpin->value();
}

void WolfApp::OnChangeSpeedMult( float offset ) {
	m_ui->moveSpeedMultSpin->setValue( m_ui->moveSpeedMultSpin->value() + offset );
	m_renderer.cameraRT.speedMult = m_ui->moveSpeedMultSpin->value();
}
