#include <avr/io.h>
#include <util/delay.h>
//Offset of LED PIN in port (PORTB pin 1 for Arduino Eth)
#define LED_PIN 1

void updateLed();
uint8_t timedOut();
void ResetTick();
