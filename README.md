# Heizungsschaltung
Arduino Home Automat implements a couple of useful functions to control the heating system or other uses

Uses:
- Arduino UNO
- Clock Shield (aka Tick Tock Shield)
- 4x Relais Block 240V/10A
- 4V adjustable voltage power converter
- SIM800L cellphone/sms/gprs/bluetooth module
- 6V external power supply (USB is not sufficient to power 4 relais)
 
Features:
- controllable over USB
- controllable over SMS
- gives SMS feedback optional 
- subscriber list (up to 10 allowed phones)
- storable configuration, power persistent
- exact RTC (Real Time Clock)
- 4 usable analog inputs (temperature and light sensor at the moment)
- 1 button menu control (leaves pins free)

Uses about 60% space on an ATmega328, but if you cut out the textmessages it can be much shorter.

Menu:
```
Arduino Home Automat V1.2.1
  User: XXXXXX
    (CCBY) 2015 by Hanno Behrens
           https://plus.google.com/u/0/+HannoBehrens
 Help
  Rx=1     set relais x 1=ON 0=OFF
  T=hh:mm  sets time
  Mx=y     SMS message number
   R=x,y   - read number, nochange
   L=y     - list y=types
   D=x,y   - delete number,classes
   F=x     - activate text format
  ^x       - control sequence ^A..^Z
  Cxx      config
   L       - load
   S       - store
   F       - factory reset
   U       - username
   P       - pin
   A       - apn
   R       - store relais defaults
   B       - baud rates
    S=rate -- serial
    F=rate -- fon
   E=x     - echo on/off
   N       - subscriber numbers
    L[=0/1]-- list off/on
    A=num  -- add
    D=idx  -- del
  S        status
  H        this help
READY.
```
