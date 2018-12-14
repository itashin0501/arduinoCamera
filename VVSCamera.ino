#include <Time.h>
#include <TimeLib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <time.h>
#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include <SD.h>

#define JST     3600*9

const uint8_t pinLED = 14;
const uint8_t pinSW = 0;
const uint8_t pinArduCamCS = 15;
const uint8_t pinSDCS = 2;
const uint8_t pinSDA = 4;
const uint8_t pinSCL = 5;
const uint8_t pinMotion = 16;

ArduCAM arduCam(OV2640, pinArduCamCS);

const int intervalTakePic = 900000;
//const char* ssid = "ita5";
//const char* password = "3331itashin";
//const char* ssid = "vivioffice-2.4G";//"vivioffice-5G";
//const char* password = "r3RGKStmLV";

const char* ssid = "VIVICAM";
const char* password = "12341234";

int tryConnectCount = 0;
static bool hasSD = false;
ESP8266WebServer server(80);
IPAddress ip( 192, 168, 128, 22); 
IPAddress subnet( 255, 255, 255, 0 ); 

static bool takePicFlag = true;

void setup() {
  Serial.begin(115200);
  delay(1);
  Serial.print("\n\n-----------------------\n");
  Serial.print("Program Start\n");
  
  pinMode(pinLED, OUTPUT);   // LED
  pinMode(pinSW, INPUT);     // スイッチ、照度センサーINT信号
  pinMode(pinMotion, INPUT_PULLDOWN_16);  // モーションセンサー入力

  // SPI CSはHIGHにしておく
  pinMode(pinArduCamCS, OUTPUT);
  pinMode(pinSDCS, OUTPUT);
  digitalWrite(pinArduCamCS, HIGH);
  digitalWrite(pinSDCS, HIGH);

  //wifi接続 時刻取得
//  WiFi.begin(ssid, password);
//  while(WiFi.status() != WL_CONNECTED) {
//    Serial.print('.');
//    tryConnectCount++;
//
//    if(tryConnectCount > 15){
//      break;
//    }
//    delay(1000);
//  }
//  Serial.println();
//  Serial.printf("Connected, IP address: ");
//  Serial.println(WiFi.localIP());
//  configTime( JST, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");

  WiFi.mode(WIFI_AP); 
  WiFi.softAPConfig(ip, ip, subnet); 
  WiFi.softAP(ssid, password); 

  //サーバ－起動
  server.on("/takepic", handleTakePic);
  server.onNotFound(onNotFound_);
  server.begin();

// 日時を手動設定
//  setTime(14,40,00,13,11,2018); // hh mm ss dd MM yyyy

  // カメラ接続確認
  Serial.println("Verify camera module");
  Wire.begin();
  SPI.begin();

  // I2Cセンサーの確認
  uint8_t pid = 0;
  uint8_t vid = 0;
  arduCam.wrSensorReg8_8(0xFF, 0x01);
  arduCam.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  arduCam.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if((vid != 0x26 ) && (( !pid != 0x41 ) || ( pid != 0x42 ))){
    Serial.println("Camera Not Found (I2C)");
    return;
  }

  // SPIバッファメモリの確認
  arduCam.write_reg(ARDUCHIP_TEST1, 0xA5);
  uint8_t data = arduCam.read_reg(ARDUCHIP_TEST1);
  if(data != 0xA5){
    Serial.println("Camera Not Found (SPI)");
    return;
  }

  // 初期設定
  Serial.println("Config camera module");
  arduCam.set_format(JPEG);
  arduCam.InitCAM();
  arduCam.OV2640_set_JPEG_size(OV2640_1280x1024);  // 画像サイズ設定

  // JPEG画質設定
  arduCam.wrSensorReg8_8(0xFF, 0x00);
  arduCam.wrSensorReg8_8(0x44, 0x8);  // 0x4:画質優先 0x8:バランス 0xC:圧縮率優先

  delay(4000);  // カメラが安定するまで待つ
  
  if(SD.begin(pinSDCS)){
    Serial.println("sd ok");
    hasSD = true;
  }else{
    Serial.println("sd error");
    hasSD = false;
  }

  Serial.println("setup Finished");  
}
void handleTakePic(){
  if(server.hasArg("fname")){
    String fname = server.arg("fname");
    takePic("pics", fname);
    server.send(200, "text/html","<h1>TAKE PIC "+fname+"</h1>");
  }else{
    server.send(200, "text/html","<h1>VIVICAM BAD COMMAND</h1>");
  }
  Serial.println("takePicture");
  server.send(200, "text/html","<h1>VIVICAM</h1>");
}
void loop() {
  server.handleClient();

//  if(takePicFlag){
//    takePicFlag = false;
//    
//    time_t t;
//    struct tm *tm;
//    static const char *wd[7] = {"Sun","Mon","Tue","Wed","Thr","Fri","Sat"};
//   
//    t = time(NULL);
//    tm = localtime(&t);
//    char dirPath[13] = "";
//    char fileName[5] = "";
//  
//    //日時をサーバ取得できる場合
//    sprintf(dirPath, "%04d/%02d/%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
//    sprintf(fileName, "%02d%02d%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
//  // 日時を指定した場合   
//  //  sprintf(dirPath, "%04d/%02d/%02d", year(), month(), day());
//  //  sprintf(fileName, "%02d%02d%02d", hour(), minute(), second());
//  
//    takePic(dirPath, fileName);
//    
//  //  delay(intervalTakePic);
//  }
}

bool loadFromSdCard(String path) {
  Serial.print("loadFromSdCard :");
  Serial.println(path);
  SD.mkdir("/");
  
  String dataType = "text/html";
  File dataFile = SD.open(path, FILE_READ);
  if (dataFile.isDirectory() || !dataFile) {
    Serial.println("open file failed with uri");
    dataFile.close();
    SD.mkdir("html");
    path = "html/index.htm";
    dataFile = SD.open(path, FILE_READ);
  }
  
  if (!dataFile){
    Serial.println("loadFromSdCard : not found file in SD card");
    Serial.println(path);
    return false;
  }
  Serial.println("open file");
  Serial.println(path);

//  String responseStr = "";
//  while(dataFile.available()){
////    Serial.println(dataFile);  
//    responseStr.concat(dataFile.read());
//  }
////  server.sendHeader("Content-Length",String(dataFile.size()));
////  server.sendHeader("Connection","close");
////  server.sendHeader("Access-Control-Allow-Origin","*");
//  server.send(200, dataType, responseStr);
//  WiFiClient client = server.client();
//  size_t totalSize = dataFile.size();
//  client.write(dataFile, HTTP_DOWNLOAD_UNIT_SIZE);

  server.streamFile(dataFile, "image/jpeg");
  dataFile.close();
  
  Serial.println("loadFromSdCard : end");
  return true;
}

void onNotFound_() {
  if(hasSD && loadFromSdCard(server.uri())) return;
 
  //SDカードから読み込みできなかった場合
  String message = "Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void takePic(String dirPath, String fileName){
  arduCam.flush_fifo();
  arduCam.clear_fifo_flag();
  Serial.println("Start capture");
  arduCam.start_capture();    // 撮影情報をカメラモジュールのバッファーにメモリに転送
  while(!arduCam.get_bit(ARDUCHIP_TRIG , CAP_DONE_MASK)){ // 終わるまで待つ
    delay(1);
  }

  // カメラモジュールのバッファーメモリからSDカードに転送
  Serial.println("Write to SD card");
  String path = dirPath;
  path.concat("/");
  path.concat(fileName);
  path.concat(".jpg");
  Serial.println(path);
  
  const uint8_t bufferSize = 32;  // ESP8266に確保するバッファーサイズ
  uint8_t*buf;
  buf = new uint8_t[bufferSize];
  uint8_t prev = 0;
  uint16_t index = 0;
  
    SD.mkdir(dirPath);
    File file = SD.open(path, FILE_WRITE);
    if(file){
      arduCam.CS_LOW();
      arduCam.set_fifo_burst();
      buf[index] = SPI.transfer(0x0);  // 先頭に不要な0x00が入るのを回避
      
      while(1){
        buf[index] = SPI.transfer(0x0);

        // JPEGファイルの最後を検出したら終了
        if(prev==0xFF && buf[index]==0xD9){
          arduCam.CS_HIGH();
          file.write(buf, index+1);
          break;
        }

        if(index==bufferSize-1){
          arduCam.CS_HIGH();
          file.write(buf, bufferSize);
          arduCam.CS_LOW();
          arduCam.set_fifo_burst();
          index = 0;
        }else{
          prev = buf[index];
          index++;
        }
        delay(0);
      }
      file.close();
      arduCam.CS_HIGH();
    }else{
      Serial.println("Could not open file on SD card");
    }
  delete[] buf;
}
