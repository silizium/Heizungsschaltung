#ifndef __RELAIS_H__
#define __RELAIS_H__

#include <Arduino.h>

class Relais{
public:
    static const byte SIZE=4;
private:
    byte __relais[SIZE];
public:
    Relais(byte r0, byte r1, byte r2, byte r3){
        __relais[0]=r0;
        __relais[1]=r1;
        __relais[2]=r2;
        __relais[3]=r3;
        for(byte i=0; i<SIZE; i++){
          pinMode(__relais[i], OUTPUT);
          digitalWrite(__relais[i], LOW);
        }
    }
    byte num(){
      return sizeof(__relais);
    }
    
    void on(byte relais){                               // set relais
        if(relais<SIZE) digitalWrite(__relais[relais], HIGH);
    }
    
    void off(byte relais){                              // clear relais
        if(relais<SIZE) digitalWrite(__relais[relais], LOW);
    }
    
    unsigned char state(byte relais){                   // return state of relais, HIGH: on, LOW: off
      if(relais>=SIZE) return LOW;
      return digitalRead(__relais[relais]);
    }
    boolean set(byte relais, boolean val){
      if(relais>=SIZE) return LOW;
      digitalWrite(__relais[relais], val);
      return val;
    }

    void flip(byte relais){
      if(relais>=SIZE) return;
      byte port=__relais[relais];
      digitalWrite(port, !digitalRead(port));
    }

    void onAll(){
      for(byte i=0; i<sizeof(__relais); i++)
        on(i);
    }
    void offAll(){
      for(byte i=0; i<sizeof(__relais); i++)
        off(i);
    }
    void switchAll(){
      for(byte i=0; i<sizeof(__relais); i++)
        switch(i);
    }
  
};

#endif
