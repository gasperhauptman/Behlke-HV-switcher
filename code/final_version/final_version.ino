/*  
 *   Behlke HV Switcher
 *   IJS, march 2026
 *   
 *   ========================================================================
 *   
 *   LCD display of coolant temperature and fan speed 
 *   
 *   Libraries used:
 *    TimerOne by Paul Stoffregen
 *    OneWire by Paul Stoffregen
 *    LCD_I2C by Blackhack
 *    DallasTemperature by Miles Burton
 *    
 *   The LCD address is 0x3F.
 *   PCF8574 chip is from NXP and and A0..A2 are open (= high)
 *   16 chars 2 line display
 *   
 *   Use Arduino Nano hardware I2C pins: SDA=A4, SCL=A5, NOT the defulat IJS PCB V1.0 connector!!!
 *   
 *   Temperature DS18B20 sensor is connected to D2 with 4.7k pullup resistor - originally meant for LCD on PCB V1.0.
*/


#include <TimerOne.h>           //  v1.1.1
#include <OneWire.h>            //  v2.3.8
#include <LCD_I2C.h>            //  v2.4.0
#include <DallasTemperature.h>  //  v4.0.6

#define lcd_i2c_address 0x3f     
#define OneWireBus 2            
#define Fan1 4                  // pin that fan1 tacho wire is connected to
#define Fan2 5                  // pin that fan2 ...
#define updateFreq 500000    //lcd display refresh interval in microseconds, shouldn't be shorter than temp sensor convertion time

//initializes onewire and i2c communication
LCD_I2C lcd(lcd_i2c_address);
OneWire oneWire(OneWireBus);
DallasTemperature T_sensor(&oneWire);

float TempC = 0;
volatile unsigned long period1 = 0;
volatile unsigned long period2 = 0;
volatile unsigned long last1 = 0;
volatile unsigned long last2 = 0;
volatile int loopCount1 = 0;
volatile int loopCount2 = 0;
volatile bool lcd_flag = 0;
int rpm1;
int rpm2;

//custom LCD characters
const byte degreesSymbol[] = {
  0b01110,
  0b01010,
  0b01110,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

const byte fanSymbol[] = {
  0b00000,
  0b00000,
  0b11001,
  0b01011,
  0b00100,
  0b11010,
  0b10011,
  0b00000
};

void setup() {

  Serial.begin(115200);

  pinMode(Fan1, INPUT);
  pinMode(Fan2, INPUT);

  T_sensor.begin();
  T_sensor.setResolution(0, 11); //temp sensor resolution, min 9, max 12 bits, which is also default

  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, degreesSymbol);
  lcd.createChar(1, fanSymbol);
  lcd.setCursor(0, 0);
  lcd.print("Behlke HVS");
  lcd.setCursor(0, 1);
  lcd.print("IJS, 2026");
  delay(2000);
  lcd.clear();

  //initializes timer 1 and it's interrupt routine
  Timer1.initialize(updateFreq);
  Timer1.attachInterrupt(timerIntr);

  PCICR |= 0b00000100;        //enable portd interrupts
  PCMSK2 |= 0b00110000;       //enable pins d4 and d5

  last1 = micros();
  last2 = micros();
  interrupts();
}

void loop() {
  if (lcd_flag) {
    update_lcd();
    lcd_flag = 0;
    T_sensor.requestTemperatures();
    TempC = T_sensor.getTempCByIndex(0);
  }
}

void timerIntr() {    //interupt triggered by timer 1, sets flag for lcd refresh
  lcd_flag = 1;
}


ISR(PCINT2_vect) {
  static uint8_t lastPort = 0xFF;
  uint8_t port = PIND;

  // Detects falling edge on D4
  if ((lastPort & (1 << PIND4)) && !(port & (1 << PIND4))) {
    unsigned long now = micros();
    period1 += (now - last1);
    loopCount1++;
    last1 = now;
  }

  // Detects falling edge on D5
  if ((lastPort & (1 << PIND5)) && !(port & (1 << PIND5))) {
    unsigned long now = micros();
    period2 += (now - last2);
    loopCount2++;
    last2 = now;
  }
  lastPort = port;
}

void update_lcd() {

  //disables interrupts to prevent corrupted values
  noInterrupts();
  unsigned long p1 = period1;
  unsigned long p2 = period2;
  int lc1 = loopCount1;
  int lc2 = loopCount2;

  period1 = 0;
  period2 = 0;
  loopCount1 = 0;
  loopCount2 = 0;
  interrupts();  

  if (lc1 > 0) p1 /= lc1;
  else p1 = 0;
  if (lc2 > 0) p2 /= lc2;
  else p2 = 0;

  //calculates fan rpm from measured period between pulses
  if (p1 > 0) rpm1 = 30000000 / p1; // = (1000000 / p1) * 60 / number of pulses per rotation, which is 2
  else  rpm1 = 0;

  if (p2 > 0) rpm2 = 30000000 / p2;
  else  rpm2 = 0;

  lcd.setCursor(0, 0);
  lcd.print("T        ");
  lcd.print(TempC, 1);
  lcd.print(char(0));
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print(char(1));
  lcd.print("    ");
  lcd.print(rpm1);
  lcd.print(", ");
  lcd.print(rpm2);
}

// end
