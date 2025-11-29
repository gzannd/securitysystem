#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/wdt.h> //Needed to enable/disable watch dog timer

//This code assumes the ATTiny is running at 1 Mhz. 

int blinkPin = 0;
int watchdog_counter = 0;

#define FAST_BLINK_POWER_OFF 100
#define FAST_BLINK_POWER_ON 100 

#define SLOW_BLINK_POWER_OFF 500 
#define SLOW_BLINK_POWER_ON 50

ISR(WDT_vect) {
  digitalWrite(blinkPin, HIGH);
  delay(SLOW_BLINK_POWER_ON);
  digitalWrite(blinkPin, LOW);
  watchdog_counter++;
}

void blink(int powerOnInterval, int powerOffInterval)
{
  digitalWrite(blinkPin, HIGH);
  delay(powerOnInterval);
  digitalWrite(blinkPin, LOW);
  delay(powerOffInterval);
}


void blinkFast(int numberOfTimes)
{
  for(int i=0;i<numberOfTimes;i++)
  {
    blink(FAST_BLINK_POWER_ON, FAST_BLINK_POWER_OFF);
  }

  digitalWrite(blinkPin, LOW);
}

void setup()
{
  pinMode(blinkPin, OUTPUT);

  //Fast blink for a second to indicate power-on.
  blinkFast(12);

  //digitalWrite(blinkPin, LOW);
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN); //Power down everything, wake up from WDT
  //sleep_enable();
}

void loop()
{
  if(watchdog_counter > 30)
  {
    watchdog_counter = 0;
  }
  
  ADCSRA &= ~(1<<ADEN); //Disable ADC, saves ~230uA
  setup_watchdog(6); //Setup watchdog to go off after 1sec
  sleep_mode(); //Go to sleep
} 

void setup_watchdog(int timerPrescaler) {

  if (timerPrescaler > 9 ) timerPrescaler = 9; //Limit incoming amount to legal settings

  byte bb = timerPrescaler & 7; 
  if (timerPrescaler > 7) bb |= (1<<5); //Set the special 5th bit if necessary

  //This order of commands is important and cannot be combined
  MCUSR &= ~(1<<WDRF); //Clear the watch dog reset
  WDTCR |= (1<<WDCE) | (1<<WDE); //Set WD_change enable, set WD enable
  WDTCR = bb; //Set new watchdog timeout value
  WDTCR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int
}
