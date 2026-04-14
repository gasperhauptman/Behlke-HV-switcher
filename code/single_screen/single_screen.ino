#include <TimerOne.h>           //  v1.1.1
#include <OneWire.h>            //  v2.3.8
#include <LCD_I2C.h>            //  v2.4.0
#include <DallasTemperature.h>  //  v4.0.6

#define lcd_i2c_address 0x3f    //adress of temperature sensor on wire 
#define OneWireBus 2            // pin which wire bus is connected to
#define Fan1 4                  // pin that fan1 tacho wire is connected to
#define Fan2 5                  // pin that fan2 ...
#define measuring_freq 1000000    //interrupt interval in microseconds

LCD_I2C lcd(lcd_i2c_address);
OneWire oneWire(OneWireBus);
DallasTemperature T_sensor(&oneWire);

float TempC = 0;
volatile unsigned long period4 = 0;
volatile unsigned long period5 = 0;
volatile unsigned long last4 = 0;
volatile unsigned long last5 = 0;
volatile bool lcd_flag = 0;
float rpm1;
float rpm2;

const byte Degrees_symbol[] = {
  0b01110,
  0b01010,
  0b01110,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

byte fanSymbol[] = {
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
  pinMode(LED_BUILTIN, OUTPUT);

  T_sensor.begin();

  lcd.begin();
  lcd.clear();
  lcd.backlight();
  lcd.createChar(0, Degrees_symbol);
  lcd.createChar(1, fanSymbol);
  lcd.setCursor(0, 0);
  lcd.print("Behlke HVS");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("IJS, 2026");
  delay(2000);
  lcd.clear();

  //initializes timer 1 and it's interrupt routine
  Timer1.initialize(measuring_freq);
  Timer1.attachInterrupt(timer_itr);

  PCICR |= 0b00000100;        //enable portd interrupts
  PCMSK2 |= 0b00110000;       //enable pins d4 and d5

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

void timer_itr() {    //interupt triggered by timer 1, sets flag for lcd refresh
  lcd_flag = 1;
}


ISR(PCINT2_vect) {
  static uint8_t lastPort = 0xFF;
  uint8_t port = PIND;

  // Detect falling edge on D4
  if ((lastPort & (1 << PIND4)) && !(port & (1 << PIND4))) {
    unsigned long now = micros();
    period4 = now - last4;
    last4 = now;
  }

  // Detect falling edge on D5
  if ((lastPort & (1 << PIND5)) && !(port & (1 << PIND5))) {
    unsigned long now = micros();
    period5 = now - last5;
    last5 = now;
  }
  lastPort = port;
}

void update_lcd() {

  //disables interrupts to prevent corrupted values
  noInterrupts();
  unsigned long p4 = period4;
  unsigned long p5 = period5;
  interrupts();

  //calculates fan rpm from measured period between pulses
  if (p4 > 0) rpm1 = 30000000 / p4; // = (1000000 / p4) * 60 / number of pulses per rotation, which is 2
  else  rpm1 = 0;

  if (p5 > 0) rpm2 = 30000000 / p5;
  else  rpm2 = 0;

  lcd.setCursor(0, 0);
  lcd.print("T        ");
  lcd.print(TempC, 1);
  lcd.print(char(0));
  lcd.print("C");
  lcd.setCursor(0, 1);
  lcd.print(char(1));
  lcd.print("    ");
  lcd.print(rpm1, 0);
  lcd.print(", ");
  lcd.print(rpm2, 0);
}
