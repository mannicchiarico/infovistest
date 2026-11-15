/*
  RECEPTOR DE REFERENCIA - Arduino Nano 33 BLE / Nano 33 IoT / Nano ESP32 / UNO R4 WiFi (librería ArduinoBLE)

  Recibe mensajes "N:x\n" (x = nivel de 0 a 10) por BLE (versión A) y también por USB (versión B),
  y los imprime por el Monitor Serial. Las luces y la vibración van en aplicarNivel().

  - Instalar en el IDE: Gestor de librerías > "ArduinoBLE".
  - Baudios: 9600 (igual que detector.py).
  - BLE: Nordic UART Service. Nombre: "Aji-Scoville". Los UUID deben coincidir con los de index.html.
  - Error común: si el Monitor Serial está abierto, detector.py no puede usar el puerto (y al revés).
*/

#include <ArduinoBLE.h>

// UUID del Nordic UART Service (los mismos de index.html)
BLEService servicio("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLECharacteristic rx("6E400002-B5A3-F393-E0A9-E50E24DCCA9E",   // aquí escribe el teléfono
                     BLEWrite | BLEWriteWithoutResponse, 20);

String bufferBLE = "";   // caracteres recibidos por BLE hasta el "\n"
String bufferUSB = "";   // caracteres recibidos por USB hasta el "\n"


// Convierte una línea "N:7" en el número 7. Devuelve -1 si el mensaje no es válido.
int interpretarLinea(String linea) {
  linea.trim();
  if (!linea.startsWith("N:")) return -1;
  int nivel = linea.substring(2).toInt();
  if (nivel < 0 || nivel > 10) return -1;
  return nivel;
}


// ===== AQUÍ VA LO DEL COMPAÑERO: LEDs y vibración según el nivel (0 = todo apagado) =====
void aplicarNivel(int nivel) {
  Serial.print("Nivel recibido: ");
  Serial.println(nivel);
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
  Serial.begin(9600);
  // Espera hasta 3 s a que se abra el puerto (para no perder los primeros mensajes)
  unsigned long inicio = millis();
  while (!Serial && millis() - inicio < 3000) {}

  if (!BLE.begin()) {
    Serial.println("Error: no se pudo iniciar BLE");
    while (true) {}
  }

  BLE.setLocalName("Aji-Scoville");
  BLE.setAdvertisedService(servicio);   // para que index.html lo encuentre
  servicio.addCharacteristic(rx);
  BLE.addService(servicio);
  BLE.advertise();

  Serial.println("Receptor listo (BLE + USB)");
}


void loop() {
  BLE.poll();   // atiende la conexión Bluetooth

  // 1) Mensajes por BLE (versión A, index.html)
  if (rx.written()) {
    int largo = rx.valueLength();
    const uint8_t* datos = rx.value();
    for (int i = 0; i < largo; i++) {
      agregarCaracter(bufferBLE, (char)datos[i]);
    }
  }

  // 2) Mensajes por USB (versión B, detector.py)
  while (Serial.available() > 0) {
    agregarCaracter(bufferUSB, (char)Serial.read());
  }
}
