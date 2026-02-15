#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>

const int ICM = 0x68;
int mode;
int flag = 0;
int co = 0;
int tapmsg = 0;
int detect;

// Initialize all pointers
BLEServer* pServer = NULL;                        // Pointer to the server
BLECharacteristic* pCharacteristic_1 = NULL;      // Pointer to Characteristic 1
BLECharacteristic* pCharacteristic_2 = NULL;      // Pointer to Characteristic 2
BLEDescriptor *pDescr_1;                          // Pointer to Descriptor of Characteristic 1
BLE2902 *pBLE2902_1;                              // Pointer to BLE2902 of Characteristic 1
BLE2902 *pBLE2902_2;                              // Pointer to BLE2902 of Characteristic 2

// Some variables to keep track on device connected
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Variable that will continuously be increased and written to the client
uint32_t value = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
// UUIDs used in this example:
#define SERVICE_UUID          "bdf0e32b-970d-41a0-99ac-4a997939c875"
#define CHARACTERISTIC_UUID_1 "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_2 "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"

// Callback function that is called whenever a client is connected or disconnected
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

int Reg_read(unsigned long add){     // reading data from the particular register
  Wire.beginTransmission(ICM);
  Wire.write(add);
  Wire.endTransmission();
  Wire.requestFrom(ICM, 1);
  if (Wire.available()){
    return Wire.read();}
}

void Reg_write(unsigned long add, uint8_t and_=255, uint8_t or_=0){    // write data to specific bits
  Wire.beginTransmission(ICM);
  Wire.write(add);
  Wire.endTransmission();
  Wire.requestFrom(ICM, 1);
  if (Wire.available()){
    mode=(Wire.read()&and_)|or_;
    Wire.beginTransmission(ICM);
    Wire.write(add);
    Wire.write(mode);
    Wire.endTransmission();
  }
}

void Reg_dir_write(unsigned long add, uint8_t val){     // write data to whole byte
  Wire.beginTransmission(ICM);
  Wire.write(add);
  Wire.write(val);
  Wire.endTransmission();
}

void Tap_detect(){    // ISR
  flag=1;
}

void setup() {
  //Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("ESP32");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic_1 = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_1,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );                   

  pCharacteristic_2 = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_2,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |                      
                      BLECharacteristic::PROPERTY_NOTIFY
                    );  

  // Create a BLE Descriptor  
  pDescr_1 = new BLEDescriptor((uint16_t)0x2901);
  pDescr_1->setValue("A very interesting variable");
  pCharacteristic_1->addDescriptor(pDescr_1);

  // Add the BLE2902 Descriptor because we are using "PROPERTY_NOTIFY"
  pBLE2902_1 = new BLE2902();
  pBLE2902_1->setNotifications(true);                 
  pCharacteristic_1->addDescriptor(pBLE2902_1);

  pBLE2902_2 = new BLE2902();
  pBLE2902_2->setNotifications(true);
  pCharacteristic_2->addDescriptor(pBLE2902_2);

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  //Serial.println("Waiting a client connection to notify...");

  Wire.begin();

  Reg_dir_write(0x76, 0);   // set bank =0

  Reg_dir_write(0x50, 15);  //  ACCEL_ODR = 15 (500 Hz)

  Reg_write(0x4E, 254, 2);  // accel mode =2
  Reg_write(0x4D, 247);  // accel clk = 0
  Reg_write(0x53, 253, 4);  // ACCEL_DEC2_M2_ORD = 2
  Reg_write(0x52, 15, 64);  // ACCEL_UI_FILT_BW = 4

  delay(500);

  Reg_dir_write(0x76, 4);   // set bank =4

  Reg_write(0x47, 219, 91);  // TAP_TMAX to 2, TAP_TMIN to 3, TAP_TAVG to 3
  Reg_dir_write(0x46, 254);  // TAP_MIN_JERK_THR to 31, TAP_MAX_PEAK_TOL to 0

  delay(500);

  Reg_write(0x4D, 255, 1);  // enable INT1 = 1

  delay(500);

  Reg_dir_write(0x76, 0);   // set bank =0

  Reg_write(0x56, 255, 64);  // enable INT1 = 1

  attachInterrupt(digitalPinToInterrupt(5), Tap_detect, RISING);    // calling ISR at interrupt
  
  delay(100);
  //Serial.println("Setup Initiated");

}

void loop() {

    if (flag==1){           // from ISR after detection
    detect = Reg_read(0x38);
    if (detect%2==1){                // double check register for tap detection
      //Serial.println("Tapp");
      tapmsg = 1;
      flag = 0;
      co++;
      //Serial.println(co);
      if (deviceConnected) {
        pCharacteristic_1->setValue(co);
        pCharacteristic_1->notify();
        String txValue = "Tap: " + String(tapmsg);
        pCharacteristic_2->setValue(txValue.c_str());
        delay(100);
      }
    }
    }

    // notify changed value
    //if (deviceConnected) {
      // pCharacteristic_1 is an integer that is increased with every second
      // in the code below we send the value over to the client and increase the integer counter
      //pCharacteristic_1->setValue(co);
      //pCharacteristic_1->notify(); co++;
      //value++;
  
      // pCharacteristic_2 is a std::string (NOT a String). In the code below we read the current value
      // write this to the Serial interface and send a different value back to the Client
      // Here the current value is read using getValue() 
      //std::string rxValue = pCharacteristic_2->getValue();
      //Serial.print("Characteristic 2 (getValue): ");
      //Serial.println(rxValue.c_str());

      // Here the value is written to the Client using setValue();
      //String txValue = "Tap: " + String(tapmsg);
     // pCharacteristic_2->setValue(txValue.c_str());
      //Serial.println("Characteristic 2 (setValue): " + txValue);

      // In this example "delay" is used to delay with one second. This is of course a very basic 
      // implementation to keep things simple. I recommend to use millis() for any production code
      //delay(100);
    //}
    // The code below keeps the connection status uptodate:
    // Disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        //Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // Connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }

    tapmsg = 0;

}