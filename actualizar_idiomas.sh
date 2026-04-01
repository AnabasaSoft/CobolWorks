#!/bin/bash

# --- CONFIGURACIÓN ---
LUPDATE="/usr/lib/qt6/bin/lupdate"
LRELEASE="/usr/lib/qt6/bin/lrelease"
CARPETA_BUILD="build"
CARPETA_IDIOMAS="$CARPETA_BUILD/idiomas"

VERDE='\033[0;32m'
AZUL='\033[0;34m'
ROJO='\033[0;31m'
NC='\033[0m'

echo -e "${AZUL}--- Gestor Dinámico de Idiomas CobolWorks ---${NC}"

if [ ! -f "$LUPDATE" ] || [ ! -f "$LRELEASE" ]; then
    echo -e "${ROJO}Error: Herramientas Qt6 no encontradas.${NC}"
    exit 1
fi

# 1. Si el usuario pasa un idioma (ej: ./actualizar_idiomas.sh fr_FR)
if [ ! -z "$1" ]; then
    NUEVO_TS="$1.ts"
    echo -e "${VERDE}>> Creando nuevo idioma: $1${NC}"
    # Ejecutamos lupdate forzando la creación de este archivo concreto
    $LUPDATE *.cpp *.h -ts "$NUEVO_TS"
fi

# 2. Escanear código fuente y actualizar TODOS los .ts existentes
echo -e "${VERDE}>> Actualizando estructuras XML de todos los idiomas...${NC}"
$LUPDATE *.cpp *.h -ts *.ts

# 3. Pasar por el traductor automático de Python
echo -e "${VERDE}>> Ejecutando traductor automático de Google...${NC}"
for ts_file in *.ts; do
    # Evitar errores si no hay ningún archivo .ts en la carpeta
    if [ -f "$ts_file" ]; then
        python auto_traductor.py "$ts_file"
    fi
done

# 4. Crear carpeta de destino
if [ ! -d "$CARPETA_IDIOMAS" ]; then
    mkdir -p "$CARPETA_IDIOMAS"
fi

# 5. Compilar a binarios .qm
echo -e "${VERDE}>> Compilando diccionarios a formato .qm...${NC}"
for ts_file in *.ts; do
    if [ -f "$ts_file" ]; then
        qm_file="${ts_file%.ts}.qm"
        $LRELEASE "$ts_file" -qm "$CARPETA_IDIOMAS/$qm_file"
        echo -e "   [OK] $ts_file -> $CARPETA_IDIOMAS/$qm_file"
    fi
done

echo -e "${AZUL}--- ¡Proceso finalizado con éxito! ---${NC}"