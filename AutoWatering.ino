#include "Adafruit_GFX.h"     // OLED библиотека
#include "Adafruit_SSD1306.h"
#include <Wire.h>             // Библиотека I2C
#include <avr/wdt.h>          // Watchdog
#include <EEPROM.h>           // Работаем с EEPROM
#include "Menu.h"             // Константы со строками

#define SCREEN_WIDTH        128 // Ширина OLED-дисплея, в пикселях
#define SCREEN_HEIGHT       32 // Высота OLED-дисплея в пикселях
#define OLED_RESET          -1 // т.к.у дисплея нет пина сброса прописываем -1, используется общий сброс Arduino
#define FIRST_BUTTON_BIT    0
#define SEC_BUTTON_BIT      1
#define RESET_COUNT_ADRESS  0
#define HOURS_ADRESS        1
#define INTERVAL_ADRESS     2
#define AMOUNT_ADRESS       4

const uint8_t menuLen = 2;

MenuItem menu_items[] = {
  { interval: { 0, 0 }, caption: "Interval" },
  { interval: { 0, 0 }, caption: "Amount" },
};

Adafruit_SSD1306 screen = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // Объявляем имя и задаем параметры

// volatile используется чтобы убедиться, что переменные доступны программе в прерываниях
volatile uint8_t hours = 0;
volatile uint8_t minutes = 0;
volatile uint8_t seconds = 0;
volatile uint8_t current_menu = 0;
volatile bool need_update = false; // Принудительное обновление экрана
volatile bool in_menu = false; // Принудительное обновление экрана
volatile bool work_now = false; // Время работы помпы

uint8_t last_second = 255; // Последняя отрисованная секунда
uint8_t reset_counter = 0;

volatile byte buttons_state = 0b00000000; // Для хранения состояния всех кнопок используем один байт
volatile byte last_buttons_state = buttons_state; // Для принудительного обновления при нажатых кнопках


void setup() {
    pinMode(2, INPUT_PULLUP); // D2 в режим входа с подтяжкой
    pinMode(3, INPUT_PULLUP); // D3 в режим входа с подтяжкой
    pinMode(4, INPUT_PULLUP); // D4 в режим входа с подтяжкой
    pinMode(5, INPUT_PULLUP); // D5 в режим входа с подтяжкой
    pinMode(12, OUTPUT); // Пин реле
    digitalWrite(12, LOW);

    cli(); // Останавливаем прерывания
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0; // Инициализируем значение счетчика timer1 равным 0
    // Устанавливаем регистр сравнения с шагом 1 Гц
    OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 ( должно быть <65536)
    TCCR1B |= (1 << WGM12); // Включить режим CTC
    TCCR1B |= (1 << CS12) | (1 << CS10); // Установить биты CS12 и CS10 для 1024-го делителя счётчика таймера
    TIMSK1 |= (1 << OCIE1A); // Включить прерывание по таймеру
    sei(); // Разрешаем прерывания

    attachInterrupt(digitalPinToInterrupt(2), buttonsIntettuptHandler, CHANGE); // Включаем обработку прерываний по нажатию кнопки
    attachInterrupt(digitalPinToInterrupt(3), buttonsIntettuptHandler, CHANGE); // Включаем обработку прерываний по нажатию кнопки
    // Варианты обработк: CHANGE/RISING/FALLING

    screen.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Запуск дисплея
    screen.setTextColor(WHITE); // Задаем цвет текста (текущая версия - всегда белый, нет смысла выносить это в луп)
    screen.display();
    delay(2000); // Пауза для инизиализации дисплея

    hours = EEPROM.read(HOURS_ADRESS);
    if (hours == 255) { // 255 в этой ячейке памяти не должно быть при нормальной работе, следовательно перезагрузок ещё не было
      hours = 0;
    } else { // Раз перезагрузки были, нужно восстановить значения
      EEPROM.get(INTERVAL_ADRESS, menu_items[0].interval);
      EEPROM.get(AMOUNT_ADRESS, menu_items[1].interval);
    }

    reset_counter = EEPROM.read(RESET_COUNT_ADRESS) + 1; // Считаем перезапуски
    EEPROM.put(RESET_COUNT_ADRESS, reset_counter);

    wdt_enable(WDTO_4S); // Запускаем вачдог
}

void loop() {
    digitalWrite(12, (uint8_t)work_now); // Помпа

    if (last_second == seconds && !need_update) { return; } // Не дёргаем дисплей слишком часто
    if (in_menu) {
      drawSubmenu(current_menu);
      return;
    }

    screen.clearDisplay();

    if (bitRead(buttons_state, FIRST_BUTTON_BIT) && (current_menu >= 1)) { current_menu--; } // Жмём вниз
    if (bitRead(buttons_state, SEC_BUTTON_BIT) && (current_menu < menuLen-1)) { current_menu++; } // Жмём вверх

    if (checkButtonState(5)) { // Зажали крайнюю правую
      in_menu = true;
    }

    screen.setCursor(0, 0);
    screen.println("Resets: " + (String)reset_counter);
    drawTime();

    for (int i = 0; i < menuLen; i++) {
        if (current_menu == i) {
          screen.setCursor(0, i*10 + 15);
          screen.write(0x10); // Рисуем треугольник-указатель меню
        }
        screen.setCursor(10, (i*10) + 15);
        screen.println(menu_items[i].caption);
    }

    screen.display(); // Команда для отображения всего этого на дисплее

    last_second = seconds;
    delay(20);
}

void drawSubmenu(uint8_t menu_index) {
  if (menu_index >= menuLen) return;
  screen.clearDisplay();
  screen.setCursor(0, 0);
  screen.setTextSize(2); // Задаем размер текста
  screen.println(menu_items[menu_index].caption);
  screen.setCursor(0, 18);
  screen.println((String)menu_items[menu_index].interval.hours + ":" + (String)menu_items[menu_index].interval.minutes);
  screen.display();

  if (bitRead(buttons_state, FIRST_BUTTON_BIT)) {
    increaseInterval(menu_items[menu_index].interval);
  }
  if (bitRead(buttons_state, SEC_BUTTON_BIT)) {
    decreaseInterval(menu_items[menu_index].interval);
  }

  if (checkButtonState(4)) {
    EEPROM.put(INTERVAL_ADRESS, menu_items[0].interval);
    EEPROM.put(AMOUNT_ADRESS, menu_items[1].interval);
    screen.clearDisplay();
    in_menu = false;
  }

  delay(50);
}

bool checkButtonState(uint8_t pin) { // Двукратное чтение чтобы избежать дребезга
    bool second_read = false;
    bool first_read = false;
    first_read = digitalRead(pin);
    delayMicroseconds(2); // Не использует таймеры, поэтому можно вызывать в прерываниях
    second_read = digitalRead(pin);
    return first_read == second_read ? !first_read : false; // Инверсия потому что кнопка подтягивает к земле
}

void clearTime() {
    hours = 0;
    minutes = 0;
    seconds = 0;
}

void drawTime() {
    screen.setTextSize(1); // Задаем размер текста
    screen.setCursor(80, 0); // Задаем координату начала текста в пикселях
    String hoursStr = (String)hours;
    if (hours < 10) hoursStr = "0" + hoursStr;
    String minutesStr = (String)minutes;
    if (minutes < 10) minutesStr = "0" + minutesStr;
    String secondsStr = (String)seconds;
    if (seconds < 10) secondsStr = "0" + secondsStr;
    screen.println(hoursStr + ":" + minutesStr + ":" + secondsStr);
}

/////////////////////////////////////////////////////////////////////////////////
// Далее обработчики прерываний
/////////////////////////////////////////////////////////////////////////////////

void buttonsIntettuptHandler() {
    bitWrite(buttons_state, FIRST_BUTTON_BIT, checkButtonState(2));
    bitWrite(buttons_state, SEC_BUTTON_BIT, checkButtonState(3));
    if (last_buttons_state == buttons_state) need_update = true;
}

ISR (TIMER1_COMPA_vect) { // Обработка прерывания таймера
    wdt_reset();
    
    seconds += 1; // увеличиваем секунды
    if (seconds >= 60) {
        minutes += 1;
        seconds = 0;
    }
    if (minutes >= 60) {
        hours += 1;
        minutes = 0;
        EEPROM.put(HOURS_ADRESS, hours);
    }

    // Мы считаем время постоянно, и при достижении нужного времени включаем или отключаем попму
    // Таким образом, счётчик всегда отображает текущий интервал и обнуляется каждый раз


    if (!work_now) {
      if (hours > menu_items[0].interval.hours || (hours == menu_items[0].interval.hours && minutes >= menu_items[0].interval.minutes)) {
      // Из-за этой логики помпа работает при первом запуске, т.к. ни интервал, ни часы с минутами ещё не установлены
        clearTime();
        work_now = true;
        return;
      }
    } else if (minutes >= menu_items[1].interval.hours && seconds >= menu_items[1].interval.minutes) {
      clearTime();
      work_now = false;
      return;
    }
}
