/******************************************************************************
 * Proyecto: CobolWorks IDE
 * Autor: AnabasaSoft (https://github.com/anabasasoft)
 * Licencia: GNU General Public License v3.0 (GPL-3.0)
 * * Descripción: IDE moderno para COBOL con análisis de flujo, depuración visual
 * e integración con IA para migración de código.
 * * "Domina el código Legacy con herramientas del futuro."
 *****************************************************************************/

#include "mainwindow.h"
#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QElapsedTimer>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    // 1. Configurar Splash
    QPixmap pixmap(":/AnabasaSoft.png");
    QSplashScreen *splash = new QSplashScreen(pixmap, Qt::WindowStaysOnTopHint);
    splash->show();

    // 2. Espera activa de 2 segundos
    // Esto mantiene el Splash pintándose correctamente sin bloquear el sistema
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 2000) {
        a.processEvents();
    }

    // 3. Crear la ventana (ahora no se mostrará sola porque quitamos el showMaximized)
    MainWindow w;

    // 4. Terminar splash y mostrar ventana maximizada
    splash->finish(&w);
    w.showMaximized();

    return a.exec();
}
