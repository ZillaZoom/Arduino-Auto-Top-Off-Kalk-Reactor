/*************************************************************************
* This program uses a dosing pump with auto topoff sensor capability. 
* The small dosing pump I'm currently using does 1ml/min or 1 drop every 3 seconds
* I have a pot hooked to a1 line to control the off time for the pump.
* At this point the pump is on for 3 seconds with selectible off time. 
* I'm definitly considering a bigger pump as the current one is only rated for 500 hours-
*   but that sounds like a pretty short life.
* Parts needed are 5v dosing pump I run it off 12v 50% PWM, Arduino Uno, LED keypad shield, mosfet driver board-
*   ,5v stirrer, connectors and containers. I have a word document that shows more details.
* If the dosing pump and stirrer exceed 500ma then you'll need to connect the 5v power supply-
*   internally to vcc (recommended) You can slice up a usb cable.
* 3d printer files are listed in the word document.
*
* MIT ZillaZoom Software License 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software. 
*/

// Setup for LCD / Keypad shield See Documents
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <avr/wdt.h>
const int pin_RS = 8; 
const int pin_EN = 9; 
const int pin_d4 = 4; 
const int pin_d5 = 5; 
const int pin_d6 = 6; 
const int pin_d7 = 7; 
const int pin_BL = 10; 
LiquidCrystal lcd( pin_RS,  pin_EN,  pin_d4,  pin_d5,  pin_d6,  pin_d7);

// Keypad defines - input is analog with resistors to limit current/voltage 
// Standard for LCD Keypad Shield

#define RIGHTVAL 60
#define RIGHT 0
#define UPVAL 200
#define UP 1
#define DOWNVAL 400
#define DOWN 2
#define LEFTVAL 600
#define LEFT 3
#define SELECTVAL 800
#define SELECT 4
#define DEBOUNCE 275

#define POT A1                // Pin to read potentiometer
#define PUMP 3                // Pin to turn on/off dosing pump can be pwm
#define STIR 11               // Pin to turn on/off stirrer can be pwm
#define SETTLE 45             // Number of minutes to let solution to settle
#define PERCONV 10.24         // Convert analog signal from pot to 0-100 value
#define SECPERDROP 3          // Run time of the pump - in this case 3 seconds for 1 drop
#define STIRTIME 1            // Stir time in minutes
#define PUMPPWM 180            // This is for a 3-5v dosing pump with 12v input, PWM lowers the voltage
#define STIRPWM 255           // I'm using a 5v stirrer so set to max, If you run the stirrer off 12v adjust pwm
#define OFF 0

#define KEY 123               // We test this to see if eeprom has been programmed on seconds and stir time
#define KEYADDR   0
#define ONSECADDR 1           
#define STIRADDR  4     

int stirtime;            // Changeable values store in eeprom

int potval;             // Raw potval Analog to Digital pot value
int potvalsecold;       // This is to test for pot rotation in seconds 
int potvalsec;          // Pot Value converted to seconds
int retpopdelay;        // Popdelay returns values based on keypad press

unsigned long cumillis = millis();    // Setup millis counters
unsigned long mincounter=millis();
int animcounter=0;                    // Used to change animation on display
long pumptimer;

byte onseconds;
byte epromset;
long onmillis;
bool lock;

void setup()
{
  
  lock=true;

  // delay so we can reprogram before watchdog timer starts
  delay(2000);
  // Enable the watchdog timer with a 4-second timeout duration
  wdt_enable(WDTO_4S); 

  // Set all the motor control pins to outputs
  
  wdt_disable();
    // Set all the motor control pins to outputs
  pinMode(PUMP, OUTPUT);
  pinMode(STIR, OUTPUT);
 
  analogWrite(PUMP,OFF);                  // Turn off dosing pump
  analogWrite(STIR,OFF);                  // Turn off stirrer

  potval=analogRead(POT);                 // Read potentiometer 
  potvalsec = abs(potval/PERCONV-100);    // Convert to seconds 0 to 100
  potvalsecold = potvalsec;               // Store value for later comparison

  lcd.begin(16, 2);
  lcd.setCursor(0,0);
  lcd.print(" Z's K Reactor ");
  delay(1000);
  checkeprom();                            //Check eprom to see if it has valid values first byte = 123
}
 
void loop()
{
  lcd.setCursor(0,1);
  lcd.print("Pumping   ");
  analogWrite(PUMP,PUMPPWM);
  onmillis=(long)onseconds*1000;
  popdelay(onmillis);
  //digitalWrite(PUMP,LOW);
  analogWrite(PUMP,0);
  lcd.setCursor(0,1);
  lcd.print("Pausing   ");
  if (potvalsec>0) popdelay((long)potvalsec*1000);
}


//**********************************************************
int GetKeyPress()
{
  int x;
   x = analogRead(0);
  if (x < RIGHTVAL) {
    delay(DEBOUNCE);
   return(RIGHT);
    }
     else if (x < UPVAL) {
      delay(DEBOUNCE);
      return(UP);
     }
      else if (x < DOWNVAL){
        delay(DEBOUNCE);
       return(DOWN);
      }
       else if (x < LEFTVAL){
        delay(DEBOUNCE);
        return(LEFT);
       }
       else if (x < SELECTVAL){
        delay(DEBOUNCE);
         return(SELECT);
       }
  return(-1); //nokeypress
} // EndFunc

//**************************************************************
//* Interuptible Delay
//*  
int popdelay(unsigned long period)
{
  unsigned long curmillis;
  unsigned long endmillis;
  unsigned long loopcounter=0;
  wdt_reset();                      /* Reset Watchdog timer */
  curmillis=millis();
  endmillis=curmillis+period;
  int keypress;
    do {
     keypress=GetKeyPress();
     switch (keypress) {
        case SELECT:
          stir();
          break;
        case LEFT:
          SetPumpOnTime();
          break;
        case UP:        
          break;
        case RIGHT:
        if (lock) { lock=false; }
          else {lock=true;}
        
          break;
     }
    curmillis=millis();
    if (!lock) {
      potval=analogRead(POT);
      delay(250);
    }
    potvalsec = abs(potval/PERCONV-100);
    loopcounter++;
      // Check to see if pot moved or timer increased by 1 sec, if so update display
    if (curmillis > mincounter+1000){ 
        updatedisplay();
      } 
      else if (potvalsec!=potvalsecold){
        potvalsecold=potvalsec;
        delay(100);           // short delay if pot value changed before displaying
        updatedisplay();
      }
    }while(endmillis > curmillis);
  return(-1);
}

//**********************************************************
//* Stir Timer Stir for Stirtime and then delay SETTLE minutes
//**********************************************************
void stir() {
  int countdownmin=STIRTIME;
  int countdownsec=0;
  digitalWrite(PUMP,LOW);
  digitalWrite(STIR,HIGH);
  // Turn on stirrer for STIRTIME
  
  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print("Stirring ");

    do {
      lcd.setCursor(9,1);
      if (countdownmin < 10) lcd.print("0");
      lcd.print(countdownmin); lcd.print(":"); 
      if (countdownsec < 10) lcd.print("0");
      lcd.print(countdownsec);
      countdownsec -= 1;
      if ((countdownsec < 0) && (countdownmin > 0)) {
        countdownmin -= 1;
        countdownsec=59;
      }
      delay(1000);
      // reset watchdog timer while we wait.
      wdt_reset();  
    }while ((countdownmin > 0) || (countdownsec > 0 ));
    //Turn off Stirrer
  digitalWrite(STIR,LOW);
  
  // Lets wait SETTLE  minutes so solution will settle
  countdownmin=SETTLE;
  countdownsec=0;
  digitalWrite(STIR,LOW);

  lcd.setCursor(0,1);
  lcd.print("                ");
  lcd.setCursor(0,1);
  lcd.print("Settling ");

    do {
      lcd.setCursor(9,1);
      if (countdownmin < 10) lcd.print("0");
      lcd.print(countdownmin); lcd.print(":"); 
      if (countdownsec < 10) lcd.print("0");
      lcd.print(countdownsec);
      countdownsec -= 1;
      if ((countdownsec < 0) && (countdownmin > 0)) {
        countdownmin -= 1;
        countdownsec=59;
      }
      delay(1000);
       // reset watchdog timer while we wait.
      wdt_reset(); 
    }while ((countdownmin > 0) || (countdownsec > 0 ));
  
  // Turn Pump back on
  lcd.setCursor(0,1);
  lcd.print("                ");
  digitalWrite(PUMP,HIGH);
} 


/***********************************************************************
*/
void updatedisplay() {
 
  lcd.setCursor(0,0);
  lcd.print("               ");
  lcd.setCursor(0,0);
  lcd.print("Delay Sec = ");
  if (potvalsec<10) lcd.print("0");
  lcd.print(potvalsec);
  lcd.print(" ");
 
  if (animcounter == 1) {
     lcd.print("+");
     animcounter = 0;
  }
  else {
    lcd.print("-");
    animcounter = 1;
  }   
  //***** Display On time in seconds
  lcd.setCursor(8,1);
  lcd.print("               ");
  lcd.setCursor(8,1);
  lcd.print("OnT = ");
  if (onseconds <10 ) lcd.print("0");
  lcd.print(onseconds);
  mincounter=millis();   //reset minute timer

 }


/***********************************************************************
* Check EEprom to see if values have been set first byte should be 123
*/

void checkeprom() {
epromset=EEPROM.read(KEYADDR);
if (epromset == KEY) {
  onseconds = EEPROM.read(ONSECADDR);
  return;
}
EEPROM.write(KEYADDR,KEY);
EEPROM.write(ONSECADDR,SECPERDROP);
}


//**********************************************************
void SetPumpOnTime()
{
  int x = -1;
  lcd.setCursor(0,0);
  lcd.print("Set Pump On Time");
  lcd.setCursor(0,1); // second display line
  lcd.print("                "); //clear line 2
  lcd.setCursor(0,1);

  if (onseconds < 10) lcd.print("0");
  lcd.print(onseconds); 
  do {
    do {
      x=GetKeyPress();
    } while(x==-1);
    
    if (x==UP) {
      onseconds++;
      if (onseconds > 30) onseconds=0;
    }
        if (x==DOWN) {
      onseconds--;
      if (onseconds == 0) onseconds=99;
    }
    lcd.setCursor(0,1);
    if(onseconds < 10) lcd.print(0);
    lcd.print(onseconds);
  }while (x!=LEFT);
  EEPROM.write(ONSECADDR,onseconds);
 }




