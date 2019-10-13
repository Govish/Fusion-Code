/*
   ATTiny24 Reset Controller for Fusion Pack
   By Ishaan Govindarajan

   So tbh the whole point of code is because I couln't write a custom bootloader for the STM32
   Long story short, I want to be able to reflash firmware to the STM32 when the user powers
   the pack up while pressing the boot button. However, there would be no way to exit the BOOT
   mode without connecting to a computer.
   This means that if the user accidentally enters BOOT mode without some sort of laptop,
   they're screwed lol.

   This basically means I'll have to either write a custom USB DFU bootloader (which I'm too
   incompetent to do) or have a separate microcontroller handle literally just resetting
   the STM32

   Aaaaaaand we're here.
   So the purpose of the ATtiny24 is to read a GPIO pin (EN_SUP) that is pulled low
   by the STM32 in normal operation and left floating when the main MCU is in DFU mode.
   We'll only enter boot mode after a power-on-reset event, basically when the pack goes
   from deep sleep to low power standby. Therefore we only need to sample the EN_SUP pin
   on startup.

   Operation:
   enable the pullup for the EN_SUP pin immediately on startup
   delay 1 second about - wait for the STM32 to boot and enable drive EN_SUP low if it needs to
   if its low, enter deep sleep for forever (until next POR)
   OTHERWISE
   flash the LED on the pushbutton every half second while also...
   seeing if the pushbutton is pressed
      if we time out, just reset the processor
      if the button is pressed and held for a little, hold the LED high, drive it low
        and reset the processor a second after the led shuts off
*/

#include <avr/sleep.h>  //for entering low power mode
#include <avr/wdt.h>    //for disabling watchdog

// =========== Pin Definitions and constants ===============
// pin definitions taken from
//https://raw.githubusercontent.com/SpenceKonde/ATTinyCore/master/avr/extras/Pinout_x4.jpg
// use the "inside pins" for the definitions

#define LED_PIN 9 //PB1
#define EN_SUP 7  //PA7
#define RST_MCU 8 //PB2
#define BUTTON_PIN 10 //PB0

#define TIMEOUT 600000 //timeout in ms after which to reset the STM32
//timeout isn't super precise since internal oscillator

#define HOLDDOWN_TIME 3000 //how long we need to hold the pushbutton down to reset everything

uint32_t time_var;
boolean led_high;

// ====================== Inline Functions ======================
static inline void LED_ON() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
}
static inline void LED_OFF() {
  pinMode(LED_PIN, INPUT);
}

#define BUTTON digitalRead(BUTTON_PIN)
//====================== END inline functions ===================

// ===================== Other Functions =====================
void powerdown() {
  /*
     from the datasheet:
     (-disable pin change and watchdog interrupts)
     - set the SE bit in the MCUCR register (logic 1)
     - set the appropriate bits in the MCUCR register (SM1..0)
          for power down, set these to `2'b10`
     execute a `sleep` instruction
  */
  ADCSRA &= ~(1 << ADEN); //disabling ADC for extra power saving

  wdt_disable(); //disable watchdog timer

  cli(); //disable interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //set the sleep mode to powerdown
  sleep_enable(); //set the sleep enable bit
  sleep_mode(); //go to sleeeeeeep
}

void reset_stuff() {
  //here, we'll shut off the LED, wait a bit, drive the reset pin low, and reset the ATtiny
  LED_OFF();
  delay(3000);
  digitalWrite(RST_MCU, LOW);

  cli();
  WDTCSR = (1 << WDIE) | WDTO_1S;
  sei();
  wdt_reset();
  while(1);
}
// =================== END Other Functions ===================

void setup() {
  wdt_disable();
  pinMode(EN_SUP, INPUT_PULLUP);
  delay(1500);
  if (!digitalRead(EN_SUP)) {
    //if the STM32 entered normal operational mode
    //disable the input pullup (for power saving)
    pinMode(EN_SUP, INPUT);
    //and enter deep sleep
    powerdown();
  }
  //otherwise continue to the normal loop
}

void loop() {
  if (BUTTON) {
    //if the button is pressed
    LED_ON();
    time_var = millis();
    while (BUTTON) {
      if ((millis() - time_var) > HOLDDOWN_TIME) reset_stuff();
    }
  }

  else {
    //if we're gonna time out, reset the STM32 and the ATtiny
    if (millis() > TIMEOUT) reset_stuff();

    //otherwise just blink the LED
    else if (millis() > time_var) {
      time_var = millis() + 500;
      led_high = !led_high;
      if (led_high) LED_ON();
      else LED_OFF();
    }
  }
  delay(25);
}
