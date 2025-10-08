#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "BluetoothSerial.h"
#include <ESP32Servo.h>            ////////


BluetoothSerial SerialBT;
Adafruit_MPU6050 mpu;
Servo myservo;                    /////////


void setup() {
  Serial.begin(115200);
  myservo.attach(12);              ///////////

  SerialBT.begin("Placuli");

  while (!Serial)
    delay(10);

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

  if(SerialBT.available())
  {
    char c=SerialBT.read();
  }

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
  float pitch = atan(-a.acceleration.x / 
              sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180 / PI;


  SerialBT.print("Pitch: ");
  SerialBT.print(pitch);
  SerialBT.print("°   Roll: ");
  SerialBT.print(roll);
  SerialBT.println("°");

  float rollMap = constrain(map(roll,-90,90,0,180),0,180);          //////////////////
  myservo.write(rollMap);        ////////////

  delay(50);                    //
}



