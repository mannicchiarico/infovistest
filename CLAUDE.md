# Reconocimiento de tarjetas de ajíes (E2, IIC2026 Visualización de Información)

## Proyecto
Fisicalización del picor de ajíes según la escala Scoville. Cada ají tiene una tarjeta física: por un lado una foto
y por el otro un marcador ArUco. El teléfono reconoce el marcador con su cámara y le envía al Arduino un nivel de
picor; el Arduino reacciona con LEDs y vibración. La programación de luces y vibración la hace otro integrante del
grupo; esta carpeta solo cubre el reconocimiento y el envío del mensaje.

El estado detallado del código y lo que falta están en [ESTADO.md](ESTADO.md).

## Arquitectura actual: solo la "versión C"
Teléfono Android (Chrome) → página web → **cable USB con adaptador OTG (WebUSB)** → Arduino Leonardo/Micro.
- `index.html`: página única (HTML + CSS + JS), con la detección y la conexión WebUSB.
- `ajies.json`: tabla ID → nombre → SHU, más `shu_max`. Se carga con `fetch`, así que tiene que estar junto a `index.html`.
- `arduino/receptor_webusb/receptor_webusb.ino`: sketch de referencia para el compañero. Recibe por WebUSB y,
  para pruebas, también por el Monitor Serial. Imprime el nivel, enciende el LED 13 y deja `aplicarNivel()` vacía.

Historia: también existieron una versión A (web + BLE, para ESP32/Nano 33 BLE) y una versión B (Python + OpenCV +
pyserial). **El usuario pidió eliminarlas**: no las reintroduzcas salvo que lo pida.

## Contrato fijo con el Arduino (no cambiar sin acuerdo del grupo)
- Mensaje ASCII `"N:x\n"`, con x entero de 0 a 10. `"N:0\n"` = no hay tarjeta (apaga todo).
- El nivel se calcula en el teléfono: `nivel = round(10 * log10(SHU + 1) / log10(SHU_max + 1))`, limitado a 0–10.
  Se usa `shu_max` de ajies.json o, si falta, el SHU máximo de la tabla.
- Estabilidad: un ID se acepta tras 4 cuadros seguidos (`CUADROS_PARA_ACEPTAR`); solo se envía cuando cambia el
  nivel; tras 1,5 s sin marcador (`MS_SIN_TARJETA`) se envía `N:0`. Al conectar, se reenvía el nivel actual.

## Decisiones técnicas verificadas
- **Marcadores**: ArUco DICT_4X4_50, IDs 0–24 (25 ajíes de ejemplo; el diccionario admite hasta el 49), generados en https://chev.me/arucogen/ con el diccionario "4x4".
  En papel mate, de al menos 4–5 cm y con margen blanco.
- **Librería JS**: js-aruco2 2.0.0 desde jsDelivr (`cv.js`, `aruco.js`, `dictionaries/aruco_4x4_1000.js`, en ese orden).
  Su `ARUCO_4X4_1000` fue extraído de `predefined_dictionaries.hpp` de OpenCV. OpenCV usa la misma lista de bytes para
  4x4_50/100/250/1000, así que los IDs 0–49 coinciden. js-aruco2 acepta un código si `distancia < tau`;
  `maxHammingDistance: 1` = solo coincidencias exactas. Además se ignoran los IDs que no están en la tabla.
- **Placa**: inferida desde R2 (`../iic2026-grupo-7-E2/docs/R2/app.js`, que usa Protobject). Protobject se conecta por WebUSB y filtra los
  vendorId 0x2341 (productos 0x8036 Leonardo, 0x8037 Micro, MKR/Zero) y 0x239A (Adafruit). El Uno no está en la
  lista. Conclusión: Leonardo o Micro (ATmega32U4). **El usuario aún no lo ha confirmado mirando la placa.**
- **WebUSB**: el código replica el demo oficial de https://github.com/webusb/arduino (`demos/serial.js`): buscar la
  interfaz de clase 0xFF, `claimInterface`, `selectAlternateInterface`, `controlTransferOut` con request 0x22 y
  value 1 (DTR; Protobject hace lo mismo) y `transferIn`/`transferOut` de 64 bytes.
- **Sketch**: necesita la librería WebUSB (copiar `library/WebUSB` del repo al sketchbook; no está en el Library
  Manager) y cambiar `#define USB_VERSION 0x200` a `0x210` en `USBCore.h` del core Arduino AVR. `operator bool` de
  `WebUSB` no tiene delay; el de `Serial` (CDC) tiene `delay(10)`, así que no se llama en cada `loop()`.
- **Web Serial en Android** por cable sigue siendo experimental (Chrome 148 beta, Android 17); por eso se usa WebUSB.
- **Navegadores**: WebUSB solo funciona en Chrome/Edge (en Android o en el computador). En iPhone, Safari y Firefox no
  existe: la página desactiva el botón y activa el "Modo sin Arduino". La cámara exige HTTPS o localhost.

## Despliegue y pruebas
- El usuario aloja la página en GitHub Pages (repositorio propio, aparte). Hay que subir `index.html` y `ajies.json`
  a la misma carpeta. Tras cada cambio, el usuario tiene que volver a subir `index.html`.
- Pruebas locales: `python3 -m http.server 8000` en esta carpeta + `http://localhost:8000`. Para el teléfono sin
  hosting: `chrome://inspect/#devices` → "Port forwarding..." 8000 → localhost:8000.
- El "Modo sin Arduino" muestra en el registro los mensajes que se habrían enviado.

## Cómo trabajar aquí
- Responder en español. Los usuarios son estudiantes de diseño: código simple, muy comentado, con nombres en
  español, sin abstracciones innecesarias.
- Cada archivo lleva al inicio un comentario breve con cómo usarlo y los errores comunes. No crear guías aparte.
- **Solo código: no publicar nada** (ni artefactos, páginas, documentos, repositorios ni despliegues). El usuario
  ejecuta y aloja todo.
- Esta carpeta no es un repositorio git. El repositorio del grupo está en `../iic2026-grupo-7-E2/`; no hacer commits
  ahí salvo que el usuario lo pida.
- No hay hardware ni herramientas de Arduino en este entorno: el JS se puede revisar con `node --check` (extrayendo
  el `<script>` de `index.html`), pero el sketch no se puede compilar ni probar en vivo.
