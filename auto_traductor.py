import xml.etree.ElementTree as ET
from deep_translator import GoogleTranslator
import sys
import os

# Verificamos que se le pase el archivo como parámetro desde Bash
if len(sys.argv) < 2:
    print("Uso: python auto_traductor.py <archivo.ts>")
    sys.exit(1)

archivo_ts = sys.argv[1]

def traducir_ts():
    if not os.path.exists(archivo_ts):
        print(f"No se encontró el archivo {archivo_ts}")
        return

    print(f"Abriendo {archivo_ts} para traducir...")
    tree = ET.parse(archivo_ts)
    root = tree.getroot()
    
    # Extraemos el código de idioma para Google (ej: 'fr_FR.ts' -> 'fr')
    nombre_base = os.path.basename(archivo_ts).replace('.ts', '')
    codigo_google = nombre_base.split('_')[0].lower()

    # Excepción vital para el Chino Simplificado
    if nombre_base == "zh_CN":
        codigo_google = "zh-CN"

    try:
        # Configuramos el traductor: de Español al idioma detectado
        traductor = GoogleTranslator(source='es', target=codigo_google)
    except Exception as e:
        print(f"Error al configurar el idioma '{codigo_google}': {e}")
        return

    cambios = False

    # Buscamos y traducimos los mensajes sin terminar
    for message in root.findall('.//message'):
        source = message.find('source')
        translation = message.find('translation')

        if source is not None and source.text and translation is not None:
            if translation.get('type') == 'unfinished' or not translation.text:
                texto_original = source.text
                print(f"Traduciendo a {codigo_google}: '{texto_original}' ...")
                
                try:
                    texto_traducido = traductor.translate(texto_original)
                    
                    # Mantenemos los ampersands de los atajos de teclado
                    if '&' in texto_original and '&' not in texto_traducido:
                         texto_traducido = '&' + texto_traducido

                    translation.text = texto_traducido
                    translation.attrib.pop('type', None) # Lo marcamos como terminado
                    cambios = True
                except Exception as e:
                    print(f"Error al traducir '{texto_original}': {e}")

    if cambios:
        tree.write(archivo_ts, encoding='utf-8', xml_declaration=True)
        print(f"¡Traducción de {archivo_ts} completada con éxito!")
    else:
        print(f"El archivo {archivo_ts} ya estaba 100% traducido.")

if __name__ == "__main__":
    traducir_ts()
