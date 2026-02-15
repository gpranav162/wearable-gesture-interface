#include <Wire.h>
#include <esp_now.h>
#include <WiFi.h>

const int ICM = 0x68;
int mode;
int flag = 0;
int count = 0;
int lp = 0;
int detect;

uint8_t rear_add[] = {0xA0,0xB7,0x65,0x61,0xCE,0xFC};

const uint8_t threshold = 254;  //threshold and tolerance variable -- 252 

//Structure example to receive data
//Must match the sender structure
//typedef struct rear_msg {
//  int rpm;
//  int duty;
//} rear_msg;

//Create a struct_message called msg
//rear_msg msg;

typedef struct tap_msg {
  //int id;
  //int throttle;
  int tap;
  //int mode;
} tap_msg;

tap_msg tmsg;

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status: \t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

//void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
//  memcpy(&msg, incomingData, sizeof(msg));
  //Serial.print("Bytes received: ");
  //Serial.println(len);
  //Serial.print("rpm: ");
  //Serial.println(msg.rpm);
  //Serial.print("duty: ");
  //Serial.println(msg.duty);
  //Serial.println();}

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

  Serial.begin(115200);

  // while (Serial.available() == 0) {
  // }

  // threshold = Serial.parseInt();
  // Serial.println(threshold);

  // while (Serial.available() == 0) {
  // }

  // uint8_t Tmin = Serial.parseInt();
  // Serial.println(Tmin);
  
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
  Reg_dir_write(0x46, threshold);  // TAP_MIN_JERK_THR to 31, TAP_MAX_PEAK_TOL to 0

  delay(500);

  Reg_write(0x4D, 255, 1);  // enable INT1 = 1

  delay(500);

  Reg_dir_write(0x76, 0);   // set bank =0

  Reg_write(0x56, 255, 64);  // enable INT1 = 1

  attachInterrupt(digitalPinToInterrupt(5), Tap_detect, RISING);    // calling ISR at interrupt
  
  delay(100);
  Serial.println("Setup Initiated");

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  
  esp_now_register_send_cb(OnDataSent);
   
  // register peer
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // register first peer  
  memcpy(peerInfo.peer_addr, rear_add, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  //esp_now_register_recv_cb(OnDataRecv);

  delay(100);
  Serial.println("Setup Done");
  //pinMode(LED_BUILTIN, OUTPUT);
  
}

void loop() {
  //tmsg.id = 0;
  tmsg.tap = lp;
  //tmsg.throttle = -1;
  //tmsg.mode = 0;

  if (flag==1){           // from ISR after detection
    detect = Reg_read(0x38);
    if (detect%2==1){                // double check register for tap detection
      Serial.println("Tapp");
      flag = 0;
      count++;
      Serial.println(count);
      tmsg.tap = -1;
      //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  //delay(1000);                      // wait for a second
  //digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  //delay(1000);  
    }    
  }
  esp_err_t result = esp_now_send(rear_add, (uint8_t *) &tmsg, sizeof(tmsg ));
   
  if (result == ESP_OK) {
    Serial.println("Sent with success");
    lp++;
  }
  else {
    Serial.println("Error sending the data");
  }
  delay(20);

}
