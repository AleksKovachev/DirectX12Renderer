#ifndef WOLF_APP_H
#define WOLF_APP_H

#include "AppGUI.h"
#include "Renderer.hpp"
#include <memory>
#include <QColorDialog>
#include <QObject>

namespace Core {
	struct AppData;
}

struct ColorPickerData {
	QColor color;
	QString style;
};

class WolfApp : public QObject {
	Q_OBJECT

public:
	WolfApp( Core::AppData&, Core::WolfRenderer& );
	~WolfApp();

	/// Prepare the application for rendering.
	bool init();

public slots:
	/// Initiate frame rendering.
	void OnIdleTick();

	/// Close the editor properly, wait for the current GPU tasks.
	void OnQuit();

	/// Stops FPS and Idle timers, changes render mode and starts the timers.
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

	/// Sets the size and position of the main window without the aspect ratio.
	void SetupMainWindowSizeAndPosition();

	/// All signal->slot connections.
	void ConnectUIEvents();

	/// Sets up FPS and Idle timers.
	void SetupFPSTimers();

	/// Fills the scene file entry box with the path to the initial scene.
	void SetInitialSceneFileLocation();

	/// Packs a QColor into a single uint32_t to save GPU memory.
	uint32_t PackColor( const QColor& );

	/// Unpakcs a uint32_t-packed color to a QColor.
	/// @param[in] packedColor  An 8-bit color packedin format AABBGGRR.
	QColor UnpackColor( const uint32_t );

	/// Sets up the initial aspect ratio.
	/// Doing it in SetupMainWindowSizeAndPosition is too early.
	void SetupAspectRatio();

	/// Sets up application shortcuts.
	/// Setting them in actions doesn't work with fullscreen on.
	void SetupShortcuts();

	/// Sets up the initial parameters for the reused color picker widget.
	void SetupColorPicker();

	/// Takes a color and generates a style sheet for a QToolButton with
	/// background color set to it, and a hover color calculated from it.
	QString GetButtonStyle( const QColor& );

	/// Sets up the variable sent to the GPU for the albedo color based on the switches.
	void SetupOutputAlbedoColor();

	/// Sets up bindings for any color button needed for the dynamic color picker to work.
	/// @param[in-out] colorVar  A reference to the variable that stores the color.
	/// @param[in] btn  A pointer to the QToolButton that was clicked to update with new color.
	void SetupColorButtonConnections( DirectX::XMFLOAT4&, QToolButton* );

	/// Sets up bindings for any color button needed for the dynamic color picker to work.
	/// Used with packed uint32_t colors.
	/// @param[in-out] colorVar  A reference to the variable that stores the color.
	/// @param[in] btn  A pointer to the QToolButton that was clicked to update with new color.
	void SetupColorButtonConnectionsPacked( uint32_t&, QToolButton* );

	/// Enagles/Disables switches related to the output albedo target.
	/// @param[in] currWidget  A pointer to the widget that executed this function.
	void OutputTargetChanged( QCheckBox* );

	/// Enables/Disables the color buttons for the procedural textures/
	void ToggleTextureColorButtonsEnabled();

	/// Enables/Disables widgets controlling procedural texture parameters.
	void ToggleProcTextureParamWidgetsEnabled();

private: // Members
	Core::AppData* m_appData;             ///< Application data.
	Core::WolfRenderer* m_renderer;       ///< The actual GPU DX12 renderer.
	WolfMainWindow* m_mainWin{ nullptr }; ///< The main window of the application.
	QTimer* m_idleTimer{ nullptr };       ///< Timer for implementing the rendering loop.
	QTimer* m_fpsTimer{ nullptr };        ///< Timer to track the FPS value.
	int m_frameIdxAtLastFPSCalc{};        ///< Updated each second.
	float m_offsetX{};
	float m_offsetY{};
	const Ui::AppGUI* m_ui{ nullptr };
	QColorDialog* m_colorDialog{ nullptr };

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
	void ToggleFullscreen();
};


#endif // WOLF_APP_H
