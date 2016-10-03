#include <Wire.h>
#include "DS1307.h"

//Clear Bit
#ifndef cbi
  #define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
//Set Bit
#ifndef sbi
  #define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

//#define DEBUG 1;

/**
 * Номер пина для импульсного питания
 * PWM arduino pin. Needed for generating 170v
 */
const byte PWM_PIN = 3;
/**
 * Пин к которому подключена кнопка режимов
 * pins for clock buttons
 */
const byte BTN_MODE_PIN = 5;  //MODE button
const byte BTN_UP_PIN = 4;    //UP button
const byte BTN_DOWN_PIN = 6;  //DONW button


/**
 * минимальный Duty Cycle
 * minimal duty cycle for PWM
 */
const byte PWM_MIN_VAL = 10;
/**
 * максимальный Duty Cycle
 * maximal duty cycle for PWM
 */
const byte PWM_MAX_VAL = 180;
/**
 * Текущее значение Duty Cycle 0 - 255
 * Store current duty cycle value
 */
volatile byte pwm_val = PWM_MIN_VAL;

/**
 * Время между нажатиями кнопок
 * Time to wait between button presses
 */
const byte BTN_PAUSE_TIME = 500;
/**
 * время когда была нажата кнопка, нужно для задержки между нажатиями
 * contains millis() when last time button was pressed
 */
unsigned long buttonReadTime = 0;

//I2C address MCP23017 
const byte MCP_ADDR = 0x20;
//Port А and B registers at MCP23017
const byte MCP_GPIOA = 0x12;
const byte MCP_GPIOB = 0x13;


/**
 * количество знаков (ламп)
 * amount of digits (lamps) at device
 */
const byte DIGITS_COUNT = 6;
/**
 * числа для отображения
 * digit values for all lamps
 */
byte timeDisp[DIGITS_COUNT];
/**
 * какие цифры должны мигать + 8-й бит: включены лампы или нет
 * set bits to 1 for digits that might blink, 8th bit to store current blink state
 * if blinkMask = 0 - don't blink
 */
byte blinkMask = 0;
/**
 * номер знака (лампы) для отображения
 * at each moment of time we switch on only 1 lamp. this stores it index
 */
byte digit = 0;

/**
 * wait this time before show anything to stabilize 170v power line
 */
const int POWER_SETUP_TIME = 1000;
/**
 * время без нажатий кнопок для сброса режима в дефолтный
 * if mode was changed and you don't touch buttons switch to default mode (show time) after amount of millis
 */
const int MODE_RESET_TIME = 5000;

/**
 * Different task timers
 */
const byte TIMERS_COUNT = 4;
const unsigned int TM_POW = 50; //ХХ миллисекунд подгоняем высокое напряжение
const unsigned int TM_BTN = 60; //Проверка кнопок каждые ХХ миллисекунд
const unsigned int TM_1s = 1000;  //1 секунда
const unsigned int TM_BLINK = 500;  //частота мигания ламп в режиме изменения
//сохраняем последнее время выполнения действия
unsigned long timers[TIMERS_COUNT];
//вызывать события для таймеров через .. миллисекунд
const unsigned int TIMER_COUNTS[TIMERS_COUNT] = {TM_POW, TM_BTN, TM_1s, TM_BLINK};
//часы реального времени
DS1307 clock;

//режим отображения
const byte MODES_COUNT = 9; //количество режимов

const byte MODE_TIME = 1; //время
const byte MODE_DATE = 2; //дата
const byte MODE_TEMP = 3; //температура
const byte MODE_CHANGE_S = 10; //изменяем секунды
const byte MODE_CHANGE_M = 11; //изменяем минуты
const byte MODE_CHANGE_H = 12; //изменяем часы
const byte MODE_CHANGE_y = 13; //изменяем год
const byte MODE_CHANGE_m = 14; //изменяем месяц
const byte MODE_CHANGE_d = 15; //изменяем день

const byte MODES[MODES_COUNT] = {MODE_TIME, MODE_DATE, MODE_TEMP, MODE_CHANGE_S, MODE_CHANGE_M, MODE_CHANGE_H, MODE_CHANGE_y, MODE_CHANGE_m, MODE_CHANGE_d};
volatile byte mode = 0; //текущий режим

void setup() {
#ifdef DEBUG
  Serial.begin(9600);
#endif

  initPWM();
  initButtons();
  initClock();
  initMCP();
  
  //theTime();
}

void loop() {
  //первые секунды стабилизируем высокое напряжение
  if(millis() < POWER_SETUP_TIME){
    adjustPower();
  }else{
    updateDisplay();
  }
  //сбрасываем режим в дефолтный
  if(MODES[mode] != MODE_TIME && millis() - buttonReadTime > MODE_RESET_TIME){
    mode = 0;
    blinkMask = 0;
  }
  int t;
  for(t = 0; t < TIMERS_COUNT; t++){
    if(abs(millis() - timers[t]) >= TIMER_COUNTS[t]){
      switch(TIMER_COUNTS[t]){
        case TM_POW:
          if(millis() > POWER_SETUP_TIME){
            adjustPower();
          }
        break;
        case TM_BTN:
          //считываем кнопки
          readButtons();
        break;
        case TM_BLINK:
            //меняем восьмой бит мигания (смена включения\выключения знаков)
            if(blinkMask > 0){
              blinkMask ^= (1 << 7);
            }
        break;
        //выполняем каждую секунду
        case TM_1s:
          //обновляем время только когда включен режим показа времени
          if(MODES[mode] == MODE_TIME){
            updateDigits();
          }
        break;
      }
      timers[t] = millis();
    }
  }
}

void updateDigits(){
  if(MODES[mode] == MODE_TIME || MODES[mode] == MODE_CHANGE_S){
    showTime();
  }else if(MODES[mode] == MODE_DATE || MODES[mode] == MODE_CHANGE_y){
    showDate();
  }else if(MODES[mode] == MODE_TEMP){
    showTemp();
  }
}

void initMCP(){
  Wire.begin();
  Wire.beginTransmission(MCP_ADDR);
  Wire.write((byte)0x00); // IODIRA register
  Wire.write((byte)0x00); // set all of bank A to outputs
  Wire.write((byte)0x00); // set all of bank B to outputs
  Wire.endTransmission();
}

void initButtons(){
  pinMode(BTN_MODE_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
}

void initPWM(){
  pinMode(PWM_PIN, OUTPUT);

  //включаем таймер
  sbi(TCCR2A,COM2A1);
  cbi(TCCR2A,COM2A0);
  sbi(TCCR2A,COM2B1);
  cbi(TCCR2A,COM2B0);

  //Счетчик до 255 получаем 31.25 kHz при 8MHz кварце
  cbi(TCCR2B,WGM22);
  sbi(TCCR2A,WGM21);
  sbi(TCCR2A,WGM20);

  cbi(TCCR2B,FOC2A);
  cbi(TCCR2B,FOC2B);

  //Частота без делителя 8 MHz для 3v3
  cbi(TCCR2B,CS22);
  cbi(TCCR2B,CS21);
  sbi(TCCR2B,CS20);

  OCR2B = pwm_val;
}

void initClock(){
  clock.begin();
#ifdef DEBUG
  debugTime();
#endif
}


#ifdef DEBUG
void debugTime(){
  clock.getTime();
  Serial.print(clock.hour, DEC);
  Serial.print(":");
  Serial.print(clock.minute, DEC);
  Serial.print(":");
  Serial.print(clock.second, DEC);

  Serial.print("  ");
  
  Serial.print(clock.dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(clock.month, DEC);
  Serial.print("/");
  Serial.println(clock.year + 2000, DEC);
}
#endif

//Вносим в буфер текущее время
void showTime(){
  clock.getTime();
  timeDisp[5] = clock.hour / 10;
  timeDisp[4] = clock.hour % 10;
  
  timeDisp[3] = clock.minute / 10;
  timeDisp[2] = clock.minute % 10;
  
  timeDisp[1] = clock.second / 10;
  timeDisp[0] = clock.second % 10;
}

//Вносим в буфер текущую дату
void showDate(){
  clock.getTime();
  timeDisp[5] = clock.dayOfMonth / 10;
  timeDisp[4] = clock.dayOfMonth % 10;
  
  timeDisp[3] = clock.month / 10;
  timeDisp[2] = clock.month % 10;
  
  timeDisp[1] = clock.year / 10;
  timeDisp[0] = clock.year % 10;
}

//Вносим в буфер текущую температуру
void showTemp(){
  float fTemp = clock.readTemperature();
  byte temp = (byte)fTemp;
  if(fTemp >= 0.5){
    temp++;
  }
#ifdef DEBUG
  Serial.print(fTemp);
  Serial.print(": ");
  Serial.println(temp);
#endif
  timeDisp[5] = 0;
  timeDisp[4] = 0;
  
  timeDisp[3] = temp / 10;
  timeDisp[2] = temp % 10;
  
  timeDisp[1] = 0;
  timeDisp[0] = 0;
}

//Select tube and display right number
void updateDisplay(){  
  //go to next digit
  digit++;
  if(digit > DIGITS_COUNT){
    digit = 0;
  }
  
  //Если цифра моргает и выключена
  if(((blinkMask >> digit) & 1) == 1 && ((blinkMask >> 7) & 1) == 0){
    return;
  }

  //исправляем косяк на плате, неправильно разведены цыфры
  byte num = (11 - timeDisp[digit])%10;
  
  byte pa = 0;
  byte pb = 1 << (digit + 2); //выбираем лампу
  //0..7 на порту А
  if(num <= 7){
    pa = 1 << num;
  }else{//8..9 на порту B
    pb |= 1 << (num - 8);
  }

  //отключаем все, разряжаем транзисторы
  //иначе видно сразу несколько цифр на лампе!
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(MCP_GPIOA);
  Wire.write(0);
  Wire.write(0);
  Wire.endTransmission();

  //отправляем на MCP
  Wire.beginTransmission(MCP_ADDR);
  Wire.write(MCP_GPIOA);
  Wire.write(pa);
  Wire.write(pb);
  Wire.endTransmission();
}

void adjustPower(){
  int val = analogRead(A0);
  float voltage = (val  * 3.30) / 1023.0;
  
  if(voltage < 1.70){
    if(pwm_val < PWM_MAX_VAL){
      pwm_val++;
    }else{
      pwm_val = PWM_MAX_VAL;
    }
  }else if(voltage > 1.75){
    if(pwm_val > PWM_MIN_VAL){
      pwm_val--;
    }else{
      pwm_val = PWM_MIN_VAL;
    }
  }

  //если нет напряжения на A0 значит что-то не то, не повышаем PWM
  if(voltage < 0.1 && pwm_val > PWM_MIN_VAL){
    pwm_val = PWM_MIN_VAL;
  }

  OCR2B = pwm_val;

/*
  Serial.print("ADC value: ");Serial.println(val);
  Serial.print("Voltage: ");Serial.println(voltage);
  Serial.print("PWM value: ");Serial.println(pwm_val);
  delay(100);
*/
}

void readButtons(){
  if(millis() - buttonReadTime < BTN_PAUSE_TIME){
    return;
  }
  if(digitalRead(BTN_MODE_PIN) == LOW){
    onModeButton();
    buttonReadTime = millis();
  }else if(digitalRead(BTN_UP_PIN) == LOW){
    onUpDownButton(true);
    buttonReadTime = millis();
  }else if(digitalRead(BTN_DOWN_PIN) == LOW){
    onUpDownButton(false);
    buttonReadTime = millis();
  }
}

void onModeButton(){
  //установили время
  if(MODES[mode] == MODE_CHANGE_H){
    setTime();
  //установили дату, сохраняем
  }else if(MODES[mode] == MODE_CHANGE_d){
    setDate();
#ifdef DEBUG
    debugTime();
#endif
  }
  mode++;
  if(mode == MODES_COUNT){
    mode = 0;
  }

#ifdef DEBUG
  Serial.print("MODE: ");
  Serial.println(mode);
#endif

  if(MODES[mode] == MODE_CHANGE_S || MODES[mode] == MODE_CHANGE_y){
    blinkMask = 0b10000011;
  }else if(MODES[mode] == MODE_CHANGE_M || MODES[mode] == MODE_CHANGE_m){
    blinkMask = 0b10001100;
  }else if(MODES[mode] == MODE_CHANGE_H || MODES[mode] == MODE_CHANGE_d){
    blinkMask = 0b10110000;
  }else{
    blinkMask = 0;
  }

  updateDigits();
}

void onUpDownButton(bool up){
  if(blinkMask == 0) return;
  byte d = 0;   //digit to change
  byte mn = 0;   //minimum
  byte mx = 59;  //maximum
  if(MODES[mode] == MODE_CHANGE_M){
    d = 2;
  }else if(MODES[mode] == MODE_CHANGE_H){
    d = 4;
    mx = 23;
  }else if(MODES[mode] == MODE_CHANGE_d){
    d = 4;
    mn = 1;
    mx = 31;
  }else if(MODES[mode] == MODE_CHANGE_m){
    d = 2;
    mn = 1;
    mx = 12;
  }else if(MODES[mode] == MODE_CHANGE_y){
    mx = 99;
  }

  //если переходим вверх или вниз, то скидываем значение
  byte num = timeDisp[d + 1] * 10 + timeDisp[d];
  if(up && num >= mx){
    timeDisp[d] = mn % 10;
    timeDisp[d + 1] = mn / 10;
  }else if(!up && num <= mn){
    timeDisp[d] = mx % 10;
    timeDisp[d + 1] = mx / 10;
  }else{  //иначе просто увеличиваем
    timeDisp[d] += (up?1:-1);

    if(timeDisp[d] == 255){
      timeDisp[d] = 9;
      timeDisp[d + 1]--;
    }else if(timeDisp[d] > 9){
      timeDisp[d] = 0;
      timeDisp[d + 1]++;
    }
  }
}

void setTime(){
  clock.fillByHMS(
    timeDisp[5] * 10 + timeDisp[4],
    timeDisp[3] * 10 + timeDisp[2],
    timeDisp[1] * 10 + timeDisp[0]
  );
  clock.setTime();
}

void setDate(){
  clock.fillByYMD(
    timeDisp[1] * 10 + timeDisp[0],
    timeDisp[3] * 10 + timeDisp[2],
    timeDisp[5] * 10 + timeDisp[4]
  );
  clock.setTime();
}


//Setup time from PC to DS1307 chip
char compileTime[] = __TIME__;
void theTime(){
  byte hour = getInt(compileTime, 0);
  byte minute = getInt(compileTime, 3);
  byte second = getInt(compileTime, 6) + 15;  //compensate time between compile and upload to arduino
  clock.fillByYMD(16,8,22);
  clock.fillByHMS(hour, minute, second);
  clock.setTime();
}
char getInt(const char* string, int startIndex) {
  return int(string[startIndex] - '0') * 10 + int(string[startIndex+1]) - '0';
}
