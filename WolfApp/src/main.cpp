#include "AppGUI.h"
#include "WolfApp.h"
#include <QtWidgets/QApplication>

int main( int argc, char* argv[] ) {
    QApplication app( argc, argv );
    WolfApp wolfApp{};
    wolfApp.init();

    return app.exec();
}
