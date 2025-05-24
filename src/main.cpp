#include <QApplication>
#include <QFile> 
#include <QStyleFactory> 
#include <QThreadPool> // For managing global thread pool
#include <QThread>     // For idealThreadCount
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // It's good practice to set this early, before threads might be implicitly started.
    // Limit concurrent thumbnail threads to reduce choppiness.
    // Using half of ideal thread count, but at least 1, and max around 4-8 for I/O bound tasks.
    int idealThreads = QThread::idealThreadCount();
    int thumbnailThreads = qMax(1, qMin(idealThreads / 2, 4)); 
    QThreadPool::globalInstance()->setMaxThreadCount(thumbnailThreads);
    qDebug() << "Global thread pool max threads set to:" << QThreadPool::globalInstance()->maxThreadCount();


    QApplication app(argc, argv);
    app.setApplicationName("Haigaku Manager");
    app.setOrganizationName("Ketengan Diffusion™"); // Optional, good for QSettings

    // Attempt to set a base style that might blend well with QSS
    // app.setStyle(QStyleFactory::create("Fusion")); // Or "WindowsVista" on Windows

    // Load and apply the custom stylesheet
    QFile styleFile(":/app_style.qss"); // Path from Qt Resource system
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        styleFile.close();
    } else {
        qWarning("Could not load stylesheet ':/app_style.qss'");
    }

    MainWindow w;
    w.setWindowTitle("Haigaku Manager"); // Set initial window title
    app.setWindowIcon(QIcon(":/icons/app_icon.png")); // Set application icon
    // We can add a dummy icon later if needed, or wait for the custom one.
    w.show();

    return app.exec();
}
