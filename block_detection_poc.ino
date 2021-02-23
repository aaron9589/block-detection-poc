// CT Sensor test
// Using UNO built-in ADC to read the sensor
// This sketch currently only supports 1 sensor.
//////////////////////////////////////////////////////////////////

#define VERSION "1.006"
#define SYS_ID "CT Sensor Test - Direct to UNO ADC"
const int adcpin = 0;

//Includes
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

//mqtt config
const char* mqtt_server = "192.168.0.60";
WiFiClient espClient;
PubSubClient client(espClient);

// Sampling Parameters
const unsigned long sampleTime = 2000UL;
const unsigned long numSamples = 100UL;
const unsigned long sampleInterval = sampleTime/numSamples;

#define SENSITIVITY 5000
#define DETECTION_MULTIPLIER 0.9
#define CALIBRATION_READS 300

// variables to hold sensor quiescent readings
float aqv;  // Average Quiescent Voltage; e.g. ADC Zero
float aqc;  // Average Quiescent Current;

//variables to stop MQTT spam - initialise as if the value has been sent, so inital values are always populated on boot.
bool boolocc;
bool boolunocc;

//uid generation
String mac = WiFi.macAddress().substring(9);

void setup()
{
  Serial.begin(9600);
  Serial.println(String(F(SYS_ID)) + String(F(" - SW:")) + String(F(VERSION)));
  Serial.print("\nCalibrating the sensor at pin ");
  Serial.println(adcpin);
  aqv = determineVQ(adcpin);
  Serial.print("AQV: ");
  Serial.print(aqv * 1000, 4);
  Serial.print(" mV\t");
  aqc = determineCQ(adcpin, aqv);
  Serial.print("AQC: ");
  Serial.print(aqc * 1000, 4);
  Serial.print(" mA\t");
  float sense = (aqc * DETECTION_MULTIPLIER) - aqc;
  Serial.print("Detection Sensitivity: ");
  Serial.print(sense * 1000, 3);
  Serial.println(" mA\n\n");
  delay(7500);

  WiFi.begin("yourwifissidhere", "yourpasswordhere"); // put your Wifi Details here
  Serial.print("Connecting");
while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  client.setServer(mqtt_server, 1883);

}

void loop(){
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float current = readCurrent(adcpin, aqv);
  float delta = abs(aqc - current);
  //bool occupied = delta > ((aqc * DETECTION_MULTIPLIER) - aqc);
  //workaround until this logic above is figured out.
  bool occupied = (current *1000) > 0.5;

  Serial.print("Current Sensed: ");
  Serial.print(current * 1000,3);
  Serial.print(" mA\t");
  Serial.println(" ");

  mac.replace(":","");

  String sensor = "Test01";
  String address = "/trains/track/sensor/" + mac + sensor;

  // Length (with one extra character for the null terminator)
  int str_len = address.length() + 1;

  // Prepare the character array (the buffer)
  char topic[str_len];

  // Copy it over
  address.toCharArray(topic, str_len);


  if(occupied){
    if (!boolocc){
    Serial.println("Occupied");
    client.publish(topic, "ACTIVE");
    boolocc = true;
    boolunocc = false;
    }
  } else {
    if (!boolunocc){
    Serial.println("Not occupied");
    client.publish(topic, "INACTIVE");
    boolocc = false;
    boolunocc = true;
    }
  }
  delay(500);
}

//////////////////////////////////////////
// Current Sensor Functions
//////////////////////////////////////////
float readCurrent(int pin, float adc_zero)
{
  float currentAcc = 0;
  unsigned int count = 0;
  unsigned long prevMicros = micros() - sampleInterval ;
  while (count < numSamples)
  {
    if (micros() - prevMicros >= sampleInterval)
    {
      float adc_raw = (float) analogRead(pin) - adc_zero; // sensor reading in volts
      adc_raw /= SENSITIVITY; // convert to amperes
      currentAcc += (adc_raw * adc_raw); // sum the squares
      count++;
      prevMicros += sampleInterval;
    }
  }
  //https://en.wikipedia.org/wiki/Root_mean_square
  float rms = sqrt((float)currentAcc / (float)numSamples);
  return rms;
}

//////////////////////////////////////////
// Calibration
// Track Power must be OFF during calibration
//////////////////////////////////////////

float determineVQ(int pin) {
  float VQ = 0;
  //read a large number of samples to stabilize value
  for (int i = 0; i < CALIBRATION_READS; i++) {
    VQ += analogRead(pin);
    delayMicroseconds(sampleInterval);
  }
  VQ /= CALIBRATION_READS;
  return VQ;
}

float determineCQ(int pin, float aqv) {
  float CQ = 0;
  // set reps so the total actual analog reads == CALIBRATION_READS
  int reps = (CALIBRATION_READS / numSamples);
  for (int i = 0; i < reps; i++) {
    CQ += readCurrent(pin, aqv);
  }
  CQ /= reps;
  return CQ;
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    mac.replace(":","");
    clientId += mac;
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.print("connected as ");
      Serial.println(clientId);
      // Once connected, publish an announcement...
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
