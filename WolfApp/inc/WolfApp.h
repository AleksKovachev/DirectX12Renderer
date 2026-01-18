#ifndef WOLF_APP_H
#define WOLF_APP_H

#include "AppGUI.h"
#include "Renderer.hpp"
#include <memory>
#include <QObject>

struct App;

class WolfApp : public QObject {
	Q_OBJECT

public:
	WolfApp();
	~WolfApp();

	/// Prepare the application for rendering.
	bool init( App* appData );

public slots:
	/// Initiate frame rendering.
	void OnIdleTick();

	/// Close the editor properly, wait for the current GPU tasks.
	void OnQuit();

	void OnRenderModeChanged( bool );

private: // Functions
	/// Create the main window for the editor.
	bool InitWindow();

	/// Initiate frame rendering and consume the result.
	void RenderFrame();

	/// Update the rendering stats based on the FPS timer.
	void UpdateRenderStats();

	/// Set the rendering mode both in the renderer and the GUI.
	void SetRenderMode( Core::RenderMode );

	/// Gets all initial values from the UI and sets them in the renderer.
	void SetInitialValues();

	/// Hides the widgets that are irrelevant to the current render mode.
	void HideIrrelevantWidgets();

	void SetupMainWindowSizeAndPosition();

	void ConnectUIEvents();

	void SetupFPSTimers();

	void SetInitialSceneFileLocation();
private: // Members
	Core::WolfRenderer m_renderer; ///< The actual GPU DX12 renderer.
	WolfMainWindow* m_mainWin;     ///< The main window of the application.
	QTimer* m_idleTimer;           ///< Timer for implementing the rendering loop.
	QTimer* m_fpsTimer;            ///< Timer to track the FPS value.
	int m_frameIdxAtLastFPSCalc{}; ///< Updated each second.
	float m_offsetX{};
	float m_offsetY{};
	const Ui::AppGUI* m_ui{ nullptr };

private slots:
	void OnCameraPan( float, float );
	void OnCameraDolly( float );
	void OnCameraFOV( float );
	void OnRotateGeometry( float, float );
	void OnPositionChangedRT();
	void OpenSceneBtnClicked();
	void LoadSceneClicked();
	void MoveSpeedChangedSpin();
	void MoveSpeedChangedSlider();
	void MoveSpeedMultChanged();
	void MouseSensitivityRTChanged();
	void VerticalFoVRTChanged();
	void CameraPositionChangedRT();
	void OnChangeSpeedMult( float );
	void OnResize( float, float );
	void SetAspectRatio();
};


#endif // WOLF_APP_H
