void setup() {
  Serial.begin(9600);
  pinMode(4, OUTPUT); 
}

void loop() {
  if (Serial.available()) {
    String lee = Serial.readStringUntil('>');
    lee += '>';

    if (lee == "<LED1:1>") {
      digitalWrite(4, HIGH);
      Serial.println("<ESTLED1:1>");
    } else if (lee == "<LED1:0>") {
      digitalWrite(4, LOW);
      Serial.println("<ESTLED1:0>");
    }
  }
}
