#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ESP32Servo.h>
#include <WiFi.h>

// ========== CONFIGURACIÓN WIFI ==========
const char* ssid = "Galaxy A23";       
const char* password = "mizg5169";  
WiFiServer server(80);                       // Servidor en puerto 80
WiFiClient client;
bool clienteConectado = false;

Adafruit_MPU6050 mpu;
Servo myservoh;  
Servo myservov;  

unsigned long lastSensorSend = 0;
unsigned long lastMotorSend = 0;
const unsigned long sensorInterval = 50;   
const unsigned long motorInterval = 500;   

enum Modo { NINGUNO, AUTOMATICO, MANUAL };
Modo modoActual = NINGUNO;

String movimientoTipo = "";      
String movimientoSentido = "";   
int apertura = 0;                
int velocidad = 0;               
bool preparado = false;
bool ejecutando = false;

String buffer = "";

int posH = 90;  
int posV = 90;  

void setup() {
  Serial.begin(115200);
  
  // ========== INICIAR WIFI ==========
  Serial.println();
  Serial.print("Conectando a WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi conectado!");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());  // ANOTA ESTA IP PARA TU APP WINDOWS
  
  server.begin();
  Serial.println("Servidor iniciado en puerto 80");
  Serial.println("Esperando conexión del cliente...");
  // ====================================
  
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
  // ========== MANEJAR CONEXIONES WIFI ==========
  if (!clienteConectado) {
    client = server.available();
    if (client) {
      Serial.println("Cliente conectado!");
      clienteConectado = true;
    }
  } else {
    if (!client.connected()) {
      Serial.println("Cliente desconectado");
      client.stop();
      clienteConectado = false;
    }
  }
  // ===========================================
  
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
  // ========== LEER DESDE WIFI EN LUGAR DE SERIAL ==========
  while (clienteConectado && client.available() > 0) {
    char c = client.read();
    if (c == '<') {
      buffer = "";
    } else if (c == '>') {
      interpretarTrama(buffer);
      buffer = "";
    } else {
      buffer += c;
    }
  }
  // ======================================================
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
    movimientoTipo = valor;
  }
  else if (clave == "MOVSENTIDO") {
    movimientoSentido = valor;
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

  // ========== ENVIAR POR WIFI ==========
  if (clienteConectado) {
    client.print("<PITCH=");
    client.print((int)round(pitch));
    client.println(">");

    client.print("<ROLL=");
    client.print((int)round(roll));
    client.println(">");
  }
  // ====================================
}

void enviarEstadoMotores() {
  static int prevH = 90;
  static int prevV = 90;
  unsigned long now = millis();
  
  if (now - lastMotorSend >= motorInterval) {
    lastMotorSend = now;

    int motorH = (abs(posH - prevH) > 2) ? 1 : 0;
    int motorV = (abs(posV - prevV) > 2) ? 1 : 0;

    // ========== ENVIAR POR WIFI ==========
    if (clienteConectado) {
      client.print("<MH=");
      client.print(motorH);
      client.println(">");

      client.print("<MV=");
      client.print(motorV);
      client.println(">");
    }
    // ====================================

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
      posH = 180;
    } else if (movimientoSentido == "ID") {  
      posH = 0;
    }
  } 
  else if (movimientoTipo == "V") {  
    posH = 90; 
    
    if (movimientoSentido == "ArA") {  
      posV = 0;
    } else if (movimientoSentido == "AbA") {  
      posV = 180;
    }
  }

  myservoh.write(posH);
  myservov.write(posV);
  delay(1000);

  preparado = true;
  
  // ========== ENVIAR POR WIFI ==========
  if (clienteConectado) {
    client.println("<LISTO=1>");
  }
  // ====================================
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
    } else {
      posFinal = constrain(apertura, 0, 180);
    }
  } 
  else {
    servoActivo = &myservov;
    posInicial = posV;
    
    if (movimientoSentido == "ArA") {
      posFinal = constrain(apertura, 0, 180);
    } else {
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
  
  // ========== ENVIAR POR WIFI ==========
  if (clienteConectado) {
    client.println("<FINALIZADO=1>");
  }
  // ====================================
}