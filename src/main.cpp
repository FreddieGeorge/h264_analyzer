#include "app/MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QStyleFactory>

#ifndef H264_ANALYZER_VERSION
#define H264_ANALYZER_VERSION "0.1.0"
#endif

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("H264Analyzer"));
    QCoreApplication::setApplicationName(QStringLiteral("H264 Analyzer"));
    QCoreApplication::setApplicationVersion(QStringLiteral(H264_ANALYZER_VERSION));

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    MainWindow window;
    window.show();

    return app.exec();
}
