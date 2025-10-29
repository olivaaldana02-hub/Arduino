#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>

Adafruit_MPU6050 mpu;
Servo myservoh;  
Servo myservov;  

unsigned long lastSensorSend = 0;
unsigned long lastMotorSend = 0;
const unsigned long sensorInterval = 50;   
const unsigned long motorInterval = 500;   

enum Modo { NINGUNO, AUTOMATICO, MANUAL };
Modo modoActual = NINGUNO;

String movimientoTipo = "";      // H - V
String movimientoSentido = "";   // DI - ID - ArA - AbA
int apertura = 0;                
int velocidad = 0;               
bool preparado = false;
bool ejecutando = false;

String buffer = "";

int posH = 90;  // centro horizontal 
int posV = 90;  // centro vertical 

void setup() {
  Serial.begin(115200);
  myservoh.attach(12);
  myservov.attach(14);

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
  procesarComandos();
  enviarInclinacion();
  
  if (modoActual != NINGUNO) {
    enviarEstadoMotores();
  }

  if (modoActual == AUTOMATICO) {
    modoAutomatico();
  } else if (modoActual == MANUAL && ejecutando) {
    ejecutando = false;
  }

  delay(20);
}

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

  if (clave == "MODO") {
    if (valor == "A") {
      modoActual = AUTOMATICO;
      preparado = false;
      ejecutando = false;
      lastMotorSend = 0;
    } else if (valor == "M") {
      modoActual = MANUAL;
      preparado = false;
      ejecutando = false;
      lastMotorSend = 0;
    }
  }
  else if (clave == "MOVTIPO") {
    movimientoTipo = valor;  // "H" o "V"
  }
  else if (clave == "MOVSENTIDO") {
    movimientoSentido = valor;  // "DI", "ID", "ArA", "AbA"
  }
  else if (clave == "APERTURA") {
    apertura = valor.toInt();
  }
  else if (clave == "VELOCIDAD") {
    velocidad = valor.toInt();
  }
  else if (clave == "PREPARAR" && modoActual == MANUAL) {
    prepararMovimiento();
  }
  else if (clave == "COMENZAR" && modoActual == MANUAL && preparado) {
    ejecutarMovimiento();
  }
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

void enviarEstadoMotores() {
  static int prevH = 90;
  static int prevV = 90;
  unsigned long now = millis();
  
  if (now - lastMotorSend >= motorInterval) {
    lastMotorSend = now;

    int motorH = (abs(posH - prevH) > 2) ? 1 : 0;
    int motorV = (abs(posV - prevV) > 2) ? 1 : 0;

    Serial.print("<MH=");
    Serial.print(motorH);
    Serial.println(">");

    Serial.print("<MV=");
    Serial.print(motorV);
    Serial.println(">");

    prevH = posH;
    prevV = posV;
  }
}

void modoAutomatico() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
  float pitch = atan(-a.acceleration.x / sqrt(a.acceleration.y * a.acceleration.y + 
                                               a.acceleration.z * a.acceleration.z)) * 180 / PI;

  int rollMap  = constrain(map(roll, -90, 90, 180, 0), 0, 180);
  int pitchMap = constrain(map(pitch, -90, 90, 180, 0), 0, 180);

  int prevH = posH;
  int prevV = posV;

  posH = rollMap;
  posV = pitchMap;

  myservoh.write(posH);
  myservov.write(posV);
}


void prepararMovimiento() {
  if (movimientoTipo == "H") {  
    posV = 90;  
    
    if (movimientoSentido == "DI") {  
      posH = 180;  // Tope derecha
    } else if (movimientoSentido == "ID") {  
      posH = 0;    // Tope izquierda
    }
  } 
  else if (movimientoTipo == "V") {  
    posH = 90; 
    
    if (movimientoSentido == "ArA") {  
      posV = 0;    // Tope arriba
    } else if (movimientoSentido == "AbA") {  
      posV = 180;  // Tope abajo
    }
  }

  myservoh.write(posH);         //iniciales del mov
  myservov.write(posV);
  delay(1000);                  //le da tiempo

  preparado = true;
  Serial.println("<LISTO=1>");
}

void ejecutarMovimiento() {
  ejecutando = true;
  
  int posInicial, posFinal;
  Servo* servoActivo;
  
  if (movimientoTipo == "H") {
    servoActivo = &myservoh;
    posInicial = posH;
    
    if (movimientoSentido == "DI") {
      posFinal = constrain(180 - apertura, 0, 180);
    } else {  // ID
      posFinal = constrain(apertura, 0, 180);
    }
  } 
  else {  // Vert
    servoActivo = &myservov;
    posInicial = posV;
    
    if (movimientoSentido == "ArA") {
      posFinal = constrain(apertura, 0, 180);
    } else {  // AbA
      posFinal = constrain(180 - apertura, 0, 180);
    }
  }

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
  
  servoActivo->write(posFinal);
  if (movimientoTipo == "H") {
    posH = posFinal;
  } else {
    posV = posFinal;
  }

  preparado = false;
  Serial.println("<FINALIZADO=1>");
}
