#include "WolfApp.h"
#include "RenderParams.hpp" // RenderMode

#include <QMessageBox>
#include <QTimer>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>

WolfApp::WolfApp()
	: m_mainWin{ nullptr }, m_idleTimer{ nullptr }, m_fpsTimer{ nullptr } {
}

WolfApp::~WolfApp() {
	delete m_idleTimer;
	delete m_fpsTimer;
}

bool WolfApp::init( Core::App* appData ) {
	if ( InitWindow() == false )
		return false;

	m_renderer.SetAppData( appData );
	HideIrrelevantWidgets();
	SetupMainWindowSizeAndPosition();
	SetInitialValues();
	ConnectUIEvents();

	m_mainWin->show();
	m_renderer.PrepareForRendering( m_ui->viewport->GetNativeWindowHandle() );

	SetupFPSTimers();
	SetInitialSceneFileLocation();

	return true;
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
	m_renderer.RenderFrame( m_ui->viewport->cameraInput );

	++m_frameIdxAtLastFPSCalc;
}

void WolfApp::OnRenderModeChanged( bool rayTracingOn ) {
	m_idleTimer->stop();
	m_fpsTimer->stop();

	Core::RenderMode renderMode = rayTracingOn ?
		Core::RenderMode::RayTracing : Core::RenderMode::Rasterization;

	SetRenderMode( renderMode );

	m_idleTimer->start( 0 );
	m_fpsTimer->start( 1'000 );
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
	m_renderer.dataRT.randomColors = m_ui->randomColorsRTSwitch->isChecked();

	m_renderer.dataRaster.camera.offsetZ = m_ui->zoomRasterSpin->value();
	m_renderer.dataRaster.camera.offsetZSens = m_ui->zoomSensRasterSpin->value();
	m_renderer.dataRaster.camera.offsetXYSens = m_ui->panSensRasterSpin->value();
	m_renderer.dataRaster.camera.rotSens = m_ui->rotSensRasterSpin->value();
	m_renderer.dataRaster.camera.SetFOVDeg( m_ui->FOVRasterSpin->value() );
	m_renderer.dataRaster.camera.FOVSens = m_ui->FOVSensRasterSpin->value();
	m_renderer.dataRaster.camera.smoothOffsetLerp = m_ui->panAnimSpeedRasterSpin->value();
	m_renderer.dataRaster.camera.smoothRotationLambda = m_ui->rotAnimSpeedRasterSpin->value();
	m_renderer.dataRaster.camera.nearZ = m_ui->nearZRasterSpin->value();
	m_renderer.dataRaster.camera.farZ = m_ui->farZRasterSpin->value();
	m_renderer.dataRaster.camera.aspectRatio = m_ui->aspectRatioRasterSpin->value();
	m_renderer.dataRaster.showBackfaces = m_ui->showBackfacesSwitch->isChecked();
	m_renderer.dataRaster.renderEdges = m_ui->renderEdgesRasterSwitch->isChecked();
	m_renderer.dataRaster.sceneData.useRandomColors = m_ui->randomColorsRasterSwitch->isChecked();
	m_renderer.dataRaster.sceneData.disco = m_ui->discoModeRasterSwitch->isChecked();
	m_renderer.dataRaster.sceneData.discoSpeed = m_ui->discoModeSpeedSpin->value();
	m_renderer.dataRaster.camera.coordinateSystem = static_cast<Raster::TransformCoordinateSystem>(
		m_ui->rotOrientationRasterCombo->currentIndex() );
}

void WolfApp::HideIrrelevantWidgets() {
	bool isRTMode{ m_ui->renderModeSwitch->isChecked() };

	m_ui->cameraPosBox->setVisible( isRTMode );
	m_ui->moveSpeedLblBox->setVisible( isRTMode );
	m_ui->moveSpeedSlider->setVisible( isRTMode );
	m_ui->moveSpeedSpin->setVisible( isRTMode );
	m_ui->moveSpeedMultLbl->setVisible( isRTMode );
	m_ui->moveSpeedMultSpin->setVisible( isRTMode );
	m_ui->mouseSensitivityRTLbl->setVisible( isRTMode );
	m_ui->mouseSensitivityRTSpin->setVisible( isRTMode );
	m_ui->verticalFOVLbl->setVisible( isRTMode );
	m_ui->FOVRTSpin->setVisible( isRTMode );
	m_ui->randomColorsRTLbl->setVisible( isRTMode );
	m_ui->randomColorsRTSwitch->setVisible( isRTMode );

	m_ui->zoomRasterLbl->setHidden( isRTMode );
	m_ui->zoomRasterSpin->setHidden( isRTMode );
	m_ui->zoomSensRasterLbl->setHidden( isRTMode );
	m_ui->zoomSensRasterSpin->setHidden( isRTMode );
	m_ui->panSensRasterLbl->setHidden( isRTMode );
	m_ui->panSensRasterSpin->setHidden( isRTMode );
	m_ui->rotSensRasterLbl->setHidden( isRTMode );
	m_ui->rotSensRasterSpin->setHidden( isRTMode );
	m_ui->FOVRasterLbl->setHidden( isRTMode );
	m_ui->FOVRasterSpin->setHidden( isRTMode );
	m_ui->FOVSensRasterLbl->setHidden( isRTMode );
	m_ui->FOVSensRasterSpin->setHidden( isRTMode );
	m_ui->panAnimSpeedRasterLbl->setHidden( isRTMode );
	m_ui->panAnimSpeedRasterSpin->setHidden( isRTMode );
	m_ui->rotAnimSpeedRasterLbl->setHidden( isRTMode );
	m_ui->rotAnimSpeedRasterSpin->setHidden( isRTMode );
	m_ui->nearZRasterLbl->setHidden( isRTMode );
	m_ui->nearZRasterSpin->setHidden( isRTMode );
	m_ui->farZRasterLbl->setHidden( isRTMode );
	m_ui->farZRasterSpin->setHidden( isRTMode );
	m_ui->aspectRatioRasterLbl->setHidden( isRTMode );
	m_ui->aspectRatioRasterSpin->setHidden( isRTMode );
	m_ui->showBackfacesLbl->setHidden( isRTMode );
	m_ui->showBackfacesSwitch->setHidden( isRTMode );
	m_ui->computeAspectRatioBtn->setHidden( isRTMode );
	m_ui->renderEdgesRasterLbl->setHidden( isRTMode );
	m_ui->renderEdgesRasterSwitch->setHidden( isRTMode );
	m_ui->randomColorsRasterLbl->setHidden( isRTMode );
	m_ui->randomColorsRasterSwitch->setHidden( isRTMode );
	m_ui->discoModeRasterLbl->setHidden( isRTMode );
	m_ui->discoModeRasterSwitch->setHidden( isRTMode );
	m_ui->discoModeSpeedLbl->setHidden( isRTMode );
	m_ui->discoModeSpeedSpin->setHidden( isRTMode );
	m_ui->faceColorRasterLbl->setHidden( isRTMode );
	m_ui->faceColorRasterBtn->setHidden( isRTMode );
	m_ui->rotOrientationRasterLbl->setHidden( isRTMode );
	m_ui->rotOrientationRasterCombo->setHidden( isRTMode );
	m_ui->renderFacesRasterLbl->setHidden( isRTMode );
	m_ui->renderFacesRasterSwitch->setHidden( isRTMode );
	m_ui->renderVertsRasterLbl->setHidden( isRTMode );
	m_ui->renderVertsRasterSwitch->setHidden( isRTMode );
	m_ui->edgeColorRasterLbl->setHidden( isRTMode );
	m_ui->edgeColorRasterBtn->setHidden( isRTMode );
	m_ui->vertexColorRasterLbl->setHidden( isRTMode );
	m_ui->vertexColorRasterBtn->setHidden( isRTMode );
	m_ui->vertexSizeLbl->setHidden( isRTMode );
	m_ui->vertexSizeSpin->setHidden( isRTMode );
	m_ui->vertexSizeSpin->setHidden( isRTMode );
}

void WolfApp::SetupMainWindowSizeAndPosition() {

	QScreen* screen = m_mainWin->screen();
	if ( screen == nullptr )
		screen = QApplication::primaryScreen();

	float aspectRatio{ static_cast<float>(m_renderer.scene.settings.renderWidth) /
		static_cast<float>(m_renderer.scene.settings.renderHeight) };

	// Make the window 80% of the screen size and then correct the aspect ratio.
	m_mainWin->resize( m_mainWin->screen()->size() * 0.8 );
	m_mainWin->resize( m_mainWin->width(), static_cast<int>(m_mainWin->width() / aspectRatio) );

	// Center the application window on the screen.
	const QRect screenGeometry = screen->availableGeometry();
	const QPoint screenCenter = screenGeometry.center();
	QRect windowGeometry = m_mainWin->frameGeometry();

	windowGeometry.moveCenter( screenCenter );
	m_mainWin->move( windowGeometry.topLeft() );

	// Set aspect ratio value in GUI to the aspect ratio of the viewport, NOT scene.
	float aspectRatioViewport{ static_cast<float>(
		m_ui->viewport->width() ) / static_cast<float>( m_ui->viewport->height() ) };
	m_ui->aspectRatioRasterSpin->setValue( aspectRatioViewport );
	m_renderer.dataRT.camera.aspectRatio = aspectRatioViewport;
}

void WolfApp::ConnectUIEvents() {
	connect( m_ui->viewport, &WolfViewportWidget::ToggleFullscreen, this, &WolfApp::ToggleFullscreen );
	connect( m_ui->sceneFileBtn, &QPushButton::clicked, this, &WolfApp::OpenSceneBtnClicked );
	connect( m_ui->loadSceneBtn, &QPushButton::clicked, this, &WolfApp::LoadSceneClicked );

	// Viewport bindings.
	connect( m_ui->viewport, &WolfViewportWidget::OnMouseRotationChanged,
	this, &WolfApp::OnRotateGeometry
	);
	connect( m_ui->viewport, &WolfViewportWidget::OnCameraPan,
	this, &WolfApp::OnCameraPan
	);
	connect( m_ui->viewport, &WolfViewportWidget::OnCameraDolly,
		this, &WolfApp::OnCameraDolly
	);
	connect( m_ui->viewport, &WolfViewportWidget::OnCameraFOV,
		this, &WolfApp::OnCameraFOV
	);
	connect( m_ui->viewport, &WolfViewportWidget::OnChangeSpeedMult,
		this, &WolfApp::OnChangeSpeedMult
	);
	connect( m_ui->viewport, &WolfViewportWidget::OnResize, this, &WolfApp::OnResize );
	connect( m_ui->randomColorsRasterSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) {
			if ( value )
				m_ui->discoModeRasterSwitch->setChecked( false ); } );
	connect( m_ui->discoModeRasterSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) {
			if ( value )
				m_ui->randomColorsRasterSwitch->setChecked( false ); } );

	// Ray Tracing GUI connections.
	connect( m_mainWin->GetRenderModeSwitch(), &QCheckBox::toggled,
		this, &WolfApp::OnRenderModeChanged
	);
	connect( m_ui->moveSpeedSlider, &QSlider::valueChanged,
		this, &WolfApp::MoveSpeedChangedSlider
	);
	connect( m_ui->moveSpeedMultSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MoveSpeedMultChanged
	);
	connect( m_ui->moveSpeedSpin, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MoveSpeedChangedSpin
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
	connect( m_ui->randomColorsRTSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer.dataRT.randomColors = value; }
	);
	connect( &m_ui->viewport->inputUpdateTimer, &QTimer::timeout,
		this, &WolfApp::OnPositionChangedRT
	);
	connect( m_ui->matchRTCamSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) {
			m_renderer.dataRT.SetMatchRTCameraToRaster( value );
			m_renderer.dataRT.camera.yaw += DirectX::XM_PI;
			m_renderer.dataRT.camera.position.z *=-1;
		}
	);

	// Rasterization GUI connections.
	connect( m_ui->zoomRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this]() { m_renderer.dataRaster.camera.offsetZ = m_ui->zoomRasterSpin->value(); }
	);
	connect( m_ui->zoomSensRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.offsetZSens = value;
	} );
	connect( m_ui->panSensRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.offsetXYSens = value;
	} );
	connect( m_ui->rotSensRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.rotSens = value;
	} );
	connect( m_ui->FOVRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer.dataRaster.camera.SetFOVDeg( value ); }
	);
	connect( m_ui->FOVSensRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.FOVSens = value;
	} );
	connect( m_ui->panAnimSpeedRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.smoothOffsetLerp = value;
	} );
	connect( m_ui->rotAnimSpeedRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.smoothRotationLambda = value;
	} );
	connect( m_ui->nearZRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.nearZ = value;
	} );
	connect( m_ui->farZRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.farZ = value;
	} );
	connect( m_ui->aspectRatioRasterSpin, &QDoubleSpinBox::valueChanged,
		this, [this](double value) {
			m_renderer.dataRaster.camera.aspectRatio = value;
	} );
	connect( m_ui->showBackfacesSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer.dataRaster.showBackfaces = value; }
	);
	connect( m_ui->computeAspectRatioBtn, &QPushButton::clicked,
		this, [this](){ OnResize( m_ui->viewport->width(), m_ui->viewport->height()); }
	);
	connect( m_ui->renderFacesRasterSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) {
			m_renderer.dataRaster.renderFaces = value;
			if ( value || !m_ui->randomColorsRasterSwitch->isChecked() || !m_ui->discoModeRasterSwitch->isChecked() )
				m_ui->faceColorRasterBtn->setEnabled( false );
			else
				m_ui->faceColorRasterBtn->setEnabled( true );
		}
	);
	connect( m_ui->renderEdgesRasterSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer.dataRaster.renderEdges = value; }
	);
	connect( m_ui->renderVertsRasterSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer.dataRaster.renderVerts = value; }
	);
	connect( m_ui->randomColorsRasterSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) {
			m_renderer.dataRaster.sceneData.useRandomColors = value;
			if ( value || m_ui->discoModeRasterSwitch->isChecked() || !m_ui->renderFacesRasterSwitch->isChecked() )
				m_ui->faceColorRasterBtn->setEnabled( false );
			else
				m_ui->faceColorRasterBtn->setEnabled( true );
		}
	);
	connect( m_ui->discoModeRasterSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) {
			m_renderer.dataRaster.sceneData.disco = value;
			if ( value || m_ui->randomColorsRasterSwitch->isChecked() || !m_ui->renderFacesRasterSwitch->isChecked() )
				m_ui->faceColorRasterBtn->setEnabled( false );
			else
				m_ui->faceColorRasterBtn->setEnabled( true );
		}
	);
	connect( m_ui->discoModeSpeedSpin, &QSpinBox::valueChanged,
		this, [this]( int value ) { m_renderer.dataRaster.sceneData.discoSpeed = value; }
	);
	connect( m_ui->faceColorRasterBtn, &QToolButton::clicked,
		this, [this]() {
			ColorPickerData data = SetupColorPicker(
				UnpackColor(m_renderer.dataRaster.sceneData.packedColor ) );
			if ( !data.color.isValid() )
				return;
			m_ui->faceColorRasterBtn->setStyleSheet( data.style );
			m_renderer.dataRaster.sceneData.packedColor = PackColor( data.color );
		}
	);
	connect( m_ui->edgeColorRasterBtn, &QToolButton::clicked,
		this, [this]() {
			ColorPickerData data = SetupColorPicker(
				UnpackColor( m_renderer.dataRaster.edgeColor ) );
			if ( !data.color.isValid() )
				return;
			m_ui->edgeColorRasterBtn->setStyleSheet( data.style );
			m_renderer.dataRaster.edgeColor = PackColor( data.color );
		}
	);
	connect( m_ui->vertexColorRasterBtn, &QToolButton::clicked,
		this, [this]() {
			ColorPickerData data = SetupColorPicker(
				UnpackColor( m_renderer.dataRaster.vertexColor ) );
			if ( !data.color.isValid() )
				return;
			m_ui->vertexColorRasterBtn->setStyleSheet( data.style );
			m_renderer.dataRaster.vertexColor = PackColor( data.color );
		}
	);
	connect( m_ui->rotOrientationRasterCombo, &QComboBox::currentIndexChanged,
		this, [this]( int value ) { m_renderer.dataRaster.camera.coordinateSystem =
			static_cast<Raster::TransformCoordinateSystem>(value);
		}
	);
	connect( m_ui->vertexSizeSpin, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) { m_renderer.dataRaster.vertexSize = value; }
	);
}

uint32_t WolfApp::PackColor( QColor& color ) {
	uint32_t r32 = static_cast<uint32_t>(color.red());
	uint32_t g32 = static_cast<uint32_t>(color.green());
	uint32_t b32 = static_cast<uint32_t>(color.blue());
	uint32_t a32 = static_cast<uint32_t>(color.alpha());

	return (a32 << 24) | (b32 << 16) | (g32 << 8) | r32;
}

QColor WolfApp::UnpackColor( uint32_t packedColor ) {
	int alpha{ (packedColor >> 24) & 0xFF };
	int blue{ (packedColor >> 16) & 0xFF };
	int green{ (packedColor >> 8) & 0xFF };
	int red{ packedColor & 0xFF };

	return QColor( red, green, blue, alpha );
}

void WolfApp::SetupFPSTimers() {
	m_idleTimer = new QTimer( m_mainWin );
	connect( m_idleTimer, &QTimer::timeout, this, &WolfApp::OnIdleTick );
	m_idleTimer->start( 0 ); // Continuous rendering.

	m_fpsTimer = new QTimer( m_mainWin );
	connect( m_fpsTimer, &QTimer::timeout, this, &WolfApp::UpdateRenderStats );
	m_fpsTimer->start( 1'000 ); // Update FPS every second.
}

void WolfApp::OnQuit() {
	m_idleTimer->stop();
	m_fpsTimer->stop();
	m_renderer.StopRendering();
}

void WolfApp::OnIdleTick() {
	RenderFrame();
}

void WolfApp::SetInitialSceneFileLocation() {
	std::string scenePath{ m_renderer.scene.GetRenderScenePath() };
	QString fileAbsPath{ QDir::toNativeSeparators( QDir::cleanPath(
		QDir().absoluteFilePath( QString::fromStdString( scenePath ) ) ) ) };

	m_ui->sceneFileEntry->setText( QDir::toNativeSeparators( fileAbsPath ) );
}

void WolfApp::OnCameraPan( float ndcX, float ndcY ) {
	m_renderer.AddToTargetOffset( ndcX, ndcY );
}

void WolfApp::OnCameraDolly( float offsetZ ) {
	m_renderer.AddToOffsetZ( offsetZ );
	m_ui->zoomRasterSpin->setValue( m_renderer.dataRaster.camera.offsetZ );
}

void WolfApp::OnCameraFOV( float offset ) {
	m_renderer.AddToOffsetFOV( offset );
}

void WolfApp::OnRotateGeometry( float deltaAngleX, float deltaAngleY ) {
	m_renderer.AddToTargetRotation( deltaAngleX, deltaAngleY );
}

void WolfApp::OnPositionChangedRT() {
	QSignalBlocker blockX( m_ui->camPosXSpin );
	QSignalBlocker blockY( m_ui->camPosYSpin );
	QSignalBlocker blockZ( m_ui->camPosZSpin );

	const int zSign = m_renderer.dataRT.GetMatchRTCameraToRaster();

	m_ui->camPosXSpin->setValue( m_renderer.dataRT.camera.position.x );
	m_ui->camPosYSpin->setValue( m_renderer.dataRT.camera.position.y );
	m_ui->camPosZSpin->setValue( -m_renderer.dataRT.camera.position.z * zSign );
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

	m_renderer.ReloadScene( scenePathStd, m_ui->viewport->GetNativeWindowHandle());
	// Update aspect ratio in case the new scene updated it.
	OnResize( m_ui->viewport->width(), m_ui->viewport->height() );

	m_idleTimer->start( 0 );
	m_fpsTimer->start( 1'000 );
}

void WolfApp::MoveSpeedChangedSpin() {
	m_renderer.dataRT.camera.movementSpeed =
		m_ui->moveSpeedSpin->value();

	// Block slider signals so it doesn't call MoveSpeedChangedSlider() and
	// round the value. Unblocks in destructor.
	QSignalBlocker blockSlider( m_ui->moveSpeedSlider );
	m_ui->moveSpeedSlider->setValue( std::round( m_renderer.dataRT.camera.movementSpeed ) );
}

void WolfApp::MoveSpeedChangedSlider() {
	m_renderer.dataRT.camera.movementSpeed = m_ui->moveSpeedSlider->value();
	m_ui->moveSpeedSpin->setValue( m_renderer.dataRT.camera.movementSpeed );
}

void WolfApp::MoveSpeedMultChanged() {
	m_renderer.dataRT.camera.speedMult = m_ui->moveSpeedMultSpin->value();
}

void WolfApp::MouseSensitivityRTChanged() {
	m_renderer.dataRT.camera.mouseSensitivity = m_ui->mouseSensitivityRTSpin->value();
}

void WolfApp::VerticalFoVRTChanged() {
	m_renderer.dataRT.camera.setVerticalFOVDeg( m_ui->FOVRTSpin->value() );
}

void WolfApp::CameraPositionChangedRT() {
	const int zSign{ m_renderer.dataRT.GetMatchRTCameraToRaster() };
	m_renderer.dataRT.camera.position.x = m_ui->camPosXSpin->value();
	m_renderer.dataRT.camera.position.y = m_ui->camPosYSpin->value();
	// Flipping the axis to make the UI more intuitive.
	m_renderer.dataRT.camera.position.z = -m_ui->camPosZSpin->value() * zSign;
}

void WolfApp::OnChangeSpeedMult( float offset ) {
	m_ui->moveSpeedMultSpin->setValue( m_ui->moveSpeedMultSpin->value() + offset );
	m_renderer.dataRT.camera.speedMult = m_ui->moveSpeedMultSpin->value();
}

void WolfApp::OnResize( float width, float height ) {
	m_renderer.dataRT.camera.aspectRatio = width / height;

	if ( !m_ui->renderModeSwitch->isChecked() ) {
		m_ui->aspectRatioRasterSpin->setValue( m_renderer.dataRT.camera.aspectRatio );
	}
}

void WolfApp::ToggleFullscreen() {
	m_ui->menuBar->setVisible( !m_ui->menuBar->isVisible() );
	m_ui->renderModeFrame->setVisible( !m_ui->renderModeFrame->isVisible() );
	m_ui->scrollAreaSettings->setVisible( !m_ui->scrollAreaSettings->isVisible() );
}

ColorPickerData WolfApp::SetupColorPicker( QColor currColor ) {
	// Open color picker dialog when Color Button is clicked.
	QColor color = QColorDialog::getColor( currColor, m_mainWin, "Select Geometry Color" );

	QString hoverColor;
	if ( color.valueF() > 0.5f )
		hoverColor = color.darker().name();
	else if ( color.valueF() > 0.1f )
		hoverColor = color.lighter().name();
	else
		hoverColor = "#222222";

	// Update the preview label.
	QString btnStyle = QString( R"(
		QToolButton {
			background-color: %1;
			border-radius: 6px;
		}

		QToolButton:hover {
			background-color: %2;
			border: 1px;
			border-radius: 6px;
		}

		QToolTip {
			background-color: palette(tooltip-base);
			color: palette(tooltip-text);
		})"
	).arg( color.name(), hoverColor );

	return { color, btnStyle };
}
