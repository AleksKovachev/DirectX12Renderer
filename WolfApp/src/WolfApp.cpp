#include "AppData.hpp"
#include "RenderParams.hpp" // RenderMode
#include "WolfApp.h"

#include <QMessageBox>
#include <QtGui/QShortcut>
#include <QTimer>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStackedLayout>

WolfApp::WolfApp( Core::AppData& appData, Core::WolfRenderer& renderer )
	: m_appData{ &appData }, m_renderer{ &renderer } {}

WolfApp::~WolfApp() {
	delete m_idleTimer;
	delete m_fpsTimer;
}

bool WolfApp::init() {
	if ( InitWindow() == false )
		return false;

	m_renderer->PrepareForRendering( m_ui->viewport->GetNativeWindowHandle() );

	HideIrrelevantWidgets();
	SetupMainWindowSizeAndPosition();
	SetInitialValues();
	ConnectUIEvents();
	SetupColorPicker();

	m_mainWin->show();

	SetupAspectRatio();

	SetupFPSTimers();
	SetInitialSceneFileLocation();

	SetupShortcuts();

	return true;
}

bool WolfApp::InitWindow() {
	m_mainWin = new WolfMainWindow;
	m_ui = &m_mainWin->GetUI();
	m_ui->viewport->SetRenderMode( m_renderer->renderMode );
	return true;
}

void WolfApp::RenderFrame() {
	m_renderer->RenderFrame( m_ui->viewport->cameraInput );

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
	m_ui->fpsVal->setText( QString::number( m_frameIdxAtLastFPSCalc ) );
	m_frameIdxAtLastFPSCalc = 0;
}

void WolfApp::SetRenderMode( Core::RenderMode renderMode ) {
	m_renderer->SetRenderMode( renderMode );
	m_ui->viewport->SetRenderMode( renderMode );
}

void WolfApp::SetInitialValues() {
	///////////// Colors /////////////

	///////////// Booleans /////////////
	m_renderer->dataRT.randomColors = m_ui->randomColorsSwitchRT->isChecked();
	m_renderer->SetFacePassPSO( m_ui->showBackfacesSwitchR->isChecked() );
	m_renderer->dataR.renderEdges = m_ui->renderEdgesSwitchR->isChecked();

	///////////// Integers /////////////
	m_renderer->dataR.sceneData.discoSpeed = m_ui->discoModeSpeedSpinR->value();

	///////////// Floating Points /////////////
	MoveSpeedChangedSpin();
	MoveSpeedMultChanged();
	MouseSensitivityRTChanged();
	VerticalFoVRTChanged();
	m_renderer->dataR.camera.offsetZ = m_ui->zoomSpinR->value();
	m_renderer->dataR.camera.offsetZSens = m_ui->zoomSensSpinR->value();
	m_renderer->dataR.camera.offsetXYSens = m_ui->panSensSpinR->value();
	m_renderer->dataR.camera.rotSensMultiplier = m_ui->rotSensSpinR->value();
	m_renderer->dataR.camera.SetFOVDeg( m_ui->FOVSpinR->value() );
	m_renderer->dataR.camera.FOVSens = m_ui->FOVSensSpinR->value();
	m_renderer->dataR.camera.smoothOffsetLerp = m_ui->panAnimSpeedSpinR->value();
	m_renderer->dataR.camera.smoothRotationLambda = m_ui->rotAnimSpeedSpinR->value();
	m_renderer->dataR.camera.nearZ = m_ui->nearZSpinR->value();
	m_renderer->dataR.camera.farZ = m_ui->farZSpinR->value();
	m_renderer->dataR.camera.aspectRatio = m_ui->aspectRatioSpinR->value();
	m_renderer->dataR.directionalLight.cb.specularStrength = m_ui->specStrengthSpinR->value();
	m_renderer->dataR.directionalLight.cb.ambientIntensity = m_ui->ambientLightSpinR->value();
	m_renderer->dataR.directionalLight.cb.shadowBias = m_ui->shadowDepthBiasSpinR->value();
	m_renderer->dataR.sceneData.textureTiling = m_ui->textureTileSpinR->value();
	m_renderer->dataR.sceneData.textureProportionsX = m_ui->textureProportionsXSpinR->value();
	m_renderer->dataR.sceneData.textureProportionsY = m_ui->textureProportionsYSpinR->value();

	///////////// Enums /////////////
	SetupOutputAlbedoColor();
	m_renderer->dataR.camera.coordinateSystem = static_cast<Raster::CameraCoordinateSystem>(
		m_ui->rotOrientationComboR->currentIndex() );
}

void WolfApp::HideIrrelevantWidgets() {
	bool isRTMode{ m_ui->renderModeSwitch->isChecked() };

	///////////// Containers /////////////
	m_ui->cameraPosBoxRT->setVisible( isRTMode );
	m_ui->moveSpeedLblBoxRT->setVisible( isRTMode );
	m_ui->aspectRatioLayoutWidgetR->setHidden( isRTMode );
	m_ui->dirLightBoxR->setHidden( isRTMode );

	///////////// Labels /////////////
	m_ui->moveSpeedMultLblRT->setVisible( isRTMode );
	m_ui->mouseSensitivityLblRT->setVisible( isRTMode );
	m_ui->verticalFOVLblRT->setVisible( isRTMode );
	m_ui->randomColorsLblRT->setVisible( isRTMode );
	m_ui->zoomLblR->setHidden( isRTMode );
	m_ui->zoomSensLblR->setHidden( isRTMode );
	m_ui->panSensLblR->setHidden( isRTMode );
	m_ui->rotSensLblR->setHidden( isRTMode );
	m_ui->FOVLblRT->setHidden( isRTMode );
	m_ui->FOVSensLblR->setHidden( isRTMode );
	m_ui->panAnimSpeedLblR->setHidden( isRTMode );
	m_ui->rotAnimSpeedLblR->setHidden( isRTMode );
	m_ui->nearZLblR->setHidden( isRTMode );
	m_ui->farZLblR->setHidden( isRTMode );
	m_ui->aspectRatioLblR->setHidden( isRTMode );
	m_ui->showBackfacesLblR->setHidden( isRTMode );
	m_ui->renderEdgesLblR->setHidden( isRTMode );
	m_ui->randomColorsLblR->setHidden( isRTMode );
	m_ui->discoModeLblR->setHidden( isRTMode );
	m_ui->discoModeSpeedLblR->setHidden( isRTMode );
	m_ui->faceColorLblR->setHidden( isRTMode );
	m_ui->rotOrientationLblR->setHidden( isRTMode );
	m_ui->renderFacesLblR->setHidden( isRTMode );
	m_ui->renderVertsLblR->setHidden( isRTMode );
	m_ui->edgeColorLblR->setHidden( isRTMode );
	m_ui->vertexColorLblR->setHidden( isRTMode );
	m_ui->vertexSizeLblR->setHidden( isRTMode );
	m_ui->backgroundColorLblR->setHidden( isRTMode );
	m_ui->specStrengthLblR->setHidden( isRTMode );
	m_ui->dirLightIntensityLblR->setHidden( isRTMode );
	m_ui->dirLightColorLblR->setHidden( isRTMode );
	m_ui->shadeModeLblR->setHidden( isRTMode );
	m_ui->dirLightShadowExtentLblR->setHidden( isRTMode );
	m_ui->dirLightNearZLblR->setHidden( isRTMode );
	m_ui->dirLightFatZLblR->setHidden( isRTMode );
	m_ui->shadowDepthBiasLblR->setHidden( isRTMode );
	m_ui->ambientLightLblR->setHidden( isRTMode );
	m_ui->shadowOverlayLblR->setHidden( isRTMode );
	m_ui->checkerTextureLblR->setHidden( isRTMode );
	m_ui->gridTextureLblR->setHidden( isRTMode );
	m_ui->textureTileLblR->setHidden( isRTMode );
	m_ui->textureProportionsXLblR->setHidden( isRTMode );
	m_ui->textureProportionsYLblR->setHidden( isRTMode );
	m_ui->textureColorALblR->setHidden( isRTMode );
	m_ui->textureColorBLblR->setHidden( isRTMode );

	///////////// Sliders /////////////
	m_ui->moveSpeedSliderRT->setVisible( isRTMode );

	///////////// Combo Boxes /////////////
	m_ui->rotOrientationComboR->setHidden( isRTMode );
	m_ui->shadeModeComboR->setHidden( isRTMode );

	///////////// Tool Buttons (Colors) /////////////
	m_ui->randomColorsSwitchRT->setVisible( isRTMode );
	m_ui->showBackfacesSwitchR->setHidden( isRTMode );
	m_ui->renderEdgesSwitchR->setHidden( isRTMode );
	m_ui->randomColorsSwitchR->setHidden( isRTMode );
	m_ui->discoModeSwitchR->setHidden( isRTMode );
	m_ui->renderFacesSwitchR->setHidden( isRTMode );
	m_ui->renderVertsSwitchR->setHidden( isRTMode );
	m_ui->shadowOverlaySwitchR->setHidden( isRTMode );
	m_ui->checkerTextureSwitchR->setHidden( isRTMode );
	m_ui->gridTextureSwitchR->setHidden( isRTMode );

	///////////// Push Buttons /////////////
	m_ui->computeAspectRatioBtnR->setHidden( isRTMode );
	m_ui->faceColorBtnR->setHidden( isRTMode );
	m_ui->edgeColorBtnR->setHidden( isRTMode );
	m_ui->vertexColorBtnR->setHidden( isRTMode );
	m_ui->backgroundColorBtnR->setHidden( isRTMode );
	m_ui->dirLightColorBtnR->setHidden( isRTMode );
	m_ui->textureColorABtnR->setHidden( isRTMode );
	m_ui->textureColorBBtnR->setHidden( isRTMode );

	///////////// Spin Boxes /////////////
	m_ui->discoModeSpeedSpinR->setHidden( isRTMode );

	///////////// Double Spin Boxes /////////////
	m_ui->moveSpeedSpinRT->setVisible( isRTMode );
	m_ui->moveSpeedMultSpinRT->setVisible( isRTMode );
	m_ui->mouseSensitivitySpinRT->setVisible( isRTMode );
	m_ui->FOVSpinRT->setVisible( isRTMode );
	m_ui->zoomSpinR->setHidden( isRTMode );
	m_ui->zoomSensSpinR->setHidden( isRTMode );
	m_ui->panSensSpinR->setHidden( isRTMode );
	m_ui->rotSensSpinR->setHidden( isRTMode );
	m_ui->FOVSpinR->setHidden( isRTMode );
	m_ui->FOVSensSpinR->setHidden( isRTMode );
	m_ui->panAnimSpeedSpinR->setHidden( isRTMode );
	m_ui->rotAnimSpeedSpinR->setHidden( isRTMode );
	m_ui->nearZSpinR->setHidden( isRTMode );
	m_ui->farZSpinR->setHidden( isRTMode );
	m_ui->aspectRatioSpinR->setHidden( isRTMode );
	m_ui->vertexSizeSpinR->setHidden( isRTMode );
	m_ui->specStrengthSpinR->setHidden( isRTMode );
	m_ui->dirLightIntensitySpinR->setHidden( isRTMode );
	m_ui->dirLightShadowExtentSpinR->setHidden( isRTMode );
	m_ui->dirLightNearZSpinR->setHidden( isRTMode );
	m_ui->dirLightFatZSpinR->setHidden( isRTMode );
	m_ui->shadowDepthBiasSpinR->setHidden( isRTMode );
	m_ui->ambientLightSpinR->setHidden( isRTMode );
	m_ui->textureTileSpinR->setHidden( isRTMode );
	m_ui->textureProportionsXSpinR->setHidden( isRTMode );
	m_ui->textureProportionsYSpinR->setHidden( isRTMode );
}

void WolfApp::SetupMainWindowSizeAndPosition() {
	QScreen* screen = m_mainWin->screen();
	if ( screen == nullptr )
		screen = QApplication::primaryScreen();

	float aspectRatio{ static_cast<float>(m_appData->scene.settings.renderWidth) /
		static_cast<float>(m_appData->scene.settings.renderHeight) };

	// Make the window 80% of the screen size and then correct the aspect ratio.
	m_mainWin->resize( m_mainWin->screen()->size() * 0.8 );
	m_mainWin->resize( m_mainWin->width(), static_cast<int>(m_mainWin->width() / aspectRatio) );

	// Center the application window on the screen.
	const QRect screenGeometry = screen->availableGeometry();
	const QPoint screenCenter = screenGeometry.center();
	QRect windowGeometry = m_mainWin->frameGeometry();

	windowGeometry.moveCenter( screenCenter );
	m_mainWin->move( windowGeometry.topLeft() );
}

void WolfApp::ConnectUIEvents() {
	// Application bindings
	connect( m_mainWin, &WolfMainWindow::requestQuit, this, &WolfApp::OnQuit );

	///////////// Actions /////////////
	connect( m_ui->actionExit, &QAction::triggered, this, [this]() {
		OnQuit(); QApplication::quit(); } );
	connect( m_ui->actionOpenScene, &QAction::triggered, this, &WolfApp::OpenSceneBtnClicked );
	connect( m_ui->actionLoadScene, &QAction::triggered, this, &WolfApp::LoadSceneClicked );

	///////////// Push Buttons /////////////
	connect( m_ui->sceneFileBtn, &QPushButton::clicked, this, &WolfApp::OpenSceneBtnClicked );
	connect( m_ui->loadSceneBtn, &QPushButton::clicked, this, &WolfApp::LoadSceneClicked );

	// Viewport bindings.
	connect( m_ui->viewport, &WolfViewportWidget::ToggleFullscreen, this, &WolfApp::ToggleFullscreen );
	connect( m_ui->viewport, &WolfViewportWidget::OnMouseRotationChanged,
		this, &WolfApp::OnRotateGeometry );
	connect( m_ui->viewport, &WolfViewportWidget::OnCameraPan, this, &WolfApp::OnCameraPan );
	connect( m_ui->viewport, &WolfViewportWidget::OnCameraDolly, this, &WolfApp::OnCameraDolly );
	connect( m_ui->viewport, &WolfViewportWidget::OnCameraFOV, this, &WolfApp::OnCameraFOV );
	connect( m_ui->viewport, &WolfViewportWidget::OnChangeSpeedMult, this, &WolfApp::OnChangeSpeedMult );
	connect( m_ui->viewport, &WolfViewportWidget::OnResize, this, &WolfApp::OnResize );

	// Ray Tracing GUI connections.

	///////////// Tool Buttons (Colors) /////////////
	connect( m_ui->backgroundColorBtnRT, &QToolButton::clicked,
		this, [this]() {
			const QColor currColor{ UnpackColor( m_renderer->dataRT.bgColorPacked ) };
			m_colorDialog->setCurrentColor( currColor );

			connect( m_colorDialog, &QColorDialog::currentColorChanged, this,
				[this]( const QColor& color ) {
					m_renderer->dataRT.bgColorPacked = PackColor( color );
			} );
			connect( m_colorDialog, &QColorDialog::colorSelected, this,
				[this]( const QColor& color ) {
					QString btnStyle = GetButtonStyle( color );
					m_ui->backgroundColorBtnRT->setStyleSheet( btnStyle );
			} );
			connect( m_colorDialog, &QColorDialog::rejected, this,
				[this, currColor]() {
					m_renderer->dataRT.bgColorPacked = PackColor( currColor );
			} );

			m_colorDialog->exec();
			m_colorDialog->disconnect(); // removes all signals/slots connections
		}
	);

	///////////// Checkboxes (Switches) /////////////
	connect( m_ui->renderModeSwitch, &QCheckBox::toggled,
		this, &WolfApp::OnRenderModeChanged
	);
	connect( m_ui->matchRTCamSwitch, &QCheckBox::toggled,
		this, [this]( bool value ) {
			m_renderer->dataRT.SetMatchRTCameraToRaster( value );
			m_renderer->dataRT.camera.yaw += DirectX::XM_PI;
			m_renderer->dataRT.camera.position.z *= -1;
	} );

	///////////// Sliders /////////////
	connect( m_ui->moveSpeedSliderRT, &QSlider::valueChanged,
		this, &WolfApp::MoveSpeedChangedSlider
	);
	connect( m_ui->randomColorsSwitchRT, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer->dataRT.randomColors = value; }
	);

	///////////// Double Spin Boxes /////////////
	connect( m_ui->moveSpeedMultSpinRT, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MoveSpeedMultChanged
	);
	connect( m_ui->moveSpeedSpinRT, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::MoveSpeedChangedSpin
	);
	connect( m_ui->mouseSensitivitySpinRT, &QDoubleSpinBox::editingFinished,
		this, &WolfApp::MouseSensitivityRTChanged
	);
	connect( m_ui->FOVSpinRT, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::VerticalFoVRTChanged
	);
	connect( m_ui->camPosXSpinRT, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( m_ui->camPosYSpinRT, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);
	connect( m_ui->camPosZSpinRT, &QDoubleSpinBox::valueChanged,
		this, &WolfApp::CameraPositionChangedRT
	);

	///////////// Others /////////////
	connect( &m_ui->viewport->inputUpdateTimer, &QTimer::timeout,
		this, &WolfApp::OnPositionChangedRT
	);

	// Rasterization GUI connections.

	///////////// Combo Boxes /////////////
	connect( m_ui->rotOrientationComboR, &QComboBox::currentIndexChanged,
		this, [this]( int value ) { m_renderer->dataR.camera.coordinateSystem =
			static_cast<Raster::CameraCoordinateSystem>(value);
	} );
	connect( m_ui->shadeModeComboR, &QComboBox::currentIndexChanged,
		this, [this]( int value ) { m_renderer->dataR.sceneData.shadeMode = value;
	} );

	///////////// Tool Buttons (Colors) /////////////
	connect( m_ui->backgroundColorBtnR, &QToolButton::clicked,
		this, [this] () {
			m_colorDialog->setParent( m_ui->backgroundColorBtnR->parentWidget(), Qt::Dialog );
			QColor currColor;
			float* bgColor{ m_renderer->dataR.bgColor };
			currColor.setRgbF( bgColor[0], bgColor[1], bgColor[2], bgColor[3] );
			m_colorDialog->setCurrentColor( currColor );

			connect( m_colorDialog, &QColorDialog::currentColorChanged, this,
				[this, bgColor]( const QColor& color ) {
					bgColor[0] = color.redF();
					bgColor[1] = color.greenF();
					bgColor[2] = color.blueF();
					bgColor[3] = color.alphaF();
			} );
			connect( m_colorDialog, &QColorDialog::colorSelected, this,
				[this]( const QColor& color ) {
					QString btnStyle = GetButtonStyle( color );
					m_ui->backgroundColorBtnR->setStyleSheet( btnStyle );
			} );
			connect( m_colorDialog, &QColorDialog::rejected, this,
				[this, currColor, bgColor]() {
					bgColor[0] = currColor.redF();
					bgColor[1] = currColor.greenF();
					bgColor[2] = currColor.blueF();
					bgColor[3] = currColor.alphaF();
			} );

			m_colorDialog->exec();
			m_colorDialog->disconnect(); // removes all signals/slots connections
		}
	);
	connect( m_ui->faceColorBtnR, &QToolButton::clicked,
		this, [this]() {
			m_colorDialog->setParent( m_ui->faceColorBtnR->parentWidget(), Qt::Dialog );
			DirectX::XMFLOAT4& colorVar{ m_renderer->dataR.sceneData.geometryColor };
			SetupColorButtonConnections( colorVar, m_ui->faceColorBtnR );
	} );
	connect( m_ui->edgeColorBtnR, &QToolButton::clicked,
		this, [this]() {
			m_colorDialog->setParent( m_ui->edgeColorBtnR->parentWidget(), Qt::Dialog );
			uint32_t& colorVar{ m_renderer->dataR.edgeColor };
			SetupColorButtonConnectionsPacked( colorVar, m_ui->edgeColorBtnR );
		}
	);
	connect( m_ui->vertexColorBtnR, &QToolButton::clicked,
		this, [this]() {
			m_colorDialog->setParent( m_ui->vertexColorBtnR->parentWidget(), Qt::Dialog );
			uint32_t& colorVar{ m_renderer->dataR.vertexColor };
			SetupColorButtonConnectionsPacked( colorVar, m_ui->vertexColorBtnR );
		}
	);
	connect( m_ui->dirLightColorBtnR, &QToolButton::clicked,
		this, [this]() {
			m_colorDialog->setParent( m_ui->dirLightColorBtnR->parentWidget(), Qt::Dialog );
			DirectX::XMFLOAT4& colorVar{ m_renderer->dataR.directionalLight.cb.color };
			SetupColorButtonConnections( colorVar, m_ui->dirLightColorBtnR );
	} );
	connect( m_ui->textureColorABtnR, &QToolButton::clicked,
		this, [this]() {
			m_colorDialog->setParent( m_ui->textureColorABtnR->parentWidget(), Qt::Dialog );
			DirectX::XMFLOAT4& colorVar{ m_renderer->dataR.sceneData.textureColorA };
			SetupColorButtonConnections( colorVar, m_ui->textureColorABtnR );
	} );
	connect( m_ui->textureColorBBtnR, &QToolButton::clicked,
		this, [this]() {
			m_colorDialog->setParent( m_ui->textureColorBBtnR->parentWidget(), Qt::Dialog );
			DirectX::XMFLOAT4& colorVar{ m_renderer->dataR.sceneData.textureColorB };
			SetupColorButtonConnections( colorVar, m_ui->textureColorBBtnR );
	} );

	///////////// Push Buttons /////////////
	connect( m_ui->computeAspectRatioBtnR, &QPushButton::clicked,
		this, [this](){ OnResize( m_ui->viewport->width(), m_ui->viewport->height()); }
	);

	///////////// Checkboxes (Switches) /////////////
	connect( m_ui->showBackfacesSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer->SetFacePassPSO( value ); }
	);
	connect( m_ui->renderFacesSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) {
			m_renderer->dataR.renderFaces = value;
			OutputTargetChanged( m_ui->renderFacesSwitchR );
		}
	);
	connect( m_ui->renderEdgesSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer->dataR.renderEdges = value; }
	);
	connect( m_ui->renderVertsSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) { m_renderer->dataR.renderVerts = value; }
	);
	connect( m_ui->randomColorsSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) {
			if ( value ) {
				uint32_t newValue = static_cast<uint32_t>(Raster::OutputAlbedoPS::RandomColors);
				m_renderer->dataR.sceneData.outputAlbedo = newValue;
			}
			OutputTargetChanged( m_ui->randomColorsSwitchR );
		}
	);
	connect( m_ui->discoModeSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) {
			if ( value ) {
				uint32_t newValue = static_cast<uint32_t>(Raster::OutputAlbedoPS::DiscoMode);
				m_renderer->dataR.sceneData.outputAlbedo = newValue;
			}
			ToggleTextureColorButtonsEnabled();
			OutputTargetChanged( m_ui->discoModeSwitchR );
		}
	);
	connect( m_ui->shadowOverlaySwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) {
			if ( value ) {
				uint32_t newValue = static_cast<uint32_t>(Raster::OutputAlbedoPS::ShadowOverlayDebug);
				m_renderer->dataR.sceneData.outputAlbedo = newValue;
			}
			ToggleTextureColorButtonsEnabled();
			OutputTargetChanged( m_ui->shadowOverlaySwitchR );
		}
	);
	connect( m_ui->checkerTextureSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) {
			if ( value ) {
				uint32_t newValue = static_cast<uint32_t>(Raster::OutputAlbedoPS::UVChecker);
				m_renderer->dataR.sceneData.outputAlbedo = newValue;

			}
			ToggleProcTextureParamWidgetsEnabled();
			ToggleTextureColorButtonsEnabled();
			OutputTargetChanged( m_ui->checkerTextureSwitchR );
		}
	);
	connect( m_ui->gridTextureSwitchR, &QCheckBox::toggled,
		this, [this]( bool value ) {
			if ( value ) {
				uint32_t newValue = static_cast<uint32_t>(Raster::OutputAlbedoPS::UVGrid);
				m_renderer->dataR.sceneData.outputAlbedo = newValue;
			}
			ToggleProcTextureParamWidgetsEnabled();
			ToggleTextureColorButtonsEnabled();
			OutputTargetChanged( m_ui->gridTextureSwitchR );
		}
	);

	///////////// Spin Boxes /////////////
	connect( m_ui->discoModeSpeedSpinR, &QSpinBox::valueChanged,
		this, [this]( int value ) { m_renderer->dataR.sceneData.discoSpeed = value; }
	);

	///////////// DoubleSpin Boxes /////////////
	connect( m_ui->zoomSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]() { m_renderer->dataR.camera.offsetZ = m_ui->zoomSpinR->value(); }
	);
	connect( m_ui->zoomSensSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.offsetZSens = value; } );
	connect( m_ui->panSensSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.offsetXYSens = value; } );
	connect( m_ui->rotSensSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.rotSensMultiplier = value; } );
	connect( m_ui->FOVSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.SetFOVDeg( value ); }
	);
	connect( m_ui->FOVSensSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.FOVSens = value; } );
	connect( m_ui->panAnimSpeedSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.smoothOffsetLerp = value; } );
	connect( m_ui->rotAnimSpeedSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.smoothRotationLambda = value; } );
	connect( m_ui->nearZSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.nearZ = value; } );
	connect( m_ui->farZSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.farZ = value; } );
	connect( m_ui->aspectRatioSpinR, &QDoubleSpinBox::valueChanged,
		this, [this](double value) { m_renderer->dataR.camera.aspectRatio = value; } );
	connect( m_ui->vertexSizeSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) { m_renderer->dataR.vertexSize = value; } );
	connect( m_ui->specStrengthSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.cb.specularStrength = value;
	} );
	connect( m_ui->dirLightIntensitySpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.cb.intensity = value;
	} );
	connect( m_ui->dirLightXSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.directionWS.x = value;
	} );
	connect( m_ui->dirLightYSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.directionWS.y = value;
	} );
	connect( m_ui->dirLightZSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.directionWS.z = value;
	} );
	connect( m_ui->dirLightShadowExtentSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.shadowExtent = value;
	} );
	connect( m_ui->dirLightNearZSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.nearZ = value;
	} );
	connect( m_ui->dirLightFatZSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.farZ = value;
	} );
	connect( m_ui->shadowDepthBiasSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.cb.shadowBias = value;
	} );
	connect( m_ui->ambientLightSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.directionalLight.cb.ambientIntensity = value;
	} );
	connect( m_ui->textureTileSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.sceneData.textureTiling = value;
	} );
	connect( m_ui->textureProportionsXSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.sceneData.textureProportionsX = value;
	} );
	connect( m_ui->textureProportionsYSpinR, &QDoubleSpinBox::valueChanged,
		this, [this]( double value ) {
			m_renderer->dataR.sceneData.textureProportionsY = value;
	} );
}

uint32_t WolfApp::PackColor( const QColor& color ) {
	uint32_t r32 = static_cast<uint32_t>(color.red());
	uint32_t g32 = static_cast<uint32_t>(color.green());
	uint32_t b32 = static_cast<uint32_t>(color.blue());
	uint32_t a32 = static_cast<uint32_t>(color.alpha());

	return (a32 << 24) | (b32 << 16) | (g32 << 8) | r32;
}

QColor WolfApp::UnpackColor( const uint32_t packedColor ) {
	int alpha{ (packedColor >> 24) & 0xFF };
	int blue{ (packedColor >> 16) & 0xFF };
	int green{ (packedColor >> 8) & 0xFF };
	int red{ packedColor & 0xFF };

	return QColor( red, green, blue, alpha );
}

void WolfApp::SetupAspectRatio() {
	// Set aspect ratio value in GUI to the aspect ratio of the viewport, NOT scene.
	float aspectRatioViewport{ static_cast<float>(
		m_ui->viewport->width()) / static_cast<float>(m_ui->viewport->height()) };
	m_ui->aspectRatioSpinR->setValue( aspectRatioViewport );
	m_renderer->dataRT.camera.aspectRatio = aspectRatioViewport;
}

void WolfApp::SetupShortcuts() {
	// Toggle Render Mode.
	QShortcut* toggleRenderMode = new QShortcut( QKeySequence( Qt::CTRL | Qt::Key_R ), m_mainWin );
	toggleRenderMode->setContext( Qt::ApplicationShortcut );
	connect( toggleRenderMode, &QShortcut::activated, this, [this]() {
		SetRenderMode( m_renderer->renderMode == Core::RenderMode::RayTracing ?
		Core::RenderMode::Rasterization : Core::RenderMode::RayTracing );
	} );

	// Toggle Fullscreen.
	QShortcut* toggleFullscreen = new QShortcut( QKeySequence( Qt::Key_F ), m_mainWin );
	toggleFullscreen->setContext( Qt::ApplicationShortcut );
	connect( toggleFullscreen, &QShortcut::activated, this, &WolfApp::ToggleFullscreen );

	// Exit.
	QShortcut* exit = new QShortcut( QKeySequence( Qt::CTRL | Qt::SHIFT | Qt::Key_E ), m_mainWin );
	exit->setContext( Qt::ApplicationShortcut );
	connect( exit, &QShortcut::activated, this, [this]() { OnQuit(); QApplication::quit(); } );

	// Open Scene.
	QShortcut* openScene = new QShortcut( QKeySequence( Qt::CTRL | Qt::Key_O ), m_mainWin );
	openScene->setContext( Qt::ApplicationShortcut );
	connect( openScene, &QShortcut::activated, this, &WolfApp::OpenSceneBtnClicked );

	// Load Scene.
	QShortcut* loadScene = new QShortcut( QKeySequence( Qt::CTRL | Qt::Key_L ), m_mainWin );
	loadScene->setContext( Qt::ApplicationShortcut );
	connect( loadScene, &QShortcut::activated, this, &WolfApp::LoadSceneClicked );
}

void WolfApp::SetupColorPicker() {
	m_colorDialog = new QColorDialog();
	m_colorDialog->setOption( QColorDialog::ShowAlphaChannel, true );
	m_colorDialog->setWindowModality( Qt::ApplicationModal );
	m_colorDialog->setOption( QColorDialog::DontUseNativeDialog, false );

	m_colorDialog->setWindowTitle( "Select a Color" );
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
	m_renderer->StopRendering();
}

void WolfApp::OnIdleTick() {
	RenderFrame();
}

void WolfApp::SetInitialSceneFileLocation() {
	std::string scenePath{ m_appData->scene.GetRenderScenePath() };
	QString fileAbsPath{ QDir::toNativeSeparators( QDir::cleanPath(
		QDir().absoluteFilePath( QString::fromStdString( scenePath ) ) ) ) };

	m_ui->sceneFileEntry->setText( QDir::toNativeSeparators( fileAbsPath ) );
}

void WolfApp::OnCameraPan( float ndcX, float ndcY ) {
	m_renderer->AddToTargetOffset( ndcX, ndcY );
}

void WolfApp::OnCameraDolly( float offsetZ ) {
	m_renderer->AddToOffsetZ( offsetZ );
	m_ui->zoomSpinR->setValue( m_renderer->dataR.camera.offsetZ );
}

void WolfApp::OnCameraFOV( float offset ) {
	QSignalBlocker blockX( m_ui->FOVSpinR );

	float angleRadians{ DirectX::XMConvertToRadians(
		offset * m_renderer->dataR.camera.FOVSens ) };
	m_renderer->AddToOffsetFOV( angleRadians );
	m_ui->FOVSpinR->setValue(
		m_ui->FOVSpinR->value() + DirectX::XMConvertToDegrees( angleRadians ) );
}

void WolfApp::OnRotateGeometry( float deltaAngleX, float deltaAngleY ) {
	m_renderer->AddToTargetRotation( deltaAngleX, deltaAngleY );
}

void WolfApp::OnPositionChangedRT() {
	QSignalBlocker blockX( m_ui->camPosXSpinRT );
	QSignalBlocker blockY( m_ui->camPosYSpinRT );
	QSignalBlocker blockZ( m_ui->camPosZSpinRT );
	 
	const int zSign = m_renderer->dataRT.GetMatchRTCameraToRaster();

	m_ui->camPosXSpinRT->setValue( m_renderer->dataRT.camera.position.x );
	m_ui->camPosYSpinRT->setValue( m_renderer->dataRT.camera.position.y );
	m_ui->camPosZSpinRT->setValue( -m_renderer->dataRT.camera.position.z * zSign );
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

	m_renderer->ReloadScene( scenePathStd, m_ui->viewport->GetNativeWindowHandle());
	// Update aspect ratio in case the new scene updated it.
	OnResize( m_ui->viewport->width(), m_ui->viewport->height() );

	m_idleTimer->start( 0 );
	m_fpsTimer->start( 1'000 );
}

void WolfApp::MoveSpeedChangedSpin() {
	m_renderer->dataRT.camera.movementSpeed =
		m_ui->moveSpeedSpinRT->value();

	// Block slider signals so it doesn't call MoveSpeedChangedSlider() and
	// round the value. Unblocks in destructor.
	QSignalBlocker blockSlider( m_ui->moveSpeedSliderRT );
	m_ui->moveSpeedSliderRT->setValue( std::round( m_renderer->dataRT.camera.movementSpeed ) );
}

void WolfApp::MoveSpeedChangedSlider() {
	m_renderer->dataRT.camera.movementSpeed = m_ui->moveSpeedSliderRT->value();
	m_ui->moveSpeedSpinRT->setValue( m_renderer->dataRT.camera.movementSpeed );
}

void WolfApp::MoveSpeedMultChanged() {
	m_renderer->dataRT.camera.speedMult = m_ui->moveSpeedMultSpinRT->value();
}

void WolfApp::MouseSensitivityRTChanged() {
	m_renderer->dataRT.camera.mouseSensMultiplier = m_ui->mouseSensitivitySpinRT->value();
}

void WolfApp::VerticalFoVRTChanged() {
	m_renderer->dataRT.camera.setVerticalFOVDeg( m_ui->FOVSpinRT->value() );
}

void WolfApp::CameraPositionChangedRT() {
	const int zSign{ m_renderer->dataRT.GetMatchRTCameraToRaster() };
	m_renderer->dataRT.camera.position.x = m_ui->camPosXSpinRT->value();
	m_renderer->dataRT.camera.position.y = m_ui->camPosYSpinRT->value();
	// Flipping the axis to make the UI more intuitive.
	m_renderer->dataRT.camera.position.z = -m_ui->camPosZSpinRT->value() * zSign;
}

void WolfApp::OnChangeSpeedMult( float offset ) {
	m_ui->moveSpeedMultSpinRT->setValue( m_ui->moveSpeedMultSpinRT->value() + offset );
	m_renderer->dataRT.camera.speedMult = m_ui->moveSpeedMultSpinRT->value();
}

void WolfApp::OnResize( float width, float height ) {
	m_renderer->dataRT.camera.aspectRatio = width / height;

	if ( !m_ui->renderModeSwitch->isChecked() ) {
		m_ui->aspectRatioSpinR->setValue( m_renderer->dataRT.camera.aspectRatio );
	}
}

void WolfApp::ToggleFullscreen() {
	m_ui->menuBar->setVisible( !m_ui->menuBar->isVisible() );
	m_ui->renderModeFrame->setVisible( !m_ui->renderModeFrame->isVisible() );
	m_ui->scrollAreaSettings->setVisible( !m_ui->scrollAreaSettings->isVisible() );
	OnResize( m_ui->viewport->width(), m_ui->viewport->height() );
}

void WolfApp::OutputTargetChanged( QCheckBox* currWidget ) {
	std::vector<QCheckBox*> outputSwitches{
		m_ui->randomColorsSwitchR,
		m_ui->discoModeSwitchR,
		m_ui->shadowOverlaySwitchR,
		m_ui->checkerTextureSwitchR,
		m_ui->gridTextureSwitchR,
		m_ui->showBackfacesSwitchR
	};

	if ( currWidget == m_ui->renderFacesSwitchR ) {
		if ( currWidget->isChecked() ) {
			for ( QCheckBox* widget : outputSwitches ) {
				widget->setEnabled( true );
			}
		} else {
			for ( QCheckBox* widget : outputSwitches ) {
				if ( widget->isChecked() ) {
					widget->setChecked( false );
				}
				widget->setEnabled( false );
			}
		}
		return;
	}

	bool allUnchecked{ true };

	for ( QCheckBox* widget : outputSwitches ) {
		if ( widget->isChecked() )
			allUnchecked = false;
		if ( currWidget->isChecked() && currWidget != widget )
			widget->setChecked( false );
	}

	if ( !allUnchecked && m_ui->faceColorBtnR->isEnabled() )
		m_ui->faceColorBtnR->setEnabled( false );
	else if ( allUnchecked && !m_ui->faceColorBtnR->isEnabled() ) {
		m_ui->faceColorBtnR->setEnabled( true );
		uint32_t newValue = static_cast<uint32_t>(Raster::OutputAlbedoPS::Face);
		m_renderer->dataR.sceneData.outputAlbedo = newValue;
	}
}

void WolfApp::ToggleTextureColorButtonsEnabled() {
	std::vector<QCheckBox*> switches{
		m_ui->gridTextureSwitchR,
		m_ui->checkerTextureSwitchR,
		m_ui->shadowOverlaySwitchR,
		m_ui->discoModeSwitchR
	};

	bool allUnchecked{ true };
	for ( const QCheckBox* const Switch : switches ) {
		if ( Switch->isChecked() ) {
			// Using textureColorARBtn for both colors, as they are synced.
			if ( !m_ui->textureColorABtnR->isEnabled() ) {
				m_ui->textureColorABtnR->setEnabled( true );
				m_ui->textureColorBBtnR->setEnabled( true );
			}
			allUnchecked = false;
		}
	}

	if ( allUnchecked && m_ui->textureColorABtnR->isEnabled() ) {
		m_ui->textureColorABtnR->setEnabled( false );
		m_ui->textureColorBBtnR->setEnabled( false );
	}
}

void WolfApp::ToggleProcTextureParamWidgetsEnabled() {
	std::vector<QCheckBox*> switches{
		m_ui->gridTextureSwitchR,
		m_ui->checkerTextureSwitchR,
	};

	bool allUnchecked{ true };
	for ( const QCheckBox* const Switch : switches ) {
		// Using textureTileRSpin for checking all params as they are synced.
		if ( Switch->isChecked() ) {
			allUnchecked = false;
			if ( !m_ui->textureTileSpinR->isEnabled() ) {
				m_ui->textureTileSpinR->setEnabled( true );
				m_ui->textureProportionsXSpinR->setEnabled( true );
				m_ui->textureProportionsYSpinR->setEnabled( true );
			}
		}
	}

	if ( allUnchecked && m_ui->textureTileSpinR->isEnabled() ) {
		m_ui->textureTileSpinR->setEnabled( false );
		m_ui->textureProportionsXSpinR->setEnabled( false );
		m_ui->textureProportionsYSpinR->setEnabled( false );
	}
}

QString WolfApp::GetButtonStyle( const QColor& color ) {
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

	return btnStyle;
}

void WolfApp::SetupOutputAlbedoColor() {
	uint32_t outputAlbedo;
	if ( m_ui->randomColorsSwitchR->isChecked() )
		outputAlbedo = static_cast<uint32_t>(Raster::OutputAlbedoPS::RandomColors);
	else if ( m_ui->discoModeSwitchR->isChecked() )
		outputAlbedo = static_cast<uint32_t>(Raster::OutputAlbedoPS::DiscoMode);
	else if ( m_ui->shadowOverlaySwitchR->isChecked() )
		outputAlbedo = static_cast<uint32_t>(Raster::OutputAlbedoPS::ShadowOverlayDebug);
	else if ( m_ui->checkerTextureSwitchR->isChecked() )
		outputAlbedo = static_cast<uint32_t>(Raster::OutputAlbedoPS::UVChecker);
	else if ( m_ui->gridTextureSwitchR->isChecked() )
		outputAlbedo = static_cast<uint32_t>(Raster::OutputAlbedoPS::UVGrid);
	else
		outputAlbedo = static_cast<uint32_t>(Raster::OutputAlbedoPS::Face);
	m_renderer->dataR.sceneData.outputAlbedo = outputAlbedo;
}

void WolfApp::SetupColorButtonConnections( DirectX::XMFLOAT4& colorVar, QToolButton* btn ) {
	const QColor currColor{ QColor::fromRgbF( colorVar.x, colorVar.y, colorVar.z, colorVar.w ) };
	m_colorDialog->setCurrentColor( currColor );

	connect( m_colorDialog, &QColorDialog::currentColorChanged, this,
		[this, &colorVar]( const QColor& color ) {
			colorVar = { color.redF(),  color.greenF(), color.blueF(), color.alphaF() };
	} );
	connect( m_colorDialog, &QColorDialog::colorSelected, this,
		[this, btn]( const QColor& color ) {
			QString btnStyle = GetButtonStyle( color );
			btn->setStyleSheet( btnStyle );
	} );
	connect( m_colorDialog, &QColorDialog::rejected, this,
		[this, &currColor, &colorVar]() {
			colorVar = { currColor.redF(), currColor.greenF(),
						 currColor.blueF(), currColor.alphaF() };
	} );

	m_colorDialog->exec();
	m_colorDialog->disconnect(); // removes all signals/slots connections
}

void WolfApp::SetupColorButtonConnectionsPacked( uint32_t& colorVar, QToolButton* btn ) {
	const QColor currColor{ UnpackColor( colorVar ) };
	m_colorDialog->setCurrentColor( currColor );

	connect( m_colorDialog, &QColorDialog::currentColorChanged, this,
		[this, &colorVar]( const QColor& color ) { colorVar = PackColor(color); } );
	connect( m_colorDialog, &QColorDialog::colorSelected, this,
		[this, btn]( const QColor& color ) {
			QString btnStyle = GetButtonStyle( color );
			btn->setStyleSheet( btnStyle );
	} );
	connect( m_colorDialog, &QColorDialog::rejected, this,
		[this, &currColor, &colorVar]() { colorVar = PackColor( currColor );	} );

	m_colorDialog->exec();
	m_colorDialog->disconnect(); // removes all signals/slots connections
}
