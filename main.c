/* Name: main.c
 * Author: Denis PEROTTO, from work published by Arduino
 * Copyright: Arduino
 * License: GPL http://www.gnu.org/licenses/gpl-2.0.html
 * Project: eboot
 * Function: Bootloader core
 * Version: 0.2 tftp functional on 328P
 */


#include "main.h"
#include "net.h"
#include "tftp.h"
#include <avr/pgmspace.h>
#include "debug.h"
#include <util/delay.h>
#include <avr/eeprom.h>

uint16_t lastTimer1;
uint16_t tick = 0;


#define TIMEOUT 19 //(5 seconds)
#define VERSION "0.2"
int main(void) __attribute__ ((naked)) __attribute__ ((section (".init9")));


// Function
void (*app_start)(void) = 0x0000;

void updateLed() {

  //config of timer1
  uint16_t nextTimer1 = TCNT1;



  //if timer has started a new cycle increment tick count
  if (nextTimer1 < lastTimer1){
	  tick++;
	  if(PORTB & _BV(LED_PIN)){
		  PORTB &= ~_BV(LED_PIN); //turn off LED
	  }
	  else{
		  PORTB |= _BV(LED_PIN); //_BV(LED_PIN)= byte with LED_PIN set, turns up LED
	  }
    }


  //Store timer value for next call of updateled
  lastTimer1 = nextTimer1;

}

uint8_t timedOut() {

  //Called from TFTP poll
  if (pgm_read_word(0x0000) == 0xFFFF){
	  // Never timeout if there is no code in Flash or if flash is corrupted
	  return 0;
  }
  if (tick > TIMEOUT){
	  return 1;
  }
  return 0;
}
void ResetTick(){
	tick=0;
}

int main(void) {

  debugInit();
  _delay_ms(250); //be sure that W5100 has started (ATM starts in 65ms, W5100 150-200ms)
  // Set up outputs to communicate with W5100 chip
  DDRB = _BV(LED_PIN) | _BV(SCK_PIN) | _BV(MOSI_PIN) | _BV(SS_PIN); //set pins as output
  PORTB = _BV(SCK_PIN) | _BV(MISO_PIN) | _BV(MOSI_PIN) | _BV(SS_PIN); //set pins UP

  /*
   Prescaler=0, ClkIO Period = 62,5ns

   TCCR1B values:
   0x01 -> ClkIO/1 -> 62,5ns period, 4ms max
   0x02 -> ClkIO/8 -> 500ns period, 32ms max
   0X03 -> ClkIO/64 -> 4us period, 256ms max
   0x04 -> ClkIO/256 -> 16us period, 1024ms max
   0x05 -> ClkIO/1024 -> 64us period, 4096ms max
   */

  TCCR1B = 0x03;
  SPSR = (1<<SPI2X); //Set SPI Clock to max
  trace("\r\nTFTP Bootloader for Arduino Ethernet, Version ");
  trace(VERSION);
  // Initialise W5100 chip
  trace("\r\nNet init...\r\n");
  netInit();
  trace("Net init done\r\n");


  trace("TFTP Init...\r\n");
  //Open TFTP Socket 3 (Default UDP 69)
  tftpInit();
  trace("TFTP Init done\r\n");

  for(;;) {
    if (tftpPoll()==0) break;
    updateLed();
  }
  trace("Start user app\r\n");
  // Exit to foreground application
  app_start();
  return 0;   /* never reached */
}
