#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;


void setup() {
  Serial.begin(115200);
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



//AGREGADO A COD




void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Cálculo de ángulos a partir del acelerómetro
  float roll  = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
  float pitch = atan(-a.acceleration.x / 
              sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180 / PI;

  Serial.print("Pitch: ");
  Serial.print(pitch);
  Serial.print("°   Roll: ");
  Serial.print(roll);
  Serial.println("°");

  delay(100); // refresco cada 0.1 s
}

