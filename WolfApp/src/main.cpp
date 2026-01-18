#include "AppGUI.h"
#include "Renderer.hpp"
#include "WolfApp.h"
#include <QtWidgets/QApplication>

int main( int argc, char* argv[] ) {
    QApplication app( argc, argv );
    Core::App appData{};
    WolfApp wolfApp{};
    wolfApp.init( &appData );

    return app.exec();
}
