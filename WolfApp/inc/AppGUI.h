#pragma once

#include "Renderer.hpp"
#include "ui_AppGUI.h"
#include "ViewportWidget.h"
#include <QtWidgets/QMainWindow>

class WolfMainWindow : public QMainWindow {
	Q_OBJECT

public:
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

signals:
	void requestQuit();

private: // Members
	Ui::AppGUI ui;
};
