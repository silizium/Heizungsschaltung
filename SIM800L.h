#ifndef __SIM800_H__
#define __SIM800_H__

#include <Arduino.h>
#include <string.h>
#include <SoftwareSerial.h>

#define CTRL_Z 26

class SMS{
  public:
  byte id;
  char caller[21];
  char date[21];
  char text[145];

  SMS(byte num=0):id(num){
      caller[0]=0;
      caller[sizeof(caller)-1]=0;
      date[0]=0;
      date[sizeof(date)-1]=0;
      text[0]=0;
      text[sizeof(text)-1]=0;
  }
  /*
   * Sanitize CallerID
   */
  void setCaller(char *call){
    int i;
    for(i=0; i<sizeof(caller) && *call; call++){
      if(isdigit(*call)){ 
        caller[i++]=*call; 
      }else if(*call == '+' || *call == '@'){
        caller[i++]=*call;
      }
    }
    caller[i]=0;
  }
};

char *strinsert(char *dst, char *src, int del=0){
  //  +1 for count to include terminating null
  size_t len=strlen(src);
  memmove(dst+len, dst+del, len-del+1);
  memcpy(dst, src, len);
  return dst;
}

char *i2str(char *buf, int i, char leadingZ=0){
  byte l=0, pos=4;
  if(i<0) {
    buf[l++]='-';
    i=-i;
  }
  //if(i==0)
  //  buf[l++]='0';
  //else
  for(int div=10000, mod=0; div>0; div/=10, pos--){
    mod=i%div;
    i/=div;
    
    if(leadingZ>=pos || i!=0){
       buf[l++]=i+'0';
    }
    i=mod;
  }
  buf[l]=0;
  return buf;
} 

class SIM800L: public SoftwareSerial{
  private:
    byte __pin_rst;
//    byte m_bytesRecv;
//    uint32_t m_checkTimer;
    boolean echoCmd=true;
    
  public:
    //char sendbuffer[128];
    char linebuffer[128];
  
    SIM800L(byte rx=2, byte tx=3, byte rst=4)
        :SoftwareSerial(rx, tx), 
        __pin_rst(rst){
      setTimeout(1000);
    }
    byte begin(long speed){
      reset();
      SoftwareSerial::begin(speed);
      delay(200);
      //printBreak();
      sendCommand();
    }

    int readLine(boolean flush=0){
      static int pos = 0;
      int rpos;
      if(flush){
        pos=0;
        return -1;
      }
      if(!available()) return -1;
      int readch=read();
    
      if (readch > 0) {
        switch (readch) {
          case '\r': // Ignore CR
            break;
          case '\n': // Return on NL
            rpos = pos;
            pos = 0;  // Reset position index ready for next time
            return rpos;
          default:
            if(pos < sizeof(linebuffer)-1) {
              linebuffer[pos++] = readch;
              linebuffer[pos] = 0;
            }else{
              rpos=pos;
              pos=0;
              return rpos;
            }
        }
      }
      // No end of linebuffer has been found, so return -1.
      return -1;
    }

    void reset(void){
//      Serial.println("RESET");
      pinMode(__pin_rst, OUTPUT);
      digitalWrite(__pin_rst, LOW);
      delay(100);
      digitalWrite(__pin_rst, HIGH);
      delay(3000);
    }

    void printEscape(){
      delay(1000);
      print(F("+++"));
      delay(1000);
      println();
//      Serial.println("PRINT +++");
    }

    void printAT(){ 
      print(echoCmd?F("AT"):F("ATE0"));
    }

    void purgeSerial(){
      while (available()) read();
      readLine(1); //purge linebuffer
    }

    void echo(boolean status){ 
      //return sendCommand("E", i2str(linebuffer, echo));
      echoCmd=status;
    }

    byte unlockPin(int pin){
      delay(1000);
      return sendCommand("+CPIN=",i2str(linebuffer, pin),2000, "+CPIN: READY");
    }
    byte setAPN(char *apn){
      //pinternet.interkom.de
      strcpy(linebuffer, "+CSTT=\"");
      strcat(linebuffer, apn);
      strcat(linebuffer, "\",\"\",\"\"");
      return sendCommand(linebuffer);
    }

    byte smsFormat(byte format){
      return sendCommand("+CMGF=", i2str(linebuffer, format));
    }

    SMS *readSMS(byte number){
      if(!sendCommand("+CMGR=", i2str(linebuffer, number), 2000, "+CMGR")) return NULL;
      char *tok=strtok(linebuffer, "\"");
      for(byte i=0; i<3; i++) tok=strtok(NULL, "\"");
      SMS *sms=new SMS(number);
      sms->setCaller(tok);
      for(byte i=0; i<3; i++) tok=strtok(NULL, "\"");
      strcpy(sms->date, tok);

//      uint32_t to = millis()+1000;
//      char n = 0;
//      do {
//        if ((n=readLine()) > 0) {
//          //Serial.println(linebuffer);
//          if(!strcmp(linebuffer,"OK")) break;
//          strcat(sms->text, linebuffer);
//          Serial.println(linebuffer);
//          Serial.println(sms->text);
//          Serial.println(strlen(sms->text));
//        }else delay(20);
//      } while ((millis() < to)
//          && !strcmp(linebuffer,"OK") 
//          && strlen(sms->text)<sizeof(sms->text));
      //read the whole sms textblock until EOF
      int c, i=0;
      while((c=read()) && c != CTRL_Z && c != EOF && i<sizeof(sms->text)-1){
        sms->text[i++]=c;
      }
      sms->text[i]=0;
      return sms;
    }

    byte sendSMS(SMS *sms){
      if(!smsFormat(1)){ // Textmode
        Serial.println(F("ERROR: can't switch to SMS text mode"));
        return 0;
      }
      if(!sendCommand("+CSCS=\"GSM\"")){
        Serial.println(F("ERROR: can't switch to GSM text mode"));
        return 0;
      }
      strcpy(linebuffer, "AT+CMGS=\"");
      strcat(linebuffer, sms->caller);
      strcat(linebuffer, "\"\r");
      print(linebuffer);

//Serial.println(linebuffer);
//for(byte i=0; linebuffer[i]; i++)Serial.print(linebuffer[i], HEX);
//Serial.println();
      
      delay(100);
      print(sms->text);

//Serial.println(sms->text);
//for(byte i=0; sms->text[i]; i++)Serial.print(sms->text[i], HEX);
//Serial.println();
      
      print('\r');
      delay(100);
      print((char)CTRL_Z);
      delay(100);
      println();

      uint32_t to = millis()+20000;  //timeout
      char n = 0;
      do {
        if ((n=readLine()) > 0) {
          if (strstr(linebuffer, "+CMGS")) {
            short nr=atoi(linebuffer+7);
            deleteSMS(nr);
            Serial.print(F("Message sent and deleted: #"));Serial.println(nr);
            return n;
          }else{
            Serial.println(linebuffer);
            delay(20);
          }
        }
      } while (millis() < to);     
      return 0;
    }
    
    boolean deleteSMS(byte number, byte index=0){
      i2str(linebuffer, number);
      strcat(linebuffer, ",");
      i2str(linebuffer+strlen(linebuffer), index);
//      Serial.println(number);
//      Serial.println(index);
//      Serial.println(linebuffer);
      return sendCommand("+CMGD=", linebuffer, 125000);
    }

    byte listSMS(byte style=4){
      strcpy(linebuffer, "+CMGL=");
      switch(style){
      case 0:
        strcat(linebuffer, "\"REC UNREAD\"");
        break;
      case 1:
        strcat(linebuffer, "\"REC READ\"");
        break;
      case 2:
        strcat(linebuffer, "\"STO UNSENT\"");
        break;
      case 3:
        strcat(linebuffer, "\"STO SENT\"");
        break;
      default:
        strcat(linebuffer, "\"ALL\"");
      }
      return sendCommand(linebuffer, "+CMGL", "OK");
    }

    int getSignalQuality(){
      char *cmd="+CSQ";
      sendCommand(cmd,NULL,2000, cmd);
      //Serial.println(linebuffer);
      char *p = strstr(linebuffer, cmd);
      if (p) {
        int n = atoi(linebuffer+5);
        if (n == 99 || n == -1) return 0;
        //Serial.println(linebuffer);
        //Serial.println(p);
        //Serial.println(n);
        return n * 2 - 114;
      } else {
        return 0; 
      }
    }

    /*
     * sendCommand
     * 
     * ret: 0 if Error
     *      linelength of expected match if OK
     */
    byte sendCommand(const char* cmd = NULL, const char* arg = NULL, unsigned int timeout = 2000, const char* expected = NULL, boolean noexpect=false){
      purgeSerial();
      delay(100);
      printAT();
      if(cmd) print(cmd);
      println(arg);
      //Serial.print(F("COMMAND AT")); Serial.print(cmd);Serial.println(arg);
      uint32_t to = millis()+timeout;
      char n = 0;
      do {
        if ((n=readLine()) > 0) {
          if(noexpect && strstr(linebuffer, "ERROR")){
            Serial.println(F("sendSerial: ERROR"));
            return 0;
          }else
          if (strstr(linebuffer, expected ? expected : "OK")) {
            return n;
          }else{
            Serial.println(linebuffer);
            delay(20);
          }
        }
      } while (millis() < to);
      if(noexpect){
        return 1;
      }else{
        Serial.println(F("SendCommand: TIMEOUT"));
        return 0;
      }
    }
    // send AT command and check for two possible responses
    byte sendCommand(const char* cmd, const char* expected1, const char* expected2, unsigned int timeout = 2000){
      purgeSerial();
      printAT();
      println(cmd);
      uint32_t t = millis();
      byte n = 0;
      do {
        if ((n=readLine()) > 0){
          if (strstr(linebuffer, expected1)) {
           return 1;
          }
          if (strstr(linebuffer, expected2)) {
           return 2;
          }
        }else{
          Serial.println(linebuffer);
          delay(20);
        }
      } while (millis() - t < timeout);
      Serial.println(F("SendCommand: TIMEOUT"));
      return 0;
    }
 
    
//    byte checksendbuffer(const char* expected1, const char* expected2, unsigned int timeout){
//      while (available()) {
//          char c = read();
//          if (m_bytesRecv >= sizeof(sendbuffer) - 1) {
//              // sendbuffer full, discard first half
//              m_bytesRecv = sizeof(sendbuffer) / 2 - 1;
//              memcpy(sendbuffer, sendbuffer + sizeof(sendbuffer) / 2, m_bytesRecv);
//          }
//          sendbuffer[m_bytesRecv++] = c;
//          sendbuffer[m_bytesRecv] = 0;
//          if (strstr(sendbuffer, expected1)) {
//              return 1;
//          }
//          if (expected2 && strstr(sendbuffer, expected2)) {
//              return 2;
//          }
//      }
//      return (millis() - m_checkTimer < timeout) ? 0 : 3;
//   }
};

#endif
