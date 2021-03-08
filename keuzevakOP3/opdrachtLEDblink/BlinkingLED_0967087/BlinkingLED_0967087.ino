/*
board settings:
  board: LOLIN(WEMOS) D1 R2 & mini
*/

int interval = 100;
void setup() {
pinMode(2,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(2, HIGH);
  delay(interval);
  digitalWrite(2,LOW);
  delay(interval);
}
