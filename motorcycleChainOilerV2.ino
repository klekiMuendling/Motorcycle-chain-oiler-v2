/****************************************************
 * Project:    Motorcycle chain oiler v.2
 * Target:     ESP32C3 Dev Module
 * Author:     kleki
 * Date:       IV 2026
 * Version:    2.5
 *
 * Description:
 * board controls peristaltic pump over MOS-FET by time
 *
 * Hardware:
 * - Board: ESP32-C3 Super Mini (choose ESP32C3 Dev Module)
 * - Pin:   GPIO8 (pump MOS-FET)
 * - Pin:   GPIO3/A3 (board voltage)
 * - Pin:   GPIO4/A4 (pump voltage after shunt resitor)
 *
 * Important notices:
 * - turn on USB CDC on boot
 ****************************************************/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>

////////////////////////////////voltage measurement
#define SUPPLY_VOLTAGE 3
//#define PUMP_VOLTAGE 5 - cannot be used as ADC because of the HW-mapping
//U=f(x)=ax+b
float a_supply = 0.0, b_supply = 0.0, x12_supply = 0.0, x7_supply = 0.0, vo_supply = 0.0;
float a_pump = 0.0, b_pump = 0.0, x12_pump = 0.0, x7_pump = 0.0, vo_pump = 0.0;
float vSupply = 0.0, vPump = 0.0;

////////////////////////////////parameter and variables for pump
#define LED 8
#define PUMP 7
#define PWM_RESOLUTION 8
int pwmChannel = 0;
int pwmFreq = 5000;
int pwmResolution = PWM_RESOLUTION;
//pw - PWM-value for pump, 0..100% power
//td - turn-on delay after power cycle in seconds
//tp - time between oiling doses in seconds
//to - dose time (pump working time) in tenths of second
//th - hand oiling time in tenths of second
#define PW_MAX 100
#define PW_MIN 0
#define TD_MAX 600
#define TD_MIN 10
#define TP_MAX 600
#define TP_MIN 10
#define TO_MAX 50
#define TO_MIN 0
#define TH_MAX 100
#define TH_MIN 0
int pumpRunTime = 0, pumpWaitTime = 0;
bool pumpRunning = false, stratupDelay = true;
int startupWait = 0;
Preferences prefs;
int pw = 0,  //PWM-value for pump, 0..100% power
  td = 0,    //turn-on delay after power cycle in seconds
  tp = 10,   //time between oiling doses in seconds
  to = 0,    //dose time (pump working time) in tenths of second
  th = 30;   //hand oiling time in tenths of second

////////////////////////////////pump functions  
void pumpStart() {
  ledcWrite(PUMP, ((pw) * (pow(2, PWM_RESOLUTION) - 1)) / PW_MAX);
  pumpRunning = true;
  digitalWrite(LED, LOW);
}
void pumpStop() {
  ledcWrite(PUMP, (0));
  pumpRunning = false;
  digitalWrite(LED, HIGH);
  pumpRunTime = to;
}
////////////////////////////////time generation, time basis 100ms
#define TICK_PRESCALER 100000  // 100,000 µs = 100 ms
#define CLOCK_PRESCALER 80     // 80 MHz / 80 = 1 MHz (1 µs per tick)
hw_timer_t* timer = NULL;
volatile bool tick = false;
void IRAM_ATTR onTimer() {
  tick = true;
}

////////////////////////////////bluetooth
#define DEVICE_NAME "ESP32_BLE"
#define PING_INTERVAL 3000
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_RX "12345678-1234-1234-1234-1234567890ac"  // App -> ESP32 (WRITE)
#define CHARACTERISTIC_TX "12345678-1234-1234-1234-1234567890ad"  // ESP32 -> App (NOTIFY)
String receivedValue, lastValue;
BLECharacteristic* txCharacteristic;
bool deviceConnected = false;

////////////////////////////////bluetooth classes
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("Client connected");
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("Client disconnected");
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted");
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.print("Received from app: ");
      Serial.println(value);

      // Beispiel: Antwort zurück an die App
      if (deviceConnected) {
        String response = "ESP32 got: " + value;
        receivedValue = value;
        txCharacteristic->setValue(response.c_str());
        txCharacteristic->notify();
      }
    }
  }
};

////////////////////////////////SETUP
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting system...");
  
  prefs.begin("my-app", false);
  pw = prefs.getInt("prefPw", 30);
  td = prefs.getInt("prefTd", 180);
  tp = prefs.getInt("prefTp", 60);
  to = prefs.getInt("prefTo", 20);
  th = prefs.getInt("prefTh", 30);
  a_pump = prefs.getFloat("a_pump", 0.0);
  b_pump = prefs.getFloat("b_pump", 0.0);
  a_supply = prefs.getFloat("a_supply", 0.0);
  b_supply = prefs.getFloat("b_supply", 0.0);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
  ledcAttach(PUMP, pwmFreq, pwmResolution);
  pumpStop();
  timer = timerBegin(1000000);  // frequency in Hz
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 100000, true, 0);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);  // up to ~3.3V

  Serial.println("Starting BLE...");
  stratupDelay = true;
  Serial.println("PW: " + String(pw));
  Serial.println("TD: " + String(td));
  Serial.println("TP: " + String(tp));
  Serial.println("TO: " + String(to));
  Serial.println("TH: " + String(th));
  //if ((a_pump != 0.0) && (b_pump != 0.0)) {
  //  vPump = ((float)analogRead(PUMP_VOLTAGE) * a_pump) + b_pump;
  //}
  if ((a_supply != 0.0) && (b_supply != 0.0)) {
    vSupply = ((float)analogRead(SUPPLY_VOLTAGE) * a_supply) + b_supply;
  }
  if (/*(vPump != 0.0) && */(vSupply != 0.0)) {
    Serial.print("supply voltage: ");
    Serial.println(vSupply, 1);
    //Serial.print("pump voltage: ");
    //Serial.println(vPump, 1);
  } else {
    Serial.println("voltages not calibrated!");
  }
  startupWait = td;

  BLEDevice::init(DEVICE_NAME);
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);
  // RX: App writes to ESP32
  BLECharacteristic* rxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX,
    BLECharacteristic::PROPERTY_WRITE);
  rxCharacteristic->setCallbacks(new RxCallbacks());
  // TX: ESP32 sends to App
  txCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_TX,
    BLECharacteristic::PROPERTY_NOTIFY);
  txCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE ready, waiting for connection...");
}

////////////////////////////////LOOP
void loop() {
  static unsigned long lastNotify = 0;

  if (deviceConnected && millis() - lastNotify > PING_INTERVAL) {
    lastNotify = millis();

    String msg = "Ping: " + String(millis() / 1000) + " " + receivedValue;
    txCharacteristic->setValue(msg.c_str());
    txCharacteristic->notify();

    Serial.println(msg);
  }

  if (receivedValue != "") {
    String command = receivedValue.substring(0, 2);
    String write = receivedValue.substring(2, 3);
    int value = receivedValue.substring(3).toInt();
    //pw - PWM-value for pump, 0..100% power
    //td - turn-on delay after power cycle in seconds
    //tp - time between oiling doses in seconds
    //to - dose time (pump working time) in tenths of second
    //th - hand oiling time in tenths of second

    if (command == "PW") {
      if (write == "=") {
        if ((value <= PW_MAX) && (value >= PW_MIN)) {
          prefs.putInt("prefPw", value);
          pw = prefs.getInt("prefPw");
          String msg = command.substring(0, 2) + ": " + String(pw);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        } else {
          String msg = command.substring(0, 2) + " must be " + String(PW_MIN) + " to " + String(PW_MAX);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        }
      } else {
        pw = prefs.getInt("prefPw");
        String msg = command.substring(0, 2) + ": " + String(pw);
        Serial.println(msg);
        txCharacteristic->setValue(msg.c_str());
        txCharacteristic->notify();
      }
    } else if (command == "TD") {
      if (write == "=") {
        if ((value <= TD_MAX) && (value >= TD_MIN)) {
          prefs.putInt("prefTd", value * 10);
          td = prefs.getInt("prefTd");
          String msg = command.substring(0, 2) + ": " + String(td / 10);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        } else {
          String msg = command.substring(0, 2) + " must be " + String(TD_MIN) + " to " + String(TD_MAX);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        }
      } else {
        td = prefs.getInt("prefTd");
        String msg = command.substring(0, 2) + ": " + String(td / 10);
        Serial.println(msg);
        txCharacteristic->setValue(msg.c_str());
        txCharacteristic->notify();
      }
    } else if (command == "TP") {
      if (write == "=") {
        if ((value <= TP_MAX) && (value >= TP_MIN)) {
          prefs.putInt("prefTp", value * 10);
          tp = prefs.getInt("prefTp");
          String msg = command.substring(0, 2) + ": " + String(tp / 10);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        } else {
          String msg = command.substring(0, 2) + " must be " + String(TP_MIN) + " to " + String(TP_MAX);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        }
      } else {
        tp = prefs.getInt("prefTp");
        String msg = command.substring(0, 2) + ": " + String(tp / 10);
        Serial.println(msg);
        txCharacteristic->setValue(msg.c_str());
        txCharacteristic->notify();
      }
    } else if (command == "TO") {
      if (write == "=") {
        if ((value <= TO_MAX) && (value >= TO_MIN)) {
          prefs.putInt("prefTo", value);
          to = prefs.getInt("prefTo");
          String msg = command.substring(0, 2) + ": " + String(to);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        } else {
          String msg = command.substring(0, 2) + " must be " + String(TO_MIN) + " to " + String(TO_MAX);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        }
      } else {
        to = prefs.getInt("prefTo");
        String msg = command.substring(0, 2) + ": " + String(to);
        Serial.println(msg);
        txCharacteristic->setValue(msg.c_str());
        txCharacteristic->notify();
      }
    } else if (command == "TH") {
      if (write == "=") {
        if ((value <= TH_MAX) && (value >= TH_MIN)) {
          prefs.putInt("prefTh", value);
          th = prefs.getInt("prefTh");
          String msg = command.substring(0, 2) + ": " + String(th);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        } else {
          String msg = command.substring(0, 2) + " must be " + String(TH_MIN) + " to " + String(TH_MAX);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        }
      } else {
        th = prefs.getInt("prefTh");
        String msg = command.substring(0, 2) + ": " + String(th);
        Serial.println(msg);
        txCharacteristic->setValue(msg.c_str());
        txCharacteristic->notify();
      }
    } else if (command == "MN") {
      if (write == "=") {
        if ((value <= TH_MAX) && (value >= TH_MIN) && value != 0) {
          //start manual
          stratupDelay = false;
          pumpRunTime = value;
          pumpStart();
          String msg = command.substring(0, 2) + ": " + String(th);
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        } else if (value == 0) {
          //stop manual
          pumpWaitTime = tp;
          pumpStop();
        } else {
          //stop manual
          pumpWaitTime = tp;
          pumpStop();
          String msg = command.substring(0, 2) + " must be " + String(TH_MIN) + " to " + String(TH_MAX);
          Serial.println(msg);
          //Serial.println("received value: " + String(value));//
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        }
      }
    } else if ((command == "V1")&&(write == "2")) {
        //x12_pump = (float)analogRead(PUMP_VOLTAGE);
        x12_supply = (float)analogRead(SUPPLY_VOLTAGE);
        if (/*(x7_pump != 0.0) && */(x7_supply != 0.0) /*&& (x12_pump - x7_pump != 0) */&& (x12_supply - x7_supply != 0)) {
          //a_pump = 5.0 / (x12_pump - x7_pump);
          a_supply = 5.0 / (x12_supply - x7_supply);
          //b_pump = 7.0 - (a_pump * x7_pump);
          b_supply = 7.0 - (a_supply * x7_supply);
          //prefs.putFloat("a_pump", a_pump);
          //prefs.putFloat("b_pump", b_pump);
          prefs.putFloat("a_supply", a_supply);
          prefs.putFloat("b_supply", b_supply);
          //Serial.print("pump voltage calibrated: a=");
          //Serial.print(a_pump, 2);
          //Serial.print(", b=");
          //Serial.println(b_pump, 2);
          Serial.print("supply voltage calibrated: a=");
          Serial.print(a_supply , 2);
          Serial.print(", b=");
          Serial.println(b_supply);
        }
      } else if ((command == "V0")&&(write == "7")) {
        //x7_pump = (float)analogRead(PUMP_VOLTAGE);
        x7_supply = (float)analogRead(SUPPLY_VOLTAGE);
        if (/*(x12_pump != 0.0) && */(x12_supply != 0.0) /*&& (x12_pump - x7_pump != 0)*/ && (x12_supply - x7_supply != 0)) {
          //a_pump = 5.0 / (x12_pump - x7_pump);
          a_supply = 5.0 / (x12_supply - x7_supply);
          //b_pump = 7.0 - (a_pump * x7_pump);
          b_supply = 7.0 - (a_supply * x7_supply);
          //prefs.putFloat("a_pump", a_pump);
          //prefs.putFloat("b_pump", b_pump);
          prefs.putFloat("a_supply", a_supply);
          prefs.putFloat("b_supply", b_supply);
          //Serial.print("pump voltage calibrated: a=");
          //Serial.print(a_pump, 2);
          //Serial.print(", b=");
          //Serial.println(b_pump, 2);
          Serial.print("supply voltage calibrated: a=");
          Serial.print(a_supply , 2);
          Serial.print(", b=");
          Serial.println(b_supply);
        }
      } else if ((command == "VO")&&(write == "O")) {
        if (/*(a_pump != 0.0) && */(a_supply != 0.0)) {
            //vo_pump = a_pump * (float)analogRead(PUMP_VOLTAGE) + b_pump;
            vo_supply = a_supply * (float)analogRead(SUPPLY_VOLTAGE) + b_supply;
          //Serial.print("pump voltage: ");
          //Serial.println(vo_pump, 1);
          Serial.print("supply voltage: ");
          Serial.println(vo_supply, 1);
          String msg = "board: " + String(vo_supply, 1) + "V";
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
          //msg = "pump: " + String(vo_pump, 1) + "V";
          //Serial.println(msg);
        } else {
          String msg = "voltages not calibrated!";
          Serial.println(msg);
          txCharacteristic->setValue(msg.c_str());
          txCharacteristic->notify();
        }
        String msg = "a supply = " + String(a_supply) + "; " + "b supply = " + String(b_supply) + "; " + "vo supply = " + vo_supply;
        Serial.println(msg);
        
      }
    else {
      //stop manual
      pumpWaitTime = tp;
      pumpStop();
      String msg = command.substring(0, 2) + ": " + String(th);
      Serial.println(msg);
      txCharacteristic->setValue(msg.c_str());
      txCharacteristic->notify();
    }
      receivedValue = "";
  command = "";
  value = -1;
  }


if (tick) {
  tick = false;
  if (pumpRunning == true) {
    pumpRunTime--;
  } else {
    pumpWaitTime--;
  }
  if (stratupDelay) {
    startupWait--;
    if (startupWait <= 0) {
      pumpWaitTime = 0;
      stratupDelay = false;
    }
  }
  if ((pumpWaitTime <= 0) && !stratupDelay) {
    pumpWaitTime = tp;
    pumpStart();
  }
  if (pumpRunTime <= 0) {
    pumpRunTime = to;
    pumpStop();
  }
}

delay(20);
}