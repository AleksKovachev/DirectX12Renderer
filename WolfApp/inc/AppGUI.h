#pragma once

#include "Renderer.hpp"
#include "ui_AppGUI.h"
#include "ViewportWidget.h"
#include <QtWidgets/QMainWindow>


class WolfMainWindow : public QMainWindow {
	Q_OBJECT

public:
	WolfMainWindow( QWidget* parent = nullptr );
	~WolfMainWindow();

	/// Close the editor properly, wait for the current GPU tasks.
	void closeEvent( QCloseEvent* event ) override;

	const Ui::AppGUI& GetUI() const;

signals:
	void requestQuit();

private:
	Ui::AppGUI m_ui;
};
