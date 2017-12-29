Q#define NODEID         1    //0 = broadcast, 1= central node
#include <SimpleMesh.h>
#include <avr/power.h>

SimpleMesh mesh;

void setup() {
  // SAVE ALL THE POWER
  //power_adc_disable();

  Serial.begin(9600);
  mesh.initialize(NODEID,1);

}

void loop() {
  if (mesh.receiveDone()) {
    Serial.println("NEWS");
    MessagePayload message = mesh.getMessage();
    Serial.print("SENDER ");
    Serial.print(message.senderID);
    Serial.print(" DATA ");
    Serial.println(message.data);
  }
  delay(1000);
}
