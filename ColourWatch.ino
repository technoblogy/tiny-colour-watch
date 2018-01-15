/* Tiny Colour Watch

   David Johnson-Davies - www.technoblogy.com - 15th January 2018
   ATtiny85 @ 8 MHz (external crystal; BOD disabled)
   
   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license: 
   http://creativecommons.org/licenses/by/4.0/
*/

#include <avr/sleep.h>

// Pin assignments
const int Clk = 0;
const int Data = 1;
const int Enable = 2;

// Constants
const int Bright = 0;
const int Blue = 1;
const int Green = 2;
const int Red = 3;
const int Brightness = 15;                // Brightness 1 to 31

// Colours of each 'hand'
const int Second = Green;
const int Minute = Red;
const int Hour = Blue;

const int Timeout = 15;                  // Blank display after 15 secs
int Tickspersec = 125;                   // Ticks per second
volatile uint8_t Ticks = 0;
volatile unsigned long Seconds = 0;

volatile int ShowDisplay;

// Display buffer - sets the brightness and colour of the 12 RGB LEDs
uint8_t Display[12][4];

// Software SPI **********************************************

void Write (uint8_t c) {
  int i=0x80;
  while (i) {   
    if (c & i) PORTB = PORTB | 1<<Data;  // set Data high
    else PORTB = PORTB & ~(1<<Data);     // set Data low
    PINB = 1<<Clk;                       // set Clk high
    i = i>>1;
    PINB = 1<<Clk;                       // set Clk low
  }     
}

void UpdateDisplay () {
  for (int d=0; d<4; d++) Write(0);      // Start frame
  for (int d=0; d<12; d++) {
    Write(Display[d][0] | 0xE0);         // Brightness 0-31
    Write(Display[d][1]);                // Blue
    Write(Display[d][2]);                // Green
    Write(Display[d][3]);                // Red
  }
  Write(0xFF);                           // End frame
}

void ClearBuffer () {
  for (int d=0; d<12; d++) {
    for (int c=0; c<4; c++) Display[d][c] = 0;
  }
}

// Log brightness level 0=max 4=min
inline int Level (int level) {
  return 31>>level;
}

void DisplayOff () {
  DDRB = 1<<Clk | 1<<Data | 0<<Enable;        // Make Enable an input
  PORTB = 0<<Clk | 0<<Data | 1<<Enable;       // Turn on Enable pullup
  ShowDisplay = false;
}

void DisplayOn () {
  PORTB = 0<<Clk | 0<<Data | 0<<Enable;       // Turn off Enable pullup
  DDRB = 1<<Clk | 1<<Data | 1<<Enable;        // Make Enable an output low
}

// Tick counter **********************************************

// Timer/Counter0 interrupt counts seconds
ISR(TIM0_COMPA_vect) {
  Ticks++;
  if (Ticks == Tickspersec) {Ticks = 0; Seconds++; }
}

// Read seconds with interrupts disabled to avoid false reading
unsigned long Secs() {
  cli(); unsigned long s = Seconds; sei();
  return s;
}

// Display a watch hand on two adjacent LEDs
void Hand (int Unit, int Colour) {
  int to = Unit/5;
  int from = (to+11)%12;
  int count = Unit%5;
  Display[from][Bright] = Brightness;
  Display[from][Colour] = Level(count);
  Display[to][Bright] = Brightness;
  Display[to][Colour] = Level(5-count);
}

// Setup **********************************************

void SetTime () {
  OCR0A = 49;                     // x5 speed
  unsigned long Now;
  do {
    DisplayOn();
    Now = Secs();
    int mins = Now%60;
    int hours = (unsigned long)(Now/12)%60;
    ClearBuffer();
    Hand(hours, Hour);
    Hand(mins, Minute);
    UpdateDisplay();
    while (Secs() == Now);         // Wait for end of second
    DisplayOff();
    for (volatile int i=0; i<100; i++); // Let MOSFET settle
  } while ((PINB & 1<<Enable) != 0);
  Now = Secs(); Now = (Now*60 + Now/5);
  OCR0A = 249;                     // Normal speed
  cli(); Seconds = Now; sei();     // Avoid effect of interrupt
}

// Show the time for 30 seconds
void ShowTime () {
  CLKPR = 1<<CLKPCE;               // Speed up clock to 8 MHz
  CLKPR = 0<<CLKPS0;               // Divide clock by 1 to 8 MHz
  TCCR0B = 0<<WGM02 | 4<<CS00;     // /256 = 31250
  DisplayOn();
  for (int n=0; n<Timeout; n++) {  // Display time for Timeout seconds
    unsigned long Now = Secs();
    int secs = Now%60;
    int mins = (unsigned long)(Now/60)%60;
    int hours = (unsigned long)(Now/720)%60;
    ClearBuffer();
    Hand(hours, Hour);
    Hand(mins, Minute);
    Hand(secs, Second);
    UpdateDisplay();
    while (Secs() == Now);
  }
  DisplayOff();
  // Slow down clock by factor of 256 to save power
  CLKPR = 1<<CLKPCE;               // Slow down processor clock to save power
  CLKPR = 8<<CLKPS0;               // Divide clock by 256 to 31250 Hz
  TCCR0B = 0<<WGM02 | 1<<CS00;     // /1 = 31250
}

void setup () {
// Set up Timer/Counter0 to measure the time
  TCCR0A = 2<<WGM00;               // CTC mode; count up to OCR0A
  TCCR0B = 0<<WGM02 | 4<<CS00;     // /256 = 31250
  OCR0A = 249;                     // /250 -> 125Hz
  TIMSK = 1<<OCIE0A;               // Enable compare match interrupt
  // Disable what we don't need to save power
  ADCSRA &= ~(1<<ADEN);            // Disable ADC
  PRR = 1<<PRUSI | 1<<PRADC | 1<<PRTIM1;  // Turn off clocks to unused peripherals
  set_sleep_mode(SLEEP_MODE_IDLE);
  SetTime();
  ShowTime();
}

// Just wake up for interrupts
void loop () {
  if ((PINB & 1<<Enable) == 0) ShowTime();
  sleep_enable();
  sleep_cpu();
}
