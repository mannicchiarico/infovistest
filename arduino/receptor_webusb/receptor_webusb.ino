/*
  RECEPTOR DE REFERENCIA: Arduino Leonardo / Micro conectado al TELÉFONO por cable (adaptador OTG)

  Recibe "N:x\n" (x = nivel de 0 a 10) desde index.html ("Conectar Arduino (cable USB)") usando WebUSB.
  Para probar las luces sin teléfono: conecten la placa al computador, abran el Monitor Serial (9600, "Nueva línea")
  y escriban N:5
  Responde "Nivel recibido: x" (se ve en el registro de la página como "Arduino dice: ...")
  y enciende el LED de la placa (pin 13) cuando el nivel es mayor que 0, para comprobar que llega sin computador.
  Las luces y la vibración van en aplicarNivel().

  - Placa: Arduino Leonardo o Micro (chip ATmega32U4). El Uno NO sirve para esto (no puede usar WebUSB).
  - Es el mismo tipo de conexión que usaba Protobject en R2. Este sketch REEMPLAZA el firmware de Protobject.

  INSTALACIÓN (una sola vez; si en R2 ya lo hicieron para Protobject, sáltense los pasos que ya estén hechos)
  1) Librería WebUSB: descarguen https://github.com/webusb/arduino (Code > Download ZIP), descomprímanlo y
     copien la carpeta  library/WebUSB  dentro de la carpeta "libraries" de su sketchbook
     (normalmente Documentos/Arduino/libraries). Reinicien el IDE.
  2) Cambiar la versión USB de la placa: abran el archivo USBCore.h del paquete "Arduino AVR Boards":
       Windows: %LOCALAPPDATA%\Arduino15\packages\arduino\hardware\avr\<versión>\cores\arduino\USBCore.h
       Mac:     ~/Library/Arduino15/packages/arduino/hardware/avr/<versión>/cores/arduino/USBCore.h
       Linux:   ~/.arduino15/packages/arduino/hardware/avr/<versión>/cores/arduino/USBCore.h
     y cambien la línea   #define USB_VERSION 0x200   por   #define USB_VERSION 0x210
     OJO: después de este cambio, todos los sketches para Leonardo deben incluir la librería WebUSB
     (como este), o Windows podría dejar de reconocer la placa.
  3) Suban este sketch (placa: "Arduino Leonardo").

  ERRORES COMUNES
  - La página dice "La placa no tiene la interfaz WebUSB": el sketch no se subió o falta el paso 2.
  - No aparece en la lista del teléfono: revisen el adaptador OTG y que el cable transmita datos (no solo carga).
  - Para volver a subir el sketch, desconecten la placa del teléfono y conéctenla al computador.
*/

#include <WebUSB.h>

// Página sugerida para la placa (sin "https://"). En Android no se usa, pero la librería la pide.
// Pueden poner la dirección de su GitHub Pages, ej. "usuario.github.io/repositorio/"
WebUSB WebUSBSerial(1 /* https:// */, "webusb.github.io/arduino/demos/console");

const int PIN_LED = 13;   // LED integrado del Leonardo

String bufferWeb = "";   // caracteres recibidos por WebUSB (teléfono) hasta el "\n"
String bufferUSB = "";   // caracteres recibidos por el Monitor Serial (pruebas) hasta el "\n"


// Convierte una línea "N:7" en el número 7. Devuelve -1 si el mensaje no es válido.
int interpretarLinea(String linea) {
  linea.trim();                          // quita espacios y "\r"
  if (!linea.startsWith("N:")) return -1;
  int nivel = linea.substring(2).toInt();
  if (nivel < 0 || nivel > 10) return -1;
  return nivel;
}


// ===== AQUÍ VA LO DEL COMPAÑERO: LEDs y vibración según el nivel (0 = todo apagado) =====
void aplicarNivel(int nivel) {
  // Confirmación: se responde por donde haya alguien escuchando
  if (WebUSBSerial) {
    WebUSBSerial.print("Nivel recibido: ");
    WebUSBSerial.println(nivel);
    WebUSBSerial.flush();
  }
  if (Serial) {
    Serial.print("Nivel recibido: ");
    Serial.println(nivel);
  }

  // Prueba visible: LED integrado encendido si hay picor
  digitalWrite(PIN_LED, nivel > 0 ? HIGH : LOW);

  // TODO: encender LEDs / motor de vibración según "nivel"
}


// Agrega un carácter al buffer; cuando llega "\n", interpreta la línea completa
void agregarCaracter(String &buffer, char c) {
  if (c == '\n') {
    int nivel = interpretarLinea(buffer);
    if (nivel >= 0) aplicarNivel(nivel);
    buffer = "";
  } else if (buffer.length() < 20) {
    buffer += c;
  }
}


void setup() {
  pinMode(PIN_LED, OUTPUT);
  WebUSBSerial.begin(9600);   // canal WebUSB (teléfono / navegador)
  Serial.begin(9600);         // USB normal (Monitor Serial, solo para pruebas)
}


void loop() {
  // 1) Mensajes desde el teléfono (index.html por cable)
  while (WebUSBSerial && WebUSBSerial.available() > 0) {
    agregarCaracter(bufferWeb, (char)WebUSBSerial.read());
  }

  // 2) Mensajes escritos en el Monitor Serial (pruebas desde el computador)
  while (Serial.available() > 0) {
    agregarCaracter(bufferUSB, (char)Serial.read());
  }
}
