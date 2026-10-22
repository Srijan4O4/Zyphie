#include "window.hpp"
#include <QApplication>
int main(int argc, char** argv) {
    QApplication app(argc,argv);
    app.setApplicationName("Zyphie"); app.setOrganizationName("Zyphie");
    Window window; window.show();
    return app.exec();
}
