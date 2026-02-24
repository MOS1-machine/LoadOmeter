#include <Wire.h> 
#include <SoftwareWire.h>
#include <U8g2lib.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN     6 // Пин для LED ленты
#define NUM_LEDS    4 // Количество светодиодов
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

#define TOUCH_PIN   4 // Сенсорный датчик
const int pwmPinCPU = 9; // Пин для CPU вольтметра
const int pwmPinGPU = 10; // Пин для GPU вольтметра
const int pwmMax = 176;

#define OLED_ADDR 0x3C

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C display1(U8G2_R0, U8X8_PIN_NONE);
SoftwareWire myWire(2, 3);
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C display2(U8G2_R0, 3, 2, U8X8_PIN_NONE);

int mode = 1;
int currentPwmCPU = 0;
int currentPwmGPU = 0;
bool lightsOn = false;
bool isWaitingMode = true; // Режим ожидания активен, если нет сигнала
unsigned long lastSignalTime = 0;
const unsigned long signalTimeout = 4000;
int cpuLoad = 0, gpuLoad = 0, cpuTemp = 0, gpuTemp = 0, ramUsage = 0;
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 100; // Обновление дисплея каждые 100 мс

int brightnessLevels[] = {24, 78, 132, 255}; 
int brightnessIndex = 0;

void setup() {
    Serial.begin(9600);
    display1.begin();
    display2.begin();
    
    strip.begin();
    strip.setBrightness(brightnessLevels[brightnessIndex]);
    strip.show();

    pinMode(pwmPinCPU, OUTPUT);
    pinMode(pwmPinGPU, OUTPUT);
    pinMode(TOUCH_PIN, INPUT);

    // Инициализация дисплеев с текстом ожидания
    displayWaitingText();
}

void loop() {
    if (Serial.available() > 0) {
        String data = Serial.readStringUntil('\n');
        parseData(data);
        lastSignalTime = millis();
        lightsOn = true;
        isWaitingMode = false;  // Выход из режима ожидания при получении данных
        updateVoltmeters();     // Обновление значений на вольтметрах
        updateLedMode();        // Обновление LED
    }

    if (millis() - lastSignalTime > signalTimeout) {
        // Включаем режим ожидания, если нет сигнала
        lightsOn = false;
        isWaitingMode = true;
        strip.fill(strip.Color(50, 50, 50), 0, NUM_LEDS); // Мягкий белый свет в режиме ожидания
        strip.show();

        // Останавливаем обновление вольтметров
        analogWrite(pwmPinCPU, 0);
        analogWrite(pwmPinGPU, 0);

        // Выводим текст ожидания на дисплеях
        displayWaitingText();
    }

    if (!isWaitingMode && millis() - lastUpdateTime >= updateInterval) {
        displayInformation();
        lastUpdateTime = millis();
    }

    if (digitalRead(TOUCH_PIN) == HIGH) {
        handleTouchSensor();
    }
}

void displayWaitingText() {
    // Функция отображения текста ожидания только один раз
    display1.clearBuffer();
    display1.setFont(u8g2_font_ncenB08_tr);
    display1.drawStr(0, 12, "All primary systems");
    display1.drawStr(0, 28, "are online");
    display1.sendBuffer();

    display2.clearBuffer();
    display2.setFont(u8g2_font_ncenB08_tr);
    display2.drawStr(0, 12, "Waiting for data...");
    display2.sendBuffer();
}

void parseData(String data) {
    int firstComma = data.indexOf(',');
    int secondComma = data.indexOf(',', firstComma + 1);
    int thirdComma = data.indexOf(',', secondComma + 1);
    int fourthComma = data.indexOf(',', thirdComma + 1);

    cpuTemp = data.substring(0, firstComma).toInt();
    gpuTemp = data.substring(firstComma + 1, secondComma).toInt();
    ramUsage = data.substring(secondComma + 1, thirdComma).toInt();
    cpuLoad = data.substring(thirdComma + 1, fourthComma).toInt();
    gpuLoad = data.substring(fourthComma + 1).toInt();
}

void updateVoltmeters() {
    // Быстрое, но плавное движение стрелок
    int targetPwmCPU = map(cpuLoad, 0, 100, 0, pwmMax);
    int targetPwmGPU = map(gpuLoad, 0, 100, 0, pwmMax);
    
    // Изменяем текущий PWM по 60 единиц за раз для большей скорости
    if (currentPwmCPU < targetPwmCPU) {
        currentPwmCPU = min(currentPwmCPU + 60, targetPwmCPU);
    } else if (currentPwmCPU > targetPwmCPU) {
        currentPwmCPU = max(currentPwmCPU - 60, targetPwmCPU);
    }
    
    if (currentPwmGPU < targetPwmGPU) {
        currentPwmGPU = min(currentPwmGPU + 60, targetPwmGPU);
    } else if (currentPwmGPU > targetPwmGPU) {
        currentPwmGPU = max(currentPwmGPU - 60, targetPwmGPU);
    }
    
    analogWrite(pwmPinCPU, currentPwmCPU);
    analogWrite(pwmPinGPU, currentPwmGPU);
}

void displayInformation() {
    display1.clearBuffer();
    display1.setFont(u8g2_font_ncenB08_tr);
    display1.drawStr(0, 12, "CPU Temp:");
    display1.setCursor(70, 12);
    display1.print(cpuTemp);
    display1.print("C");

    display1.drawStr(0, 28, "RAM Usage:");
    display1.setCursor(70, 28);
    display1.print(ramUsage);
    display1.print("%");
    display1.sendBuffer();

    display2.clearBuffer();
    display2.setFont(u8g2_font_ncenB08_tr);
    display2.drawStr(0, 12, "GPU Temp:");
    display2.setCursor(70, 12);
    display2.print(gpuTemp);
    display2.print("C");

    int barWidth = map(ramUsage, 0, 100, 0, 100);
    display2.drawFrame(0, 20, 100, 10);
    display2.drawBox(1, 21, barWidth, 8);
    display2.sendBuffer();
}

void handleTouchSensor() {
    delay(50);
    unsigned long touchStartTime = millis();
    while (digitalRead(TOUCH_PIN) == HIGH) {
        if (millis() - touchStartTime > 1000) {
            changeBrightness();
        }
    }
    if (millis() - touchStartTime < 1000) {
        mode = (mode + 1) % 4;
    }
}

void changeBrightness() {
    brightnessIndex = (brightnessIndex + 1) % 4;
    strip.setBrightness(brightnessLevels[brightnessIndex]);
    strip.show();
}

void updateLedMode() {
    switch (mode) {
        case 0: rainbowCycle(); break;
        case 1: updateLoadColors(cpuLoad, gpuLoad); break;
        case 2: strip.fill(strip.Color(255, 115, 0), 0, NUM_LEDS); strip.show(); break;
        case 3: strip.fill(strip.Color(0, 162, 255), 0, NUM_LEDS); strip.show(); break;
    }
}

void updateLoadColors(int cpuLoad, int gpuLoad) {
    uint32_t cpuColor = loadToColor(cpuLoad);
    uint32_t gpuColor = loadToColor(gpuLoad);
    strip.setPixelColor(0, cpuColor); 
    strip.setPixelColor(1, cpuColor);
    strip.setPixelColor(2, gpuColor);
    strip.setPixelColor(3, gpuColor);
    strip.show();
}

uint32_t loadToColor(int load) {
    int red = map(load, 0, 100, 0, 255);
    int green = 255 - red;
    return strip.Color(red, green, 0);
}

void rainbowCycle() {
    static uint16_t j = 0;
    j += 10;  // Ускоряем изменение смещения для плавной анимации
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
}

uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    if (WheelPos < 170) return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
