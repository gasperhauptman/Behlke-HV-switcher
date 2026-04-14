#include <TimerOne.h>           //  v1.1.1
#include <OneWire.h>            //  v2.3.8
#include <LCD_I2C.h>            //  v2.4.0
#include <DallasTemperature.h>  //  v4.0.6

#define lcd_i2c_address 0x3f
#define OneWireBus 2
#define Fan1 4
#define Fan2 5
#define measuring_freq 50000    //interrupt interval in microseconds
#define NumOfPulses 2           //number of pulses per rotation given by fans

LCD_I2C lcd(lcd_i2c_address);
OneWire oneWire(OneWireBus);
DallasTemperature T_sensor(&oneWire);

float TempC = 0;
volatile double period4 = 0;
volatile double period5 = 0;
volatile unsigned long last4 = 0;
volatile unsigned long last5 = 0;
volatile int loop_count = 0;
volatile bool lcd_state = 0;
volatile bool lcd_flag = 0;
volatile bool temp_flag = 1;
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

const byte first[] = {
  0b00000,
  0b00000,
  0b00100,
  0b01100,
  0b10100,
  0b00100,
  0b00101,
  0b00000
};

const byte second[] = {
  0b00000,
  0b00000,
  0b01000,
  0b10100,
  0b00100,
  0b01000,
  0b11101,
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
  lcd.createChar(1, first);
  lcd.createChar(2, second);
  lcd.setCursor(0, 0);
  lcd.print("Behlke HVS");
  delay(1000);
  lcd.setCursor(0, 1);
  lcd.print("IJS, 2026");
  delay(2000);
  lcd.clear();

  //initializes timer 1 and it's interrupt
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
  }

  if (temp_flag) {
    T_sensor.requestTemperatures();
    TempC = T_sensor.getTempCByIndex(0);
    temp_flag = 0;
  }

}

void timer_itr() {    //interupt, triggered by timer 1, periodically refreshes lcd display

  loop_count++;

  if (loop_count > 100) {
    lcd_state = !lcd_state; // switch from temp to fan speed screen and vice versa
    loop_count = 0;
  }

  if (!(loop_count % 20)) {
    temp_flag = 1;    //triggers temp measurment
    lcd_flag = 1;
  }


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

  static bool last_state;

  if (last_state != lcd_state) {    //clears display at every switch of shown data
    lcd.clear();
    last_state = lcd_state;
  }

  rpm1 = (1000000 / period4) * 60 / NumOfPulses; //calculates fan rpm from measured period of pulses
  rpm2 = (1000000 / period5) * 60 / NumOfPulses;

  switch (lcd_state) {    //displays data on lcd
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Coolant temp. ");
      lcd.setCursor(0, 1);
      lcd.print(TempC, 1);
      lcd.print(char(0));
      lcd.print("C");
      lcd.print(" ");
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Fan speed [RPM]");
      lcd.setCursor(0, 1);
      lcd.print(char(1));
      lcd.print(" ");
      lcd.print(rpm1, 0);
      lcd.print(" ");
      lcd.setCursor(8, 1);
      lcd.print(char(2));
      lcd.print(" ");
      lcd.print(rpm2, 0);
      lcd.print(" ");
      break;
  }
}
