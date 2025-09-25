#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <ESP32Servo.h>
#include <Adafruit_Sensor.h>

Adafruit _MPU6050 mpu; 
Servo motorh;
Servo motorv;

void motores (int posX, int posY) {      //puede que necesite int delay, aunque uso uno en lectura
  myservoh.write(posX);
  myservov.write(posY);
}

void setup() {
  myservoh.attach(12);                   //chequear ambos al armar/probar
  myservov.attach(14);  

  Serial.begin(115200);
  while (!Serial)
    delay(10);
  if (!mpu.begin()) {
    Serial.println("No se encontró el MPU6050");
    while (1) delay(10);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU6050 listo!");       
}

void loop() {
  sensors_event_t a, g, temp;                          //está toda la lectura de sensor, creo que está bien acá pero podría ser una función
  mpu.getEvent(&a, &g, &temp);

  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
  float pitch = atan(-a.acceleration.x / 
              sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180 / PI;
  motores (pitch, roll); 
  Serial.print("Pitch: ");
  Serial.print(pitch);
  Serial.print("Roll: ");
  Serial.print(roll);
  //Serial.println("°");

  delay(100); 
}
