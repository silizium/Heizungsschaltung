/*********************************************************************************************************
*  Heizungssteuerung
*  
*  Uses:
*  Arduino UNO (Atmega328)
*  4 Relais board
*  Clock Shield (Tick Tock board) / with only 1 button
*  SIM800L sim/gprs/sms 
*  4V adjustable voltage regulator for driving the SIM800
*  external 6V power supply
*
*  Author: Hanno Behrens
*  https://plus.google.com/u/0/+HannoBehrens
*  2015-12-20
*********************************************************************************************************/

#include <Wire.h>
#include <TimerOne.h>
#include <Streaming.h>

#include <TTSDisplay.h>
#include <TTSBuzzer.h>
#include <TTSLed.h>
#include <TTSButton.h>
#include <TTSLight.h>
#include <TTSTemp.h>
#include <TTSTime.h>
#include <OneButton.h>
#include <SoftwareSerial.h>
#include <string.h>
#include "Relais.h"
#include "SIM800L.h"
#include "storage.h"

#define VERSION "V1.2"

// STATE define here
enum{
  ST_TIME,                            // normal mode, display time and temperature
  ST_RELAIS,
  ST_RELAIS1,          
  ST_RELAIS2,         
  ST_RELAIS3,          
  ST_RELAIS4,          
//  ST_SETIME,                          // set time 
//  ST_SETALARM,                        // set time of alarm
//  ST_ALARMING,                        // alarming
//  ST_LIGHT,                           // display light
  ST_TEMP,                             // display temperature
  ST_LAST,
  ST_SETTIME
}states;

// object define here
byte brightness[]={
  BRIGHT_TYPICAL, BRIGHTEST, BRIGHT_DARKEST
};
byte brightIndex;
TTSDisplay disp;                            // instantiate an object of display
TTSLed led1(TTSLED1);                       // instantiate an object of led1
TTSLed led2(TTSLED2);                       // instantiate an object of led2
TTSLed led3(TTSLED3);                       // instantiate an object of led3
TTSLed led4(TTSLED4);                       // instantiate an object of led4
TTSBuzzer buz;                              // instantiate an object of buzzer
OneButton keyMode(TTSK3, true);
//TTSLight light;                             // instantiate an object of light sensor
TTSTemp temp1(A0), temp2(A1);                               // instantiate an object of temperature sensor
TTSTime time;                               // instantiate an object of rtc



Relais relais(12, 5, A2, A3);
#define SIM_RX 9
#define SIM_TX 10
#define SIM_RST 4
SIM800L fon=SIM800L(SIM_RX, SIM_TX, SIM_RST); // RX, TX, RESET

int state = ST_TIME;                        // state

int alarm_hour = 8;                         // hour of alarm
int alarm_min  = 0;                         // minutes of alarm

int now_hour;                               // hour of running
int now_min;                                // minutes of running
int now_sec;                                // second of running

//char buffer[256];
//byte cnt=0;

/*********************************************************************************************************
 * Function Name: timerIsr
 * Description:  timer on interrupt service function to display time per 500ms
 * Parameters: None
 * Return: None
*********************************************************************************************************/
void timerIsr(){
    static byte relcnt=0;
    static unsigned char state_colon = 0;
    if(ST_TIME == state){
        state_colon = !state_colon;
        if(state_colon){
            disp.pointOn();                             // : on
        }else{
            disp.pointOff();                            // : off
        }
        disp.time(now_hour, now_min); 
    }
}


void setup(){
    Timer1.initialize(500000);                                  // timer1 500ms
    Timer1.attachInterrupt( timerIsr ); 
  
    now_hour = time.getHour();
    now_min  = time.getMin();
    now_sec  = time.getSec();
  
    keyMode.attachClick(nextState);
    keyMode.attachDoubleClick(nextState);

    config.load();

    Serial.begin(config.serialBaud);
    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }
    Serial.println(F("SIM800L Startup"));
    Serial.setTimeout(100);

    Serial.print(F("Config Version ")); Serial.print(config.version);
    Serial.print(F(" size ")); Serial.println(sizeof(Storage));

    resetModem();
}

void resetModem(){
  fon.begin(config.fonBaud);
  fon.echo(0); //Echo off

  if(fon.unlockPin(config.pin)){
    Serial.println(F("SIM: unlocked"));

    checkSigQuality();

    if(fon.smsFormat(1)){
      Serial.println(F("SMS: text format"));
    }
    if(fon.setAPN(config.apn)){
      Serial.println(F("APN set"));
    }
    Serial.println(F("READY."));
    parseLine("H");
  }
  config.loadRelais(relais);
}

#define BUFSIZE 80

void loop(){
    static char charbuffer[BUFSIZE];    
    
    keyMode.tick();
    displayState();
    if (fon.readLine() > 0) {
      Serial.println(fon.linebuffer);
      //New SMS
      if(strstr(fon.linebuffer, "+CMTI")){
        Serial.println(F("Processing new SMS"));
        char *tok;
        short id;
        SMS *sms;
        tok=strchr(fon.linebuffer, ',');
        id=atoi(tok+1);
        if(sms=fon.readSMS(id)){
          Serial.print(F("ID:")); Serial.println(sms->id);
          Serial.print(F("CALLER:")); Serial.println(sms->caller);
          Serial.print(F("DATE:"));Serial.println(sms->date);
          Serial.print(F("TEXT:"));Serial.println(sms->text);
          if(config.isSubscriber(sms->caller)){
            Serial.println(F("valid subscription"));
            char *pos=strstr(sms->text, "s=*");
            if(!pos) pos=strstr(sms->text, "S=*");
            if(pos){
              Serial.println(F("processing S=*: "));
              if(sizeof(sms->text)>strlen(sms->text)+strlen(sms->caller)){
                strinsert(pos+2, sms->caller, 1);
              }else{
                Serial.println(F("not enough space for asterix expansion"));
              }
            }
            parseLine(sms->text);
          }else
            Serial.println(F("not a valid subscribtion, no processing"));
          fon.deleteSMS(id);
          delete sms;
        }else
          Serial.print(F("No SMS with that ID\n"));
      }
    }
    if (readline(Serial.read(), charbuffer, sizeof(charbuffer)) > 0) {
      parseLine(charbuffer);
    }
//    while(Serial.available()){
//       char c=Serial.read();
//       buffer[cnt++]=c;
//       if((c == '\n') || (cnt >= sizeof(buffer)-1)) {
//          buffer[cnt] = '\0';
//          cnt=0;
//          parseLine(buffer);
//       }
//    }
}

int readline(int readch, char *buffer, int len)
{
  static int pos = 0;
  int rpos;
  
  if (readch > 0) {
    if(config.echo) Serial.print((char)readch);
    switch (readch) {
      case '\n': // Ignore LF
        break;
      case '\r': // Return on CR
        rpos = pos;
        pos = 0;  // Reset position index ready for next time
        return rpos;
      default:
        if (pos < len-1) {
          buffer[pos++] = readch;
          buffer[pos] = 0;
        }
    }
  }
  // No end of line has been found, so return -1.
  return -1;
}

void parseLine(char *buffer){
  if(strcasestr(buffer, "AT")==buffer){
    fon.println(buffer);
  }else{
    char *tok1, *tok2, subcmd;
    long arg1;
    int arg2, arg3;
    int8_t idx;
    if(strstr(buffer, "RESET")){
      Serial.println(F("Modem reset"));
      resetModem();
      return;
    }
    tok1=strtok(buffer, " \n\r\t");
    while(tok1 != NULL){
      *tok1=toupper(*tok1);
      switch(*tok1){
        // Relais
        case 'R':
          arg1=atoi(tok1+1)-1;
          tok2=strchr(tok1, '=');
          arg2=atoi(tok2+1);
          if(arg2) relais.on(arg1); else relais.off(arg1);
          Serial.print(F("Relais "));
          Serial.print(arg1+1);
          Serial.print(F(" set to "));
          Serial.println(relais.state(arg1)?F("ON"):F("OFF"));
          break;
        // Time
        case 'T':
          tok2=strchr(tok1, '=');
          arg1=atoi(tok2+1);
          tok2=strchr(tok2, ':');
          arg2=atoi(tok2+1);
          now_hour=arg1;
          now_min=arg2;
          time.setTime(now_hour, now_min, 0);
          Serial.print(F("Time set at "));
          Serial.print(time.getHour());
          Serial.print(F(":"));
          Serial.println(time.getMin());
          break;
        // Messages
        case 'M': // Read Message #
          subcmd=toupper(*(tok1+1));
          SMS *sms;
          tok2=strchr(tok1, '=');
          arg1=atoi(tok2+1);
          switch(subcmd){
          case 'D': //MD=x delete
            tok2=strchr(tok2, ',');
            arg2=atoi(tok2+1);
            fon.deleteSMS(arg1, arg2);
            Serial.print(F("SMS deleted: ")); Serial.println(arg1);
            break;
          case 'F': //MF=x message format
            fon.smsFormat(arg1);
            Serial.print(F("SMS Text Format ")); Serial.println(arg1);
            break;
          case 'L': //List SMS
            fon.listSMS(arg1);
            break;
          default:  //read
            if(sms=fon.readSMS(arg1)){
              Serial.print(F("ID:")); Serial.println(sms->id);
              Serial.print(F("CALLER:")); Serial.println(sms->caller);
              Serial.print(F("DATE:"));Serial.println(sms->date);
              Serial.print(F("TEXT:"));Serial.println(sms->text);
              delete sms;
            }else Serial.print(F("No SMS with that ID\r\n"));
            //break;
          }
          break;
        // Status
        case 'S':
           char statusbuffer[50];
           *statusbuffer = 0;
           for(byte i=0; i<4; i++){
              strcat(statusbuffer, "R");
              i2str(statusbuffer+strlen(statusbuffer),i+1); 
              strcat(statusbuffer, "="); 
              i2str(statusbuffer+strlen(statusbuffer),relais.state(i));
              strcat(statusbuffer, " ");
           }
           strcat(statusbuffer, "T=");
           i2str(statusbuffer+strlen(statusbuffer), now_hour,1); // 2 digits
           strcat(statusbuffer, ":");
           i2str(statusbuffer+strlen(statusbuffer), now_min,1);  // 2 digits
           strcat(statusbuffer, " C(1)=");
           i2str(statusbuffer+strlen(statusbuffer), temp1.get());
           strcat(statusbuffer, " C(2)=");
           i2str(statusbuffer+strlen(statusbuffer), temp2.get());
           if(*(tok1+1) != '=')
              Serial.println(statusbuffer);
           else{ //send as SMS
              SMS *sms=new SMS();
              sms->setCaller(tok1+2);
              strcpy(sms->text, statusbuffer);
              //delay(5000);
              if(fon.sendSMS(sms)){
                Serial.print(F("Sending SMS status to "));
              }else{
                Serial.print(F("Trying sending SMS status to "));
              }
              Serial.println(sms->caller);

              delete sms;
           }
           break;
         // CTRL sequences
         case '^':
           fon.print((char)(toupper(*(tok1+1))-'A'+1));
           Serial.print(F("sent CTRL-")); Serial.println((char)toupper(*(tok1+1)));
           break;
         // Configuration
         case 'C': 
           Serial.print(F("config "));
           subcmd=toupper(*(tok1+1));
           tok2=strchr(tok1, '=');
           arg1=atol(tok2+1);
           switch(subcmd){
           case 'L': //load config
             Serial.println(F("reloading"));
             config.load();
             config.loadRelais(relais);
             break;
           case 'S': //store config
             Serial.println(F("storing"));
             config.store();
             break;
           case 'F': //factory reset config
             Serial.println(F("resetting"));
             EEPROM[0]=0;
             config.load();
             break;
           case 'U': //username
             if(tok2){
               strcpy(config.username, tok2+1);
             }
             Serial.print(F("username: "));
             Serial.println(config.username);
             break;
           case 'P': //pin
             if(tok2){
               config.pin=arg1;
             }
             Serial.print(F("pin: "));
             Serial.println(config.pin);
             break;
           case 'A': //APN
             if(tok2){
               strcpy(config.apn, tok2+1);
             }
             Serial.print(F("apn: "));
             Serial.println(config.apn);
             break;           
           case 'E':
             if(tok2){
               config.echo=arg1;
             }
             Serial.print(F("echo: "));
             Serial.println(config.echo);
             break;
           case 'B':  //baud rates
              Serial.print(F("baudrate "));
              subcmd=toupper(*(tok1+2));
              switch(subcmd){
              case 'S':
                if(tok2){
                  config.serialBaud=arg1;
                }
                Serial.print(F("serial is "));
                Serial.println(config.serialBaud);
                break;
              case 'F':
                if(tok2){
                  config.fonBaud=arg1;
                }
                Serial.print(F("fon is "));
                Serial.println(config.fonBaud);
                break;
              default:
                break;
              }
              break;
           case 'N': // subscriber numbers
             subcmd=toupper(*(tok1+2));
             switch(subcmd){
             case 'L':
               if(tok2){
                 config.enableSubscriber=arg1;
               }
               Serial.print(F(" subscriber list is "));
               Serial.println(config.enableSubscriber?F("active"):F("inactive"));
               for(byte i=0; i<config.MAX_SUBSCRIBERS; i++){
                 Serial.print(i);
                 Serial.print(F(": "));
                 Serial.println(config.subscriber[i]);
               }
               break;
             case 'A':  // add subscriber
               if(tok2){
                 idx=config.addSubscriber(tok2+1);
                 Serial.print(F("add subscriber:"));
                 Serial.println(config.subscriber[idx]);
               }
               break;
             case 'D':  // del subscriber
               if(tok2){
                 config.delSubscriber(arg1);
               }
               Serial.print(F("deleted subscriber:"));
               Serial.println(arg1);
               break;
             default:
               break;
             }
             break;
           case 'R': // store relais defaults
             config.storeRelais(relais);
             Serial.println(F("stored relais defaults"));
             break;           
           default:
             break;
           }
          break;
         case 'H':
          Serial.print(F(
            "\nArduino Home Automat " VERSION "\r\n"
            "  User: "));
          Serial.println(config.username);
          Serial.println(F(
            "    (CCBY) 2015 by Hanno Behrens\r\n"
            "           behrens.hanno@gmail.com\r\n"
            " Help\r\n"
            "  Rx=1     set relais x 1=ON 0=OFF\r\n"
            "  T=hh:mm  sets time\r\n"
            "  Mx=y     SMS message number\r\n"
            "   R=x,y   - read number, nochange\r\n"
            "   L=y     - list y=types\r\n"
            "   D=x,y   - delete number,classes\r\n"
            "   F=x     - activate text format\r\n"
            "  ^x       - control sequence ^A..^Z\r\n"
            "  Cxx      config\r\n"
            "   L       - load\r\n"
            "   S       - store\r\n"
            "   F       - factory reset\r\n"
            "   U       - username\r\n"
            "   P       - pin\r\n"
            "   A       - APN\r\n"
            "   R       - store relais defaults\r\n"
            "   B       - baud rates\r\n"
            "    S=rate -- serial\r\n"
            "    F=rate -- fon\r\n"
            "   E=x     - echo on/off\r\n"
            "   N       - subscriber numbers\r\n"
            "    L[=0/1]-- list off/on\r\n"
            "    A=num  -- add\r\n"
            "    D=idx  -- del\r\n" 
            "  S[=n]    status if n is '*' send SMS back or\r\n"
            "           send to a valid cellphone number\r\n"
            "  H        this help\r\n"
            "READY.\r\n")
            );
        default:
        break;
      }
      tok1=strtok(NULL, " ");
    }
  }
}

int checkSigQuality(){
  int q=0;
  uint32_t timeout = millis()+10500;
  do{
    if(q) delay(1000);
    q=fon.getSignalQuality();
  }while(q <= -114 && millis() < timeout);

  led4.off();
  Serial.print(F("SIG: quality=")); Serial.print(q);
  if(q <= -114)
    Serial.println(F(" NO CONNECTION"));
  else if(q <= -111)
    Serial.println(F(" POOR"));
  else if(q <= -52){
    Serial.println(F(" GOOD"));
    led2.on();
  }else{
    Serial.println(F(" PERFECT"));
    led2.on();
  }
  return q;
}

void doNothing(){}

void nextState(){
  state++;
  state %= ST_LAST;
}

void setHour(){
  buz.on();
  delay(10);
  buz.off();
  state=ST_SETTIME;
  disp.pointOff();
  keyMode.attachDoubleClick(setMinute);
  keyMode.attachClick(incHour);
  keyMode.attachDuringLongPress(incHour);
}

void setMinute(){
  buz.on();
  delay(10);
  buz.off();
  keyMode.attachDoubleClick(saveTime);
  keyMode.attachClick(incMinute);
  keyMode.attachDuringLongPress(incMinute);
}

void incHour(){
  now_hour++;
  now_hour %= 24;
  disp.time(now_hour, now_min);
  delay(100);
}

void incMinute(){
  now_min++;
  now_min %= 60;
  disp.time(now_hour, now_min);
  delay(100);
}

void saveTime(){
  time.setTime(now_hour, now_min, 0);
  keyMode.attachDoubleClick(setHour);
  keyMode.attachClick(nextState);
  keyMode.attachDuringLongPress(doNothing);
  state=ST_TIME;
  buz.on();
  delay(500);
  buz.off();
}

void flipRelais(){
  relais.flip(state-ST_RELAIS1);
  buz.on();
  delay(30);
  buz.off();
}

void setBrightness(){
  brightIndex++;
  brightIndex %= sizeof(brightness);
  disp.brightness(brightness[brightIndex]);
  buz.on();
  delay(30);
  buz.off();
}

void displayState(){
  switch(state){
  case ST_TIME:
    keyMode.attachDoubleClick(setHour);
    now_hour = time.getHour();
    now_min  = time.getMin();
//    now_sec  = time.getSec();

    //disp.time(now_hour, now_min);
    break;
  case ST_RELAIS:  
    disp.clear();
    keyMode.attachDoubleClick(flipRelais);
    state++;
  case ST_RELAIS1:
  case ST_RELAIS2:
  case ST_RELAIS3:
  case ST_RELAIS4:
    disp.pointOff();
    disp.display(3, 25); //r   disp.display(2, state-ST_RELAIS1+1);
    disp.display(2, state-ST_RELAIS1+1);
    disp.display(0, relais.state(state-ST_RELAIS1));
    break;
  case ST_TEMP:
    keyMode.attachDoubleClick(setBrightness);
    int temperature = temp1.get();
    disp.temp(temperature);
    break;
  }
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/
