#include <SoftwareSerial.h>
#include <TimerOne.h> // https://www.arduinolibraries.info/libraries/timer-one
#include "Arduino.h"

#define RX_PIN 10
#define TX_PIN 11

SoftwareSerial SIM(RX_PIN, TX_PIN);

const int btn1 = 2;  // the number of the button pin
const int out1 = 5;  // the number of the motor control pin
const int led1 = 6;  // the number of the led pin

const char myPhone1[] = "0977066594";
const char myPhone2[] = "+84977066594";

String buff = "";

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

  pinMode(out1, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(btn1, INPUT_PULLUP);

  digitalWrite(out1, HIGH);
  digitalWrite(led1, LOW);

  attachInterrupt(digitalPinToInterrupt(btn1), btnInterrup, FALLING);

  simInit();
}

void loop() {
  buff = readSerial();
  Serial.println(buff);
  if (buff.indexOf("RING") != -1 &&
      (buff.indexOf(myPhone1) != -1 || buff.indexOf(myPhone2) != -1)) {
    checkStatus();
    while (!hangoffCall()) {
    }
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
        digitalWrite(out1, LOW);
        digitalWrite(led1, HIGH);
        status = true;
        while (!sendSms(tmp_num, "DA BAT")) {
          Serial.println("try again...");
          delay(1000);
        }
      } else if (buff.indexOf("TAT") != -1 || buff.indexOf("OFF") != -1) {
        Serial.println("MOTOR TURN OFF");
        digitalWrite(out1, HIGH);
        digitalWrite(led1, LOW);
        status = false;
        while (!sendSms(tmp_num, "DA TAT")) {
          Serial.println("try again...");
          delay(1000);
        }
      }
    }

    while (!delAllSms()) {
      Serial.println("delete sms failed");
      delay(2000);
    }
  }
}

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

void checkStatus() {
  Serial.println("check status");
  if (status) {
    digitalWrite(out1, HIGH);  // off motor
    digitalWrite(led1, LOW);
    status = false;
    Serial.println("MOTOR TURN OFF");
  } else {
    digitalWrite(out1, LOW);  // on motor
    digitalWrite(led1, HIGH);
    status = true;
    Serial.println("MOTOR TURN ON");
  }
}
void simInit() {
  // Turn off echo mode
  cmdExecute("ATE0");
  // Display information coming call
  cmdExecute("AT+CLIP=1");
  // Set to text mode
  cmdExecute("AT+CMGF=1");
  // Display message directly
  // cmdExecute("AT+CNMI=2,2");
  while (!delAllSms()) {
    Serial.println("delete sms failed");
    delay(2000);
  }
  Serial.println("Successfully initialized!");
}

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

String readSerial() {
  int _timeout = 0;
  while (!SIM.available() && _timeout < 12000) {
    delay(13);
    _timeout++;
  }
  if (_timeout == 12000)
    return "";
  if (SIM.available()) {
    return SIM.readString();
  }
}

void callNumber(char *number) {
  SIM.print(F("ATD"));
  SIM.print(number);
  SIM.print(F(";\r\n"));
}

bool hangoffCall() {
  String _buffer = "";
  SIM.print(F("ATH\r\n"));
  _buffer = readSerial();
  if ((_buffer.indexOf("OK")) != -1)
    return true;
  else
    return false;
}

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
  if (((_buffer.indexOf("CMGS")) != -1)) {
    return true;
  } else {
    return false;
  }
}

bool delAllSms() {
  String _buffer = "";
  SIM.print(F("AT+CMGDA=\"DEL ALL\"\r\n"));
  _buffer = readSerial();
  if (_buffer.indexOf("OK") != -1) {
    return true;
  } else {
    return false;
  }
}

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
    } else
      return "";
  } else
    return "";
}