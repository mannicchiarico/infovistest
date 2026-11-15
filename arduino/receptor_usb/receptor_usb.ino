/*
  RECEPTOR DE REFERENCIA - solo USB serial (versión B, detector.py). Sirve para cualquier placa: Uno, Nano, Mega, etc.

  Recibe mensajes "N:x\n" (x = nivel de 0 a 10) y responde por Serial "Nivel recibido: x".
  detector.py muestra esa respuesta en su consola como "Arduino dice: ...", así se comprueba que llegó.
  Las luces y la vibración van en aplicarNivel().

  - Baudios: 9600 (igual que detector.py).
  - Error común: no se puede tener abierto el Monitor Serial del IDE mientras corre detector.py,
    ni subir el sketch mientras detector.py usa el puerto. Cierren uno para usar el otro.
  - Para probar sin Python: abran el Monitor Serial, elijan "Nueva línea" (Newline) y escriban N:5
*/

String buffer = "";   // caracteres recibidos hasta el "\n"


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


void setup() {
  Serial.begin(9600);
  Serial.println("Receptor USB listo");
}


void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      int nivel = interpretarLinea(buffer);
      if (nivel >= 0) aplicarNivel(nivel);
      buffer = "";
    } else if (buffer.length() < 20) {
      buffer += c;
    }
  }
}
