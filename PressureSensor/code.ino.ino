#define INPUT_PIN 13
#define OUTPUT_PIN 12

#define NORMAL_FLASH 0 
#define FAST_FLASH 1
#define TROUBLE_FLASH 2 

void setup() {
  Serial.begin(115200);
  pinMode(INPUT_PIN, INPUT_PULLUP);
  pinMode(OUTPUT_PIN, OUTPUT);
}
 
int tickNumber = 0;
String deviceName = "Pressure Sensor";
String deviceId = "5adc2f3c-2d9d-47a0-8977-02939730a30d";
String message = "";

bool ledOn = 0;
int ledOnTick = 0;
int ledOffTick = 0;
int mode = FAST_FLASH;

void loop() {
  tickNumber++;
  int sensorVal = digitalRead(INPUT_PIN);

  message = "Reading #" + String(tickNumber) + ": ";
  Serial.print(message + " ");

  if (sensorVal == LOW)
  {
    Serial.println("Activated!");
  }
 
  else
  {
    Serial.println("Not activated.");
  }
  
  if(ledOn == true && tickNumber > ledOffTick){
    digitalWrite(OUTPUT_PIN, LOW);
    ledOn = false;

    if(mode == NORMAL_FLASH){
      ledOnTick = tickNumber + 600;
    }
    else if(mode == FAST_FLASH){
      ledOnTick = tickNumber + 1;
    }
  }
  else if (ledOn == false && tickNumber > ledOnTick){
    digitalWrite(OUTPUT_PIN,HIGH);
    ledOffTick = tickNumber + 1;
    ledOn = true;
  }
  delay(100);
}