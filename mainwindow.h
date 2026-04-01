/******************************************************************************
 * Proyecto: CobolWorks IDE
 * Autor: AnabasaSoft (https://github.com/anabasasoft)
 * Licencia: GNU General Public License v3.0 (GPL-3.0)
 * * Descripción: IDE moderno para COBOL con análisis de flujo, depuración
 * visual e integración con IA para migración de código.
 * * "Domina el código Legacy con herramientas del futuro."
 *****************************************************************************/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <Qsci/qsciscintilla.h>
#include <QTabWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QDockWidget>
#include <QCloseEvent>
#include <QLabel>
#include <QTranslator>
#include <QActionGroup>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void nuevoArchivo();
    void nuevaPlantilla();
    void abrirArchivo();
    void guardarArchivo();
    void guardarComo();
    void cerrarPestanaActual();
    void siguientePestana();    // <--- Nueva: Ctrl+Tab
    void anteriorPestana();     // <--- Nueva: Ctrl+Shift+Tab
    void buscarTexto();
    void sustituirTexto();
    void copiarTexto();
    void cortarTexto();
    void pegarTexto();
    void toggleLineas();
    void toggleCursor();
    void irADefinicion();
    void mostrarMenuContextual(const QPoint &pos);
    void mostrarAyuda();
    void configurarAPI();
    void traducirCodigoIA(const QString &lenguaje);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void compilarEjecutar();
    void depurarCodigo();
    void togglePuntoRuptura(int margin, int line, Qt::KeyboardModifiers state);
    void compilarSolo();
    void abrirArchivoDesdeArbol(const QModelIndex &index);
    void cerrarPestana(int index);
    void marcarArchivoModificado(bool modificado);
    void actualizarStatusBar(int linea, int columna);
    void cambiarIdioma(QAction *accion);
    void actualizarEsquema();
    void actualizarFlujo();
    void saltarAFlujo(class QTreeWidgetItem *item, int column);
    void saltarALineaEsquema(class QListWidgetItem *item);
    void saltarAErrorConsola();

private:
    QTabWidget *tabWidget;
    QTreeView *arbolArchivos;
    QFileSystemModel *modeloArchivos;
    class QTextEdit *consola;
    QLabel *etiquetaPosicion;
    class QListWidget *listaEsquema; // Panel de estructura COBOL
    class QTreeWidget *arbolFlujo; // Panel de flujo de ejecución
    bool lineasVisibles = true;
    bool cursorBloque = false; // Por defecto lo dejamos en barra normal

    // Variables de control de idioma (SOLO UNA VEZ)
    QString rutaIdiomaActual;
    QTranslator *traductorAplicacion;

    void configurarEditor(QsciScintilla *editor);
    QsciScintilla* editorActual();
    QString rutaArchivoActual();
    void crearNuevaPestana(const QString &nombre, const QString &ruta, const QString &contenido);
    void crearMenus();
    void crearInterfaz();
    void cargarListaIdiomas(QMenu *menuIdioma);
    void retraducirInterfaz();
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
