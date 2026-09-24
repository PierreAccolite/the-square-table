#include <Arduino.h>
#include <Preferences.h>

static const int JOY_X_PIN = 32;
static const int JOY_Y_PIN = 33;
static const int JOY_BTN_PIN = 25;
static const uint32_t USB_BAUD = 115200;
static const uint32_t HEARTBEAT_MS = 2000;
static const uint32_t INPUT_REPORT_MS = 50;
static const uint32_t BUTTON_DEBOUNCE_MS = 30;

static const int DEFAULT_X_MIN = 0;
static const int DEFAULT_X_MAX = 4095;
static const int DEFAULT_Y_MIN = 0;
static const int DEFAULT_Y_MAX = 4095;
static const int DEFAULT_X_CENTER = 2870;
static const int DEFAULT_Y_CENTER = 2780;
static const int JOY_DEADZONE = 8;

int joyXMin=DEFAULT_X_MIN, joyXMax=DEFAULT_X_MAX;
int joyYMin=DEFAULT_Y_MIN, joyYMax=DEFAULT_Y_MAX;
int joyXCenter=DEFAULT_X_CENTER, joyYCenter=DEFAULT_Y_CENTER;

Preferences preferences;
unsigned long bootTime=0, lastHeartbeat=0, lastInputReport=0;

bool inputStreaming=false;

struct ControllerState { int rawX; int rawY; bool button; };
ControllerState controller={0,0,false};
bool lastButtonState=false;
unsigned long lastButtonChange=0;

enum CalibrationState { CAL_IDLE, CAL_CENTER, CAL_RANGE };
CalibrationState calibrationState=CAL_IDLE;
unsigned long calibrationStarted=0;
unsigned long calibrationNextProgress=0;
long centerXTotal=0, centerYTotal=0;
uint16_t centerSamples=0;
int newXMin=4095, newXMax=0, newYMin=4095, newYMax=0;
int newXCenter=0, newYCenter=0;

int clampInt(int value,int minimum,int maximum){
  if(value<minimum)return minimum;
  if(value>maximum)return maximum;
  return value;
}

int normalizeAxis(int value,int minimum,int maximum,int center){
  value=clampInt(value,minimum,maximum);
  long result;
  if(value>=center) result=map(value,center,maximum,0,100);
  else result=map(value,minimum,center,-100,0);
  if(abs((int)result)<=JOY_DEADZONE)return 0;
  return clampInt((int)result,-100,100);
}

void loadCalibration(){
  preferences.begin("pocketjoy",true);
  joyXMin=preferences.getInt("xmin",DEFAULT_X_MIN);
  joyXMax=preferences.getInt("xmax",DEFAULT_X_MAX);
  joyYMin=preferences.getInt("ymin",DEFAULT_Y_MIN);
  joyYMax=preferences.getInt("ymax",DEFAULT_Y_MAX);
  joyXCenter=preferences.getInt("xctr",DEFAULT_X_CENTER);
  joyYCenter=preferences.getInt("yctr",DEFAULT_Y_CENTER);
  preferences.end();

  if(joyXMax<=joyXMin || joyXCenter<=joyXMin || joyXCenter>=joyXMax){
    joyXMin=DEFAULT_X_MIN; joyXMax=DEFAULT_X_MAX; joyXCenter=DEFAULT_X_CENTER;
  }
  if(joyYMax<=joyYMin || joyYCenter<=joyYMin || joyYCenter>=joyYMax){
    joyYMin=DEFAULT_Y_MIN; joyYMax=DEFAULT_Y_MAX; joyYCenter=DEFAULT_Y_CENTER;
  }
}

void saveCalibration(){
  preferences.begin("pocketjoy",false);
  preferences.putInt("xmin",joyXMin); preferences.putInt("xmax",joyXMax);
  preferences.putInt("ymin",joyYMin); preferences.putInt("ymax",joyYMax);
  preferences.putInt("xctr",joyXCenter); preferences.putInt("yctr",joyYCenter);
  preferences.end();
}

void readController(){
  controller.rawX=analogRead(JOY_X_PIN);
  controller.rawY=analogRead(JOY_Y_PIN);
  controller.button=digitalRead(JOY_BTN_PIN)==LOW;
}

void sendJoystick(){
  readController();
  Serial.print("JOY,");
  Serial.print(normalizeAxis(controller.rawX,joyXMin,joyXMax,joyXCenter));
  Serial.print(",");
  Serial.print(normalizeAxis(controller.rawY,joyYMin,joyYMax,joyYCenter));
  Serial.print(",");
  Serial.println(controller.button?"1":"0");
}

void sendRawJoystick(){
  readController();
  Serial.print("JOYRAW,"); Serial.print(controller.rawX); Serial.print(",");
  Serial.print(controller.rawY); Serial.print(",");
  Serial.println(controller.button?"1":"0");
}

void sendInputEvent(const char* eventName){
  Serial.print("EVENT,"); Serial.println(eventName);
}

void updateButton(){
  bool current=digitalRead(JOY_BTN_PIN)==LOW;
  if(current!=lastButtonState && millis()-lastButtonChange>=BUTTON_DEBOUNCE_MS){
    lastButtonState=current;
    lastButtonChange=millis();
    sendInputEvent(current?"BUTTON_DOWN":"BUTTON_UP");
  }
}

void sendStatus(){
  readController();
  Serial.print("STATUS,chip="); Serial.print(ESP.getChipModel());
  Serial.print(",cpu="); Serial.print(ESP.getCpuFreqMHz());
  Serial.print(",flash_mb="); Serial.print(ESP.getFlashChipSize()/1024UL/1024UL);
  Serial.print(",heap="); Serial.print(ESP.getFreeHeap());
  Serial.print(",uptime="); Serial.print((millis()-bootTime)/1000UL);
  Serial.print(",stream="); Serial.print(inputStreaming?"1":"0");
  Serial.print(",cal="); Serial.print(joyXMin); Serial.print(":"); Serial.print(joyXMax);
  Serial.print(":"); Serial.print(joyXCenter); Serial.print(":"); Serial.print(joyYMin);
  Serial.print(":"); Serial.print(joyYMax); Serial.print(":"); Serial.println(joyYCenter);
}

void startCalibration(){
  if(calibrationState!=CAL_IDLE){
    Serial.println("CAL,ERROR,ALREADY_RUNNING");
    return;
  }

  inputStreaming=false;
  calibrationState=CAL_CENTER;
  calibrationStarted=millis();
  calibrationNextProgress=calibrationStarted;
  centerXTotal=0; centerYTotal=0; centerSamples=0;
  newXMin=4095; newXMax=0; newYMin=4095; newYMax=0;

  Serial.println("CAL,START");
  Serial.println("CAL,CENTER,KEEP_STICK_CENTERED");
}

void finishCalibration(){
  if(newXMax-newXMin<200 || newYMax-newYMin<200){
    Serial.println("CAL,ERROR,NOT_ENOUGH_RANGE");
    calibrationState=CAL_IDLE;
    return;
  }

  joyXMin=newXMin; joyXMax=newXMax; joyYMin=newYMin; joyYMax=newYMax;
  joyXCenter=newXCenter; joyYCenter=newYCenter;
  saveCalibration();

  Serial.print("CAL,DONE,X="); Serial.print(joyXMin); Serial.print(":");
  Serial.print(joyXMax); Serial.print(":"); Serial.print(joyXCenter);
  Serial.print(",Y="); Serial.print(joyYMin); Serial.print(":");
  Serial.print(joyYMax); Serial.print(":"); Serial.println(joyYCenter);

  calibrationState=CAL_IDLE;
}

void serviceCalibration(){
  if(calibrationState==CAL_IDLE)return;

  unsigned long now=millis();

  if(calibrationState==CAL_CENTER){
    int x=analogRead(JOY_X_PIN), y=analogRead(JOY_Y_PIN);
    centerXTotal+=x; centerYTotal+=y; centerSamples++;

    if(now-calibrationStarted>=1000UL){
      newXCenter=centerSamples?centerXTotal/centerSamples:analogRead(JOY_X_PIN);
      newYCenter=centerSamples?centerYTotal/centerSamples:analogRead(JOY_Y_PIN);
      Serial.print("CAL,CENTER_CAPTURED,"); Serial.print(newXCenter);
      Serial.print(","); Serial.println(newYCenter);
      Serial.println("CAL,RANGE,MOVE_STICK_FULL_RANGE");
      calibrationState=CAL_RANGE;
      calibrationStarted=now;
      calibrationNextProgress=now;
      return;
    }
  }

  if(calibrationState==CAL_RANGE){
    int x=analogRead(JOY_X_PIN), y=analogRead(JOY_Y_PIN);
    if(x<newXMin)newXMin=x; if(x>newXMax)newXMax=x;
    if(y<newYMin)newYMin=y; if(y>newYMax)newYMax=y;

    if(now>=calibrationNextProgress){
      unsigned long elapsed=now-calibrationStarted;
      int remaining=(elapsed<6000UL)?(int)((6000UL-elapsed+999UL)/1000UL):0;
      Serial.print("CAL,PROGRESS,"); Serial.println(remaining);
      calibrationNextProgress=now+1000UL;
    }

    if(now-calibrationStarted>=6000UL) finishCalibration();
  }
}

void handleCommand(String command){
  command.trim(); if(!command.length())return;
  String upper=command; upper.toUpperCase();

  if(calibrationState!=CAL_IDLE && upper!="STATUS" && upper!="JOYRAW" && upper!="PING"){
    if(upper=="CALIBRATE") Serial.println("CAL,ERROR,ALREADY_RUNNING");
    else Serial.println("ERROR,CALIBRATION_IN_PROGRESS");
    return;
  }

  if(upper=="PING") Serial.println("PONG");
  else if(upper=="STATUS") sendStatus();
  else if(upper=="JOY") sendJoystick();
  else if(upper=="JOYRAW") sendRawJoystick();
  else if(upper=="INPUT"){ inputStreaming=true; Serial.println("INPUT,STREAM,ON"); }
  else if(upper=="INPUT OFF"){ inputStreaming=false; Serial.println("INPUT,STREAM,OFF"); }
  else if(upper=="CALIBRATE") startCalibration();
  else if(upper=="START") sendInputEvent("START");
  else if(upper=="SELECT") sendInputEvent("SELECT");
  else if(upper=="MENU") sendInputEvent("MENU");
  else if(upper=="BACK") sendInputEvent("BACK");
  else if(upper=="HELP") Serial.println("HELP,PING|STATUS|JOY|JOYRAW|INPUT|INPUT OFF|CALIBRATE|START|SELECT|MENU|BACK|HELP");
  else { Serial.print("ERROR,UNKNOWN_COMMAND,"); Serial.println(command); }
}

void setup(){
  pinMode(JOY_BTN_PIN,INPUT_PULLUP);
  analogReadResolution(12);
  loadCalibration();
  Serial.begin(USB_BAUD);
  delay(500);
  bootTime=millis();

  readController();
  lastButtonState=controller.button;
  lastButtonChange=millis();

  Serial.println();
  Serial.println("========================================");
  Serial.println("       POCKET ARCADE USB PLATFORM");
  Serial.println("========================================");
  Serial.println("Transport: USB Serial");
  Serial.print("Baud: "); Serial.println(USB_BAUD);
  Serial.println("Controller: GPIO32 / GPIO33 / GPIO25");
  Serial.println("Display: Android host");
  Serial.println("----------------------------------------");
  Serial.println("READY,POCKET_ARCADE_USB");
  Serial.println("PROTOCOL,USB_SERIAL_TEXT_V1");
  Serial.println("Type HELP for commands.");
  Serial.println("----------------------------------------");
}

void loop(){
  if(Serial.available()) handleCommand(Serial.readStringUntil('\n'));

  serviceCalibration();
  updateButton();

  if(inputStreaming && calibrationState==CAL_IDLE && millis()-lastInputReport>=INPUT_REPORT_MS){
    lastInputReport=millis();
    sendJoystick();
  }

  if(millis()-lastHeartbeat>=HEARTBEAT_MS){
    lastHeartbeat=millis();
    Serial.print("HEARTBEAT,uptime="); Serial.print((millis()-bootTime)/1000UL);
    Serial.print(",heap="); Serial.println(ESP.getFreeHeap());
  }

  delay(2);
}
