#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>

Adafruit_MPU6050 mpu;
Servo myservoh;  // Servo horizontal (Roll)
Servo myservov;  // Servo vertical (Pitch)

// Timing para envío de tramas
unsigned long lastSensorSend = 0;
unsigned long lastMotorSend = 0;
const unsigned long sensorInterval = 50;   // Envío de inclinación cada 50ms
const unsigned long motorInterval = 500;   // Envío de estado de motores cada 500ms

// Variables de modo y estado
enum Modo { NINGUNO, AUTOMATICO, MANUAL };
Modo modoActual = NINGUNO;

// Variables para modo MANUAL
String movimientoTipo = "";      // "H" o "V"
String movimientoSentido = "";   // "DI", "ID", "AA", "BA"
int apertura = 0;                // Ángulos de apertura
int velocidad = 0;               // Velocidad en ms de delay
bool preparado = false;
bool ejecutando = false;

// Buffer para recepción de tramas
String buffer = "";

// Posiciones de servos
int posH = 90;  // Posición horizontal centrada
int posV = 90;  // Posición vertical centrada

void setup() {
  Serial.begin(115200);
  myservoh.attach(12);
  myservov.attach(14);

  // Centrar servos al inicio
  myservoh.write(90);
  myservov.write(90);

  if (!mpu.begin()) {
    Serial.println("No se encontró el MPU6050, revisa el cableado!");
    while (1) delay(10);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 listo!");
}

void loop() {
  // Procesar comandos seriales recibidos
  procesarComandos();

  // Enviar datos de inclinación continuamente
  enviarInclinacion();

  // Ejecutar según modo
  if (modoActual == AUTOMATICO) {
    modoAutomatico();
  } else if (modoActual == MANUAL && ejecutando) {
    // El movimiento manual se ejecuta una sola vez cuando se llama
    ejecutando = false;
  }

  delay(20);
}

// ==========================
// COMUNICACIÓN SERIAL
// ==========================

void procesarComandos() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    if (c == '<') {
      buffer = "";
    } else if (c == '>') {
      interpretarTrama(buffer);
      buffer = "";
    } else {
      buffer += c;
    }
  }
}

void interpretarTrama(String trama) {
  int idx = trama.indexOf('=');
  if (idx == -1) return;

  String clave = trama.substring(0, idx);
  String valor = trama.substring(idx + 1);

  clave.trim();
  valor.trim();

  // Comandos de modo
  if (clave == "MODO") {
    if (valor == "A") {
      modoActual = AUTOMATICO;
      preparado = false;
      ejecutando = false;
    } else if (valor == "M") {
      modoActual = MANUAL;
      preparado = false;
      ejecutando = false;
    }
  }
  // Configuración MANUAL
  else if (clave == "MOVTIPO") {
    movimientoTipo = valor;  // "H" o "V"
  }
  else if (clave == "MOVSENTIDO") {
    movimientoSentido = valor;  // "DI", "ID", "AA", "BA"
  }
  else if (clave == "APERTURA") {
    apertura = valor.toInt();
  }
  else if (clave == "VELOCIDAD") {
    velocidad = valor.toInt();
  }
  // Comando PREPARAR
  else if (clave == "PREPARAR" && modoActual == MANUAL) {
    prepararMovimiento();
  }
  // Comando COMENZAR
  else if (clave == "COMENZAR" && modoActual == MANUAL && preparado) {
    ejecutarMovimiento();
  }
  // Comando REPETIR
  else if (clave == "REPETIR" && modoActual == MANUAL) {
    prepararMovimiento();
  }
}

void enviarInclinacion() {
  unsigned long now = millis();
  if (now - lastSensorSend < sensorInterval) return;
  lastSensorSend = now;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
  float pitch = atan(-a.acceleration.x / sqrt(a.acceleration.y * a.acceleration.y + 
                                               a.acceleration.z * a.acceleration.z)) * 180 / PI;

  Serial.print("<PITCH=");
  Serial.print((int)round(pitch));
  Serial.println(">");

  Serial.print("<ROLL=");
  Serial.print((int)round(roll));
  Serial.println(">");
}

// ==========================
// MODO AUTOMÁTICO
// ==========================

void modoAutomatico() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
  float pitch = atan(-a.acceleration.x / sqrt(a.acceleration.y * a.acceleration.y + 
                                               a.acceleration.z * a.acceleration.z)) * 180 / PI;

  // Mapear ángulos a posición servo (invertido para compensar)
  int rollMap  = constrain(map(roll, -90, 90, 180, 0), 0, 180);
  int pitchMap = constrain(map(pitch, -90, 90, 180, 0), 0, 180);

  // Guardar posiciones actuales
  int prevH = posH;
  int prevV = posV;

  // Actualizar posiciones
  posH = rollMap;
  posV = pitchMap;

  // Mover servos
  myservoh.write(posH);
  myservov.write(posV);

  // Enviar estado de motores cada motorInterval
  unsigned long now = millis();
  if (now - lastMotorSend >= motorInterval) {
    lastMotorSend = now;

    // Determinar si hay movimiento (umbral de 3 grados)
    int motorH = (abs(posH - prevH) > 3) ? 1 : 0;
    int motorV = (abs(posV - prevV) > 3) ? 1 : 0;

    Serial.print("<MH=");
    Serial.print(motorH);
    Serial.println(">");

    Serial.print("<MV=");
    Serial.print(motorV);
    Serial.println(">");
  }
}

// ==========================
// MODO MANUAL
// ==========================

void prepararMovimiento() {
  // Posicionar según tipo y sentido de movimiento
  if (movimientoTipo == "H") {  // Horizontal
    posV = 90;  // Centrar vertical
    
    if (movimientoSentido == "DI") {  // Derecha a Izquierda
      posH = 180;  // Tope derecha
    } else if (movimientoSentido == "ID") {  // Izquierda a Derecha
      posH = 0;    // Tope izquierda
    }
  } 
  else if (movimientoTipo == "V") {  // Vertical
    posH = 90;  // Centrar horizontal
    
    if (movimientoSentido == "AA") {  // Arriba a Abajo
      posV = 0;    // Tope arriba
    } else if (movimientoSentido == "BA") {  // Abajo a Arriba
      posV = 180;  // Tope abajo
    }
  }

  // Mover a posición inicial
  myservoh.write(posH);
  myservov.write(posV);
  delay(1000);  // Esperar que llegue a posición

  preparado = true;
  Serial.println("<LISTO=1>");
}

void ejecutarMovimiento() {
  ejecutando = true;
  
  int posInicial, posFinal;
  Servo* servoActivo;
  
  // Determinar servo, posición inicial y final
  if (movimientoTipo == "H") {
    servoActivo = &myservoh;
    posInicial = posH;
    
    if (movimientoSentido == "DI") {
      posFinal = constrain(180 - apertura, 0, 180);
    } else {  // ID
      posFinal = constrain(apertura, 0, 180);
    }
  } 
  else {  // V
    servoActivo = &myservov;
    posInicial = posV;
    
    if (movimientoSentido == "AA") {
      posFinal = constrain(apertura, 0, 180);
    } else {  // BA
      posFinal = constrain(180 - apertura, 0, 180);
    }
  }

  // Realizar movimiento suave
  int paso = (posInicial < posFinal) ? 1 : -1;
  
  for (int pos = posInicial; pos != posFinal; pos += paso) {
    servoActivo->write(pos);
    
    if (movimientoTipo == "H") {
      posH = pos;
    } else {
      posV = pos;
    }
    
    delay(velocidad);
  }
  
  // Asegurar llegada a posición final
  servoActivo->write(posFinal);
  if (movimientoTipo == "H") {
    posH = posFinal;
  } else {
    posV = posFinal;
  }

  preparado = false;
  Serial.println("<FINALIZADO=1>");
}
