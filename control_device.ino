#include <SoftwareSerial.h>
#include <TimerOne.h>  // https://www.arduinolibraries.info/libraries/timer-one

#include "Arduino.h"

#define RX_PIN 10
#define TX_PIN 11
#define retryMax 3

const int btn1 = 2;   // the number of the button pin
const int motor = 5;  // the number of the motor control pin
const int led1 = 6;   // the number of the led pin
const int reset = 8;
const int ring = 3;

#define turnOnMotor() digitalWrite(motor, LOW);
#define turnOffMotor() digitalWrite(motor, HIGH);
#define turnOnLed() digitalWrite(led1, HIGH);
#define turnOffLed() digitalWrite(led1, LOW);

// phone number must be removed the first number "0"
const char *phoneNumber[] = {"706199838", "384331808", NULL};
String buff = "";

SoftwareSerial SIM(RX_PIN, TX_PIN);

// motor on: status = true
// motor off: status = false
bool status = false;
int second = 0, mSec = 0;

void setup() {
  Serial.begin(115200);
  SIM.begin(9600);

  Timer1.initialize(100000);
  Timer1.attachInterrupt(timerIsr);
  Timer1.stop();

  pinMode(motor, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(reset, OUTPUT);
  pinMode(ring, INPUT);
  pinMode(btn1, INPUT_PULLUP);

  turnOffMotor();
  turnOffLed();
  // module sim reset at low level
  digitalWrite(reset, HIGH);

  attachInterrupt(digitalPinToInterrupt(btn1), btnInterrup, FALLING);

  simInit();
}

void loop() {
  buff = readSerial();
  Serial.println(buff);
  if (buff.indexOf("RING") != -1 && checkListPhone(buff, phoneNumber)) {
    checkStatus();
    utilHangoffCall();
  } else if (buff.indexOf("+CMTI") != -1) {
    buff = readSms(1);
    Serial.println(buff);
    String num = "";
    if (buff.length() > 10) {
      uint8_t _idx1 = buff.indexOf("+CMGR:");
      _idx1 = buff.indexOf("\",\"", _idx1 + 1);
      num = buff.substring(_idx1 + 3, buff.indexOf("\",\"", _idx1 + 4));
    } else {
      Serial.println("Sms empty!");
    }

    if (buff.indexOf("OK") != -1 && buff.length() > 7) {
      buff.toUpperCase();
      char tmp_num[13];
      num.toCharArray(tmp_num, num.length() + 1);
      Serial.println(tmp_num);
      if (buff.indexOf("BAT") != -1 || buff.indexOf("ON") != -1) {
        Serial.println("MOTOR TURN ON");
        turnOnMotor();
        turnOnLed();
        status = true;
        utilSendSms(tmp_num, "DA BAT");
      } else if (buff.indexOf("TAT") != -1 || buff.indexOf("OFF") != -1) {
        Serial.println("MOTOR TURN OFF");
        turnOffMotor();
        turnOffLed();
        status = false;
        utilSendSms(tmp_num, "DA TAT");
      }
    }
    utilDelAllSms();
  }
  processSIMHang();
}

/**
 * @brief timer to prevent brutal pushing
 *
 */
void timerIsr(void) {
  mSec++;
  if (mSec > 10) {
    second++;
    mSec = 0;
    Serial.print("second: ");
    Serial.println(second);
    if (second > 3) {
      second = 0;
      Timer1.stop();
    }
  }
}

/**
 * @brief interrupt button switch current state
 *
 */
void btnInterrup() {
  if (!digitalRead(btn1)) {
    delay(30);
    if (!digitalRead(btn1) && second == 0) {
      mSec = 0;
      second = 1;
      Timer1.start();
      checkStatus();
    }
  }
}

/**
 * @brief reset module sim if pingping is false
 *
 */
void processSIMHang() {
  if (!digitalRead(ring)) {
    delay(30);
    if (!digitalRead(ring) && !utilPingPong()) {
      checkStatus();
    }
  }
}

/**
 * @brief verify if belong list phoneNumber
 *
 * @param buff
 * @param listPhone
 * @return true/false
 */
bool checkListPhone(String buff, const char **listPhone) {
  int i = 0;
  while (listPhone[i]) {
    if (phoneNumberAvailable(buff, listPhone[i])) return true;
    i++;
  }
  return false;
}

/**
 * @brief verify if message contain phomenumber
 *
 * @param buff
 * @param phoneNumber
 * @return true/false
 */
bool phoneNumberAvailable(String buff, const char *phoneNumber) {
  const String myPhone1 = "0" + String(phoneNumber);
  const String myPhone2 = "+84" + String(phoneNumber);
  return (buff.indexOf(myPhone1) != -1 || buff.indexOf(myPhone2) != -1);
}

/**
 * @brief reset module sim by
 * pull down to ground at least 105ms module will reset
 *
 */
void resetSIM() {
  digitalWrite(reset, LOW);
  delay(200);
  digitalWrite(reset, HIGH);
  delay(3000);
  simInit();
}

/**
 * @brief wrapper of pingPong
 * retry if ping pong failed
 *
 */
bool utilPingPong() {
  int nRetry = 0;
  while (nRetry < retryMax) {
    if (pingPong()) return true;
    nRetry++;
    delay(1000);
    Serial.println("module sim unavailable");
  }
  return false;
}

/**
 * @brief wrapper of hangoffCall
 * retry if hang off call failed
 *
 */
void utilHangoffCall() {
  int nRetry = 0;
  while (nRetry < retryMax) {
    if (hangoffCall()) return;
    nRetry++;
    delay(1000);
    Serial.println("hang off failed");
  }
}

/**
 * @brief wrapper of delAllSms
 * retry if delete failed
 *
 */
void utilDelAllSms() {
  int nRetry = 0;
  while (nRetry < retryMax) {
    if (delAllSms()) return;
    nRetry++;
    delay(1000);
    Serial.println("delete sms failed");
  }
}

/**
 * @brief wrapper of sendSms
 * retry if send failed
 *
 */
void utilSendSms(char *number, char *text) {
  int nRetry = 0;
  while (nRetry < retryMax) {
    if (sendSms(number, text)) return;
    nRetry++;
    delay(1000);
    Serial.println("send sms failed");
  }
}

/**
 * @brief return True if communicate with module sim success
 *
 * @return true/false
 */
bool pingPong() {
  String _buffer = "";
  SIM.print(F("AT\r\n"));
  _buffer = readSerial();
  return (_buffer.indexOf("OK") != -1);
}

/**
 * @brief toggle state current status
 *
 */
void checkStatus() {
  Serial.println("check status");
  if (status) {
    turnOffMotor();
    turnOffLed();
    status = false;
    Serial.println("MOTOR TURN OFF");
  } else {
    turnOnMotor();
    turnOnLed();
    status = true;
    Serial.println("MOTOR TURN ON");
  }
}

/**
 * @brief init module sim
 *
 */
void simInit() {
  // Turn off echo mode
  cmdExecute("ATE0");
  // Display information coming call
  cmdExecute("AT+CLIP=1");
  // Set to text mode
  cmdExecute("AT+CMGF=1");
  // Display message directly
  // cmdExecute("AT+CNMI=2,2");
  utilDelAllSms();
  Serial.println("Successfully initialized!");
}

/**
 * @brief execute AT command
 *
 * @param cmd
 */
void cmdExecute(char *cmd) {
  String tmp = "";
  SIM.print(cmd);
  SIM.print(F("\r\n"));
  Serial.println(cmd);
  tmp = readSerial();
  Serial.println(tmp);
  while (tmp.indexOf("ER") != -1 || tmp == NULL) {
    Serial.print(cmd);
    Serial.println(" error, try again");
    delay(2000);
    SIM.print(cmd);
    SIM.print(F("\r\n"));
    tmp = readSerial();
  }
}

/**
 * @brief read feetback from module sim
 *
 * @return String
 */
String readSerial() {
  int _timeout = 0;
  while (!SIM.available() && _timeout < 12000) {
    delay(13);
    _timeout++;
  }
  if (_timeout == 12000) return "";
  if (SIM.available()) {
    return SIM.readString();
  }
}

/**
 * @brief make a phone call
 *
 * @param number
 */
void callNumber(char *number) {
  SIM.print(F("ATD"));
  SIM.print(number);
  SIM.print(F(";\r\n"));
}

/**
 * @brief hang off call
 *
 * @return true/false
 */
bool hangoffCall() {
  String _buffer = "";
  SIM.print(F("ATH\r\n"));
  _buffer = readSerial();
  return (_buffer.indexOf("OK") != -1);
}

/**
 * @brief send a message to phone number
 *
 * @param number
 * @param text
 * @return true/false
 */
bool sendSms(char *number, char *text) {
  String _buffer = "";
  char tmp[25];
  sprintf(tmp, "AT+CMGS=\"%s\"", number);
  cmdExecute(tmp);
  SIM.print(text);
  Serial.println(text);
  SIM.print((char)26);
  _buffer = readSerial();
  Serial.println(_buffer);
  return (_buffer.indexOf("CMGS") != -1);
}

/**
 * @brief delete all message in sim
 *
 * @return true/false
 */
bool delAllSms() {
  String _buffer = "";
  SIM.print(F("AT+CMGDA=\"DEL ALL\"\r\n"));
  _buffer = readSerial();
  return (_buffer.indexOf("OK") != -1);
}

/**
 * @brief read content sms
 *
 * @param index
 * @return String
 */
String readSms(uint8_t index) {
  String _buffer = "";
  SIM.print(F("AT+CMGF=1\r"));
  if ((readSerial().indexOf("ER")) == -1) {
    SIM.print(F("AT+CMGR="));
    SIM.print(index);
    SIM.print("\r");
    _buffer = readSerial();
    if (_buffer.indexOf("CMGR:") != -1) {
      return _buffer;
    }
  }
  return "";
}