/*
  RECEPTOR DE REFERENCIA - ESP32 (sirve para la versión A por BLE y para la B por USB, al mismo tiempo)

  Recibe mensajes "N:x\n" (x = nivel de 0 a 10) y los imprime por el Monitor Serial.
  Las luces y la vibración se programan en la función aplicarNivel() (está vacía a propósito).

  - Placa: cualquier ESP32 con Bluetooth (ESP32 DevKit, etc.). OJO: el ESP32-S2 no tiene Bluetooth.
  - Instalar en el IDE: Gestor de placas > "esp32" de Espressif. La librería BLE viene incluida.
  - Baudios: 9600 (igual que detector.py).
  - BLE: Nordic UART Service. Nombre del dispositivo: "Aji-Scoville". Los UUID deben coincidir con los de index.html.
  - Error común: si el Monitor Serial está abierto, detector.py no puede usar el puerto (y al revés).
    Con la versión A (BLE) sí pueden tener el Monitor Serial abierto para ver los mensajes.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// UUID del Nordic UART Service (los mismos de index.html)
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define RX_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"   // aquí escribe el teléfono
const char* NOMBRE_BLE = "Aji-Scoville";

volatile int nivelPendiente = -1;   // nivel recibido por BLE que falta aplicar (-1 = ninguno)
String bufferBLE = "";               // caracteres recibidos por BLE hasta el "\n"
String bufferUSB = "";               // caracteres recibidos por USB hasta el "\n"


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
  Serial.print("Nivel recibido: ");
  Serial.println(nivel);
  // TODO: encender LEDs / motor de vibración según "nivel"
}


// Se ejecuta cada vez que el teléfono escribe en la característica RX
class AlRecibirBLE : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* caracteristica) {
    // "auto" funciona con las versiones 2.x (std::string) y 3.x (String) del paquete ESP32
    auto valor = caracteristica->getValue();
    for (size_t i = 0; i < valor.length(); i++) {
      char c = valor[i];
      if (c == '\n') {
        int nivel = interpretarLinea(bufferBLE);
        if (nivel >= 0) nivelPendiente = nivel;   // se aplica en loop()
        bufferBLE = "";
      } else if (bufferBLE.length() < 20) {
        bufferBLE += c;
      }
    }
  }
};

// Avisos de conexión y desconexión
class AlConectarBLE : public BLEServerCallbacks {
  void onConnect(BLEServer* servidor) {
    Serial.println("BLE: telefono conectado");
  }
  void onDisconnect(BLEServer* servidor) {
    Serial.println("BLE: telefono desconectado");
    BLEDevice::startAdvertising();   // volver a anunciarse para poder reconectar
  }
};


void setup() {
  Serial.begin(9600);
  Serial.println("Receptor ESP32 listo (BLE + USB)");

  // Crear el servidor BLE con el servicio y la característica de escritura
  BLEDevice::init(NOMBRE_BLE);
  BLEServer* servidor = BLEDevice::createServer();
  servidor->setCallbacks(new AlConectarBLE());

  BLEService* servicio = servidor->createService(SERVICE_UUID);
  BLECharacteristic* rx = servicio->createCharacteristic(
    RX_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  rx->setCallbacks(new AlRecibirBLE());
  servicio->start();

  // Anunciar el servicio para que index.html lo encuentre
  BLEAdvertising* anuncio = BLEDevice::getAdvertising();
  anuncio->addServiceUUID(SERVICE_UUID);
  anuncio->setScanResponse(true);
  BLEDevice::startAdvertising();
}


void loop() {
  // 1) Mensajes por USB (versión B, detector.py)
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      int nivel = interpretarLinea(bufferUSB);
      if (nivel >= 0) aplicarNivel(nivel);
      bufferUSB = "";
    } else if (bufferUSB.length() < 20) {
      bufferUSB += c;
    }
  }

  // 2) Mensajes por BLE (versión A, index.html)
  if (nivelPendiente >= 0) {
    int nivel = nivelPendiente;
    nivelPendiente = -1;
    aplicarNivel(nivel);
  }
}
