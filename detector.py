"""
VERSIÓN B: Python + USB serial. Detecta tarjetas ArUco (4x4_50) y envía "N:x\n" al Arduino.

CÓMO USARLO
  1) Instalar (una vez):        pip install -r requirements.txt
  2) Probar sin Arduino:        python detector.py --sin-arduino
  3) Con Arduino:               python detector.py --puerto COM3          (Windows)
                                python detector.py --puerto /dev/ttyUSB0  (Linux; en Mac: /dev/cu.usbserial-...)
  4) Ver puertos disponibles:   python detector.py --listar-puertos
  Cámara:  --camara 0 (webcam), --camara 1 (otra cámara, o DroidCam instalado como webcam virtual)
           --camara http://192.168.1.50:4747/video   (app DroidCam por wifi)
           --camara http://192.168.1.50:8080/video   (app IP Webcam por wifi)
  También pueden cambiar los valores en CONFIGURACIÓN (abajo). La tabla de ajíes está en ajies.json.
  Para salir: tecla q o ESC sobre la ventana del video.

ERRORES COMUNES
  - "Access denied" / "Permission denied" / "puerto ocupado": cierren el Monitor Serial del IDE de Arduino.
    Python y el Monitor Serial no pueden usar el puerto a la vez. Tampoco se puede subir un sketch mientras esto corre.
    (En Linux, si sigue sin permiso:  sudo usermod -a -G dialout $USER  y volver a iniciar sesión.)
  - "No se pudo abrir la cámara": prueben otro número (--camara 1, 2...), cierren otros programas que usen la cámara
    (Zoom, Meet, DroidCam Client) o revisen que el teléfono y el computador estén en la misma red wifi.
  - "module 'cv2' has no attribute 'aruco'": instalen opencv-contrib-python, no opencv-python
    (pip uninstall opencv-python  y luego  pip install opencv-contrib-python).
"""

import argparse
import json
import math
import time
import unicodedata
from pathlib import Path

import cv2
import serial
import serial.tools.list_ports

# =====================================================================
# CONFIGURACIÓN (los argumentos de la línea de comandos tienen prioridad)
# =====================================================================

CAMARA = 0              # 0, 1, ... = webcam;  o una URL, ej. "http://192.168.1.50:4747/video"
PUERTO = None           # ej. "COM3" o "/dev/ttyUSB0". None = preguntar al iniciar
BAUDIOS = 9600          # debe ser igual a Serial.begin(...) del Arduino
SIN_ARDUINO = False     # True = solo imprime los mensajes en la consola

CUADROS_PARA_ACEPTAR = 4      # cuadros seguidos con el mismo ID antes de aceptarlo (3-5)
SEGUNDOS_SIN_TARJETA = 1.5    # segundos sin marcador antes de enviar "N:0"

ARCHIVO_AJIES = Path(__file__).with_name("ajies.json")   # mismo archivo que usa index.html


# =====================================================================
# 1. TABLA DE AJÍES Y NIVEL
# =====================================================================

def cargar_tabla():
    """Lee ajies.json y devuelve ({id: (nombre, shu)}, shu_max)."""
    with open(ARCHIVO_AJIES, encoding="utf-8") as archivo:
        datos = json.load(archivo)

    tabla = {}
    for aji in datos["ajies"]:
        tabla[int(aji["id"])] = (aji["nombre"], aji["shu"])

    # Si viene "shu_max" se usa ese; si no, el SHU más alto de la tabla
    shu_max = datos.get("shu_max") or max(shu for _, shu in tabla.values()) or 1
    return tabla, shu_max


def calcular_nivel(shu, shu_max):
    """Escala logarítmica de 0 a 10 (misma fórmula que index.html)."""
    nivel = round(10 * math.log10(shu + 1) / math.log10(shu_max + 1))
    return max(0, min(10, nivel))   # por si algún SHU supera shu_max


def sin_tildes(texto):
    """OpenCV no puede dibujar tildes ni ñ en la ventana: 'Jalapeño' -> 'Jalapeno'."""
    return unicodedata.normalize("NFKD", texto).encode("ascii", "ignore").decode("ascii")


# =====================================================================
# 2. DETECTOR ARUCO (funciona con OpenCV nuevo y antiguo)
# =====================================================================

def crear_detector():
    """Devuelve una función detectar(imagen_gris) -> (esquinas, ids)."""
    diccionario = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_4X4_50)

    if hasattr(cv2.aruco, "ArucoDetector"):
        # API nueva (OpenCV 4.7 o superior)
        detector = cv2.aruco.ArucoDetector(diccionario, cv2.aruco.DetectorParameters())

        def detectar(gris):
            esquinas, ids, _ = detector.detectMarkers(gris)
            return esquinas, ids
    else:
        # API antigua (OpenCV 4.6 o anterior)
        parametros = cv2.aruco.DetectorParameters_create()

        def detectar(gris):
            esquinas, ids, _ = cv2.aruco.detectMarkers(gris, diccionario, parameters=parametros)
            return esquinas, ids

    return detectar


# =====================================================================
# 3. PUERTO SERIAL (USB)
# =====================================================================

def listar_puertos():
    puertos = list(serial.tools.list_ports.comports())
    if not puertos:
        print("No se encontró ningún puerto serial. ¿Está conectado el Arduino por USB?")
    for i, p in enumerate(puertos):
        print(f"  [{i}] {p.device}  -  {p.description}")
    return puertos


def abrir_serial(puerto, baudios):
    """Abre el puerto. Si no se indicó uno, muestra la lista y pregunta.
    Devuelve el objeto Serial, o None para trabajar sin Arduino."""
    if puerto is None:
        print("No se indicó puerto. Puertos disponibles:")
        puertos = listar_puertos()
        if not puertos:
            print("Se continúa en MODO SIN ARDUINO.")
            return None
        eleccion = input("Escribe el número del puerto (o Enter para seguir sin Arduino): ").strip()
        if not eleccion.isdigit() or int(eleccion) >= len(puertos):
            print("Se continúa en MODO SIN ARDUINO.")
            return None
        puerto = puertos[int(eleccion)].device

    try:
        conexion = serial.Serial(puerto, baudios, timeout=0)
    except serial.SerialException as error:
        print(f"\nNo se pudo abrir {puerto}: {error}")
        print("¿Está abierto el Monitor Serial del IDE de Arduino? Ciérrenlo y vuelvan a intentar.")
        print("Puertos disponibles:")
        listar_puertos()
        raise SystemExit(1)

    print(f"Conectado a {puerto} a {baudios} baudios. Esperando que el Arduino reinicie...")
    time.sleep(2)   # muchas placas (Uno, Nano) se reinician al abrir el puerto
    return conexion


def enviar_nivel(conexion, nivel):
    """Envía "N:x\n" al Arduino, o lo imprime si no hay Arduino."""
    mensaje = f"N:{nivel}\n"
    if conexion is None:
        print(f"(sin Arduino) habría enviado: N:{nivel}")
    else:
        conexion.write(mensaje.encode("ascii"))
        print(f"Enviado: N:{nivel}")


def mostrar_respuestas(conexion):
    """Imprime lo que el Arduino escriba con Serial.println (útil para depurar)."""
    if conexion is not None and conexion.in_waiting:
        texto = conexion.read(conexion.in_waiting).decode("ascii", errors="ignore")
        for linea in texto.splitlines():
            if linea.strip():
                print("  Arduino dice:", linea.strip())


# =====================================================================
# 4. DIBUJAR EN LA VENTANA
# =====================================================================

def dibujar_panel(cuadro, id_aceptado, tabla, shu_max, texto_estado):
    """Dibuja el nombre, SHU, nivel y una barra de verde a rojo en la parte de arriba."""
    alto, ancho = cuadro.shape[:2]
    cv2.rectangle(cuadro, (0, 0), (ancho, 110), (30, 30, 30), -1)

    if id_aceptado is None:
        nivel = 0
        cv2.putText(cuadro, "Ninguna tarjeta", (12, 38), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 2)
    else:
        nombre, shu = tabla[id_aceptado]
        nivel = calcular_nivel(shu, shu_max)
        cv2.putText(cuadro, sin_tildes(nombre), (12, 38), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 255, 255), 2)
        detalle = f"ID {id_aceptado}   {shu:,} SHU   Nivel {nivel}/10".replace(",", ".")
        cv2.putText(cuadro, detalle, (12, 68), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (200, 200, 200), 1)

    # Barra: de verde (nivel 0) a rojo (nivel 10). OpenCV usa colores en orden B, G, R
    rojo = int(255 * nivel / 10)
    verde = int(255 * (1 - nivel / 10))
    cv2.rectangle(cuadro, (12, 80), (ancho - 12, 98), (70, 70, 70), -1)
    largo = int((ancho - 24) * nivel / 10)
    if largo > 0:
        cv2.rectangle(cuadro, (12, 80), (12 + largo, 98), (0, verde, rojo), -1)

    cv2.putText(cuadro, texto_estado, (12, alto - 12), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 255, 255), 1)


# =====================================================================
# 5. PROGRAMA PRINCIPAL
# =====================================================================

def leer_argumentos():
    parser = argparse.ArgumentParser(description="Detector de tarjetas ArUco de ajíes -> Arduino por USB")
    parser.add_argument("--camara", default=None, help="número de webcam (0, 1...) o URL de cámara IP")
    parser.add_argument("--puerto", default=None, help="puerto serial, ej. COM3 o /dev/ttyUSB0")
    parser.add_argument("--baudios", type=int, default=None, help="baudios (por defecto 9600)")
    parser.add_argument("--sin-arduino", action="store_true", help="no enviar, solo imprimir los mensajes")
    parser.add_argument("--listar-puertos", action="store_true", help="mostrar los puertos y salir")
    return parser.parse_args()


def main():
    args = leer_argumentos()
    if args.listar_puertos:
        listar_puertos()
        return

    # Configuración final: argumento si se dio, si no el valor de arriba
    camara = args.camara if args.camara is not None else CAMARA
    if isinstance(camara, str) and camara.isdigit():
        camara = int(camara)   # "0" -> 0 (webcam)
    baudios = args.baudios or BAUDIOS
    sin_arduino = args.sin_arduino or SIN_ARDUINO

    tabla, shu_max = cargar_tabla()
    print(f"Tabla cargada: {len(tabla)} ajíes, SHU máximo = {shu_max}")
    detectar = crear_detector()

    conexion = None if sin_arduino else abrir_serial(args.puerto or PUERTO, baudios)
    if conexion is None:
        print("MODO SIN ARDUINO: los mensajes solo se imprimen aquí.")

    print(f"Abriendo cámara: {camara}")
    captura = cv2.VideoCapture(camara)
    captura.set(cv2.CAP_PROP_BUFFERSIZE, 1)   # menos retraso con cámaras IP
    if not captura.isOpened():
        print("No se pudo abrir la cámara. Prueben --camara 1 (u otro número) o revisen la URL / el wifi.")
        raise SystemExit(1)

    # Estado de la detección
    candidato_id = None        # ID que se está viendo ahora
    cuadros_seguidos = 0       # cuántos cuadros seguidos lleva ese ID
    id_aceptado = None         # ID confirmado (el que se muestra)
    nivel_enviado = None       # último nivel enviado (para enviar solo cuando cambia)
    ultima_deteccion = time.time()
    fallos_seguidos = 0

    try:
        while True:
            ok, cuadro = captura.read()
            if not ok:
                fallos_seguidos += 1
                if fallos_seguidos > 50:
                    print("La cámara dejó de enviar imágenes.")
                    break
                time.sleep(0.05)
                continue
            fallos_seguidos = 0

            # a) Detectar marcadores
            gris = cv2.cvtColor(cuadro, cv2.COLOR_BGR2GRAY)
            esquinas, ids = detectar(gris)
            if ids is not None:
                cv2.aruco.drawDetectedMarkers(cuadro, esquinas, ids)   # contorno + ID

            # b) Quedarse solo con los IDs que están en la tabla
            validos = []
            if ids is not None:
                for esq, id_marcador in zip(esquinas, ids.flatten()):
                    if int(id_marcador) in tabla:
                        validos.append((esq, int(id_marcador)))

            # c) Lógica de estabilidad
            ahora = time.time()
            texto_estado = ""
            if validos:
                # Si hay varias tarjetas, se usa la más grande (la más cercana)
                esq, id_visto = max(validos, key=lambda v: cv2.contourArea(v[0].reshape(-1, 2)))
                ultima_deteccion = ahora

                if id_visto == candidato_id:
                    cuadros_seguidos += 1
                else:
                    candidato_id = id_visto
                    cuadros_seguidos = 1

                # Se acepta el ID cuando lleva suficientes cuadros seguidos
                if cuadros_seguidos >= CUADROS_PARA_ACEPTAR and id_visto != id_aceptado:
                    id_aceptado = id_visto
                    nombre, shu = tabla[id_aceptado]
                    nivel = calcular_nivel(shu, shu_max)
                    print(f"Tarjeta aceptada: ID {id_aceptado} - {nombre} ({shu} SHU) -> nivel {nivel}")
                    if nivel != nivel_enviado:       # solo se envía cuando cambia el nivel
                        enviar_nivel(conexion, nivel)
                        nivel_enviado = nivel

                if id_visto != id_aceptado:
                    texto_estado = f"Detectando ID {id_visto}... ({cuadros_seguidos}/{CUADROS_PARA_ACEPTAR})"
            else:
                # Ningún marcador en este cuadro
                candidato_id = None
                cuadros_seguidos = 0
                if ahora - ultima_deteccion > SEGUNDOS_SIN_TARJETA and nivel_enviado != 0:
                    id_aceptado = None
                    enviar_nivel(conexion, 0)        # "N:0" apaga todo
                    nivel_enviado = 0

            # d) Mostrar lo que responda el Arduino
            mostrar_respuestas(conexion)

            # e) Mostrar la ventana
            dibujar_panel(cuadro, id_aceptado, tabla, shu_max, texto_estado)
            cv2.imshow("Detector de ajies  (q o ESC para salir)", cuadro)
            tecla = cv2.waitKey(1) & 0xFF
            if tecla in (ord("q"), 27):
                break

    except KeyboardInterrupt:
        pass   # Ctrl+C también cierra bien

    finally:
        # Al salir: apagar todo en el Arduino y liberar la cámara y el puerto
        if conexion is not None:
            conexion.write(b"N:0\n")
            conexion.close()
        captura.release()
        cv2.destroyAllWindows()
        print("Listo.")


if __name__ == "__main__":
    main()
