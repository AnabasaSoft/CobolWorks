import sys
import json
import urllib.request
import urllib.error

if len(sys.argv) < 5:
    print("Error: Faltan argumentos.", file=sys.stderr)
    sys.exit(1)

# Limpiamos los argumentos por si llevan saltos de línea ocultos desde la terminal o el IDE
api_key = sys.argv[1].strip()
lenguaje = sys.argv[2].strip()
archivo_in = sys.argv[3].strip()
archivo_out = sys.argv[4].strip()

# --- FASE 1: AUTO-DESCUBRIMIENTO DEL MODELO ---
url_models = f"https://generativelanguage.googleapis.com/v1beta/models?key={api_key}"
modelo_elegido = ""

try:
    req_models = urllib.request.Request(url_models)
    with urllib.request.urlopen(req_models) as response:
        data_models = json.loads(response.read().decode('utf-8'))

        # Recorremos la lista que nos da Google buscando el mejor candidato
        for model in data_models.get('models', []):
            metodos = model.get('supportedGenerationMethods', [])
            nombre = model.get('name', '')

            # Buscamos uno que sea de la familia gemini y soporte generación de texto
            if 'generateContent' in metodos and 'gemini' in nombre and 'vision' not in nombre:
                modelo_elegido = nombre
                print(f"Modelo autodescubierto: {modelo_elegido}") # Lo imprimimos para chivarnos
                break

        if not modelo_elegido:
            print("Error: Tu API Key no tiene ningún modelo Gemini compatible asignado.", file=sys.stderr)
            sys.exit(1)

except urllib.error.HTTPError as e:
    error_info = e.read().decode('utf-8')
    print(f"Error HTTP al buscar modelos {e.code}: {error_info}", file=sys.stderr)
    sys.exit(1)
except Exception as e:
    print(f"Error de red al buscar modelos: {str(e)}", file=sys.stderr)
    sys.exit(1)


# --- FASE 2: TRADUCCIÓN CON EL MODELO ENCONTRADO ---
with open(archivo_in, 'r', encoding='utf-8') as f:
    codigo_cobol = f.read()

if lenguaje == "Explicación":
    prompt = f"Actúa como un programador experto en COBOL. Analiza este código y explica paso a paso qué hace, su flujo y variables. Usa párrafos cortos, listas y saltos de línea para que sea muy fácil de leer. Responde en español:\n\n{codigo_cobol}"
else:
    prompt = f"Traduce este código COBOL a {lenguaje} moderno. Solo devuelve el código resultante, sin explicaciones, ni formato markdown alrededor:\n\n{codigo_cobol}"

# Usamos el nombre del modelo que hemos descubierto (modelo_elegido ya incluye la parte 'models/')
url_generate = f"https://generativelanguage.googleapis.com/v1beta/{modelo_elegido}:generateContent?key={api_key}"

headers = {'Content-Type': 'application/json'}
data = {
    "contents": [{"parts": [{"text": prompt}]}],
    "safetySettings": [
        {"category": "HARM_CATEGORY_HARASSMENT", "threshold": "BLOCK_NONE"},
        {"category": "HARM_CATEGORY_HATE_SPEECH", "threshold": "BLOCK_NONE"},
        {"category": "HARM_CATEGORY_SEXUALLY_EXPLICIT", "threshold": "BLOCK_NONE"},
        {"category": "HARM_CATEGORY_DANGEROUS_CONTENT", "threshold": "BLOCK_NONE"}
    ]
}

req = urllib.request.Request(url_generate, data=json.dumps(data).encode('utf-8'), headers=headers, method='POST')

try:
    with urllib.request.urlopen(req) as response:
        respuesta_json = json.loads(response.read().decode('utf-8'))
        texto_generado = respuesta_json['candidates'][0]['content']['parts'][0]['text']

        texto_limpio = texto_generado.strip()

        # Solo intentamos limpiar los bloques de código si NO es una explicación
        if lenguaje != "Explicación" and texto_limpio.startswith("```"):
            lineas = texto_limpio.split('\n')
            texto_limpio = '\n'.join(lineas[1:-1])
            if texto_limpio.lower().startswith(lenguaje.lower()):
                texto_limpio = '\n'.join(texto_limpio.split('\n')[1:])

        with open(archivo_out, 'w', encoding='utf-8') as f:
            f.write(texto_limpio.strip())

        print("Traducción completada con éxito.")
        sys.exit(0)

except urllib.error.HTTPError as e:
    error_info = e.read().decode('utf-8')
    print(f"Error HTTP en traducción {e.code}: {error_info}", file=sys.stderr)
    sys.exit(1)
except Exception as e:
    print(f"Error parseando traducción: {str(e)}", file=sys.stderr)
    sys.exit(1)
