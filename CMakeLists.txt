cmake_minimum_required(VERSION 3.16)
project(CobolWorks VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Encontramos Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Definimos el ejecutable
add_executable(${PROJECT_NAME}
    main.cpp
    mainwindow.cpp
    mainwindow.h
    cobollexer.cpp
    cobollexer.h
    recursos.qrc
)

# Forzamos explícitamente al compilador a mirar en la ruta de Qt6 de Manjaro
target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/qt6 /usr/include)

# Enlazamos Qt y la librería de QScintilla directamente por el nombre de su binario (.so)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network qscintilla2_qt6)

# --- REGLAS DE INSTALACIÓN PARA LINUX ---
# Le decimos que instale todo en la carpeta /opt/CobolWorks para no ensuciar el sistema
set(CMAKE_INSTALL_PREFIX "/opt/CobolWorks")

# Instalamos el ejecutable principal
install(TARGETS CobolWorks DESTINATION .)

# Instalamos la carpeta de idiomas (copia el contenido de build/idiomas a /opt/CobolWorks/idiomas)
install(DIRECTORY ${CMAKE_BINARY_DIR}/idiomas DESTINATION .)

# --- EMPAQUETADO CON CPACK PARA LINUX Y WINDOWS ---
set(CPACK_PACKAGE_NAME "CobolWorks")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "IDE moderno para COBOL")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AnabasaSoft")
set(CPACK_RPM_PACKAGE_AUTOREQPROV "ON")

if(LINUX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)
