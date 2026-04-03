/******************************************************************************
 * Proyecto: CobolWorks IDE
 * Autor: AnabasaSoft (https://github.com/anabasasoft)
 * Licencia: GNU General Public License v3.0 (GPL-3.0)
 * * Descripción: IDE moderno para COBOL con análisis de flujo, depuración visual
 * e integración con IA para migración de código.
 * * "Domina el código Legacy con herramientas del futuro."
 *****************************************************************************/

#include "mainwindow.h"
#include "cobollexer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFont>
#include <QKeyEvent>
#include <QFileDialog>
#include <QToolTip>
#include <QHelpEvent>
#include <QMenuBar>
#include <QProcess>
#include <QSplitter>
#include <QInputDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QLineEdit>
#include <QFile>
#include <QTextStream>
#include <QTextEdit>
#include <QTextBrowser>
#include <QSettings>
#include <QTemporaryFile>
#include <QProgressDialog>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QDir>
#include <Qsci/qsciapis.h>
#include <QStatusBar>
#include <QLabel>
#include <QLocale>
#include <QApplication>
#include <QListWidget>
#include <QRegularExpression>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QProcessEnvironment>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Inicializamos el sistema de traducción
    traductorAplicacion = new QTranslator(this);

    // --- NUEVO: Cargar idioma guardado ANTES de crear la interfaz ---
    QSettings settings("AnabasaSoft", "CobolWorks");
    rutaIdiomaActual = settings.value("idioma_interfaz", "").toString();

    if (!rutaIdiomaActual.isEmpty() && traductorAplicacion->load(rutaIdiomaActual)) {
        qApp->installTranslator(traductorAplicacion);
    }

    // CONFIGURACIÓN DEL ICONO
    setWindowIcon(QIcon(":/icon.png"));

    setWindowTitle("CobolWorks");

    crearInterfaz();
    crearMenus();

    // --- NUEVO: Restaurar tamaño de ventana y posición de los paneles ---
    restoreGeometry(settings.value("geometria").toByteArray());
    restoreState(settings.value("estado_paneles").toByteArray());
    // Restaurar estado del cursor (si no existe, usa el true por defecto)
    cursorBloque = settings.value("cursor_bloque", true).toBool();

    // Comprobador silencioso de actualizaciones al arrancar
    managerActualizaciones = new QNetworkAccessManager(this);
    connect(managerActualizaciones, &QNetworkAccessManager::finished, this, &MainWindow::procesarRespuestaActualizacion);
    // Motor del Linter en tiempo real
    timerLinter = new QTimer(this);
    timerLinter->setSingleShot(true);
    timerLinter->setInterval(1000); // Espera 1 segundo de inactividad
    connect(timerLinter, &QTimer::timeout, this, &MainWindow::ejecutarLinter);

    procesoLinter = new QProcess(this);
    connect(procesoLinter, &QProcess::finished, this, &MainWindow::leerSalidaLinter);
    comprobarActualizaciones();

    nuevoArchivo();
}

MainWindow::~MainWindow() {}

void MainWindow::crearInterfaz() {
    // --- Panel Izquierdo: Árbol de archivos ---
    QDockWidget *dock = new QDockWidget(tr("Explorador"), this);
    dock->setObjectName("panelExplorador");
    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dock->setStyleSheet("QDockWidget { color: #E2F1E7; } QDockWidget::title { background: #1E3E62; padding-left: 5px; }");

    arbolArchivos = new QTreeView(dock);
    modeloArchivos = new QFileSystemModel(this);
    modeloArchivos->setRootPath(QDir::homePath());

    arbolArchivos->setModel(modeloArchivos);
    arbolArchivos->setRootIndex(modeloArchivos->index(QDir::homePath()));

    for (int i = 1; i < modeloArchivos->columnCount(); ++i) {
        arbolArchivos->hideColumn(i);
    }
    arbolArchivos->setHeaderHidden(true);
    arbolArchivos->setStyleSheet("QTreeView { background-color: #0B192C; color: #E2F1E7; border: none; }");

    connect(arbolArchivos, &QTreeView::doubleClicked, this, &MainWindow::abrirArchivoDesdeArbol);

    dock->setWidget(arbolArchivos);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    // --- Panel Izquierdo: Mapa de Dependencias ---
    QDockWidget *dockDependencias = new QDockWidget(tr("Dependencias"), this);
    dockDependencias->setObjectName("panelDependencias");
    dockDependencias->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dockDependencias->setStyleSheet("QDockWidget { color: #E2F1E7; } QDockWidget::title { background: #1E3E62; padding-left: 5px; }");

    arbolDependencias = new QTreeWidget(dockDependencias);
    arbolDependencias->setHeaderHidden(true);
    arbolDependencias->setStyleSheet("QTreeWidget { background-color: #0B192C; color: #E2F1E7; border: none; outline: none; } QTreeWidget::item { padding: 4px; } QTreeWidget::item:selected { background-color: #478CCF; color: white; font-weight: bold; }");

    dockDependencias->setWidget(arbolDependencias);
    addDockWidget(Qt::LeftDockWidgetArea, dockDependencias);

    // Apilamos el explorador y las dependencias (se crearán pestañas abajo)
    tabifyDockWidget(dock, dockDependencias);

    connect(arbolDependencias, &QTreeWidget::itemDoubleClicked, this, &MainWindow::abrirDependencia);

    // --- Panel Derecho: Estructura COBOL ---
    QDockWidget *dockEsquema = new QDockWidget(tr("Estructura COBOL"), this);
    dockEsquema->setObjectName("panelEsquema");
    dockEsquema->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dockEsquema->setStyleSheet("QDockWidget { color: #E2F1E7; } QDockWidget::title { background: #1E3E62; padding-left: 5px; }");

    listaEsquema = new QListWidget(dockEsquema);
    listaEsquema->setStyleSheet("QListWidget { background-color: #0B192C; color: #E2F1E7; border: none; outline: none; } QListWidget::item { padding: 4px; } QListWidget::item:selected { background-color: #478CCF; color: white; font-weight: bold; }");

    dockEsquema->setWidget(listaEsquema);
    addDockWidget(Qt::RightDockWidgetArea, dockEsquema);

    // --- Panel Derecho: Flujo COBOL ---
    QDockWidget *dockFlujo = new QDockWidget(tr("Flujo de Ejecución"), this);
    dockFlujo->setObjectName("panelFlujo");
    dockFlujo->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    dockFlujo->setStyleSheet("QDockWidget { color: #E2F1E7; } QDockWidget::title { background: #1E3E62; padding-left: 5px; }");

    arbolFlujo = new QTreeWidget(dockFlujo);
    arbolFlujo->setHeaderHidden(true);
    arbolFlujo->setStyleSheet("QTreeWidget { background-color: #0B192C; color: #E2F1E7; border: none; outline: none; } QTreeWidget::item { padding: 4px; }");

    dockFlujo->setWidget(arbolFlujo);
    addDockWidget(Qt::RightDockWidgetArea, dockFlujo);

    // Conectamos el doble clic en el árbol de flujo para saltar al código
    connect(arbolFlujo, &QTreeWidget::itemDoubleClicked, this, &MainWindow::saltarAFlujo);

    // Apilamos los dos paneles en forma de pestañas para ahorrar espacio
    tabifyDockWidget(dockEsquema, dockFlujo);

    connect(listaEsquema, &QListWidget::itemClicked, this, &MainWindow::saltarALineaEsquema);

    // --- Panel Central: Pestañas y Consola ---
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);

    tabWidget = new QTabWidget(splitter);
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::actualizarEsquema);
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::actualizarDependencias);
    tabWidget->setTabsClosable(true);
    tabWidget->setStyleSheet("QTabBar::tab { background: #1E3E62; color: #E2F1E7; padding: 8px; } QTabBar::tab:selected { background: #0B192C; font-weight: bold; } QTabWidget::pane { border: 1px solid #1E3E62; }");
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::cerrarPestana);

    // Contenedor para agrupar el botón y la consola
    QWidget *contenedorConsola = new QWidget(splitter);
    QVBoxLayout *layoutConsola = new QVBoxLayout(contenedorConsola);
    layoutConsola->setContentsMargins(0, 0, 0, 0);
    layoutConsola->setSpacing(0);

    // Botón de limpiar
    QPushButton *btnLimpiarConsola = new QPushButton(tr("🧹 Limpiar Consola"), contenedorConsola);
    btnLimpiarConsola->setStyleSheet("QPushButton { background-color: #1E3E62; color: #A6E3E9; border: none; padding: 4px 10px; text-align: left; font-weight: bold; } QPushButton:hover { background-color: #2A4D77; }");

    consola = new QTextEdit(contenedorConsola);
    consola->setReadOnly(true);
    consola->setStyleSheet("background-color: #040D1A; color: #A6E3E9; font-family: 'Courier New'; font-size: 11pt; border: none;");

    // Le decimos al panel interno de la consola que nos mande sus eventos (clics, teclas...)
    consola->viewport()->installEventFilter(this);

    // Conectamos el botón directamente a la función de borrado nativa
    connect(btnLimpiarConsola, &QPushButton::clicked, consola, &QTextEdit::clear);

    layoutConsola->addWidget(btnLimpiarConsola);
    layoutConsola->addWidget(consola);

    splitter->addWidget(tabWidget);
    splitter->addWidget(contenedorConsola);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);

    // --- Barra de estado ---
    etiquetaPosicion = new QLabel(tr(" Línea: 1, Col: 1 "));
    etiquetaPosicion->setStyleSheet("color: #E2F1E7; padding-right: 10px;");
    statusBar()->addPermanentWidget(etiquetaPosicion);
    statusBar()->setStyleSheet("background-color: #0B192C; color: #E2F1E7; border-top: 1px solid #1E3E62;");
}

void MainWindow::configurarEditor(QsciScintilla *editor) {
    editor->setUtf8(true);
    QFont fuente("Courier New", 12);
    editor->setFont(fuente);
    editor->setMarginsFont(fuente);

    editor->setMarginWidth(0, "00000");
    editor->setMarginLineNumbers(0, true);

    // --- NUEVO: Margen 1 para Puntos de Ruptura Visuales ---
    editor->setMarginWidth(1, 16); // 16 píxeles de ancho
    editor->setMarginType(1, QsciScintilla::SymbolMargin);
    editor->setMarginSensitivity(1, true); // Permite hacer clic
    editor->setMarginsBackgroundColor(QColor("#0B192C"));

    // Definimos el Marcador 0 como un Círculo Rojo
    editor->markerDefine(QsciScintilla::Circle, 0);
    editor->setMarkerBackgroundColor(QColor("#E06C75"), 0); // Fondo rojo
    editor->setMarkerForegroundColor(QColor("#040D1A"), 0); // Borde oscuro

    editor->setPaper(QColor("#1E3E62"));
    editor->setColor(QColor("#E2F1E7"));
    editor->setMarginsBackgroundColor(QColor("#0B192C"));
    editor->setMarginsForegroundColor(QColor("#478CCF"));

    editor->setIndentationsUseTabs(false);
    editor->setTabWidth(4);

    editor->setFolding(QsciScintilla::BoxedTreeFoldStyle, 2);
    editor->setFoldMarginColors(QColor("#0B192C"), QColor("#0B192C"));

    editor->setCaretLineVisible(true);
    editor->setCaretLineBackgroundColor(QColor("#2A4D77"));

    // --- NUEVO: Indicador 0 para el Linter (Subrayado rojo ondulado) ---
    editor->SendScintilla(QsciScintilla::SCI_INDICSETSTYLE, 0, QsciScintilla::INDIC_SQUIGGLE);
    editor->SendScintilla(QsciScintilla::SCI_INDICSETFORE, 0, QColor("#E06C75"));

    // --- NUEVO: Enseñar a Scintilla a leer COBOL ---
    // Le decimos que el guion '-' es una letra válida para formar palabras
    editor->SendScintilla(QsciScintilla::SCI_SETWORDCHARS, (uintptr_t)0, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-");

    // --- NUEVO: Estilo del cursor (Barra o Bloque) ---
    // Scintilla usa el mensaje SCI_SETCARETSTYLE. 1 = Línea (defecto), 2 = Bloque
    if (cursorBloque) {
        editor->SendScintilla(QsciScintilla::SCI_SETCARETSTYLE, 2);
    } else {
        editor->SendScintilla(QsciScintilla::SCI_SETCARETSTYLE, 1);
    }

    editor->setBraceMatching(QsciScintilla::SloppyBraceMatch);
    editor->setMatchedBraceBackgroundColor(QColor("#98C379"));
    editor->setMatchedBraceForegroundColor(QColor("#040D1A"));
    editor->setUnmatchedBraceBackgroundColor(QColor("#E06C75"));

    if (lineasVisibles) {
        editor->SendScintilla(QsciScintilla::SCI_SETEDGEMODE, 3);
        editor->SendScintilla(QsciScintilla::SCI_MULTIEDGEADDLINE, 7, QColor("#E06C75"));
        editor->SendScintilla(QsciScintilla::SCI_MULTIEDGEADDLINE, 11, QColor("#98C379"));
        editor->SendScintilla(QsciScintilla::SCI_MULTIEDGEADDLINE, 72, QColor("#E06C75"));
    }

    CobolLexer *lexer = new CobolLexer(editor);
    editor->setLexer(lexer);

    QsciAPIs *api = new QsciAPIs(lexer);
    QStringList palabrasClave = {
        "IDENTIFICATION", "ENVIRONMENT", "DATA", "PROCEDURE", "DIVISION", "SECTION",
        "PROGRAM-ID", "WORKING-STORAGE", "LINKAGE", "FILE", "PIC", "PICTURE",
        "DISPLAY", "ACCEPT", "STOP", "RUN", "PERFORM", "COMPUTE", "IF", "ELSE", "END-IF",
        "EXEC", "SQL", "CICS", "END-EXEC"
    };
    for (const QString &palabra : palabrasClave) {
        api->add(palabra);
    }
    api->prepare();

    editor->setAutoCompletionSource(QsciScintilla::AcsAll);
    editor->setAutoCompletionThreshold(2);
    editor->setAutoCompletionCaseSensitivity(false);
    editor->setAutoCompletionReplaceWord(true);
    // Conectamos el clic en el margen a nuestra función
    connect(editor, &QsciScintilla::marginClicked, this, &MainWindow::togglePuntoRuptura);
    editor->installEventFilter(this);
    // Aseguramos que también interceptamos los eventos del ratón en la zona de texto puro
    editor->viewport()->installEventFilter(this);
}

QsciScintilla* MainWindow::editorActual() {
    if (tabWidget->count() > 0) {
        return qobject_cast<QsciScintilla*>(tabWidget->currentWidget());
    }
    return nullptr;
}

QString MainWindow::rutaArchivoActual() {
    if (tabWidget->count() > 0) {
        // Guardamos la ruta oculta en el ToolTip de la pestaña
        return tabWidget->tabToolTip(tabWidget->currentIndex());
    }
    return "";
}

void MainWindow::crearNuevaPestana(const QString &nombre, const QString &ruta, const QString &contenido) {
    QsciScintilla *nuevoEditor = new QsciScintilla(this);
    configurarEditor(nuevoEditor);

    nuevoEditor->setText(contenido);
    nuevoEditor->setModified(false);

    int index = tabWidget->addTab(nuevoEditor, nombre);
    tabWidget->setTabToolTip(index, ruta);
    tabWidget->setCurrentIndex(index);

    connect(nuevoEditor, &QsciScintilla::modificationChanged, this, &MainWindow::marcarArchivoModificado);
    connect(nuevoEditor, &QsciScintilla::cursorPositionChanged, this, &MainWindow::actualizarStatusBar);
    connect(nuevoEditor, &QsciScintilla::textChanged, this, &MainWindow::actualizarEsquema);
    connect(nuevoEditor, &QsciScintilla::textChanged, this, &MainWindow::actualizarFlujo);
    connect(nuevoEditor, &QsciScintilla::textChanged, this, &MainWindow::actualizarDependencias);
    connect(nuevoEditor, &QsciScintilla::textChanged, this, &MainWindow::reiniciarTemporizadorLinter);

    // --- NUEVO: Menú contextual (clic derecho) ---
    nuevoEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(nuevoEditor, &QWidget::customContextMenuRequested, this, &MainWindow::mostrarMenuContextual);

    actualizarEsquema();
    actualizarFlujo();
    actualizarDependencias();
    actualizarAutocompletado();
}

void MainWindow::cerrarPestana(int index) {
    QsciScintilla *editor = qobject_cast<QsciScintilla*>(tabWidget->widget(index));

    // Comprobamos si el motor de texto ha registrado algún cambio
    if (editor && editor->isModified()) {
        tabWidget->setCurrentIndex(index); // Ponemos el foco en esa pestaña para ver qué archivo es

        QMessageBox::StandardButton respuesta;
        respuesta = QMessageBox::warning(this, tr("Guardar cambios"),
                                         tr("El archivo '%1' tiene cambios sin guardar.\n¿Deseas guardarlos?").arg(tabWidget->tabText(index)),
                                         QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (respuesta == QMessageBox::Save) {
            guardarArchivo();
            // Si después de darle a guardar sigue modificado (ej: cancelaste el diálogo "Guardar como"), abortamos el cierre
            if (editor->isModified()) return;
        } else if (respuesta == QMessageBox::Cancel) {
            return; // Abortamos el cierre
        }
    }

    QWidget *widget = tabWidget->widget(index);
    tabWidget->removeTab(index);
    delete widget;
}

void MainWindow::abrirArchivoDesdeArbol(const QModelIndex &index) {
    if (!modeloArchivos->isDir(index)) {
        QString ruta = modeloArchivos->filePath(index);

        // Evitar abrirlo de nuevo si ya está en una pestaña
        for (int i = 0; i < tabWidget->count(); ++i) {
            if (tabWidget->tabToolTip(i) == ruta) {
                tabWidget->setCurrentIndex(i);
                return;
            }
        }

        QFile file(ruta);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            crearNuevaPestana(QFileInfo(ruta).fileName(), ruta, QString::fromUtf8(file.readAll()));
        }
    }
}

void MainWindow::crearMenus() {
    QMenu *menuArchivo = menuBar()->addMenu(tr("&Archivo"));

    QAction *accionNuevo = menuArchivo->addAction(tr("Nuevo"), this, &MainWindow::nuevoArchivo);
    // Usamos el estándar de Qt que automáticamente asigna Ctrl+N
    accionNuevo->setShortcut(QKeySequence::New);
    accionNuevo->setShortcutContext(Qt::ApplicationShortcut); // <-- ESTA ES LA MAGIA

    menuArchivo->addAction(tr("Nuevo desde plantilla"), this, &MainWindow::nuevaPlantilla);
    menuArchivo->addAction(tr("Abrir..."), QKeySequence::Open, this, &MainWindow::abrirArchivo);
    menuArchivo->addAction(tr("Guardar"), QKeySequence::Save, this, &MainWindow::guardarArchivo);
    menuArchivo->addAction(tr("Guardar como..."), QKeySequence::SaveAs, this, &MainWindow::guardarComo);
    menuArchivo->addSeparator();

    // AÑADIDO: setShortcutContext(Qt::ApplicationShortcut) fuerza la prioridad sobre Scintilla
    QAction *accionCerrarTab = menuArchivo->addAction(tr("Cerrar Pestaña"), this, &MainWindow::cerrarPestanaActual);
    accionCerrarTab->setShortcut(QKeySequence("Ctrl+W"));
    accionCerrarTab->setShortcutContext(Qt::ApplicationShortcut);

    QAction *accionSalir = menuArchivo->addAction(tr("Salir"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *menuEdicion = menuBar()->addMenu(tr("&Edición"));
    menuEdicion->addAction(tr("Cortar"), QKeySequence::Cut, this, &MainWindow::cortarTexto);
    menuEdicion->addAction(tr("Copiar"), QKeySequence::Copy, this, &MainWindow::copiarTexto);
    menuEdicion->addAction(tr("Pegar"), QKeySequence::Paste, this, &MainWindow::pegarTexto);
    menuEdicion->addSeparator();
    menuEdicion->addAction(tr("Buscar..."), QKeySequence::Find, this, &MainWindow::buscarTexto);
    menuEdicion->addAction(tr("Sustituir ..."), QKeySequence::Replace, this, &MainWindow::sustituirTexto);
    menuEdicion->addSeparator();
    QAction *accionDefinicion = menuEdicion->addAction(tr("Ir a la definición"), this, &MainWindow::irADefinicion);
    QAction *accionAutocompletar = menuEdicion->addAction(tr("Forzar Autocompletado"), this, [this](){
        if (QsciScintilla *ed = editorActual()) ed->autoCompleteFromAll();
    });
        accionAutocompletar->setShortcut(QKeySequence("Ctrl+Space"));
        addAction(accionAutocompletar);
    accionDefinicion->setShortcut(QKeySequence(Qt::Key_F2));
    addAction(accionDefinicion); // Para que funcione F2 globalmente
    QAction *accionNavegador = menuEdicion->addAction(tr("Navegador de Funciones"), this, &MainWindow::mostrarNavegadorFunciones);
    accionNavegador->setShortcut(QKeySequence("Ctrl+P"));
    addAction(accionNavegador); // Globalizamos el atajo

    QMenu *menuVista = menuBar()->addMenu(tr("&Vista"));
    QAction *accionLineas = menuVista->addAction(tr("Mostrar líneas guía (Col 8 y 12)"), this, &MainWindow::toggleLineas);
    accionLineas->setCheckable(true);
    accionLineas->setChecked(true);
    QAction *accionCursor = menuVista->addAction(tr("Cursor en bloque"), this, &MainWindow::toggleCursor);
    accionCursor->setCheckable(true);
    accionCursor->setChecked(cursorBloque);

    menuVista->addSeparator();
    menuVista->addAction(tr("Acercar (Zoom In)"), QKeySequence::ZoomIn, this, &MainWindow::zoomIn);
    menuVista->addAction(tr("Alejar (Zoom Out)"), QKeySequence::ZoomOut, this, &MainWindow::zoomOut);
    menuVista->addAction(tr("Restaurar Zoom"), QKeySequence("Ctrl+0"), this, &MainWindow::zoomReset);

    // Navegación de pestañas con prioridad forzada
    QAction *accionSigTab = new QAction(this);
    accionSigTab->setShortcut(QKeySequence("Ctrl+Tab"));
    accionSigTab->setShortcutContext(Qt::ApplicationShortcut);
    connect(accionSigTab, &QAction::triggered, this, &MainWindow::siguientePestana);

    QAction *accionAntTab = new QAction(this);
    accionAntTab->setShortcut(QKeySequence("Ctrl+Shift+Tab"));
    accionAntTab->setShortcutContext(Qt::ApplicationShortcut);
    connect(accionAntTab, &QAction::triggered, this, &MainWindow::anteriorPestana);

    // --- MENÚ EMPRESA (DevOps & APIs) ---
    QMenu *menuEmpresa = menuBar()->addMenu(tr("E&mpresa"));
    menuEmpresa->addAction(tr("Git: Comprobar Estado (Status)"), this, &MainWindow::integrarGitStatus);
    menuEmpresa->addAction(tr("Generar Pipeline CI/CD (GitHub Actions)"), this, &MainWindow::generarPipelineCI);
    menuEmpresa->addAction(tr("Generar Cliente API REST (Boilerplate)"), this, &MainWindow::generarBoilerplateAPI);
    menuEmpresa->addSeparator();

    QMenu *menuZOS = menuEmpresa->addMenu(tr("Mainframe / zOS"));
    menuZOS->addAction(tr("Descargar Copybook Remoto..."), this, &MainWindow::sincronizarCopybookRemoto);
    menuZOS->addAction(tr("Generar Job JCL (Simulación)"), this, &MainWindow::generarJCL);

    QMenu *menuConstruir = menuBar()->addMenu(tr("&Construir"));
    QAction *accionCompilarSolo = menuConstruir->addAction(tr("Compilar"), this, &MainWindow::compilarSolo);
    accionCompilarSolo->setShortcut(QKeySequence(Qt::Key_F6));

    QAction *accionCompilarEjecutar = menuConstruir->addAction(tr("Compilar y Ejecutar"), this, &MainWindow::compilarEjecutar);
    accionCompilarEjecutar->setShortcut(QKeySequence(Qt::Key_F5));

    QAction *accionDepurar = menuConstruir->addAction(tr("Depurar con GDB"), this, &MainWindow::depurarCodigo);
    accionDepurar->setShortcut(QKeySequence(Qt::Key_F8));
    addAction(accionDepurar); // Para que funcione F8 globalmente
    QAction *accionDocker = menuConstruir->addAction(tr("Compilar en contenedor Docker"), this, &MainWindow::compilarEnDocker);

    menuConstruir->addSeparator();
    QAction *accionConfigurar = menuConstruir->addAction(tr("Configuración del Compilador..."), this, &MainWindow::configurarCompilador);
    addAction(accionConfigurar);

    // Registro de acciones en la ventana
    addAction(accionNuevo);
    addAction(accionCerrarTab);
    addAction(accionSalir);
    addAction(accionSigTab);
    addAction(accionAntTab);
    addAction(accionCompilarSolo);
    addAction(accionCompilarEjecutar);

    // --- Selector Dinámico de Idiomas ---
    QMenu *menuIdioma = menuBar()->addMenu(tr("&Idioma"));
    cargarListaIdiomas(menuIdioma);

    // --- Menú Inteligencia Artificial ---
    QMenu *menuIA = menuBar()->addMenu(tr("I&A (Gemini)"));
    menuIA->addAction(tr("Configurar API Key..."), this, &MainWindow::configurarAPI);
    menuIA->addSeparator();

    // Usamos funciones lambda [this]() para pasarle el idioma como parámetro al momento de hacer clic
    menuIA->addAction(tr("Traducir COBOL a Python"), this, [this](){ traducirCodigoIA("Python"); });
    menuIA->addAction(tr("Traducir COBOL a Java"), this, [this](){ traducirCodigoIA("Java"); });
    menuIA->addAction(tr("Explicar código COBOL"), this, [this](){ traducirCodigoIA("Explicación"); });

    // --- Menú Ayuda ---
    QMenu *menuAyuda = menuBar()->addMenu(tr("A&yuda"));
    QAction *accionAcercaDe = menuAyuda->addAction(tr("Acerca de CobolWorks"), this, &MainWindow::mostrarAyuda);
    accionAcercaDe->setShortcut(QKeySequence::HelpContents); // Esto suele asignar F1 automáticamente
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    // 1. Detectar doble clic en la consola negra de abajo
    if (consola && watched == consola->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        saltarAErrorConsola();
        return true; // Capturamos el evento para que no haga nada más
    }

    // 2. Detectar tabulador en el editor de código
    QsciScintilla *editor = qobject_cast<QsciScintilla*>(watched);
    if (editor && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        // Permitimos que atajos como Ctrl+W o Ctrl+Tab funcionen
        if (keyEvent->modifiers() & Qt::ControlModifier) {
            return QMainWindow::eventFilter(watched, event);
        }

        if (keyEvent->key() == Qt::Key_Tab && keyEvent->modifiers() == Qt::NoModifier) {
            int linea, columna;
            editor->getCursorPosition(&linea, &columna);

            int espacios_a_insertar = 0;
            if (columna < 7) espacios_a_insertar = 7 - columna;
            else if (columna < 11) espacios_a_insertar = 11 - columna;
            else espacios_a_insertar = 4;

            editor->insert(QString(espacios_a_insertar, ' '));
            editor->setCursorPosition(linea, columna + espacios_a_insertar);
            return true;
        }
    }
    // 3. Detectar Tooltip (Ratón quieto sobre una variable)
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

        // El evento puede venir del editor o de su capa gráfica (viewport)
        QsciScintilla *editorHover = qobject_cast<QsciScintilla*>(watched);
        if (!editorHover) editorHover = qobject_cast<QsciScintilla*>(watched->parent());

        if (editorHover) {
            // SCI_POSITIONFROMPOINTCLOSE devuelve la posición o -1 si el ratón está en un hueco vacío
            int pos = editorHover->SendScintilla(QsciScintilla::SCI_POSITIONFROMPOINTCLOSE, helpEvent->pos().x(), helpEvent->pos().y());
            if (pos != -1) {
                int linea, columna;
                editorHover->lineIndexFromPosition(pos, &linea, &columna);

                // QScintilla nos da la palabra entera automáticamente (con guiones incluidos)
                QString palabra = editorHover->wordAtLineIndex(linea, columna);

                if (!palabra.isEmpty() && palabra.length() > 2) {
                    QString textoDoc = editorHover->text();
                    QStringList lineas = textoDoc.split('\n');

                    // Buscamos si es una variable definida en la Data Division
                    QRegularExpression reVariable("^(0[1-9]|[1-4][0-9]|77|88|FD|SD)\\s+" + QRegularExpression::escape(palabra) + "(?:\\s|\\.|$)");

                    for (const QString &l : lineas) {
                        QString lineaLimpia = l.trimmed();
                        if (lineaLimpia.startsWith("*") || lineaLimpia.startsWith("*>")) continue;

                        if (reVariable.match(lineaLimpia).hasMatch()) {
                            // Encontramos la definición: pintamos el globo emergente
                            QString globo = "<b>" + palabra + "</b><hr>" + lineaLimpia;
                            QToolTip::showText(helpEvent->globalPos(), globo, editorHover);
                            return true;
                        }
                    }
                }
            }
            // Si el ratón está en un sitio vacío o no es variable, ocultamos cualquier globo
            QToolTip::hideText();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::nuevoArchivo() {
    crearNuevaPestana(tr("Sin título"), "", "");
}

void MainWindow::nuevaPlantilla() {
    // 1. Detectamos si la pestaña actual es un "lienzo en blanco" virgen ANTES de cargar la plantilla
    bool cerrarPestanaVacia = false;
    int indiceActual = tabWidget->currentIndex();

    if (QsciScintilla *ed = editorActual()) {
        if (!ed->isModified() && ed->text().trimmed().isEmpty() && tabWidget->tabToolTip(indiceActual).isEmpty()) {
            cerrarPestanaVacia = true;
        }
    }

    QString plantilla = "       IDENTIFICATION DIVISION.\n"
    "       PROGRAM-ID. PlantillaMaestra.\n\n"
    "       ENVIRONMENT DIVISION.\n"
    "       *> Aquí enlazaremos los archivos JCL y VSAM más adelante.\n\n"
    "       DATA DIVISION.\n"
    "       WORKING-STORAGE SECTION.\n"
    "       01  NOMBRE              PIC X(15).\n\n"
    "       PROCEDURE DIVISION.\n"
    "       000-INICIO.\n"
    "           DISPLAY \"=== INICIANDO ENTORNO MAINFRAME ===\".\n"
    "           DISPLAY \"Introduce tu nombre (max 15 chars): \".\n"
    "           ACCEPT NOMBRE.\n"
    "           DISPLAY \"¡Hola \" NOMBRE \"! Todo funciona perfecto.\".\n"
    "           STOP RUN.\n";

    crearNuevaPestana("Plantilla.cbl", "", plantilla);

    // 2. Si veníamos de una pestaña vacía, la fulminamos
    if (cerrarPestanaVacia) {
        QWidget *pestana = tabWidget->widget(indiceActual);
        tabWidget->removeTab(indiceActual);
        pestana->deleteLater();
    }
}

void MainWindow::abrirArchivo() {
    QString ruta = QFileDialog::getOpenFileName(this, tr("Abrir archivo COBOL"), "", tr("Archivos COBOL (*.cob *.cbl *.cpy *.ccp *.sqb);;Todos los archivos (*.*)"));

    if (!ruta.isEmpty()) {
        // 1. Detectamos si la pestaña actual es un "lienzo en blanco" virgen ANTES de abrir el nuevo archivo
        bool cerrarPestanaVacia = false;
        int indiceActual = tabWidget->currentIndex();

        if (QsciScintilla *ed = editorActual()) {
            // Si no está modificado, el texto está vacío y no tiene ruta asignada (tooltip vacío)
            if (!ed->isModified() && ed->text().trimmed().isEmpty() && tabWidget->tabToolTip(indiceActual).isEmpty()) {
                cerrarPestanaVacia = true;
            }
        }

        QFile file(ruta);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            crearNuevaPestana(QFileInfo(ruta).fileName(), ruta, QString::fromUtf8(file.readAll()));

            // 2. Si veníamos de una pestaña vacía, la destruimos para dejar solo el archivo nuevo
            if (cerrarPestanaVacia) {
                QWidget *pestana = tabWidget->widget(indiceActual);
                tabWidget->removeTab(indiceActual);
                pestana->deleteLater();
            }
        }
    }
}

void MainWindow::guardarArchivo() {
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    QString ruta = rutaArchivoActual();
    if (ruta.isEmpty()) {
        guardarComo();
    } else {
        QFile file(ruta);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << editor->text();
            file.close();
            editor->setModified(false); // Le decimos al motor que ya no hay cambios pendientes

            actualizarAutocompletado();
        }
    }
}

void MainWindow::guardarComo() {
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    QString ruta = QFileDialog::getSaveFileName(this, tr("Guardar como..."), "", tr("Archivos COBOL (*.cob *.cbl *.cpy *.ccp *.sqb);;Todos los archivos (*.*)"));
    if (!ruta.isEmpty()) {
        QFile file(ruta);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << editor->text();
            file.close();

            editor->setModified(false); // Le decimos al motor que ya no hay cambios pendientes

            int index = tabWidget->currentIndex();
            tabWidget->setTabText(index, QFileInfo(ruta).fileName());
            tabWidget->setTabToolTip(index, ruta);

            actualizarAutocompletado();
        }
    }
}

void MainWindow::buscarTexto() {
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    bool ok;
    QString palabra = QInputDialog::getText(this, tr("Buscar"), tr("Buscar:"), QLineEdit::Normal, "", &ok);
    if (ok && !palabra.isEmpty()) {
        editor->findFirst(palabra, false, false, false, true);
    }
}

void MainWindow::sustituirTexto() {
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    // Creamos la ventana como NO modal para que puedas editar el código mientras está abierta
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Buscar y Sustituir"));
    dialog->setStyleSheet("background-color: #0B192C; color: #E2F1E7;");
    dialog->setAttribute(Qt::WA_DeleteOnClose); // Se limpia de la memoria automáticamente al cerrarla

    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QFormLayout *form = new QFormLayout();

    QLineEdit *buscarEdit = new QLineEdit(dialog);
    QLineEdit *sustituirEdit = new QLineEdit(dialog);
    buscarEdit->setStyleSheet("background-color: #1E3E62; color: white; border: 1px solid #478CCF; padding: 4px;");
    sustituirEdit->setStyleSheet("background-color: #1E3E62; color: white; border: 1px solid #478CCF; padding: 4px;");

    // Detalle estilo Kate: Si el usuario tiene una palabra seleccionada, la ponemos en la caja automáticamente
    QString seleccion = editor->selectedText();
    if (!seleccion.isEmpty() && !seleccion.contains('\n')) {
        buscarEdit->setText(seleccion);
    }

    form->addRow(tr("Buscar:"), buscarEdit);
    form->addRow(tr("Sustituir con:"), sustituirEdit);
    layout->addLayout(form);

    QHBoxLayout *botonesLayout = new QHBoxLayout();
    QPushButton *btnBuscar = new QPushButton(tr("Buscar siguiente"), dialog);
    QPushButton *btnSustituir = new QPushButton(tr("Sustituir"), dialog);
    QPushButton *btnSustituirTodo = new QPushButton(tr("Sustituir Todo"), dialog);
    QPushButton *btnCerrar = new QPushButton(tr("Cerrar"), dialog);

    QString estiloBotones = "QPushButton { background-color: #478CCF; color: white; border: none; padding: 6px; } QPushButton:hover { background-color: #2A4D77; }";
    btnBuscar->setStyleSheet(estiloBotones);
    btnSustituir->setStyleSheet(estiloBotones);
    btnSustituirTodo->setStyleSheet(estiloBotones);
    btnCerrar->setStyleSheet(estiloBotones);

    botonesLayout->addWidget(btnBuscar);
    botonesLayout->addWidget(btnSustituir);
    botonesLayout->addWidget(btnSustituirTodo);
    botonesLayout->addWidget(btnCerrar);
    layout->addLayout(botonesLayout);

    // --- 1. Lógica: Buscar Siguiente ---
    connect(btnBuscar, &QPushButton::clicked, [this, buscarEdit]() {
        if (QsciScintilla *ed = editorActual()) {
            QString texto = buscarEdit->text();
            if (!texto.isEmpty()) {
                // Parámetros: texto, regex(f), mayús(f), palabra entera(f), dar la vuelta(v), hacia adelante(v)
                ed->findFirst(texto, false, false, false, true, true);
            }
        }
    });

    // --- 2. Lógica: Sustituir (uno a uno) ---
    connect(btnSustituir, &QPushButton::clicked, [this, buscarEdit, sustituirEdit]() {
        if (QsciScintilla *ed = editorActual()) {
            QString buscar = buscarEdit->text();
            QString sustituir = sustituirEdit->text();
            if (!buscar.isEmpty()) {
                // Si la palabra actual seleccionada es justo la que buscamos, la cambiamos
                if (ed->hasSelectedText() && ed->selectedText() == buscar) {
                    ed->replace(sustituir);
                }
                // Y saltamos automáticamente a la siguiente coincidencia para verla
                ed->findFirst(buscar, false, false, false, true, true);
            }
        }
    });

    // --- 3. Lógica: Sustituir Todo ---
    connect(btnSustituirTodo, &QPushButton::clicked, [this, buscarEdit, sustituirEdit]() {
        if (QsciScintilla *ed = editorActual()) {
            QString buscar = buscarEdit->text();
            QString sustituir = sustituirEdit->text();
            if (buscar.isEmpty()) return;

            int contador = 0;

            // Agrupamos todas las sustituciones para que un solo Ctrl+Z las deshaga todas a la vez
            ed->beginUndoAction();

            // Empezamos desde la línea 0, columna 0.
            // IMPORTANTE: Ponemos 'wrap' a false para que no dé la vuelta y no cree un bucle infinito
            bool encontrado = ed->findFirst(buscar, false, false, false, false, true, 0, 0);
            while (encontrado) {
                ed->replace(sustituir);
                contador++;
                // Buscamos la siguiente coincidencia sin dar la vuelta al documento
                encontrado = ed->findFirst(buscar, false, false, false, false, true);
            }

            ed->endUndoAction();
            statusBar()->showMessage(tr("Se han realizado %1 sustituciones.").arg(contador), 4000);

            // Registramos la acción en la consola inferior con colores
            if (contador > 0) {
                consola->append(tr("<font color='#98C379'>Sustitución masiva: Se han reemplazado %1 coincidencias de '%2'.</font>").arg(contador).arg(buscar));
            } else {
                consola->append(tr("<font color='#E5C07B'>Sustitución: No se encontró '%1' en el documento.</font>").arg(buscar));
            }
        }
    });

    connect(btnCerrar, &QPushButton::clicked, dialog, &QDialog::close);

    // Mostramos la ventana flotante sin bloquear el editor
    dialog->show();
}

void MainWindow::toggleLineas() {
    lineasVisibles = !lineasVisibles;
    for (int i = 0; i < tabWidget->count(); ++i) {
        QsciScintilla *editor = qobject_cast<QsciScintilla*>(tabWidget->widget(i));
        if (editor) {
            if (lineasVisibles) {
                editor->SendScintilla(QsciScintilla::SCI_MULTIEDGEADDLINE, 7, QColor("#E06C75"));
                editor->SendScintilla(QsciScintilla::SCI_MULTIEDGEADDLINE, 11, QColor("#98C379"));
                editor->SendScintilla(QsciScintilla::SCI_MULTIEDGEADDLINE, 72, QColor("#E06C75")); // Límite estricto
            } else {
                editor->SendScintilla(QsciScintilla::SCI_MULTIEDGECLEARALL);
            }
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Recorremos todas las pestañas de atrás hacia adelante
    for (int i = tabWidget->count() - 1; i >= 0; --i) {
        tabWidget->setCurrentIndex(i);
        QsciScintilla *editor = qobject_cast<QsciScintilla*>(tabWidget->widget(i));

        if (editor && editor->isModified()) {
            QMessageBox::StandardButton respuesta;
            respuesta = QMessageBox::warning(this, tr("Guardar cambios"),
                                             tr("El archivo '%1' tiene cambios sin guardar.\n¿Deseas guardarlos antes de salir?").arg(tabWidget->tabText(i)),
                                             QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

            if (respuesta == QMessageBox::Save) {
                guardarArchivo();
                if (editor->isModified()) {
                    event->ignore(); // Abortar salida
                    return;
                }
            } else if (respuesta == QMessageBox::Cancel) {
                event->ignore(); // Abortar salida
                return;
            }
        }
    }

    // --- NUEVO: Guardar configuración antes de salir ---
    QSettings settings("AnabasaSoft", "CobolWorks");
    settings.setValue("geometria", saveGeometry());
    settings.setValue("estado_paneles", saveState());
    settings.setValue("idioma_interfaz", rutaIdiomaActual);
    settings.setValue("cursor_bloque", cursorBloque);

    event->accept(); // Todo guardado o descartado, podemos salir
}

void MainWindow::compilarSolo() {
    QString ruta = rutaArchivoActual();
    if (ruta.isEmpty()) {
        consola->append(tr("<font color='#E06C75'>Error: Guarda el archivo antes de compilar (Archivo -> Guardar como...)</font>"));
        return;
    }

    guardarArchivo();
    consola->clear();
    consola->append(tr("<font color='#A6E3E9'>Compilando %1...</font>").arg(QFileInfo(ruta).fileName()));

    QString directorio = QFileInfo(ruta).absolutePath();
    QString nombreBase = QFileInfo(ruta).baseName();
    QString rutaEjecutable = directorio + "/" + nombreBase;

    QProcess compilador;
    compilador.setWorkingDirectory(directorio);

    QSettings settings("AnabasaSoft", "CobolWorks");
    QString customFlags = settings.value("compilador_flags", "").toString();
    QString customEnv = settings.value("compilador_entorno", "").toString();

    // Inyectar entorno
    if (!customEnv.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QStringList variables = customEnv.split(';');
        for (const QString &var : variables) {
            QStringList partes = var.split('=');
            if (partes.length() == 2) {
                env.insert(partes[0].trimmed(), partes[1].trimmed());
            }
        }
        compilador.setProcessEnvironment(env);
    } else {
        compilador.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    }

    // Inyectar argumentos
    QStringList argumentos;
    argumentos << "-x";
    if (!customFlags.isEmpty()) {
        argumentos << customFlags.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    }
    argumentos << ruta << "-o" << rutaEjecutable;

    compilador.start("cobc", argumentos);
    compilador.waitForFinished();

    QString errores = compilador.readAllStandardError();
    if (compilador.exitCode() != 0 || !errores.isEmpty()) {
        consola->append("<font color='#E06C75'><b>" + tr("--- RESULTADOS DE LA COMPILACIÓN ---") + "</b></font>");

        QStringList lineasError = errores.split('\n');
        QRegularExpression re("([^\\s:]+\\.(?:cob|cbl|cpy|ccp|sqb)):(\\d+):");

        for (const QString &linea : lineasError) {
            if (linea.trimmed().isEmpty()) continue;

            QString htmlLinea = linea.toHtmlEscaped();
            QRegularExpressionMatch match = re.match(linea);

            if (match.hasMatch()) {
                QString coincidencia = match.captured(0).toHtmlEscaped();
                // Subrayamos y destacamos en blanco la parte del archivo y la línea para que sea intuitivo hacer clic
                htmlLinea.replace(coincidencia, "<u><b><font color='#E2F1E7'>" + coincidencia + "</font></b></u>");

                // Coloreamos el resto de la línea según sea error o advertencia
                if (htmlLinea.contains("error:", Qt::CaseInsensitive)) {
                    htmlLinea = "<font color='#E06C75'>" + htmlLinea + "</font>";
                } else if (htmlLinea.contains("warning:", Qt::CaseInsensitive)) {
                    htmlLinea = "<font color='#E5C07B'>" + htmlLinea + "</font>";
                }
            }
            consola->append(htmlLinea);
        }
    } else {
        consola->append(tr("<font color='#98C379'>Compilación exitosa. Ejecutable generado correctamente sin lanzarlo.</font>"));
    }
}

void MainWindow::compilarEjecutar() {
    QString ruta = rutaArchivoActual();
    if (ruta.isEmpty()) {
        consola->append(tr("<font color='#E06C75'>Error: Guarda el archivo antes de compilar (Archivo -> Guardar como...)</font>"));
        return;
    }

    guardarArchivo();
    consola->clear();
    consola->append(tr("<font color='#A6E3E9'>Compilando %1...</font>").arg(QFileInfo(ruta).fileName()));

    QString directorio = QFileInfo(ruta).absolutePath();
    QString nombreBase = QFileInfo(ruta).baseName();
    QString rutaEjecutable = directorio + "/" + nombreBase;

    QProcess compilador;
    compilador.setWorkingDirectory(directorio);

    QSettings settings("AnabasaSoft", "CobolWorks");
    QString customFlags = settings.value("compilador_flags", "").toString();
    QString customEnv = settings.value("compilador_entorno", "").toString();

    // Inyectar entorno
    if (!customEnv.isEmpty()) {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        QStringList variables = customEnv.split(';');
        for (const QString &var : variables) {
            QStringList partes = var.split('=');
            if (partes.length() == 2) {
                env.insert(partes[0].trimmed(), partes[1].trimmed());
            }
        }
        compilador.setProcessEnvironment(env);
    } else {
        compilador.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    }

    // Inyectar argumentos
    QStringList argumentos;
    argumentos << "-x";
    if (!customFlags.isEmpty()) {
        argumentos << customFlags.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    }
    argumentos << ruta << "-o" << rutaEjecutable;

    compilador.start("cobc", argumentos);
    compilador.waitForFinished();

    QString errores = compilador.readAllStandardError();
    if (compilador.exitCode() != 0 || !errores.isEmpty()) {
        consola->append("<font color='#E06C75'><b>" + tr("--- RESULTADOS DE LA COMPILACIÓN ---") + "</b></font>");

        QStringList lineasError = errores.split('\n');
        QRegularExpression re("([^\\s:]+\\.(?:cob|cbl|cpy|ccp|sqb)):(\\d+):");

        for (const QString &linea : lineasError) {
            if (linea.trimmed().isEmpty()) continue;

            QString htmlLinea = linea.toHtmlEscaped();
            QRegularExpressionMatch match = re.match(linea);

            if (match.hasMatch()) {
                QString coincidencia = match.captured(0).toHtmlEscaped();
                htmlLinea.replace(coincidencia, "<u><b><font color='#E2F1E7'>" + coincidencia + "</font></b></u>");

                if (htmlLinea.contains("error:", Qt::CaseInsensitive)) {
                    htmlLinea = "<font color='#E06C75'>" + htmlLinea + "</font>";
                } else if (htmlLinea.contains("warning:", Qt::CaseInsensitive)) {
                    htmlLinea = "<font color='#E5C07B'>" + htmlLinea + "</font>";
                }
            }
            consola->append(htmlLinea);
        }
    } else {
        consola->append(tr("<font color='#98C379'>Compilación exitosa. Lanzando programa...</font>"));

        QProcess *terminal = new QProcess(this);
        terminal->setWorkingDirectory(directorio);

        if (QFile::exists("/usr/bin/konsole")) {
            terminal->start("konsole", QStringList() << "--hold" << "-e" << rutaEjecutable);
        } else {
            terminal->start("xterm", QStringList() << "-hold" << "-e" << rutaEjecutable);
        }
    }
}

void MainWindow::cortarTexto() {
    if (QsciScintilla *editor = editorActual()) {
        editor->cut();
    }
}

void MainWindow::copiarTexto() {
    if (QsciScintilla *editor = editorActual()) {
        editor->copy();
    }
}

void MainWindow::pegarTexto() {
    if (QsciScintilla *editor = editorActual()) {
        editor->paste();
    }
}

void MainWindow::marcarArchivoModificado(bool modificado) {
    // Averiguamos qué editor ha emitido la señal
    QsciScintilla *editor = qobject_cast<QsciScintilla*>(sender());
    if (!editor) return;

    int index = tabWidget->indexOf(editor);
    if (index != -1) {
        QString titulo = tabWidget->tabText(index);
        // Si está modificado y no tiene asterisco, se lo ponemos
        if (modificado && !titulo.endsWith("*")) {
            tabWidget->setTabText(index, titulo + "*");
        }
        // Si ya no está modificado (acaba de guardarse) y tiene asterisco, se lo quitamos
        else if (!modificado && titulo.endsWith("*")) {
            tabWidget->setTabText(index, titulo.left(titulo.length() - 1));
        }
    }
}

void MainWindow::actualizarStatusBar(int linea, int columna) {
    // QScintilla cuenta desde 0, así que le sumamos 1 para que sea natural
    etiquetaPosicion->setText(tr(" Línea: %1, Col: %2 ").arg(linea + 1).arg(columna + 1));
}

void MainWindow::zoomIn() {
    if (QsciScintilla *editor = editorActual()) {
        editor->zoomIn();
    }
}

void MainWindow::zoomOut() {
    if (QsciScintilla *editor = editorActual()) {
        editor->zoomOut();
    }
}

void MainWindow::zoomReset() {
    if (QsciScintilla *editor = editorActual()) {
        editor->zoomTo(0); // 0 devuelve la fuente a su tamaño original
    }
}

// --- SISTEMA DINÁMICO DE IDIOMAS ---

void MainWindow::cargarListaIdiomas(QMenu *menuIdioma) {
    QActionGroup *grupoIdiomas = new QActionGroup(this);

    // 1. Opción Español (Nativo)
    QAction *accionEs = menuIdioma->addAction(tr("Español (Nativo)"));
    accionEs->setCheckable(true);
    accionEs->setData("");
    grupoIdiomas->addAction(accionEs);

    // Si no hay ruta guardada, es que estamos en Español
    if (rutaIdiomaActual.isEmpty()) {
        accionEs->setChecked(true);
    }

    // 2. Buscar archivos .qm
    QString rutaDir = QApplication::applicationDirPath() + "/idiomas";
    QDir dir(rutaDir);
    if (dir.exists()) {
        QStringList archivos = dir.entryList(QStringList() << "*.qm", QDir::Files);
        for (const QString &archivo : archivos) {
            QString rutaCompleta = dir.absoluteFilePath(archivo);
            QString codigo = archivo.left(archivo.indexOf('.'));

            QLocale locale(codigo);
            QString nombre = locale.nativeLanguageName();
            if (nombre.isEmpty()) nombre = codigo;
            nombre[0] = nombre[0].toUpper();

            QAction *accion = menuIdioma->addAction(nombre);
            accion->setCheckable(true);
            accion->setData(rutaCompleta);
            grupoIdiomas->addAction(accion);

            // SI ESTA ES LA RUTA QUE TENEMOS CARGADA, LA MARCAMOS
            if (rutaCompleta == rutaIdiomaActual) {
                accion->setChecked(true);
            }
        }
    }
    connect(grupoIdiomas, &QActionGroup::triggered, this, &MainWindow::cambiarIdioma);
}

void MainWindow::cambiarIdioma(QAction *accion) {
    rutaIdiomaActual = accion->data().toString(); // Guardamos la ruta del .qm elegido

    qApp->removeTranslator(traductorAplicacion);

    if (!rutaIdiomaActual.isEmpty()) {
        if (traductorAplicacion->load(rutaIdiomaActual)) {
            qApp->installTranslator(traductorAplicacion);
        }
    }

    retraducirInterfaz();
}

void MainWindow::retraducirInterfaz() {
    // Limpiar barra de menús
    menuBar()->clear();

    // Limpiar acciones de la ventana (evita duplicar atajos de teclado)
    QList<QAction*> accionesVentana = this->actions();
    for (QAction* a : accionesVentana) {
        this->removeAction(a);
    }

    // RECREAR MENÚS (esto llamará a cargarListaIdiomas con el check correcto)
    crearMenus();

    // Traducir pestañas "Sin título"
    for (int i = 0; i < tabWidget->count(); ++i) {
        if (tabWidget->tabToolTip(i).isEmpty()) {
            QString texto = tabWidget->tabText(i);
            bool asterisco = texto.endsWith("*");
            tabWidget->setTabText(i, tr("Sin título") + (asterisco ? "*" : ""));
        }
    }

    // Traducir todos los paneles laterales (Docks) buscando por su nombre interno
    if (QDockWidget *d = findChild<QDockWidget*>("panelExplorador")) d->setWindowTitle(tr("Explorador"));
    if (QDockWidget *d = findChild<QDockWidget*>("panelDependencias")) d->setWindowTitle(tr("Dependencias"));
    if (QDockWidget *d = findChild<QDockWidget*>("panelEsquema")) d->setWindowTitle(tr("Estructura COBOL"));
    if (QDockWidget *d = findChild<QDockWidget*>("panelFlujo")) d->setWindowTitle(tr("Flujo de Ejecución"));

    // Traducir Barra de Estado
    if (QsciScintilla *ed = editorActual()) {
        int l, c;
        ed->getCursorPosition(&l, &c);
        actualizarStatusBar(l, c);
    }
}

void MainWindow::cerrarPestanaActual() {
    int index = tabWidget->currentIndex();
    if (index != -1) {
        cerrarPestana(index);
    }
}

void MainWindow::siguientePestana() {
    int total = tabWidget->count();
    if (total > 1) {
        int siguiente = (tabWidget->currentIndex() + 1) % total;
        tabWidget->setCurrentIndex(siguiente);
    }
}

void MainWindow::anteriorPestana() {
    int total = tabWidget->count();
    if (total > 1) {
        // Sumamos 'total' antes del módulo para evitar números negativos
        int anterior = (tabWidget->currentIndex() - 1 + total) % total;
        tabWidget->setCurrentIndex(anterior);
    }
}

// --- SISTEMA DE ESTRUCTURA COBOL (OUTLINE) ---

void MainWindow::actualizarEsquema() {
    listaEsquema->clear();
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    QString texto = editor->text();
    QStringList lineas = texto.split('\n');

    for (int i = 0; i < lineas.size(); ++i) {
        QString linea = lineas[i].trimmed();

        // Ignoramos líneas vacías o comentarios puros
        if (linea.isEmpty() || linea.startsWith("*") || linea.startsWith("*>")) {
            continue;
        }

        // Regla básica para detectar DIVISION, SECTION o párrafos (líneas que acaban en punto y no tienen espacios)
        bool esDivision = linea.contains("DIVISION");
        bool esSeccion = linea.contains("SECTION");
        bool esParrafo = linea.endsWith(".") && !linea.contains(" ") && linea.length() > 2;
        // NUEVO: Detectar variables (niveles 01-49, 77, 88 o FD/SD)
        QRegularExpression reVariable("^(0[1-9]|[1-4][0-9]|77|88|FD|SD)\\s+([a-zA-Z0-9\\-]+)");
        QRegularExpressionMatch matchVar = reVariable.match(linea);
        bool esVariable = matchVar.hasMatch();

        if (esDivision || esSeccion || esParrafo || esVariable) {
            // Limpiamos el texto para que se vea bonito en la lista
            QString textoMostrar = linea;
            textoMostrar.remove("."); // Quitamos el punto final

            QListWidgetItem *item = new QListWidgetItem(textoMostrar, listaEsquema);
            item->setData(Qt::UserRole, i); // Guardamos el número de línea oculto en el item

            // Le damos formato según el nivel de importancia
            if (esDivision) {
                item->setBackground(QColor("#1E3E62"));
                QFont fuente = item->font();
                fuente.setBold(true);
                item->setFont(fuente);
            } else if (esSeccion) {
                item->setForeground(QColor("#478CCF"));
                item->setText("  " + textoMostrar); // Sangría
            } else if (esParrafo) {
                item->setForeground(QColor("#98C379"));
                item->setText("    " + textoMostrar); // Doble sangría
            } else if (esVariable) {
                item->setForeground(QColor("#D19A66")); // Naranja
                item->setText("      " + matchVar.captured(1) + " " + matchVar.captured(2)); // Triple sangría
            }
        }
    }
}

void MainWindow::saltarALineaEsquema(QListWidgetItem *item) {
    QsciScintilla *editor = editorActual();
    if (editor && item) {
        int linea = item->data(Qt::UserRole).toInt();
        editor->setCursorPosition(linea, 0);      // Movemos el cursor
        editor->ensureLineVisible(linea);         // Hacemos scroll hasta allí
        editor->setFocus();                       // Le devolvemos el foco al editor
    }
}

void MainWindow::toggleCursor() {
    cursorBloque = !cursorBloque; // Invertimos el estado

    // Recorremos todas las pestañas abiertas para aplicar el cambio al instante
    for (int i = 0; i < tabWidget->count(); ++i) {
        QsciScintilla *editor = qobject_cast<QsciScintilla*>(tabWidget->widget(i));
        if (editor) {
            if (cursorBloque) {
                editor->SendScintilla(QsciScintilla::SCI_SETCARETSTYLE, 2); // Bloque
            } else {
                editor->SendScintilla(QsciScintilla::SCI_SETCARETSTYLE, 1); // Línea
            }
        }
    }
}

void MainWindow::saltarAErrorConsola() {
    // Pillamos el cursor interno de la consola para saber en qué línea hizo clic
    QTextCursor cursor = consola->textCursor();
    cursor.select(QTextCursor::LineUnderCursor);
    QString lineaError = cursor.selectedText();

    // El compilador cobc siempre dice: "archivo.cbl:14: error: lo que sea"
    // Buscamos ese patrón exacto con una expresión regular
    QRegularExpression re("([^\\s:]+\\.(?:cob|cbl|cpy|ccp|sqb)):(\\d+):");
    QRegularExpressionMatch match = re.match(lineaError);

    if (match.hasMatch()) {
        // Sacamos el nombre del archivo (ignorando rutas largas) y la línea
        QString nombreArchivo = QFileInfo(match.captured(1)).fileName();
        int numeroLinea = match.captured(2).toInt() - 1; // Scintilla cuenta desde 0

        // Buscamos si tenemos ese archivo abierto en alguna pestaña
        for (int i = 0; i < tabWidget->count(); ++i) {
            QString tituloPestana = tabWidget->tabText(i).remove("*"); // Quitamos asterisco de modificado
            if (tituloPestana == nombreArchivo) {
                // Saltamos a la pestaña
                tabWidget->setCurrentIndex(i);
                QsciScintilla *editor = qobject_cast<QsciScintilla*>(tabWidget->widget(i));
                if (editor) {
                    // Movemos el cursor y la vista
                    editor->setCursorPosition(numeroLinea, 0);
                    editor->ensureLineVisible(numeroLinea);
                    editor->setFocus();

                    // Seleccionamos la línea entera para que destaque bien el error
                    int longitudLinea = editor->lineLength(numeroLinea);
                    editor->setSelection(numeroLinea, 0, numeroLinea, longitudLinea);
                }
                break;
            }
        }
    }
}

void MainWindow::mostrarMenuContextual(const QPoint &pos) {
    QsciScintilla *editor = qobject_cast<QsciScintilla*>(sender());
    if (!editor) return;

    QMenu menu(this);
    menu.setStyleSheet("QMenu { background-color: #1E3E62; color: #E2F1E7; border: 1px solid #0B192C; } QMenu::item:selected { background-color: #478CCF; }");

    menu.addAction(tr("Ir a la definición (F2)"), this, &MainWindow::irADefinicion);
    menu.addSeparator();
    menu.addAction(tr("Copiar"), editor, &QsciScintilla::copy);
    menu.addAction(tr("Pegar"), editor, &QsciScintilla::paste);

    // Muestra el menú en la posición exacta de la pantalla
    menu.exec(editor->mapToGlobal(pos));
}

void MainWindow::irADefinicion() {
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    int linea, columna;
    editor->getCursorPosition(&linea, &columna);

    // --- NUEVO: INTERCEPTAR INSTRUCCIONES COPY ---
    QString lineaActualStr = editor->text(linea).trimmed();
    if (lineaActualStr.startsWith("COPY", Qt::CaseInsensitive) || lineaActualStr.contains(" COPY ", Qt::CaseInsensitive)) {
        // Buscamos el nombre del copybook, ignorando comillas
        QRegularExpression reCopy("COPY\\s+[\"']?([^\"'\\.\\s]+)(?:\\.cpy|\\.cob|\\.cbl)?[\"']?", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch matchCopy = reCopy.match(lineaActualStr);

        if (matchCopy.hasMatch()) {
            QString nombreCopy = matchCopy.captured(1);
            QString dir = QFileInfo(rutaArchivoActual()).absolutePath();

            // Probamos varias extensiones típicas de COBOL por si el código omite el .cpy
            QStringList extensiones = {".cpy", ".cob", ".cbl", ".ccp", ".sqb", ""};
            for (const QString &ext : extensiones) {
                QString rutaCopy = dir + "/" + nombreCopy + ext;
                if (QFile::exists(rutaCopy)) {
                    // Evitamos abrirlo doble si ya está en una pestaña
                    for (int i = 0; i < tabWidget->count(); ++i) {
                        if (tabWidget->tabToolTip(i) == rutaCopy) {
                            tabWidget->setCurrentIndex(i);
                            return;
                        }
                    }
                    // Lo abrimos
                    QFile file(rutaCopy);
                    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        crearNuevaPestana(QFileInfo(rutaCopy).fileName(), rutaCopy, QString::fromUtf8(file.readAll()));
                        return;
                    }
                }
            }
            statusBar()->showMessage(tr("No se encontró el archivo COPYBOOK: %1").arg(nombreCopy), 4000);
            return;
        }
    }

    // 1. Obtener la palabra bajo el cursor
    int posActual = editor->SendScintilla(QsciScintilla::SCI_GETCURRENTPOS);
    int startPos = editor->SendScintilla(QsciScintilla::SCI_WORDSTARTPOSITION, posActual, true);
    int endPos = editor->SendScintilla(QsciScintilla::SCI_WORDENDPOSITION, posActual, true);

    editor->SendScintilla(QsciScintilla::SCI_SETSELECTIONSTART, startPos);
    editor->SendScintilla(QsciScintilla::SCI_SETSELECTIONEND, endPos);
    QString palabra = editor->selectedText().trimmed();

    if (palabra.isEmpty()) {
        editor->setCursorPosition(linea, columna);
        return;
    }

    // 2. Buscar la palabra en el documento
    QString texto = editor->text();
    QStringList lineas = texto.split('\n');

    for (int i = 0; i < lineas.size(); ++i) {
        QString l = lineas[i].trimmed();
        if (l.startsWith("*") || l.startsWith("*>")) continue;

        if (l.contains(palabra)) {
            QRegularExpression reVariable("^(0[1-9]|[1-4][0-9]|77|88|FD|SD)\\s+" + QRegularExpression::escape(palabra) + "\\b");
            QRegularExpression reParrafo("^" + QRegularExpression::escape(palabra) + "\\s*\\.");

            if (reVariable.match(l).hasMatch() || reParrafo.match(l).hasMatch()) {
                editor->setCursorPosition(i, 0);
                editor->ensureLineVisible(i);

                int longitud = editor->lineLength(i);
                editor->setSelection(i, 0, i, longitud);
                return;
            }
        }
    }

    editor->setCursorPosition(linea, columna);
    statusBar()->showMessage(tr("No se encontró la definición de '%1'").arg(palabra), 4000);
}

void MainWindow::mostrarAyuda() {
    // Creamos la ventana emergente principal
    QDialog *dialogo = new QDialog(this);
    dialogo->setWindowTitle(tr("Ayuda de CobolWorks"));
    dialogo->resize(750, 550);
    dialogo->setStyleSheet("background-color: #0B192C; color: #E2F1E7;");

    QVBoxLayout *layout = new QVBoxLayout(dialogo);

    // 1. Añadimos el Logo centrado en la parte superior
    QLabel *labelLogo = new QLabel(dialogo);
    QPixmap pixmap(":/logo.png");
    labelLogo->setPixmap(pixmap.scaledToWidth(350, Qt::SmoothTransformation));
    labelLogo->setAlignment(Qt::AlignCenter);
    layout->addWidget(labelLogo);

    // 2. Creamos el sistema de pestañas
    QTabWidget *pestanas = new QTabWidget(dialogo);
    pestanas->setStyleSheet(
        "QTabBar::tab { background: #1E3E62; color: #E2F1E7; padding: 8px 20px; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #478CCF; font-weight: bold; }"
        "QTabWidget::pane { border: 1px solid #478CCF; background-color: #1E3E62; }"
    );

    // --- PESTAÑA 1: ATAJOS DE TECLADO ---
    QTextBrowser *tabAtajos = new QTextBrowser(pestanas);
    tabAtajos->setStyleSheet("background-color: transparent; color: #E2F1E7; font-size: 11pt; border: none; padding: 15px;");
    tabAtajos->setHtml(tr(
        "<h2>Edición y Navegación</h2>"
        "<ul>"
        "<li><b>Ctrl + P:</b> Navegador de Funciones. Filtra y salta rápido a cualquier párrafo o variable del código.</li>"
        "<li><b>F2:</b> Ir a la definición. Pon el cursor sobre una variable, párrafo o COPYBOOK y pulsa F2 para saltar a su origen.</li>"
        "<li><b>Ctrl + Espacio:</b> Fuerza el menú de autocompletado (incluye variables de COPYBOOKs externos).</li>"
        "<li><b>Tabulador:</b> Si estás al principio de la línea, salta automáticamente a la Columna 7 o Columna 11 (Área A y B).</li>"
        "<li><b>Ctrl + F:</b> Buscar texto en el archivo actual.</li>"
        "<li><b>Ctrl + H / Ctrl + R:</b> Abrir panel avanzado de Buscar y Sustituir.</li>"
        "</ul>"
        "<h2>Archivos y Vista</h2>"
        "<ul>"
        "<li><b>Ctrl + N / O / S:</b> Nuevo, Abrir y Guardar archivo.</li>"
        "<li><b>Ctrl + W:</b> Cerrar la pestaña actual.</li>"
        "<li><b>Ctrl + Tab:</b> Cambiar a la siguiente pestaña abierta.</li>"
        "<li><b>Ctrl + Shift + Tab:</b> Cambiar a la pestaña anterior.</li>"
        "<li><b>Ctrl + 0:</b> Restaurar el tamaño de la fuente por defecto.</li>"
        "</ul>"
        "<h2>Construcción y Depuración</h2>"
        "<ul>"
        "<li><b>F5:</b> Compilar y ejecutar el programa en una terminal externa.</li>"
        "<li><b>F6:</b> Compilar solo para comprobar errores en la consola inferior.</li>"
        "<li><b>F8:</b> Lanzar el depurador visual (GDB/LLDB) respetando tus puntos de ruptura.</li>"
        "</ul>"
    ));

    // --- PESTAÑA 2: CARACTERÍSTICAS DEL IDE ---
    QTextBrowser *tabCaracteristicas = new QTextBrowser(pestanas);
    tabCaracteristicas->setStyleSheet("background-color: transparent; color: #E2F1E7; font-size: 11pt; border: none; padding: 15px;");
    tabCaracteristicas->setHtml(tr(
        "<h2>Potenciando el código Legacy</h2>"
        "<ul>"
        "<li><b>Linter silencioso:</b> Deja de escribir 1 segundo y CobolWorks subrayará en rojo los errores de sintaxis en tiempo real.</li>"
        "<li><b>Inspección de variables (Hover):</b> Deja el ratón quieto sobre una variable para ver su PIC y línea de definición en un globo flotante.</li>"
        "<li><b>Terminal Click-to-Error:</b> Haz doble clic sobre cualquier mensaje de error rojo en la consola inferior para saltar exactamente a esa línea en el editor.</li>"
        "<li><b>Paneles Dinámicos:</b> "
        "   <ul>"
        "   <li><b>Estructura COBOL:</b> Lista jerárquica de tus Divisiones, Secciones, Párrafos y Variables.</li>"
        "   <li><b>Flujo de Ejecución:</b> Árbol visual de tus saltos PERFORM y GO TO.</li>"
        "   <li><b>Dependencias:</b> Archivos COPYBOOK y programas externos (CALL) detectados.</li>"
        "   </ul>"
        "</li>"
        "<li><b>Formateo estricto:</b> Líneas de guía visuales para respetar las reglas del estándar COBOL.</li>"
        "<li><b>Puntos de ruptura (Breakpoints):</b> Haz clic en el margen izquierdo de cualquier línea para añadir un punto de interrupción antes de pulsar F8.</li>"
        "</ul>"
    ));

    // --- PESTAÑA 3: I.A. Y EMPRESA ---
    QTextBrowser *tabEnterprise = new QTextBrowser(pestanas);
    tabEnterprise->setStyleSheet("background-color: transparent; color: #E2F1E7; font-size: 11pt; border: none; padding: 15px;");
    tabEnterprise->setHtml(tr(
        "<h2>Inteligencia Artificial (Gemini)</h2>"
        "<p>Selecciona un fragmento de código (o nada, para el archivo entero) y usa el menú <b>I.A.</b>:</p>"
        "<ul>"
        "<li><b>Traducción:</b> Convierte código COBOL a Python o Java modernos de forma automática.</li>"
        "<li><b>Explicación:</b> Pídele a la IA que desglose la lógica compleja de un párrafo en lenguaje natural.</li>"
        "</ul>"
        "<h2>Integraciones Enterprise (Menú Empresa)</h2>"
        "<ul>"
        "<li><b>Git (Status):</b> Comprueba si tienes archivos modificados sin salir del editor.</li>"
        "<li><b>Docker:</b> Compila en un contenedor oficial (ghcr.io/gnu-cobol) garantizando compatibilidad total en producción.</li>"
        "<li><b>CI/CD:</b> Genera en un clic el YAML de GitHub Actions para probar el código en la nube en cada commit.</li>"
        "<li><b>Boilerplate APIs:</b> Genera plantillas base para consumir APIs REST utilizando llamadas al sistema.</li>"
        "</ul>"
        "<h2>Mainframe y zOS</h2>"
        "<ul>"
        "<li><b>Soporte DB2 y CICS:</b> El editor reconoce nativamente las sentencias <code>EXEC SQL</code> y <code>EXEC CICS</code>, aplicando el resaltado de sintaxis correcto.</li>"
        "<li><b>Copybooks Remotos:</b> Descarga archivos .cpy directamente desde una URL (ej. repositorio de GitHub o servidor interno). El IDE los guarda en caché local y configura el compilador automáticamente para usarlos al instante sin rutas absolutas en tu código.</li>"
        "<li><b>Simulador JCL y z/OS:</b> Genera un script de Job Control Language (JCL) de IBM y ajusta en segundo plano GnuCOBOL al estándar estricto (<code>-std=ibm-strict</code>) para emular el comportamiento exacto del Mainframe.</li>"
        "</ul>"
    ));

    // --- PESTAÑA 4: ACERCA DE ---
    QTextBrowser *tabAcerca = new QTextBrowser(pestanas);
    tabAcerca->setStyleSheet("background-color: transparent; color: #E2F1E7; font-size: 11pt; border: none; padding: 15px;");
    tabAcerca->setOpenExternalLinks(true);
    tabAcerca->setHtml(tr(
        "<h2>CobolWorks</h2>"
        "<p><b>El IDE moderno para el programador de siempre.</b></p>"
        "<p>Desarrollado para romper la barrera entre el mundo Mainframe y las filosofías ágiles de desarrollo actual. Diseñado con cariño para entornos Linux.</p>"
        "<br>"
        "<h2>Contacto y Soporte</h2>"
        "<ul>"
        "<li><b>Email:</b> <a href=\"mailto:anabasasoft@gmail.com\" style=\"color: #A6E3E9;\">anabasasoft@gmail.com</a></li>"
        "<li><b>Web:</b> <a href=\"https://anabasasoft.github.io\" style=\"color: #A6E3E9;\">anabasasoft.github.io</a></li>"
        "<li><b>GitHub:</b> <a href=\"https://github.com/anabasasoft\" style=\"color: #A6E3E9;\">github.com/anabasasoft</a></li>"
        "</ul>"
        "<hr>"
        "<p><i>Desarrollado con Qt6 y C++ en Manjaro Linux. &copy; AnabasaSoft</i></p>"
    ));

    // Ensamblamos las pestañas
    pestanas->addTab(tabAtajos, tr("Teclado"));
    pestanas->addTab(tabCaracteristicas, tr("Características"));
    pestanas->addTab(tabEnterprise, tr("IA & Empresa"));
    pestanas->addTab(tabAcerca, tr("Acerca de"));

    layout->addWidget(pestanas);

    // Mostramos la ventana
    dialogo->exec();
}

void MainWindow::actualizarFlujo() {
    arbolFlujo->clear();
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    QString texto = editor->text();
    QStringList lineas = texto.split('\n');

    bool enProcedure = false;
    QTreeWidgetItem *nodoPadreActual = nullptr;

    QRegularExpression reParrafo("^\\s*([a-zA-Z0-9\\-]+)\\s*\\.");
    QRegularExpression rePerform("\\bPERFORM\\s+([a-zA-Z0-9\\-]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reGoto("\\bGO\\s+TO\\s+([a-zA-Z0-9\\-]+)", QRegularExpression::CaseInsensitiveOption);

    for (int i = 0; i < lineas.size(); ++i) {
        QString linea = lineas[i];
        QString lineaTrim = linea.trimmed();

        if (lineaTrim.startsWith("*") || lineaTrim.startsWith("*>")) continue;

        if (lineaTrim.contains("PROCEDURE DIVISION", Qt::CaseInsensitive)) {
            enProcedure = true;
            continue;
        }

        if (!enProcedure) continue;

        if (lineaTrim.contains("DIVISION") || lineaTrim.contains("SECTION")) continue;

        // 1. Detectar Párrafo
        QRegularExpressionMatch matchParrafo = reParrafo.match(linea);
        if (matchParrafo.hasMatch() && !lineaTrim.contains(" ")) {
            QString nombreParrafo = matchParrafo.captured(1);

            nodoPadreActual = new QTreeWidgetItem(arbolFlujo);
            nodoPadreActual->setText(0, nombreParrafo);
            nodoPadreActual->setForeground(0, QColor("#E5C07B"));

            // INCORPORAMOS EL NÚMERO DE LÍNEA OCULTO
            nodoPadreActual->setData(0, Qt::UserRole, i);

            QFont fuente = nodoPadreActual->font(0);
            fuente.setBold(true);
            nodoPadreActual->setFont(0, fuente);
            continue;
        }

        // 2. Detectar llamadas dentro del párrafo actual
        if (nodoPadreActual) {
            QRegularExpressionMatch matchPerform = rePerform.match(linea);
            if (matchPerform.hasMatch()) {
                QTreeWidgetItem *hijo = new QTreeWidgetItem(nodoPadreActual);
                hijo->setText(0, " ↳ PERFORM " + matchPerform.captured(1));
                hijo->setForeground(0, QColor("#98C379"));

                // INCORPORAMOS EL NÚMERO DE LÍNEA OCULTO
                hijo->setData(0, Qt::UserRole, i);
            }

            QRegularExpressionMatch matchGoto = reGoto.match(linea);
            if (matchGoto.hasMatch()) {
                QTreeWidgetItem *hijo = new QTreeWidgetItem(nodoPadreActual);
                hijo->setText(0, " ⚡ GO TO " + matchGoto.captured(1));
                hijo->setForeground(0, QColor("#E06C75"));

                // INCORPORAMOS EL NÚMERO DE LÍNEA OCULTO
                hijo->setData(0, Qt::UserRole, i);
            }
        }
    }

    arbolFlujo->expandAll();
}

void MainWindow::saltarAFlujo(QTreeWidgetItem *item, int column) {
    QsciScintilla *editor = editorActual();
    if (editor && item) {
        // Rescatamos el número de línea que guardamos antes de forma oculta
        int linea = item->data(0, Qt::UserRole).toInt();

        editor->setCursorPosition(linea, 0);      // Movemos el cursor
        editor->ensureLineVisible(linea);         // Hacemos scroll
        editor->setFocus();                       // Le damos el control al editor

        // Seleccionamos la línea entera para que se vea claro a dónde hemos saltado
        int longitud = editor->lineLength(linea);
        editor->setSelection(linea, 0, linea, longitud);
    }
}

void MainWindow::configurarAPI() {
    // QSettings guarda esto en ~/.config/AnabasaSoft/CobolWorks.conf en Manjaro
    QSettings settings("AnabasaSoft", "CobolWorks");
    QString apiKeyActual = settings.value("gemini_api_key", "").toString();

    bool ok;
    QString nuevaKey = QInputDialog::getText(this, tr("Configurar API de IA"),
                                             tr("Introduce tu API Key de Google Gemini:\n(Se guardará de forma segura y local)"),
                                             QLineEdit::Password, apiKeyActual, &ok);
    if (ok) {
        settings.setValue("gemini_api_key", nuevaKey);
        QMessageBox::information(this, tr("Configuración Guardada"), tr("La API Key se ha guardado correctamente."));
    }
}

void MainWindow::traducirCodigoIA(const QString &lenguaje) {
    QSettings settings("AnabasaSoft", "CobolWorks");
    QString apiKey = settings.value("gemini_api_key", "").toString();

    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, tr("Falta API Key"), tr("Por favor, configura tu API Key en el menú 'IA (Gemini)' antes de usar el traductor."));
        return;
    }

    QsciScintilla *editor = editorActual();
    if (!editor) return;

    // MAGIA: Si el usuario tiene texto seleccionado, traducimos solo eso. Si no, todo el archivo.
    QString textoATraducir = editor->selectedText();
    if (textoATraducir.isEmpty()) {
        textoATraducir = editor->text();
    }

    if (textoATraducir.trimmed().isEmpty()) return;

    // Pasamos el texto a un archivo temporal para que Bash/Python no se atraganten con los saltos de línea
    QTemporaryFile tempIn;
    if (!tempIn.open()) return;
    QTextStream out(&tempIn);
    out << textoATraducir;
    tempIn.close();

    QTemporaryFile tempOut;
    if (!tempOut.open()) return;
    tempOut.close(); // Lo dejamos vacío, Python escribirá aquí el resultado

    consola->append(tr("<font color='#A6E3E9'>Conectando con Gemini para traducir a %1... Por favor, espera.</font>").arg(lenguaje));

    // Lanzamos el script de Python en segundo plano
    QProcess procesoIA;
    QStringList argumentos;
    argumentos << "ia_traductor.py" << apiKey << lenguaje << tempIn.fileName() << tempOut.fileName();

    procesoIA.start("python", argumentos);

    // --- NUEVO: Ventana de progreso modal que bloquea la interfaz ---
    QProgressDialog progreso(tr("Traduciendo código a %1 con IA...").arg(lenguaje), tr("Cancelar"), 0, 0, this);
    progreso.setWindowTitle(tr("Procesando"));
    progreso.setWindowModality(Qt::WindowModal); // Esto es lo que bloquea los clics en las pestañas
    progreso.setStyleSheet("background-color: #1E3E62; color: #E2F1E7;");
    progreso.setMinimumDuration(0); // Mostrar inmediatamente
    progreso.show();

    // Bucle de espera activa: Mantiene la interfaz "viva" pero bloqueada
    while (!procesoIA.waitForFinished(100)) {
        QApplication::processEvents(); // Procesa el dibujo de la barra de progreso

        if (progreso.wasCanceled()) {
            procesoIA.kill(); // Matamos el script de Python si el usuario cancela
            consola->append(tr("<font color='#E5C07B'>Traducción cancelada por el usuario.</font>"));
            return; // Salimos sin hacer nada más
        }
    }

    if (procesoIA.exitCode() == 0) {
        QFile fileOut(tempOut.fileName());
        if (fileOut.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString resultado = QString::fromUtf8(fileOut.readAll());

            // 1. Creamos la pestaña COMPLETAMENTE VACÍA para fijar el punto de guardado en blanco
            QString tituloPestana = (lenguaje == "Explicación") ? tr("Explicación IA") : tr("Migración %1").arg(lenguaje);
            crearNuevaPestana(tituloPestana, "", "");
            // Activamos el ajuste de línea (Word Wrap) para leer la explicación como un documento
            if (lenguaje == "Explicación" && editorActual()) {
                editorActual()->setWrapMode(QsciScintilla::WrapWord);
            }

            // 2. Ahora "escribimos" el resultado. Scintilla lo detectará como un cambio brutal del usuario
            if (QsciScintilla *nuevoEditor = editorActual()) {
                nuevoEditor->append(resultado);
                // Opcional: movemos el cursor al principio por si el append lo deja abajo del todo
                nuevoEditor->setCursorPosition(0, 0);
            }

            consola->append(tr("<font color='#98C379'>Traducción a %1 completada con éxito.</font>").arg(lenguaje));
        }
    } else {
        QString errorIA = procesoIA.readAllStandardError();
        consola->append(tr("<font color='#E06C75'>Error de conexión con la IA: %1</font>").arg(errorIA));
    }
}

void MainWindow::togglePuntoRuptura(int margin, int line, Qt::KeyboardModifiers state) {
    if (margin == 1) { // Solo actuamos si el clic fue en el margen de símbolos (margen 1)
        QsciScintilla *editor = qobject_cast<QsciScintilla*>(sender());
        if (!editor) return;

        // Comprobamos si la línea ya tiene el Marcador 0 (el punto rojo)
        int marcadores = editor->markersAtLine(line);
        if (marcadores & (1 << 0)) {
            editor->markerDelete(line, 0); // Si lo tiene, lo borramos
        } else {
            editor->markerAdd(line, 0);    // Si no, lo añadimos
        }
    }
}

void MainWindow::depurarCodigo() {
    QString ruta = rutaArchivoActual();
    if (ruta.isEmpty()) {
        consola->append(tr("<font color='#E06C75'>Error: Guarda el archivo antes de depurar.</font>"));
        return;
    }

    guardarArchivo();
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    consola->clear();
    consola->append(tr("<font color='#C678DD'>Preparando entorno de depuración para %1...</font>").arg(QFileInfo(ruta).fileName()));

    QString directorio = QFileInfo(ruta).absolutePath();
    QString nombreBase = QFileInfo(ruta).baseName();
    QString nombreArchivo = QFileInfo(ruta).fileName();
    QString rutaEjecutable = directorio + "/" + nombreBase;

    // 1. Compilar con bandera de depuración (-g) y soporte extra COBOL (-fdebugging-line)
    QProcess compilador;
    compilador.setWorkingDirectory(directorio);
    compilador.start("cobc", QStringList() << "-x" << "-g" << "-fdebugging-line" << ruta << "-o" << rutaEjecutable);
    compilador.waitForFinished();

    if (compilador.exitCode() != 0) {
        consola->append(tr("<font color='#E06C75'>Error de compilación. Corrige los errores con F6 antes de depurar.</font>"));
        return;
    }

    // 2. Recolectar Puntos de Ruptura Visuales del editor
    QStringList comandosDepurador;
    int totalLineas = editor->lines();

    #if defined(Q_OS_MAC)
    // --- LÓGICA PARA MAC (LLDB) ---
    for (int i = 0; i < totalLineas; ++i) {
        if (editor->markersAtLine(i) & (1 << 0)) {
            comandosDepurador << QString("breakpoint set --file %1 --line %2").arg(nombreArchivo).arg(i + 1);
        }
    }

    QString rutaScript = directorio + "/.lldbinit_cobolworks";
    QFile fileScript(rutaScript);
    if (fileScript.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&fileScript);
        for (const QString &cmd : comandosDepurador) out << cmd << "\n";
        if (!comandosDepurador.isEmpty()) out << "run\n";
        fileScript.close();
    }

    consola->append(tr("<font color='#98C379'>Iniciando depurador LLDB (macOS)... Revisa tu terminal.</font>"));
    QProcess *terminal = new QProcess(this);
    terminal->setWorkingDirectory(directorio);
    // En Mac abrimos la app Terminal nativa
    terminal->start("open", QStringList() << "-a" << "Terminal" << rutaEjecutable);
    // Nota: La integración perfecta de LLDB en Mac requiere un script intermedio, pero esta es la base.

    #else
    // --- LÓGICA PARA LINUX Y WINDOWS (GDB) ---
    for (int i = 0; i < totalLineas; ++i) {
        if (editor->markersAtLine(i) & (1 << 0)) {
            comandosDepurador << QString("break %1:%2").arg(nombreArchivo).arg(i + 1);
        }
    }

    QString rutaScript = directorio + "/.gdbinit_cobolworks";
    QFile fileScript(rutaScript);
    if (fileScript.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&fileScript);
        out << "set pagination off\n";
        for (const QString &cmd : comandosDepurador) out << cmd << "\n";
        if (!comandosDepurador.isEmpty()) out << "run\n";
        fileScript.close();
    }

    consola->append(tr("<font color='#98C379'>Iniciando depurador GDB... Revisa tu terminal.</font>"));
    QProcess *terminal = new QProcess(this);
    terminal->setWorkingDirectory(directorio);

    #if defined(Q_OS_WIN)
    // En Windows lanzamos el CMD nativo invocando a gdb
    terminal->start("cmd.exe", QStringList() << "/c" << "start" << "gdb" << "-q" << "-x" << rutaScript << rutaEjecutable);
    #else
    // En Linux usamos konsole o xterm
    if (QFile::exists("/usr/bin/konsole")) {
        terminal->start("konsole", QStringList() << "-e" << "gdb" << "-q" << "-x" << rutaScript << "--args" << rutaEjecutable);
    } else {
        terminal->start("xterm", QStringList() << "-e" << "gdb" << "-q" << "-x" << rutaScript << "--args" << rutaEjecutable);
    }
    #endif

#endif

}

void MainWindow::comprobarActualizaciones() {
    // Busca el archivo de control en tu web pública de GitHub Pages
    QUrl url("https://anabasasoft.github.io/version.json");
    QNetworkRequest request(url);

    // Evitamos que guarde el archivo en caché para que siempre lea la versión real
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    managerActualizaciones->get(request);
}

void MainWindow::procesarRespuestaActualizacion(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray respuesta = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(respuesta);

        if (!jsonDoc.isNull() && jsonDoc.isObject()) {
            QJsonObject jsonObj = jsonDoc.object();
            QString versionOnline = jsonObj["version"].toString();
            QString enlaceDescarga = jsonObj["enlace"].toString();

            // Comparamos versiones. Si la online es distinta a la nuestra, saltamos la alerta.
            if (!versionOnline.isEmpty() && versionOnline != VERSION_ACTUAL) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(tr("Actualización de CobolWorks"));
                msgBox.setText(tr("¡Hay una nueva versión de CobolWorks disponible!\n\nVersión actual: %1\nNueva versión: %2").arg(VERSION_ACTUAL, versionOnline));
                msgBox.setInformativeText(tr("¿Quieres abrir el navegador para descargarla ahora?"));
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::Yes);

                if (msgBox.exec() == QMessageBox::Yes) {
                    QDesktopServices::openUrl(QUrl(enlaceDescarga));
                }
            }
        }
    }
    reply->deleteLater();
}

void MainWindow::reiniciarTemporizadorLinter() {
    // Si el usuario vuelve a escribir, borramos los subrayados rojos para que no molesten
    if (QsciScintilla *editor = editorActual()) {
        editor->SendScintilla(QsciScintilla::SCI_SETINDICATORCURRENT, 0);
        editor->SendScintilla(QsciScintilla::SCI_INDICATORCLEARRANGE, 0, editor->length());
    }
    // Reiniciamos el reloj
    timerLinter->start();
}

void MainWindow::ejecutarLinter() {
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    QString texto = editor->text();
    if (texto.isEmpty()) return;

    // Guardamos el código actual en un archivo temporal en la carpeta de la memoria caché del SO
    // Así no machacamos el archivo real del usuario hasta que no le dé a Guardar
    QString rutaTemp = QDir::tempPath() + "/cobolworks_linter.cbl";
    QFile file(rutaTemp);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << texto;
        file.close();
    }

    // Le pedimos a GnuCOBOL que compruebe solo la sintaxis, sin generar ejecutable
    procesoLinter->start("cobc", QStringList() << "-fsyntax-only" << rutaTemp);
}

void MainWindow::leerSalidaLinter() {
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    // Leemos los errores del compilador fantasma
    QString errores = procesoLinter->readAllStandardError();
    if (errores.isEmpty()) return; // Si está vacío, el código es perfecto

    QStringList lineasError = errores.split('\n');
    // Buscamos las líneas de error basándonos en el nombre de nuestro archivo temporal
    QRegularExpression re("cobolworks_linter\\.cbl:(\\d+):");

    // Preparamos a Scintilla para dibujar usando el Indicador 0 (el ondulado rojo)
    editor->SendScintilla(QsciScintilla::SCI_SETINDICATORCURRENT, 0);

    for (const QString &linea : lineasError) {
        QRegularExpressionMatch match = re.match(linea);
        if (match.hasMatch()) {
            int numLinea = match.captured(1).toInt() - 1; // Scintilla cuenta desde 0

            if (numLinea >= 0 && numLinea < editor->lines()) {
                // Averiguamos dónde empieza y dónde acaba la línea afectada
                int posInicio = editor->positionFromLineIndex(numLinea, 0);
                int longitud = editor->lineLength(numLinea);

                // Pintamos el subrayado ondulado debajo de toda la línea
                editor->SendScintilla(QsciScintilla::SCI_INDICATORFILLRANGE, posInicio, longitud);
            }
        }
    }
}

void MainWindow::configurarCompilador() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Configuración de GnuCOBOL"));
    dialog.resize(500, 150);
    dialog.setStyleSheet("background-color: #0B192C; color: #E2F1E7;");

    QFormLayout form(&dialog);

    QSettings settings("AnabasaSoft", "CobolWorks");

    QLineEdit *lineFlags = new QLineEdit(&dialog);
    lineFlags->setStyleSheet("background-color: #1E3E62; color: white; border: 1px solid #478CCF; padding: 4px;");
    lineFlags->setText(settings.value("compilador_flags", "").toString());
    lineFlags->setPlaceholderText(tr("Ej: -std=cobol85 -free -I /ruta/copybooks"));
    form.addRow(tr("Flags del compilador (cobc):"), lineFlags);

    QLineEdit *lineEnv = new QLineEdit(&dialog);
    lineEnv->setStyleSheet("background-color: #1E3E62; color: white; border: 1px solid #478CCF; padding: 4px;");
    lineEnv->setText(settings.value("compilador_entorno", "").toString());
    lineEnv->setPlaceholderText(tr("Ej: COB_LIBRARY_PATH=/ruta/lib; OTRO_ENV=valor"));
    form.addRow(tr("Variables de entorno (separadas por ;):"), lineEnv);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    buttonBox.setStyleSheet("QPushButton { background-color: #478CCF; color: white; border: none; padding: 6px; } QPushButton:hover { background-color: #2A4D77; }");
    form.addRow(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        settings.setValue("compilador_flags", lineFlags->text());
        settings.setValue("compilador_entorno", lineEnv->text());
        QMessageBox::information(this, tr("Guardado"), tr("Configuración del compilador guardada correctamente."));
    }
}

void MainWindow::actualizarAutocompletado() {
    QsciScintilla *editor = editorActual();
    if (!editor || !editor->lexer()) return;

    // Rescatamos el motor de autocompletado de la pestaña actual
    QsciAPIs *api = qobject_cast<QsciAPIs*>(editor->lexer()->apis());
    if (!api) return;

    api->clear(); // Limpiamos la memoria caché anterior

    // 1. Inyectamos las palabras clave de COBOL básicas
    QStringList palabrasClave = {
        "IDENTIFICATION", "ENVIRONMENT", "DATA", "PROCEDURE", "DIVISION", "SECTION",
        "PROGRAM-ID", "WORKING-STORAGE", "LINKAGE", "FILE", "PIC", "PICTURE",
        "DISPLAY", "ACCEPT", "STOP", "RUN", "PERFORM", "COMPUTE", "IF", "ELSE", "END-IF",
        "MOVE", "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "INITIALIZE", "REPLACE", "COPY"
    };
    for (const QString &palabra : palabrasClave) {
        api->add(palabra);
    }

    // 2. La Magia: Buscar Copybooks y extraer sus variables
    QString ruta = rutaArchivoActual();
    if (!ruta.isEmpty()) {
        QDir dir(QFileInfo(ruta).absolutePath());
        QStringList filtros;
        filtros << "*.cpy" << "*.cob" << "*.cbl";
        // Buscamos todos los archivos COBOL en la misma carpeta
        QFileInfoList archivos = dir.entryInfoList(filtros, QDir::Files);

        // Expresión regular para cazar variables (niveles 01-49, 77, 88, FD, SD)
        QRegularExpression reVariable("^(?:0[1-9]|[1-4][0-9]|77|88|FD|SD)\\s+([a-zA-Z0-9\\-]+)");

        for (const QFileInfo &info : archivos) {
            // Añadimos el nombre del archivo al diccionario (útil cuando escriban COPY "...")
            api->add(info.fileName());

            // Si es un archivo distinto al actual, lo escaneamos por dentro
            if (info.absoluteFilePath() != ruta) {
                QFile file(info.absoluteFilePath());
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    while (!in.atEnd()) {
                        QString linea = in.readLine().trimmed();
                        QRegularExpressionMatch match = reVariable.match(linea);
                        if (match.hasMatch()) {
                            // Encontramos una variable externa, ¡al autocompletado!
                            api->add(match.captured(1));
                        }
                    }
                    file.close();
                }
            }
        }
    }
    // Compilamos el diccionario para que sea ultrarrápido al teclear
    api->prepare();
}

void MainWindow::mostrarNavegadorFunciones() {
    QsciScintilla *editor = editorActual();
    if (!editor || listaEsquema->count() == 0) return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Navegador de Funciones"));
    dialog.resize(450, 500);
    dialog.setStyleSheet("background-color: #0B192C; color: #E2F1E7;");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLineEdit *buscador = new QLineEdit(&dialog);
    buscador->setPlaceholderText(tr("Escribe para filtrar párrafos, secciones o variables..."));
    buscador->setStyleSheet("background-color: #1E3E62; color: white; border: 1px solid #478CCF; padding: 6px; font-size: 12pt;");
    layout->addWidget(buscador);

    QListWidget *lista = new QListWidget(&dialog);
    lista->setStyleSheet("QListWidget { background-color: #040D1A; color: #E2F1E7; border: 1px solid #1E3E62; font-family: monospace; font-size: 11pt; } QListWidget::item:selected { background-color: #478CCF; font-weight: bold; }");
    layout->addWidget(lista);

    // Clonamos los elementos que ya hemos parseado en el panel lateral de Estructura
    for (int i = 0; i < listaEsquema->count(); ++i) {
        QListWidgetItem *itemOriginal = listaEsquema->item(i);
        QListWidgetItem *nuevoItem = new QListWidgetItem(itemOriginal->text().trimmed(), lista);
        nuevoItem->setData(Qt::UserRole, itemOriginal->data(Qt::UserRole)); // Copiamos la línea exacta
        nuevoItem->setForeground(itemOriginal->foreground()); // Mantenemos el color original (Naranja variables, Verde párrafos...)
    }

    // Magia en tiempo real: Filtramos la lista según el usuario escribe
    connect(buscador, &QLineEdit::textChanged, [&lista](const QString &texto) {
        for (int i = 0; i < lista->count(); ++i) {
            QListWidgetItem *item = lista->item(i);
            // Comprobación insensible a mayúsculas
            item->setHidden(!item->text().contains(texto, Qt::CaseInsensitive));
        }
        // Seleccionamos el primer elemento visible para poder darle a Enter rápido
        for (int i = 0; i < lista->count(); ++i) {
            if (!lista->item(i)->isHidden()) {
                lista->setCurrentRow(i);
                break;
            }
        }
    });

    // Acción para saltar a la línea y cerrar la ventana
    auto saltar = [this, &dialog, editor](QListWidgetItem *item) {
        if (item) {
            int linea = item->data(Qt::UserRole).toInt();
            editor->setCursorPosition(linea, 0);
            editor->ensureLineVisible(linea);

            // Subrayamos la línea para que quede claro dónde hemos caído
            editor->setSelection(linea, 0, linea, editor->lineLength(linea));
            editor->setFocus();
            dialog.accept();
        }
    };

    // Saltar al hacer Doble Clic o darle a Enter sobre la lista
    connect(lista, &QListWidget::itemActivated, saltar);

    // Saltar al darle a Enter mientras escribes en el buscador
    connect(buscador, &QLineEdit::returnPressed, [&lista, saltar]() {
        saltar(lista->currentItem());
    });

    buscador->setFocus();
    dialog.exec();
}

void MainWindow::actualizarDependencias() {
    arbolDependencias->clear();
    QsciScintilla *editor = editorActual();
    if (!editor) return;

    QString texto = editor->text();
    QStringList lineas = texto.split('\n');

    QTreeWidgetItem *nodoCopy = new QTreeWidgetItem(arbolDependencias);
    nodoCopy->setText(0, tr("COPYBOOKs"));
    nodoCopy->setForeground(0, QColor("#E5C07B"));
    QFont fCopy = nodoCopy->font(0);
    fCopy.setBold(true);
    nodoCopy->setFont(0, fCopy);

    QTreeWidgetItem *nodoCall = new QTreeWidgetItem(arbolDependencias);
    nodoCall->setText(0, tr("Llamadas (CALL)"));
    nodoCall->setForeground(0, QColor("#C678DD"));
    QFont fCall = nodoCall->font(0);
    fCall.setBold(true);
    nodoCall->setFont(0, fCall);

    // Expresiones regulares para cazar COPYs y CALLs en cualquier formato
    QRegularExpression reCopy("COPY\\s+[\"']?([^\"'\\.\\s]+)(?:\\.cpy|\\.cob|\\.cbl)?[\"']?", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression reCall("CALL\\s+[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);

    QStringList copiesEncontrados;
    QStringList callsEncontrados;

    for (const QString &linea : lineas) {
        QString l = linea.trimmed();
        if (l.startsWith("*") || l.startsWith("*>")) continue; // Ignoramos comentarios

        QRegularExpressionMatch matchCopy = reCopy.match(l);
        if (matchCopy.hasMatch()) {
            QString nombreCopy = matchCopy.captured(1);
            if (!copiesEncontrados.contains(nombreCopy)) { // Evitamos duplicados en la lista
                copiesEncontrados << nombreCopy;
                QTreeWidgetItem *hijo = new QTreeWidgetItem(nodoCopy);
                hijo->setText(0, nombreCopy);
                hijo->setForeground(0, QColor("#98C379"));
                hijo->setData(0, Qt::UserRole, "COPY"); // Le pegamos una etiqueta invisible
            }
        }

        QRegularExpressionMatch matchCall = reCall.match(l);
        if (matchCall.hasMatch()) {
            QString nombreCall = matchCall.captured(1);
            if (!callsEncontrados.contains(nombreCall)) {
                callsEncontrados << nombreCall;
                QTreeWidgetItem *hijo = new QTreeWidgetItem(nodoCall);
                hijo->setText(0, nombreCall);
                hijo->setForeground(0, QColor("#A6E3E9"));
                hijo->setData(0, Qt::UserRole, "CALL"); // Etiqueta invisible
            }
        }
    }

    // Si no hay archivos, ocultamos las carpetas principales para dejarlo limpio
    if (nodoCopy->childCount() == 0) delete nodoCopy;
    if (nodoCall->childCount() == 0) delete nodoCall;

    arbolDependencias->expandAll();
}

void MainWindow::abrirDependencia(QTreeWidgetItem *item, int column) {
    // Si hace doble clic en el título de la carpeta ("COPYBOOKs") no hacemos nada
    if (!item || item->parent() == nullptr) return;

    QString tipo = item->data(0, Qt::UserRole).toString();
    QString nombre = item->text(0);
    QString dir = QFileInfo(rutaArchivoActual()).absolutePath();

    if (tipo == "COPY") {
        QStringList extensiones = {".cpy", ".cob", ".cbl", ".ccp", ".sqb", ""};
        for (const QString &ext : extensiones) {
            QString rutaCopy = dir + "/" + nombre + ext;
            if (QFile::exists(rutaCopy)) {
                // Comprobamos si ya está abierto para no abrirlo 2 veces
                for (int i = 0; i < tabWidget->count(); ++i) {
                    if (tabWidget->tabToolTip(i) == rutaCopy) {
                        tabWidget->setCurrentIndex(i);
                        return;
                    }
                }
                QFile file(rutaCopy);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    crearNuevaPestana(QFileInfo(rutaCopy).fileName(), rutaCopy, QString::fromUtf8(file.readAll()));
                    return;
                }
            }
        }
        statusBar()->showMessage(tr("No se encontró el archivo COPYBOOK: %1").arg(nombre), 4000);
    } else if (tipo == "CALL") {
        // Para los CALL, normalmente buscan un archivo de programa fuente
        QStringList extensiones = {".cob", ".cbl", ""};
        for (const QString &ext : extensiones) {
            QString rutaProg = dir + "/" + nombre + ext;
            if (QFile::exists(rutaProg)) {
                for (int i = 0; i < tabWidget->count(); ++i) {
                    if (tabWidget->tabToolTip(i) == rutaProg) {
                        tabWidget->setCurrentIndex(i);
                        return;
                    }
                }
                QFile file(rutaProg);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    crearNuevaPestana(QFileInfo(rutaProg).fileName(), rutaProg, QString::fromUtf8(file.readAll()));
                    return;
                }
            }
        }
        statusBar()->showMessage(tr("No se encontró el programa fuente: %1").arg(nombre), 4000);
    }
}

// --- FUNCIONES ENTERPRISE (GIT, DOCKER, CI/CD, APIs) ---

void MainWindow::integrarGitStatus() {
    QString ruta = rutaArchivoActual();
    QString directorio = ruta.isEmpty() ? QDir::homePath() : QFileInfo(ruta).absolutePath();

    consola->append(tr("<font color='#C678DD'>Ejecutando Git Status en %1...</font>").arg(directorio));

    QProcess *gitProcess = new QProcess(this);
    gitProcess->setWorkingDirectory(directorio);

    connect(gitProcess, &QProcess::finished, [this, gitProcess]() {
        QString salida = gitProcess->readAllStandardOutput();
        QString errores = gitProcess->readAllStandardError();

        if (!salida.isEmpty()) consola->append(salida.toHtmlEscaped());
        if (!errores.isEmpty()) consola->append("<font color='#E06C75'>" + errores.toHtmlEscaped() + "</font>");

        gitProcess->deleteLater();
    });

    gitProcess->start("git", QStringList() << "status");
}

void MainWindow::compilarEnDocker() {
    QString ruta = rutaArchivoActual();
    if (ruta.isEmpty()) {
        consola->append(tr("<font color='#E06C75'>Error: Guarda el archivo antes de compilar en Docker.</font>"));
        return;
    }

    guardarArchivo();
    consola->clear();

    QString directorio = QFileInfo(ruta).absolutePath();
    QString nombreArchivo = QFileInfo(ruta).fileName();

    consola->append(tr("<font color='#A6E3E9'>Compilando %1 dentro del contenedor oficial ghcr.io/gnu-cobol/gnucobol...</font>").arg(nombreArchivo));

    QProcess *dockerProcess = new QProcess(this);
    dockerProcess->setWorkingDirectory(directorio);

    connect(dockerProcess, &QProcess::finished, [this, dockerProcess]() {
        QString salida = dockerProcess->readAllStandardOutput();
        QString errores = dockerProcess->readAllStandardError();

        if (!salida.isEmpty()) consola->append(salida.toHtmlEscaped());
        if (!errores.isEmpty()) consola->append("<font color='#E06C75'>" + errores.toHtmlEscaped() + "</font>");

        if (dockerProcess->exitCode() == 0) {
            consola->append(tr("<font color='#98C379'>Compilación Docker finalizada con éxito.</font>"));
        } else {
            consola->append(tr("<font color='#E06C75'>Error en la compilación Docker.</font>"));
        }
        dockerProcess->deleteLater();
    });

    // Comando: docker run --rm -v "$PWD":/workspace -w /workspace ghcr.io/gnu-cobol/gnucobol:latest cobc -x archivo.cbl
    QStringList argumentos;
    argumentos << "run" << "--rm"
    << "-v" << directorio + ":/workspace"
    << "-w" << "/workspace"
    << "ghcr.io/gnu-cobol/gnucobol:latest"
    << "cobc" << "-x" << nombreArchivo;

    dockerProcess->start("docker", argumentos);
}

void MainWindow::generarPipelineCI() {
    QString ruta = rutaArchivoActual();
    QString directorio = ruta.isEmpty() ? QDir::homePath() : QFileInfo(ruta).absolutePath();

    QDir dir(directorio);
    dir.mkpath(".github/workflows"); // Crea las carpetas si no existen
    QString pipelinePath = dir.absoluteFilePath(".github/workflows/cobol_ci.yml");

    QString contenidoYAML =
    "name: Integración Continua COBOL\n\n"
    "on:\n"
    "  push:\n"
    "    branches: [ \"main\", \"master\" ]\n"
    "  pull_request:\n"
    "    branches: [ \"main\", \"master\" ]\n\n"
    "jobs:\n"
    "  build:\n"
    "    runs-on: ubuntu-latest\n"
    "    container:\n"
    "      image: ghcr.io/gnu-cobol/gnucobol:latest\n"
    "    steps:\n"
    "      - name: Checkout del código\n"
    "        uses: actions/checkout@v3\n\n"
    "      - name: Compilar todos los programas COBOL\n"
    "        run: |\n"
    "          for file in *.cbl *.cob; do\n"
    "            if [ -f \"$file\" ]; then\n"
    "              echo \"Compilando $file...\"\n"
    "              cobc -x \"$file\"\n"
    "            fi\n"
    "          done\n\n"
    "      - name: Ejecutar batería de pruebas\n"
    "        run: |\n"
    "          echo \"Configura aquí tus scripts de testing\"\n";

    QFile file(pipelinePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << contenidoYAML;
        file.close();

        // Abrimos el archivo en una pestaña nueva para que el usuario lo revise
        crearNuevaPestana("cobol_ci.yml", pipelinePath, contenidoYAML);
        consola->append(tr("<font color='#98C379'>Pipeline CI/CD creado en: %1</font>").arg(pipelinePath));
    } else {
        consola->append(tr("<font color='#E06C75'>Error al crear el archivo del Pipeline CI/CD.</font>"));
    }
}

void MainWindow::generarBoilerplateAPI() {
    // Genera una estructura base en COBOL para consumir una API REST usando utilidades del sistema
    QString plantilla =
    "       IDENTIFICATION DIVISION.\n"
    "       PROGRAM-ID. ClienteAPI-REST.\n"
    "       *> ---------------------------------------------------------\n"
    "       *> Boilerplate generado por CobolWorks Enterprise\n"
    "       *> Demostración de consumo de API REST desde GnuCOBOL\n"
    "       *> ---------------------------------------------------------\n"
    "       ENVIRONMENT DIVISION.\n"
    "       DATA DIVISION.\n"
    "       WORKING-STORAGE SECTION.\n"
    "       01  WS-URL-API             PIC X(100) VALUE \"https://jsonplaceholder.typicode.com/todos/1\".\n"
    "       01  WS-COMANDO-CURL        PIC X(200).\n"
    "       01  WS-RESULTADO-SISTEMA   PIC S9(4) COMP.\n\n"
    "       PROCEDURE DIVISION.\n"
    "       000-MAIN.\n"
    "           DISPLAY \"[INFO] Iniciando peticion REST a la API...\".\n"
    "           \n"
    "           *> Preparamos el comando cURL en modo silencioso\n"
    "           STRING \"curl -s -X GET \" WS-URL-API DELIMITED BY SIZE\n"
    "                  INTO WS-COMANDO-CURL\n"
    "           \n"
    "           *> Ejecutamos la llamada al sistema\n"
    "           CALL \"SYSTEM\" USING WS-COMANDO-CURL\n"
    "                         RETURNING WS-RESULTADO-SISTEMA.\n"
    "           \n"
    "           IF WS-RESULTADO-SISTEMA = 0\n"
    "               DISPLAY \" \"\n"
    "               DISPLAY \"[OK] Peticion finalizada con exito.\"\n"
    "           ELSE\n"
    "               DISPLAY \"[ERROR] Fallo al conectar con la API externa.\"\n"
    "           END-IF.\n"
    "           \n"
    "           STOP RUN.\n";

    crearNuevaPestana("ClienteAPI.cbl", "", plantilla);
}

void MainWindow::sincronizarCopybookRemoto() {
    bool ok;
    QString urlStr = QInputDialog::getText(this, tr("Importar Copybook Remoto"),
                                           tr("Introduce la URL directa del archivo .cpy (ej: raw de GitHub, servidor interno):"),
                                           QLineEdit::Normal, "", &ok);

    if (!ok || urlStr.isEmpty()) return;

    // Creamos la carpeta de caché oculta en la home del usuario
    QString dirCache = QDir::homePath() + "/.cobolworks_copybooks";
    QDir().mkpath(dirCache);

    QUrl url(urlStr);
    QString nombreArchivo = url.fileName();
    if (nombreArchivo.isEmpty()) nombreArchivo = "remoto_importado.cpy";

    QString rutaDestino = dirCache + "/" + nombreArchivo;

    consola->append(tr("<font color='#A6E3E9'>Descargando Copybook desde %1...</font>").arg(urlStr));

    // Usamos el manager que ya tienes instanciado en MainWindow para actualizaciones
    QNetworkRequest request(url);
    QNetworkReply *reply = managerActualizaciones->get(request);

    // Conectamos la respuesta mediante una lambda temporal
    connect(reply, &QNetworkReply::finished, [this, reply, rutaDestino, dirCache, nombreArchivo]() {
        if (reply->error() == QNetworkReply::NoError) {
            QFile file(rutaDestino);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(reply->readAll());
                file.close();

                // MAGIA: Añadimos la carpeta caché a los flags del compilador automáticamente
                QSettings settings("AnabasaSoft", "CobolWorks");
                QString flagsActuales = settings.value("compilador_flags", "").toString();
                QString flagInclude = "-I " + dirCache;

                if (!flagsActuales.contains(flagInclude)) {
                    settings.setValue("compilador_flags", flagsActuales + " " + flagInclude);
                }

                consola->append(tr("<font color='#98C379'>Copybook guardado en %1.</font>").arg(rutaDestino));
                consola->append(tr("<font color='#98C379'>El compilador ya está configurado para buscar en esta ruta automáticamente (-I).</font>"));

                // Lo abrimos para que el usuario lo vea
                QFile fileLeida(rutaDestino);
                if (fileLeida.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    crearNuevaPestana(nombreArchivo, rutaDestino, QString::fromUtf8(fileLeida.readAll()));
                }
            }
        } else {
            consola->append(tr("<font color='#E06C75'>Error al descargar el Copybook: %1</font>").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

void MainWindow::generarJCL() {
    QString ruta = rutaArchivoActual();
    QString directorio = ruta.isEmpty() ? QDir::homePath() : QFileInfo(ruta).absolutePath();
    QString nombreBase = ruta.isEmpty() ? "PROGRAMA" : QFileInfo(ruta).baseName().toUpper();

    QString rutaJCL = directorio + "/" + nombreBase + ".jcl";

    // Plantilla clásica de JCL para IBM z/OS (MVS)
    QString contenidoJCL =
    "//COBOLJOB JOB (1234),'COBOL COMPILATION',CLASS=A,MSGCLASS=X,MSGLEVEL=(1,1)\n"
    "//*\n"
    "//* Job Control Language generado por CobolWorks IDE\n"
    "//* Simulación de entorno Mainframe IBM z/OS\n"
    "//*\n"
    "//STEP1    EXEC IGYWCL\n"
    "//COBOL.SYSIN  DD DSN=USER." + nombreBase + ".COBOL,DISP=SHR\n"
    "//LKED.SYSLMOD DD DSN=USER.LOADLIB(" + nombreBase + "),DISP=SHR\n"
    "//*\n"
    "//STEP2    EXEC PGM=" + nombreBase + "\n"
    "//STEPLIB  DD DSN=USER.LOADLIB,DISP=SHR\n"
    "//SYSOUT   DD SYSOUT=*\n"
    "//SYSIN    DD *\n"
    "/* Datos de entrada aqui */\n"
    "//\n";

    QFile file(rutaJCL);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << contenidoJCL;
        file.close();

        // Aplicamos el estándar IBM a GnuCOBOL para emular Mainframe
        QSettings settings("AnabasaSoft", "CobolWorks");
        QString flagsActuales = settings.value("compilador_flags", "").toString();
        if (!flagsActuales.contains("-std=ibm-strict")) {
            settings.setValue("compilador_flags", "-std=ibm-strict " + flagsActuales);
            consola->append(tr("<font color='#E5C07B'>Aviso: El compilador se ha ajustado al estándar '-std=ibm-strict' para emular el Mainframe.</font>"));
        }

        crearNuevaPestana(nombreBase + ".jcl", rutaJCL, contenidoJCL);
        consola->append(tr("<font color='#98C379'>Archivo JCL generado en: %1</font>").arg(rutaJCL));
    }
}
