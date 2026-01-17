#pragma once

#include "Renderer.hpp"
#include "ui_AppGUI.h"
#include "ViewportWidget.h"
#include <QtWidgets/QMainWindow>


class WolfMainWindow : public QMainWindow {
	Q_OBJECT

public: // Members
	WolfViewportWidget* viewport{ nullptr };

public:
	WolfMainWindow( QWidget* parent = nullptr );
	~WolfMainWindow();

	/// Update the status UI element for FPS with the given value.
	void SetFPS( const int fps );

	/// Change the viewport image with the given one.
	void UpdateViewport( const QImage& image );

	/// Close the editor properly, wait for the current GPU tasks.
	void closeEvent( QCloseEvent* event ) override;

	QCheckBox* GetRenderModeSwitch() const;

	WolfViewportWidget* GetViewport() const;

	void SetRenderMode( Core::RenderMode mode );

	QAction* GetActionExit() const;

	const Ui::AppGUI& GetUI();

signals:
	void requestQuit();

private:
	Ui::AppGUI m_ui;
};
