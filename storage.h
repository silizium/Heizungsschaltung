#ifndef __STORAGE_H__
#define __STORAGE_H__
#include <EEPROM.h>

//#define MAX_SUBSCRIBERS 10

class Storage {
  public:
  static const byte MAX_SUBSCRIBERS=10;

    byte    version;
    int32_t serialBaud;
    int32_t fonBaud;
    char    username[21];         //Gerhard Hinz
    short   pin;                  //1111
    char    apn[40];              //pinternet.interkom.de
    boolean relais[4];            //relais defaults
    boolean enableSubscriber;
    char    subscriber[MAX_SUBSCRIBERS][21];   //10 numbers may subscribe and can get permit

    Storage(): version(1) {}

       
    void store() {
      for (unsigned int mem = 0; mem < EEPROM.length() && mem < sizeof(Storage); mem++) {
        EEPROM[mem] = (uint8_t) * (((byte *)this) + mem);
      }
    }
    void load() {
      if (EEPROM[0] != version) {
        //default values
        serialBaud=57600;
        fonBaud=57600;
        strcpy(username, "Gerhard");
        pin = 1111;
        strcpy(apn, "pinternet.interkom.de");
        enableSubscriber = true;
        strcpy(subscriber[0], "+4915157312058");
        strcpy(subscriber[1], "+4917690726378");
        strcpy(subscriber[2], "+4917648164144");
      } else
        for (unsigned int mem = 0; mem < EEPROM.length() && mem < sizeof(Storage); mem++) {
          *(((byte *)this) + mem) = (uint8_t) EEPROM[mem];
        }
    }
    /*
      Sanitize CallerID
    */
    void setSubscriber(int8_t idx, char *number) {
      int8_t i;
      for (i = 0; i < sizeof(subscriber[idx]) && *number; number++) {
        if (isdigit(*number)) {
          subscriber[idx][i++] = *number;
        } else if (*number == '+' || *number == '@') {
          subscriber[idx][i++] = *number;
        }
      }
      subscriber[idx][i] = 0;
    }
    /*
     * Gets a free subscriber 
     * or -1
     */
    int8_t freeSubscriber(){
      int8_t idx;
      for(idx=MAX_SUBSCRIBERS-1; idx>=0; idx--)
        if(subscriber[idx][0]==0) break;

      return idx;
    }
    void delSubscriber(int8_t idx){
      subscriber[idx][0]=0;
    }
    int8_t addSubscriber(char *number){
      int8_t idx=freeSubscriber();
      if(idx < 0) idx=0;
      setSubscriber(idx, number);
      return idx;
    }
    int8_t findSubscriber(char *number){
      int8_t idx;
      for(idx=MAX_SUBSCRIBERS-1; idx>=0; idx--){
        if(strstr(subscriber[idx], number)) break;
      }
      return idx;
    }
    boolean isSubscriber(char *number){
      if(!enableSubscriber) return true;
      int8_t idx;
      for(idx=MAX_SUBSCRIBERS-1; idx>=0; idx--){
        if(!strcmp(subscriber[idx], number)) break; //equal
      }
      return idx<0?false:true;
    }
    void storeRelais(Relais &hardware){
      for(byte i=0; i<hardware.num(); i++)
        relais[i]=hardware.state(i);
    }
    void loadRelais(Relais &hardware){
      for(byte i=0; i<hardware.num(); i++)
        hardware.set(i, relais[i]);
    }
} config;

#endif

