// Created By: merkaba/pradhonoaji
// Created on: 30-Aug-2019

#include <SimpleTimer.h>
#include <OneWire.h>
#include <SoftwareSerial.h>

#define PIN_RESET 9
#define PIN_DS1   10
#define LED_POWER 12
#define LED_DATA  13

SimpleTimer timer;
OneWire ds(PIN_DS1);
SoftwareSerial mySerial(7,8);

String _inputString;
//String _t1 = "20,00";
//String _t2 = "34,00";
float _t2 = 34.00;
float _t1 = 20.00;
float _suhu;
int _timerIDData;
int _timerIDPingAt;
int _timerIDReadSuhu;

unsigned const long _interval_data_cloud  = 60000;  // cloud access every 1 min
unsigned const long _interval_read_suhu   = 30000;  // temperature read every 30 seconds
unsigned const long _interval_ping_at = 10000;      // keep-alive freq of GSM shield every 10 seconds

void setup() {
  // put your setup code here, to run once:  
  setupSerial();
  Serial.println("************ SETUP BEGIN ************");  
  initLed();  
  setupTimer();  
  disableAllTimers();
  restartShield();  
  setupAPN();
//  sendData();
  enableAllTimers();
//  Serial.println("************ SETUP DONE ************");
}

void loop() {
  timer.run();
}

void enableAllTimers() {
  timer.enable(_timerIDData);
  timer.enable(_timerIDPingAt);  
  timer.enable(_timerIDReadSuhu);  
}

void disableAllTimers() {
  timer.disable(_timerIDData);
  timer.disable(_timerIDPingAt);  
  timer.disable(_timerIDReadSuhu);  
}

void enableTimers23() {
  timer.enable(_timerIDPingAt);  
  timer.enable(_timerIDReadSuhu); 
}

void disableTimers23() {
  timer.disable(_timerIDPingAt);  
  timer.disable(_timerIDReadSuhu); 
}

void setupSerial() {
  Serial.begin(9600);
  mySerial.begin(9600);
  _inputString.reserve(200);
}

void initLed() {
  pinMode(LED_POWER, OUTPUT);
  pinMode(LED_DATA, OUTPUT);
  pinMode(PIN_RESET, OUTPUT);
  ledPower(1);
  ledData(0);
}

void setupTimer() {
  _timerIDData      = timer.setInterval(_interval_data_cloud, dataCloud); // timer 1
  _timerIDPingAt    = timer.setInterval(_interval_ping_at, pingShield);   // timer 2 
  _timerIDReadSuhu  = timer.setInterval(_interval_read_suhu, readSuhu);   // timer 3
}

void pingShield() {
  char result = sendATReply("AT\r\n","OK",1000);
  if(result==0) {
    Serial.println("SHIELD RESPONDED");
  } else {
    disableAllTimers();
    restartShield();
    setupAPN();
    enableAllTimers();
  }
}

void ledPower(boolean on) {
  digitalWrite(LED_POWER, !on);
}

void ledData(boolean on) {
  digitalWrite(LED_DATA, !on);
}

void restartShield() {
  int i;
  ledData(1);
  while(1) {
    i++;
    ledData(0);  
    Serial.print("FOR: ");
    Serial.println(i);
    delay(1000);
    ledData(1);
    Serial.println("Power State Changing...");  
    digitalWrite(PIN_RESET, LOW);
    delay(1000);
    ledData(0);
    digitalWrite(PIN_RESET, HIGH);
    delay(2000);
    ledData(1);
    digitalWrite(PIN_RESET, LOW);
    delay(3000);
    ledData(0);
    mySerialEvent(5000,"POWER DOWN");
    Serial.print("INPUTSTRING: ");
    Serial.println(_inputString);
    if(_inputString.indexOf("POWER DOWN")>0) {
      Serial.println("_________ power down occured _____________");      
    } else if(_inputString.indexOf("RDY")>0){
      Serial.println("_________ call ready _____________");
      Serial.println("Power Cycle Finished.");
      break;
    } else {
      Serial.println("_________ call ready _____________");
      Serial.println("Power Cycle Finished.");
      break;
    }
    ledData(1);
  }
  ledData(0);
}

char mySerialEvent(unsigned long waitms, char* reply) {
  _inputString = "";
  char okCode;
  int GSM_Response = 0;  
  unsigned long prevMillis = millis();  
  unsigned long currentMillis = millis();  
  while (GSM_Response==0 && ((currentMillis - prevMillis) < waitms))
  {
    while(mySerial.available()) {
      char inChar = (char)mySerial.read();
      _inputString += inChar;
      if(reply=="") {
        if(_inputString.indexOf("OK")>0) {
          GSM_Response = 1;
          okCode = 0;
          break;     
        } else if(_inputString.indexOf("ERROR")>0) {
          GSM_Response = 1;
          okCode = 1;
          break;
        }
      } else {
        if(_inputString.indexOf(reply)>0) {
          GSM_Response = 1;
          okCode = 0;
          break;     
        } else if(_inputString.indexOf("ERROR")>0) {
          GSM_Response = 1;
          okCode = 1;
          break;
        }
      }
    }   
    currentMillis = millis(); 
  }
  return okCode;
}

void setupAPN() {
  sendAT("AT\r\n",1000);
  sendAT("AT+SAPBR=3,1,\"APN\",\"internet\"\r\n", 5000);
  delay(1000);
}

void readSuhu() {
  readCelcius();
  if(_suhu >= _t2 || _suhu <= _t1) {
    sendSms(_suhu);
    callPhone();
  }
}

void readCelcius() {
  Serial.println(" ___________ BEGIN READ TEMPERATURE ___________");
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius = 999.99, fahrenheit;
  boolean not_family = 0;

  for(byte d=0;d<8;d++) {
    if ( !ds.search(addr)) {
//      Serial.println("No more addresses.");
      ds.reset_search();
      delay(250);
      break;
    }
    
//    Serial.print("ROM =");
    for( i = 0; i < 8; i++) {
//      Serial.write(' ');
//      Serial.print(addr[i], HEX);
    }
  
    if (OneWire::crc8(addr, 7) != addr[7]) {
//        Serial.println("CRC is not valid!");
        break;
    }
//    Serial.println();
   
    // the first ROM byte indicates which chip
    switch (addr[0]) {
      case 0x10:
        Serial.println("  Chip = DS18S20");  // or old DS1820
        type_s = 1;
        break;
      case 0x28:
        Serial.println("  Chip = DS18B20");
        type_s = 0;
        break;
      case 0x22:
        Serial.println("  Chip = DS1822");
        type_s = 0;
        break;
      default:
        Serial.println("Device is not a DS18x20 family device.");
        not_family = 1;
        break;
//        return error_t;
    } 

    if(not_family) break;
    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);        // start conversion, with parasite power on at the end
    
    delay(1000);     // maybe 750ms is enough, maybe not
    // we might do a ds.depower() here, but the reset will take care of it.
    
    present = ds.reset();
    ds.select(addr);    
    ds.write(0xBE);         // Read Scratchpad
  
//    Serial.print("  Data = ");
//    Serial.print(present, HEX);
//    Serial.print(" ");
    for ( i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = ds.read();
//      Serial.print(data[i], HEX);
//      Serial.print(" ");
    }
//    Serial.print(" CRC=");
//    Serial.print(OneWire::crc8(data, 8), HEX);
//    Serial.println();
  
    // Convert the data to actual temperature
    // because the result is a 16 bit signed integer, it should
    // be stored to an "int16_t" type, which is always 16 bits
    // even when compiled on a 32 bit processor.
    int16_t raw = (data[1] << 8) | data[0];
    if (type_s) {
      raw = raw << 3; // 9 bit resolution default
      if (data[7] == 0x10) {
        // "count remain" gives full 12 bit resolution
        raw = (raw & 0xFFF0) + 12 - data[6];
      }
    } else {
      byte cfg = (data[4] & 0x60);
      // at lower res, the low bits are undefined, so let's zero them
      if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
      else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
      else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
      //// default is 12 bit resolution, 750 ms conversion time
    }
    celsius = (float)raw / 16.0;
    fahrenheit = celsius * 1.8 + 32.0;
    Serial.print("  Temperature = ");
    Serial.print(celsius);
    Serial.print(" Celsius, ");
  }
  _suhu = celsius;
}

char sendAT(char *cmd, unsigned long waitms) {
  mySerial.write(cmd);
  Serial.print("CMD: ");
  Serial.write(cmd);
  char okcode = mySerialEvent(waitms,"");
  Serial.println(_inputString);
  return okcode;
}

char sendATReply(char* cmd, char* replychar, unsigned long waitms) {
  mySerial.write(cmd);
  char okcode = mySerialEvent(waitms,replychar);
  Serial.println(_inputString);
  return okcode;
}

void dataCloud() {
  disableTimers23();
  sendData();
  getData();
  enableTimers23();
}

void getData() {
  
}

void sendData() {
  ledData(1);
  sendAT("AT+SAPBR=1,1\r\n",5000);
  ledData(0);
  sendAT("AT+SAPBR=2,1\r\n",5000);
  ledData(1);
  sendAT("AT+HTTPINIT\r\n",5000);    
  ledData(0);
  sendAT("AT+HTTPPARA=\"CID\",1\r\n",5000);
  ledData(1);
  char c_cmd[200] = "AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/apps/thinghttp/send_request?api_key=9SP21LSZXW4M8WMG&temperature=";
//  float suhu = readSuhu();
  char c_suhu[10];
  dtostrf(_suhu,8,2,c_suhu);
  strcat(c_cmd, c_suhu);
  strcat(c_cmd, "\"");
  strcat(c_cmd, "\r\n");
  Serial.print(c_cmd);
  ledData(0);
  sendAT(c_cmd, 5000);
  ledData(1);
  sendATReply("AT+HTTPACTION=0\r\n", "+HTTPACTION:0,200", 5000);    
  ledData(0);
  sendAT("AT+HTTPREAD\r\n",5000);
  ledData(1);
  sendAT("AT+HTTPTERM\r\n",5000);
  ledData(0);
  sendAT("AT+SAPBR=0,1\r\n", 5000);
  delay(2000);
  //
  
}

void callPhone() {
  ledData(1);
//  mySerial.print("AT+CMGF=0\r\n");  
  delay(2000);
  mySerial.print("ATD+62819899545;\r\n");
  delay(20000);
  mySerial.print("ATH\r\n");
  delay(3000);
  ledData(0);
}

void sendSms(float temp) {
  String s = "Suhu skg: " + String(temp) + " diluar batas dari " + String(_t1) + " dan " + String(_t2) + "\r\n";
  Serial.println(s);
//  ledData(1);
//  mySerial.print("AT+CMGF=1\r"); //sending SMS in text mode
//  delay(1000);
//  mySerial.print("AT+CMGS=\"+62819899545\"\r\n"); // phone number
//  delay(1000);
//  String s = "Suhu skg: " + String(temp) + " diluar batas dari " + t1 + " dan " + t2 + "\r\n";
//  mySerial.print(s); // message 
//  delay(1000);
//  mySerial.write(0x1A); //send a Ctrl+Z(end of the message)
//  delay(5000); 
//  ledData(0);

}

