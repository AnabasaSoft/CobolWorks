/******************************************************************************
 * Proyecto: CobolWorks IDE
 * Autor: AnabasaSoft (https://github.com/anabasasoft)
 * Licencia: GNU General Public License v3.0 (GPL-3.0)
 * * Descripción: IDE moderno para COBOL con análisis de flujo, depuración visual
 * e integración con IA para migración de código.
 * * "Domina el código Legacy con herramientas del futuro."
 *****************************************************************************/

#include "mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFont>
#include <QKeyEvent>
#include <QFileDialog>
#include <QMessageBox>
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
#include "cobollexer.h"
#include <Qsci/qsciapis.h>
#include <QStatusBar>
#include <QLabel>
#include <QDir>
#include <QLocale>
#include <QApplication>
#include <QListWidget>
#include <QRegularExpression>
#include <QDialog>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>

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

    // Comprobador silencioso de actualizaciones al arrancar
    managerActualizaciones = new QNetworkAccessManager(this);
    connect(managerActualizaciones, &QNetworkAccessManager::finished, this, &MainWindow::procesarRespuestaActualizacion);
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
    tabWidget->setTabsClosable(true);
    tabWidget->setStyleSheet("QTabBar::tab { background: #1E3E62; color: #E2F1E7; padding: 8px; } QTabBar::tab:selected { background: #0B192C; font-weight: bold; } QTabWidget::pane { border: 1px solid #1E3E62; }");
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::cerrarPestana);

    consola = new QTextEdit(splitter);
    consola->setReadOnly(true);
    consola->setStyleSheet("background-color: #040D1A; color: #A6E3E9; font-family: 'Courier New'; font-size: 11pt; border: none;");

    // NUEVO: Le decimos al panel interno de la consola que nos mande sus eventos (clics, teclas...)
    consola->viewport()->installEventFilter(this);

    splitter->addWidget(tabWidget);
    splitter->addWidget(consola);
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
        "DISPLAY", "ACCEPT", "STOP", "RUN", "PERFORM", "COMPUTE", "IF", "ELSE", "END-IF"
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

    // --- NUEVO: Menú contextual (clic derecho) ---
    nuevoEditor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(nuevoEditor, &QWidget::customContextMenuRequested, this, &MainWindow::mostrarMenuContextual);

    actualizarEsquema();
    actualizarFlujo();
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
    accionDefinicion->setShortcut(QKeySequence(Qt::Key_F2));
    addAction(accionDefinicion); // Para que funcione F2 globalmente

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

    QMenu *menuConstruir = menuBar()->addMenu(tr("&Construir"));
    QAction *accionCompilarSolo = menuConstruir->addAction(tr("Compilar"), this, &MainWindow::compilarSolo);
    accionCompilarSolo->setShortcut(QKeySequence(Qt::Key_F6));

    QAction *accionCompilarEjecutar = menuConstruir->addAction(tr("Compilar y Ejecutar"), this, &MainWindow::compilarEjecutar);
    accionCompilarEjecutar->setShortcut(QKeySequence(Qt::Key_F5));

    QAction *accionDepurar = menuConstruir->addAction(tr("Depurar con GDB"), this, &MainWindow::depurarCodigo);
    accionDepurar->setShortcut(QKeySequence(Qt::Key_F8));
    addAction(accionDepurar); // Para que funcione F8 globalmente

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
    compilador.start("cobc", QStringList() << "-x" << ruta << "-o" << rutaEjecutable);
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
    compilador.start("cobc", QStringList() << "-x" << ruta << "-o" << rutaEjecutable);
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

    // Traducir todos los paneles laterales (Docks)
    QList<QDockWidget *> docks = findChildren<QDockWidget *>();
    if (docks.size() >= 3) {
        docks[0]->setWindowTitle(tr("Explorador"));
        docks[1]->setWindowTitle(tr("Estructura COBOL"));
        docks[2]->setWindowTitle(tr("Flujo de Ejecución"));
    }

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

        if (esDivision || esSeccion || esParrafo) {
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
    // Creamos la ventana emergente
    QDialog *dialogo = new QDialog(this);
    dialogo->setWindowTitle(tr("Ayuda de CobolWorks"));
    dialogo->resize(650, 550);
    dialogo->setStyleSheet("background-color: #0B192C; color: #E2F1E7;");

    QVBoxLayout *layout = new QVBoxLayout(dialogo);

    // 1. Añadimos el Logo centrado en la parte superior
    QLabel *labelLogo = new QLabel(dialogo);
    QPixmap pixmap(":/logo.png");
    labelLogo->setPixmap(pixmap.scaledToWidth(400, Qt::SmoothTransformation));
    labelLogo->setAlignment(Qt::AlignCenter);
    layout->addWidget(labelLogo);

    // 2. Añadimos el área de texto con scroll
    QTextBrowser *textoAyuda = new QTextBrowser(dialogo);
    textoAyuda->setReadOnly(true);
    textoAyuda->setOpenExternalLinks(true); // Permitir que los clics abran el navegador web
    textoAyuda->setStyleSheet("background-color: #1E3E62; color: #E2F1E7; font-size: 11pt; border: 1px solid #478CCF; padding: 10px;");

    // Redactamos el manual usando HTML para que quede bonito
    QString contenido = tr(
        "<h1>CobolWorks</h1>"
        "<p><b>El IDE moderno para el programador de siempre.</b></p>"
        "<p>CobolWorks es un entorno de desarrollo integrado (IDE) ligero y potente, diseñado específicamente para facilitar la escritura, compilación y depuración de código COBOL. Cuenta con un sistema avanzado de análisis de código, estructura interactiva y utilidades diseñadas para la máxima productividad.</p>"

        "<h2>Atajos de Teclado Principales</h2>"
        "<ul>"
        "<li><b>Ctrl + N:</b> Nuevo archivo vacío</li>"
        "<li><b>Ctrl + O:</b> Abrir archivo</li>"
        "<li><b>Ctrl + S:</b> Guardar archivo actual</li>"
        "<li><b>Ctrl + W:</b> Cerrar la pestaña actual</li>"
        "<li><b>Ctrl + Tab:</b> Ir a la pestaña siguiente</li>"
        "<li><b>Ctrl + Shift + Tab:</b> Ir a la pestaña anterior</li>"
        "<li><b>F2:</b> Ir a la definición (variables, párrafos, o abrir COPYBOOKs)</li>"
        "<li><b>F5:</b> Compilar y ejecutar en terminal</li>"
        "<li><b>F6:</b> Compilar y mostrar errores</li>"
        "<li><b>Ctrl + 0:</b> Restaurar el tamaño de la letra (Zoom)</li>"
        "<li><b>Ctrl + F:</b> Buscar en el código</li>"
        "<li><b>Ctrl + H / Ctrl + R:</b> Buscar y Sustituir texto avanzado</li>"
        "</ul>"

        "<h2>Características Destacadas</h2>"
        "<ul>"
        "<li><b>Terminal Interactiva:</b> Haz doble clic sobre cualquier error rojo en la consola inferior para saltar a la línea afectada.</li>"
        "<li><b>Navegador de Estructura y Flujo:</b> Panel lateral con el esquema y el árbol de ejecución (PERFORM/GO TO). Haz doble clic en cualquier elemento para navegar rápido.</li>"
        "<li><b>Navegación COPYBOOK (F2):</b> Pon el cursor sobre un <code>COPY \"ARCHIVO.CPY\"</code> y pulsa F2 para abrirlo en una pestaña nueva automáticamente.</li>"
        "<li><b>Migración con IA (Gemini):</b> Traduce bloques de código o archivos enteros a Python y Java modernos configurando tu API Key desde el menú.</li>"
        "<li><b>Formateo COBOL Estricto:</b> Líneas guía visuales en las columnas 7, 11 y 72. El tabulador respeta las áreas A y B.</li>"
        "<li><b>Traducción Dinámica:</b> Cambia el idioma de la interfaz al vuelo sin reiniciar.</li>"
        "</ul>"

        "<h2>Contacto y Enlaces</h2>"
        "<ul>"
        "<li><b>Email:</b> <a href=\"mailto:anabasasoft@gmail.com\" style=\"color: #A6E3E9;\">anabasasoft@gmail.com</a></li>"
        "<li><b>Web:</b> <a href=\"https://anabasasoft.github.io\" style=\"color: #A6E3E9;\">anabasasoft.github.io</a></li>"
        "<li><b>GitHub:</b> <a href=\"https://github.com/anabasasoft\" style=\"color: #A6E3E9;\">github.com/anabasasoft</a></li>"
        "</ul>"

        "<hr>"
        "<p><i>Desarrollado con Qt6 y C++ en Manjaro Linux. &copy; AnabasaSoft</i></p>"
    );

    textoAyuda->setHtml(contenido);
    layout->addWidget(textoAyuda);

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
            crearNuevaPestana(tr("Migración %1").arg(lenguaje), "", "");

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
