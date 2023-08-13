#include <WiFi.h>
#include <IOXhop_FirebaseESP32.h>
#include <PZEM004Tv30.h>
#include <time.h>
#include <math.h>

#define FIREBASE_HOST "https://electricity-eff87-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "5YP03HIKfOnpi2P8On7ouJHJMSIDjqsMLoxNKlPh"
#define WIFI_SSID "Your Wifi's name"
#define WIFI_PASSWORD "Your Password"
#define min(a,b) ((a)<(b)?(a):(b))

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN 22
#define PZEM_TX_PIN 23
#endif

#if !defined(PZEM_SERIAL)
#define PZEM_SERIAL Serial2
#endif
int timezone = 7;       // Zone +7 for Thailand
char ntp_server1[20] = "ntp.ku.ac.th";
char ntp_server2[20] = "fw.eng.ku.ac.th";
char ntp_server3[20] = "time.uni.net.th";
int dst = 0;
int  Sec = 0;
PZEM004Tv30 pzem(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN);

int thisyear = 0;
int thismonth = -1;
int thisday = -1;
float energyd = 0;
float billd = 0;
void setup() {
    // Debugging Serial port
    Serial.begin(115200); // ตั้งค่าการแสดงผลผ่าน หน้าจอคอมพิวเตอร์
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // เชื่อมต่อ wifi 
    Serial.print("connecting");
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
    Serial.print("connected: ");
    Serial.println(WiFi.localIP());
    configTime(timezone * 3600, dst, ntp_server1, ntp_server2, ntp_server3);
    while (!time(nullptr)) {
      Serial.print(".");
      delay(500);
    }
    NowString();
    Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

void loop() {
    // Print the custom address of the PZEM
    Serial.print("Custom Address:");
    Serial.println(pzem.readAddress(), HEX);
    time_t now = time(nullptr);
    struct tm* newtime = localtime(&now);
    // Read the data from the sensor
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();
    while (newtime->tm_year+1900<2020){
      Serial.print(".");
      NowString();
    }
    // Check if the data is valid
    if(isnan(voltage)){
        Serial.println("Error reading voltage");
    } else if (isnan(current)) {
        Serial.println("Error reading current");
    } else if (isnan(power)) {
        Serial.println("Error reading power");
    } else if (isnan(energy)) {
        Serial.println("Error reading energy");
    } else {

        // Print the values to the Serial console
        Serial.print("Voltage: ");      Serial.print(voltage);      Serial.println("V");
        Serial.print("Current: ");      Serial.print(current);      Serial.println("A");
        Serial.print("Power: ");        Serial.print(power);        Serial.println("W");
        Serial.print("Energy: ");       Serial.print(energy*1000);     Serial.println("Wh");
    
    Firebase.setString("/Time", NowString());
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    Firebase.setFloat("/Voltage", voltage);
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    Firebase.setFloat("/Electric Current", current);
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    Firebase.setFloat("/Power", power);
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    Firebase.setFloat("/Energy Used", energy*1000);
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    if(thismonth <0){
      thismonth = newtime->tm_mon;
    }
    if(thisday <0){
      thisday = newtime->tm_mday;
    }
    float bill = callBill(energy,Firebase.getFloat("/ft"),Firebase.getBool("/Over150"));
    Firebase.setFloat("/Bills", bill);
    if (Firebase.failed()) {
      Serial.print("pushing failed:");
      Serial.println(Firebase.error());
      return;
    }
    if(newtime->tm_mday!=thisday){
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.createObject();
      energyd=energy-energyd;
      billd = bill-billd;
      root["Energy"] = energyd*1000;
      root["Bill"] = billd;
      root["time"] = NowString();
      String name = Firebase.push("/HistoryD", root);
    // handle error
      if (Firebase.failed()) {
        Serial.print("pushing failed:");
        Serial.println(Firebase.error());
        return;
      }
      Serial.print("pushed: /logPower/");
      Serial.println(name);
      energyd=energy;
      billd=bill;
      thisday = newtime->tm_mday;
    }
    if(newtime->tm_mon!=thismonth){
      Serial.print(thismonth);
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.createObject();
      root["Energy"] = energy*1000;
      root["Bill"] = bill;
      root["time"] = NowString();
      String name = Firebase.push("/History", root);
    // handle error
      if (Firebase.failed()) {
        Serial.print("pushing failed:");
        Serial.println(Firebase.error());
        return;
      }
      Serial.print("pushed: /logPower/");
      Serial.println(name);
      thismonth = newtime->tm_mon;
      Serial.print(" ");
      Serial.print(thismonth);
      pzem.resetEnergy();
    }

    }
    Serial.println();
}

String NowString() {
  time_t now = time(nullptr);
  struct tm* newtime = localtime(&now);
  
  String tmpNow = "";
  tmpNow += String(newtime->tm_year + 1900);
  tmpNow += "-";
  tmpNow += String(newtime->tm_mon + 1);
  tmpNow += "-";
  tmpNow += String(newtime->tm_mday);
  tmpNow += " ";
  tmpNow += String(newtime->tm_hour);
  tmpNow += ":";
  tmpNow += String(newtime->tm_min);
  tmpNow += ":";
  tmpNow += String(newtime->tm_sec);
  Serial.println(tmpNow);
  Sec = newtime->tm_sec;
  return tmpNow;

}
float callBill(float Unit, float ft, bool over_150_Unit_per_month) {
  float Service = over_150_Unit_per_month ? 24.62 : 8.19;

  float total = 0;

  if (!over_150_Unit_per_month) {
    float Rate15 = 2.3488;
    float Rate25 = 2.9882;
    float Rate35 = 3.2405;
    float Rate100 = 3.6237;
    float Rate150 = 3.7171;
    float Rate400 = 4.2218;
    float RateMore400 = 4.4217;

    total += min(Unit, 15) * Rate15;
    if (Unit >= 16) total += min(Unit - 15, 10) * Rate25;
    if (Unit >= 26) total += min(Unit - 25, 10) * Rate35;
    if (Unit >= 36) total += min(Unit - 35, 65) * Rate100;
    if (Unit >= 101) total += min(Unit - 100, 50) * Rate150;
    if (Unit >= 151) total += min(Unit - 150, 250) * Rate400;
    if (Unit >= 401) total += (Unit - 400) * RateMore400;
  } else {
    float Rate150 = 3.2484;
    float Rate400 = 4.2218;
    float RateMore400 = 4.4217;

    total += min(Unit, 150) * Rate150;
    if (Unit >= 151) total += min(Unit - 150, 150) * Rate400;
    if (Unit >= 401) total += (Unit - 400) * RateMore400;\
  }

  total += Service;
  total += Unit * (ft);
  total += total * 7 / 100;

  return total;
}
