# Estado del código de reconocimiento

Última actualización: 17 de noviembre de 2026.

## Resumen
El sistema funciona así: el teléfono Android abre la página en Chrome, la cámara reconoce la tarjeta ArUco, se
calcula el nivel de picor (0–10) y se envía `N:x\n` al Arduino Leonardo/Micro conectado por **cable USB con
adaptador OTG** (WebUSB, la misma conexión que se usó en R2 con Protobject).

## Archivos
| Archivo | Para qué sirve | Estado |
|---|---|---|
| `index.html` | Página del teléfono: cámara, detección, panel y conexión por cable | Completo |
| `ajies.json` | Tabla ID → nombre → SHU, y `shu_max` (= nivel 10) | **Valores de ejemplo** |
| `arduino/receptor_webusb/receptor_webusb.ino` | Sketch de referencia para el Leonardo/Micro | Completo, sin compilar |

## Qué hace `index.html`
1. **Carga `ajies.json`.** Si falla, muestra "Tabla no cargada".
2. **Abre la cámara trasera** y procesa cada cuadro reducido a 640 px de ancho con js-aruco2 (diccionario 4x4,
   compatible con DICT_4X4_50).
3. **Dibuja los marcadores:** contorno verde para los IDs de la tabla y gris para los que no están (esos se ignoran).
   Si hay varias tarjetas, usa la más grande.
4. **Estabilidad:** acepta un ID después de 4 cuadros seguidos y envía solo cuando cambia el nivel. Después de 1,5 s
   sin tarjeta, envía `N:0`.
5. **Panel:** muestra nombre, ID, SHU, nivel X/10 y una barra de verde a rojo.
6. **Botón "Conectar Arduino (cable USB)":** se conecta por WebUSB, reenvía el nivel actual al conectar y muestra en
   el registro lo que responde la placa ("Arduino dice: Nivel recibido: x"). Si se toca otra vez, desconecta. También
   detecta cuando se desenchufa el cable.
7. **"Modo sin Arduino":** solo anota en el registro lo que se habría enviado. Se activa solo si el navegador no
   tiene WebUSB (iPhone, Safari, Firefox).

Parámetros que se pueden ajustar al inicio del `<script>`: `CUADROS_PARA_ACEPTAR` (4), `MS_SIN_TARJETA` (1500),
`ANCHO_PROCESO` (640), `FILTROS_USB` y `maxHammingDistance` del detector (1 = solo lecturas exactas; si detecta
poco, probar con 2).

## Qué hace el sketch `receptor_webusb.ino`
- Recibe `N:x\n` por WebUSB (desde el teléfono) y por el Monitor Serial (para probar sin teléfono escribiendo `N:5`).
- Responde "Nivel recibido: x" y enciende el LED integrado (pin 13) cuando el nivel es mayor que 0.
- `aplicarNivel(nivel)` está vacía (`TODO`): ahí van los LEDs y la vibración.

## Qué está verificado
- El JavaScript de `index.html` no tiene errores de sintaxis (`node --check`).
- El formato de `ajies.json` es válido.
- La fórmula da estos niveles para los IDs 0–24 de la tabla de ejemplo (los 25 ajíes de R2):
  `0,3,4,4,5,5,5,5,6,6,6,7,7,7,7,7,7,7,8,8,8,9,9,10,10`.
- js-aruco2 trae el diccionario 4x4 de OpenCV, y los IDs 0–49 coinciden con DICT_4X4_50 (revisado en el código
  fuente).
- La conexión WebUSB sigue los pasos del demo oficial de la librería WebUSB para Arduino, que también usa Protobject.

## Qué NO está verificado
- **La detección en un teléfono real** con tarjetas impresas.
- **Que js-aruco2 lea los mismos IDs que OpenCV.** Se dedujo del código fuente; la prueba cruzada no se pudo ejecutar.
- **La conexión por cable con una placa real**, y el sketch (no se compiló).
- **El modelo exacto de la placa.** Se dedujo de R2 que es Leonardo o Micro; falta mirarla.

## Pendientes
1. **Confirmar la placa:** debería decir Leonardo o Micro. Un Uno no sirve con esta versión.
2. **Preparar el IDE de Arduino**, si no quedó hecho en R2: instalar la librería WebUSB y cambiar `USB_VERSION` de
   `0x200` a `0x210` en `USBCore.h`. Los pasos están en el comentario del sketch.
3. **Subir `receptor_webusb.ino`** a la placa. Reemplaza el firmware de Protobject.
4. **Reemplazar la tabla de ejemplo** de `ajies.json` por los ajíes reales y sus IDs (0–49), y ajustar `shu_max`.
5. **Generar e imprimir las tarjetas** en chev.me (diccionario 4x4, papel mate, de al menos 4–5 cm, con margen blanco).
6. **Volver a subir `index.html` y `ajies.json`** a GitHub Pages: la versión publicada todavía puede tener el botón
   de Bluetooth.
7. **Probar por etapas:** primero la detección en "Modo sin Arduino"; luego la conexión por cable (ver
   "Arduino dice: …" y el LED 13); al final, las luces y la vibración que programe el compañero en `aplicarNivel()`.

## Versiones descartadas
La versión A (web + Bluetooth, para ESP32 o Nano 33 BLE) y la versión B (Python + OpenCV + USB serial en un
computador) se eliminaron a pedido del grupo, porque la idea es que el Arduino vaya conectado directo al teléfono y
la placa de R2 no tiene Bluetooth.
