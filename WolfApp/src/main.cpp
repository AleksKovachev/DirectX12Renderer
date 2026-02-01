#include "AppGUI.h"
#include "Renderer.hpp"
#include "WolfApp.h"
#include <QtWidgets/QApplication>

int main( int argc, char* argv[] ) {
    QApplication app( argc, argv );
    Core::AppData appData{};
    Core::WolfRenderer renderer( appData );
    WolfApp wolfApp{ appData, renderer };
    wolfApp.init();

    return app.exec();
}
