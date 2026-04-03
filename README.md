[![CobolWorks IDE Logo](https://github.com/AnabasaSoft/CobolWorks/raw/main/logo.png)](https://anabasasoft.github.io)

# 🚀 CobolWorks IDE

*El entorno de desarrollo moderno para el programador de COBOL de siempre.*

[![Website](https://img.shields.io/badge/Web-anabasasoft.github.io-blue?style=flat-square&logo=github)](https://anabasasoft.github.io)
[![Email](https://img.shields.io/badge/Contacto-anabasasoft%40gmail.com-red?style=flat-square&logo=gmail)](mailto:anabasasoft@gmail.com)
[![Platform](https://img.shields.io/badge/Plataforma-Linux%20%7C%20Windows-green?style=flat-square)](https://github.com/AnabasaSoft/CobolWorks)
[![License](https://img.shields.io/badge/Licencia-GPL--3.0-orange?style=flat-square)](LICENSE)
[![Version](https://img.shields.io/badge/Versión-1.0.0-informational?style=flat-square)](https://github.com/AnabasaSoft/CobolWorks/releases)

---

[![Captura de pantalla de CobolWorks IDE](https://github.com/AnabasaSoft/CobolWorks/raw/main/captura.png)](https://github.com/AnabasaSoft/CobolWorks/blob/main/captura.png)

---

CobolWorks no es solo un editor; es una **estación de trabajo completa** diseñada para revitalizar el mantenimiento y la migración de sistemas legacy. Desarrollado en **C++ y Qt6** con QScintilla para ofrecer el máximo rendimiento en entornos profesionales.

---

## ✨ Características Principales

### 🖊️ Editor de Código Profesional

| Función | Descripción |
|---|---|
| **Resaltado de Sintaxis COBOL** | Lexer personalizado (`CobolLexer`) con soporte completo para divisiones, secciones, cláusulas PIC, SQL embebido y CICS |
| **Autocompletado Inteligente** | Completa palabras clave COBOL (`PERFORM`, `COMPUTE`, `EXEC SQL`...) con umbral configurable a partir de 2 caracteres |
| **Líneas Guía COBOL** | Marcadores visuales en columnas 7, 11 y 72 para respetar el formato fijo de COBOL (activables/desactivables) |
| **Plegado de Código** | Estilo árbol con cajas (`BoxedTreeFoldStyle`) para colapsar divisiones y párrafos |
| **Detección de Llaves** | Resalta automáticamente pares de paréntesis y llaves (válido e inválido) |
| **Cursor Configurable** | Alterna entre cursor de barra clásico o cursor de bloque estilo mainframe |
| **Zoom** | Acercar (`Ctrl++`), alejar (`Ctrl+-`) y restaurar (`Ctrl+0`) el tamaño del texto |
| **Esquema de Color Oscuro** | Paleta profesional azul marino / cian integrada, sin configuración adicional |

### 📂 Gestión de Archivos y Pestañas

| Función | Descripción |
|---|---|
| **Múltiples Pestañas** | Edición simultánea de varios archivos COBOL con pestañas cerrables |
| **Navegación de Pestañas** | Salto rápido con `Ctrl+Tab` / `Ctrl+Shift+Tab` |
| **Cierre Seguro** | Pregunta guardar cambios al cerrar pestaña o salir de la aplicación |
| **Explorador de Archivos** | Panel izquierdo con árbol de directorios (raíz en home del usuario) |
| **Apertura Inteligente** | Evita duplicar pestañas si el archivo ya está abierto |
| **Plantilla Maestra** | Crea un nuevo archivo con estructura COBOL completa (todas las divisiones) desde el menú |
| **Persistencia de Sesión** | Guarda y restaura geometría de ventana y disposición de paneles entre sesiones |

### 🔍 Análisis y Navegación de Código

| Función | Descripción |
|---|---|
| **Ir a la Definición (F2)** | Salta a la declaración de la variable bajo el cursor en la WORKING-STORAGE SECTION |
| **Panel Estructura COBOL** | Lista en tiempo real todas las divisiones, secciones y párrafos del archivo actual; clic para navegar |
| **Panel Flujo de Ejecución** | Árbol interactivo de llamadas `PERFORM` y `GO TO`; doble clic salta a la línea correspondiente |
| **Panel Dependencias** | Detecta y lista todos los `COPY`, `CALL` y `EXEC SQL` del programa con navegación por doble clic |
| **Navegador de Funciones (Ctrl+P)** | Búsqueda rápida de párrafos y secciones del programa |
| **Tooltip de Variables** | Al pasar el ratón sobre una variable, muestra su definición PIC completa en un globo emergente |
| **Menú Contextual** | Clic derecho en el editor con opciones de contexto |

### 🤖 Integración con Inteligencia Artificial (Gemini)

| Función | Descripción |
|---|---|
| **Traducir a Python** | Envía el código COBOL seleccionado a la API de Gemini y muestra la traducción en Python |
| **Traducir a Java** | Traducción automática de rutinas COBOL a Java |
| **Explicar Código** | Pide a Gemini una explicación en lenguaje natural del código COBOL seleccionado |
| **Configuración de API Key** | Diálogo dedicado para introducir y guardar la clave de la API de Gemini |

### 🐞 Compilación y Depuración

| Función | Descripción |
|---|---|
| **Compilar (F6)** | Compila el archivo con `cobc` y muestra errores y advertencias con navegación por clic |
| **Compilar y Ejecutar (F5)** | Compila y lanza el programa en la consola integrada |
| **Depurador Visual (F8)** | Lanza GDB/LLDB con interfaz gráfica para depuración interactiva |
| **Puntos de Ruptura Visuales** | Clic en el margen izquierdo para añadir/quitar breakpoints (círculos rojos) |
| **Compilar en Docker** | Compila el programa dentro de un contenedor Docker para entornos aislados |
| **Configuración del Compilador** | Diálogo para definir flags personalizados de `cobc` y variables de entorno |
| **Consola Integrada** | Muestra la salida de compilación y ejecución con colores; doble clic en errores salta a la línea |
| **Limpiar Consola** | Botón dedicado para vaciar la salida de la consola |

### ⚡ Linter en Tiempo Real

| Función | Descripción |
|---|---|
| **Análisis Automático** | Ejecuta el linter 1 segundo después de dejar de escribir |
| **Subrayado Ondulado Rojo** | Marca los errores directamente en el editor con indicadores visuales (estilo INDIC_SQUIGGLE) |
| **Sin Interrupciones** | El temporizador se reinicia con cada pulsación de tecla para no molestar mientras se escribe |

### 🏢 Integraciones Enterprise

| Función | Descripción |
|---|---|
| **Git Status** | Comprueba el estado del repositorio Git del proyecto actual |
| **Generar Pipeline CI/CD** | Genera un archivo de workflow para GitHub Actions listo para usar |
| **Boilerplate API REST** | Genera código cliente para integración con servicios REST |
| **Soporte z/OS — Descargar Copybook Remoto** | Descarga copybooks desde un mainframe remoto vía configuración |
| **Generar JCL** | Genera un Job Control Language de simulación para entornos mainframe |

### 🌍 Internacionalización

| Función | Descripción |
|---|---|
| **Múltiples Idiomas** | Interfaz traducida a inglés (`en_US`), euskera (`eu_ES`) y francés (`fr_FR`), con sistema extensible |
| **Cambio en Caliente** | Cambia el idioma de la interfaz desde el menú sin reiniciar la aplicación |
| **Persistencia de Idioma** | El idioma seleccionado se recuerda entre sesiones |
| **Traductor Automático** | Scripts Python (`auto_traductor.py`, `ia_traductor.py`) para generar nuevas traducciones |

### 📋 Edición Avanzada

| Función | Descripción |
|---|---|
| **Buscar (Ctrl+F)** | Búsqueda incremental con vuelta al inicio |
| **Buscar y Sustituir (Ctrl+H)** | Diálogo no modal con sustitución individual y masiva ("Sustituir Todo"); agrupa cambios en un solo Ctrl+Z |
| **Tabulador Inteligente** | La tecla Tab inserta espacios alineados a las columnas COBOL (7, 11 y luego cada 4) en lugar de un tabulador |
| **Forzar Autocompletado (Ctrl+Space)** | Lanza el autocompletado en cualquier momento |
| **Copiar / Cortar / Pegar** | Accesos estándar del sistema operativo |

### 🔄 Actualizaciones Automáticas

- Comprueba silenciosamente si hay una nueva versión disponible al arrancar la aplicación.
- Notifica al usuario si la versión instalada queda desactualizada respecto al repositorio.

---

## 💰 ¿Quieres ahorrar tiempo? Compra el Binario Oficial

Este proyecto es **100% Código Abierto**. Puedes clonar el repositorio y compilarlo por tu cuenta siguiendo las instrucciones de abajo.

Sin embargo, si prefieres **evitar la configuración de compiladores** y quieres un instalador listo para usar — y de paso apoyar el desarrollo — puedes adquirir el binario oficial compilado y firmado para **Windows y Linux**:

👉 **[Descargar CobolWorks Pro (Ejecutable listo para usar)](https://gum.new/gum/cmnfzibk2000o04l232dybufq)**

---

## 🛠️ Cómo Compilar

### Requisitos previos

**Linux (Arch / Manjaro):**
```bash
sudo pacman -S qt6-base qscintilla-qt6 cmake gnu-cobol gdb
```

**Linux (Ubuntu / Debian):**
```bash
sudo apt install qt6-base-dev cmake gnucobol gdb
```

**Windows:** Instala [Qt6](https://www.qt.io/download), [CMake](https://cmake.org/download/) y [GnuCOBOL](https://gnucobol.sourceforge.io/) usando sus instaladores oficiales.

### Compilación

```bash
git clone https://github.com/anabasasoft/cobolworks.git
cd cobolworks
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Instalación

```bash
sudo cmake --install build
```

---

## ⌨️ Atajos de Teclado

| Acción | Atajo |
|---|---|
| Nuevo archivo | `Ctrl+N` |
| Abrir archivo | `Ctrl+O` |
| Guardar | `Ctrl+S` |
| Guardar como | `Ctrl+Shift+S` |
| Cerrar pestaña | `Ctrl+W` |
| Siguiente pestaña | `Ctrl+Tab` |
| Pestaña anterior | `Ctrl+Shift+Tab` |
| Buscar | `Ctrl+F` |
| Buscar y sustituir | `Ctrl+H` |
| Ir a la definición | `F2` |
| Navegador de funciones | `Ctrl+P` |
| Forzar autocompletado | `Ctrl+Space` |
| Compilar | `F6` |
| Compilar y ejecutar | `F5` |
| Depurar con GDB | `F8` |
| Zoom In | `Ctrl++` |
| Zoom Out | `Ctrl+-` |
| Restaurar Zoom | `Ctrl+0` |
| Ayuda / Acerca de | `F1` |

---

## 📬 Contacto y Soporte

¿Tienes dudas, encontraste un bug o quieres contribuir?

- 🌐 **Web:** [anabasasoft.github.io](https://anabasasoft.github.io)
- 📧 **Email:** [anabasasoft@gmail.com](mailto:anabasasoft@gmail.com)
- 🐛 **Issues:** Usa la pestaña [Issues](https://github.com/AnabasaSoft/CobolWorks/issues) de este repositorio

---

Hecho con ❤️ por [AnabasaSoft](https://anabasasoft.github.io)
