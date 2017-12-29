#include <SigFox.h>
#include <ArduinoLowPower.h>

int sleep = 15; // in minutes

bool node[5]; // is true if had data
float temperatures[5][60]; // 5 nodes 60 minutes
float humidity[5][60];
int doorOpen[5]; // sum
int readings[5]; //num entries

char reading[99];
int readingCount = 0;

unsigned long lastTime = 0;

void dummy(){
  node[2] = true; // 0 is broadcast ID, 1 is central node ID
  node[3] = true;
  node[4] = true;

  temperatures[2][0] = 1.1358;
  temperatures[2][1] = 3.1358;
  temperatures[2][2] = 2.1358;
  temperatures[3][0] = 1.1;
  temperatures[4][0] = 1.1;

  humidity[2][0] = 1.1;
  humidity[2][1] = 1.1;
  humidity[2][2] = 1.1;
  humidity[3][0] = 1.1;
  humidity[4][0] = 1.1;

  readings[2] = 3;
  readings[3] = 1;
  readings[4] = 1;

  doorOpen[2] = 3;
  doorOpen[3] = 1;
  doorOpen[4] = 1;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  //dummy();
  Serial.begin(9600);
  Serial1.begin(9600);
  //while (!Serial) {}; // plz remove in prod
  while (!Serial1) {};

  if (!SigFox.begin()) { // test if SigFox chip is not dead
    Serial.println("Shield error or not present!");
    return;
  }  
  delay(100);
  SigFox.end();

  Serial.println("Central node ready");
}

void loop()
{
  if (Serial1.available() > 0) {
    char data = Serial1.read();
    Serial.print(data);
    reading[readingCount] = data;
    if (data == '\n'){
      parseData(String(reading));
      
      memset(reading, 0, sizeof reading);
      readingCount = 0;
    } else {
      readingCount++;
    }
  }

  doTimers();
}

void doTimers() {
  unsigned long now = millis();
  if (lastTime > now) {
    //reset happened
    lastTime = 0;
  }

  //Serial.println(now);
  //Serial.println((lastTime + (sleep * 60 * 1000)));

  if ((lastTime + (sleep * 60 * 1000)) < now) { // send measurements every sleep
      Serial.println("Sending Data!");
      sendDataAndGetResponse();
      lastTime = now;
  }  
}

void parseData(String data) {
  if (!data.startsWith("SENDER")) return; // not our data!

  // [0] = SENDER
  int sender = getValue(data, ' ', 1).toInt(); // [1] = senderID
  // [2] = DATA
  // [3] = TMP
  float tmp = getValue(data, ' ', 4).toFloat(); // [4] = tmpFloat
  // [5] = HUM
  float hum = getValue(data, ' ', 6).toFloat(); // [6] = humFloat
  // [7] = DOR
  int door = getValue(data, ' ', 8).toInt(); // [8] = doorInt

  node[sender] = true;
  temperatures[sender][readings[sender]] = tmp;
  humidity[sender][readings[sender]] = hum;
  doorOpen[sender] += door;

  Serial.println(tmp);
  Serial.println(hum);
  Serial.println(door);

  readings[sender]++;
  blink();
}

void resetCounters() {
  for (int i = 0; i < sizeof(node); i++) {
    node[i] = false;
    readings[i] = 0;
    doorOpen[i] = 0;
  }
}
void sendDataAndGetResponse() {
  SigFox.debug(); // a bug: somehow debug has to be enabled or the module is dead
  // Start the module
  SigFox.begin();
  // Wait at least 30mS after first configuration (100mS before)
  delay(100);
  // Clears all pending interrupts
  SigFox.status();
  delay(1);

  SigFox.beginPacket();
  for (int i = 0; i < sizeof(node); i++) {
    if (node[i]) {
       String out = "";
       out += String(i);
       char temp[3];
       char hum[2];
       
       prepareForSend(temperatures[i], readings[i], 99, 1, temp);
       prepareForSend(humidity[i], readings[i], 99, 0, hum);
       if (average(temperatures[i], readings[i]) < 10) {
        out += "0";
       }
       out += temp;
       Serial.println(temp);
       Serial.println(String(temp));
       if (average(humidity[i], readings[i]) < 10) {
        out += "0";
       }
       out += hum;
       Serial.println(hum);
       
       if (doorOpen[i] < 10) { // pad left? meh
          out += "0";
       }
       if (doorOpen[i] > 99) { // over 99? better not send
           out += "99";
       } else {
          out += doorOpen[i];
       }
       int outInt = (uint) out.toInt();
       Serial.println(out);
       //Serial.println(outInt);
       //Serial.print("hex");
       Serial.println(outInt, HEX);
       SigFox.write(outInt);  
    }
  }

  int ret = SigFox.endPacket(true);  // send buffer to SIGFOX network and wait for a response
  if (ret > 0) {
    Serial.println("No transmission");
  } else {
    Serial.println("Transmission ok");
  }

  if (SigFox.parsePacket()) {
    Serial.println("Response from server");
    int count = 0;
    while (SigFox.available()) {
      char data = SigFox.read();
      if (count == 0) {
        sleep = data;
      }
      Serial.println(data);
      count++;
    }
  } else {
    Serial.println("Could not get any response from the server");
  }
  
  SigFox.noDebug();
  SigFox.end();

  resetCounters();
}

float average (float *array, int len) {
  float sum = 0L ;  // sum will be larger than an item, long for safety.
  for (int i = 0 ; i < len ; i++){
    sum += array[i];
   }
  return  ((float) sum) / len ;  // average will be fractional, so float may be appropriate.
}

void prepareForSend(float * in, int len, float max, int dec, char out[]) {
  float av = average(in, len);
  if (av > max) {
    av = max;
  }
  
  if (dec == 0){
    sprintf(out, "%.0f", av);
    return;
  }

  int intpart = (int) av;
  float decimal = av - intpart;
  int intdecimal = (decimal * (10.0 * dec));
  
  sprintf(out, "%i%i", intpart, intdecimal);
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void blink() {
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(1000);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
}
