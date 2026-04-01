cmake_minimum_required(VERSION 3.16)
project(CobolWorks VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Encontramos Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

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
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets qscintilla2_qt6)
