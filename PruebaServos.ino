#include <ESP32Servo.h>

Servo myservo; 
int pos = 0;   

void setup() {
  myservo.attach(12); 
}

void loop() {
  //for (pos = 0; pos <= 180; pos += 1) { 
    myservo.write(0);    
    delay(500); 
    myservo.write(180);         
    delay(1000);

  //}
  //for (pos = 180; pos >= 0; pos -= 1) { 
    //myservo.write(pos);              
    //delay(2);                       
  //}
}