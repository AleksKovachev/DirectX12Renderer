#ifndef WOLF_APP_H
#define WOLF_APP_H

#include "AppGUI.h"
#include "Renderer.hpp"
#include <memory>
#include <QObject>

class WolfApp : public QObject {
	Q_OBJECT

public:
	WolfApp();
	~WolfApp();

	/// Prepare the application for rendering.
	bool init();

public slots:
	/// Initiate frame rendering.
	void OnIdleTick();

	/// Close the editor properly, wait for the current GPU tasks.
	void OnQuit();

private: // Functions
	/// Create the main window for the editor.
	bool InitWindow();

	/// Initiate frame rendering and consume the result.
	void RenderFrame();

	/// Update the rendering stats based on the FPS timer.
	void UpdateRenderStats();

private: // Members
	Core::WolfRenderer m_renderer; //!< The actual GPU DX12 renderer.
	WolfMainWindow* m_mainWin;     //!< The main window of the application.
	QTimer* m_idleTimer;           //!< Timer for implementing the rendering loop.
	QTimer* m_fpsTimer;            //!< Timer to track the FPS value.
	int m_frameIdxAtLastFPSCalc{}; //!< Updated each second.
};


#endif // WOLF_APP_H
