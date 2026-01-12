/*
  Программа для управления системой измерения каталитической активности
  с использованием светодиодного излучения и датчика освещенности.
  Включает управление ультрафиолетовым диодом, красным лазером,
  запись данных на SD-карту и управление через последовательный порт.
*/

#include <Wire.h>
#include <BH1750.h>
#include <SPI.h>
#include <SdFat.h>
#include <SoftwareSerial.h>
#include <GParser.h>

// Конфигурация пинов
#define RED_LASER_PIN 7
#define UV_DIODE_PIN 8
#define SD_CS_PIN 10

// Объекты периферии
SoftwareSerial mySerial(3, 2);    // RX, TX для внешнего управления
File dataFile;                    // Файл для записи данных
BH1750 lightMeter;                // Датчик освещенности
SdFat sdCard;                     // Карта памяти

// Глобальные переменные
uint32_t adsorptionTime = 6000;     // Время адсорбции (мс)
uint32_t catalysisTime = 10000;     // Общее время катализа (мс)
uint32_t measurementInterval = 2000; // Интервал измерений (мс)
uint32_t previousTime = 0;          // Время предыдущего измерения
int catalysisCycles = 5;           // Количество циклов катализа
int adsorptionCycles = 3;          // Количество циклов адсорбции
int cycleCounter = 0;              // Счетчик циклов
bool isCatalysisActive = false;    // Флаг активности катализа
bool isLightError = false;         // Флаг ошибки датчика света
bool isSdError = false;            // Флаг ошибки SD-карты
String dataFileName = "";          // Имя файла для данных

/**
 * Чтение уровня освещенности с усреднением.
 * @return Среднее значение освещенности (люкс)
 */
float measureLightLevel() {
  float totalLight = 0;
  for (int i = 0; i < 10; i++) {
    if (lightMeter.measurementReady()) {
      totalLight += lightMeter.readLightLevel();
    }
    delay(100);
  }
  return totalLight / 10.0;
}

/**
 * Запись данных измерения в файл на SD-карте.
 * @param cycleNumber Номер текущего цикла
 * @param lightLevel Измеренный уровень освещенности
 */
void writeDataToFile(int cycleNumber, float lightLevel) {
  dataFile = sdCard.open(dataFileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println(String(cycleNumber) + '\t' + 
                     String(millis() - previousTime) + '\t' + 
                     String(lightLevel));
    dataFile.close();
    Serial.println("Данные записаны.");
  } else {
    Serial.println("Ошибка открытия файла!");
    isSdError = true;
  }
}

/**
 * Отправка данных через последовательный порт.
 * @param lightLevel Измеренный уровень освещенности
 */
void sendDataViaSerial(float lightLevel) {
  mySerial.println(lightLevel);
}

/**
 * Обработка начального измерения (нулевой цикл).
 */
void performInitialMeasurement() {
  digitalWrite(UV_DIODE_PIN, LOW);
  digitalWrite(RED_LASER_PIN, HIGH);
  
  float lightLevel = measureLightLevel();
  
  digitalWrite(RED_LASER_PIN, LOW);
  writeDataToFile(cycleCounter, lightLevel);
  sendDataViaSerial(lightLevel);
  
  cycleCounter++;
}

/**
 * Обработка регулярных измерений.
 */
void performRegularMeasurement() {
  if (millis() - previousTime >= measurementInterval) {
    previousTime = millis();
    
    digitalWrite(UV_DIODE_PIN, LOW);
    digitalWrite(RED_LASER_PIN, HIGH);
    
    float lightLevel = measureLightLevel();
    
    digitalWrite(RED_LASER_PIN, LOW);
    writeDataToFile(cycleCounter, lightLevel);
    sendDataViaSerial(lightLevel);
    
    cycleCounter++;
  }
}

/**
 * Обработка входящих команд через последовательный порт.
 */
void processIncomingCommands() {
  if (mySerial.available()) {
    char buffer[200];
    int bytesReceived = mySerial.readBytesUntil(';', buffer, 200);
    buffer[bytesReceived] = '\0';
    
    Serial.println(buffer);
    
    GParser parser(buffer, ',');
    int parametersCount = parser.split();
    
    switch (parser.getInt(0)) {
      case 369: // Установка параметров
        adsorptionTime = parser.getInt(1);
        catalysisTime = parser.getInt(2);
        measurementInterval = parser.getInt(3);
        dataFileName = parser[4];
        catalysisCycles = catalysisTime / measurementInterval;
        adsorptionCycles = adsorptionTime / measurementInterval;
        
        mySerial.println("Адсорбция: " + String(adsorptionTime) + 
                        "; Катализ: " + String(catalysisTime) + 
                        "; Измерение: " + String(measurementInterval) + 
                        "; Файл: " + String(dataFileName));
        break;
        
      case 1: // Запуск измерений
        isCatalysisActive = true;
        mySerial.println("Измерения начаты");
        break;
        
      case 2: // Остановка измерений
        isCatalysisActive = false;
        mySerial.println("Измерения остановлены");
        break;
        
      case 3: // Включение красного лазера
        digitalWrite(RED_LASER_PIN, HIGH);
        if (lightMeter.measurementReady()) {
          mySerial.println(lightMeter.readLightLevel());
        }
        break;
        
      case 4: // Выключение красного лазера
        digitalWrite(RED_LASER_PIN, LOW);
        break;
    }
  }
}

/**
 * Основной цикл измерений.
 */
void runMeasurementCycle() {
  if (isCatalysisActive) {
    if (cycleCounter == 0) {
      performInitialMeasurement();
    }
    
    if (cycleCounter > adsorptionCycles) {
      digitalWrite(UV_DIODE_PIN, HIGH);
    }
    
    performRegularMeasurement();
    
    if (cycleCounter > (catalysisCycles + adsorptionCycles)) {
      isCatalysisActive = false;
    }
  }
}

/**
 * Инициализация системы.
 */
void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);
  
  while (!Serial) {
    ; // Ожидание подключения последовательного порта
  }
  
  Wire.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
  pinMode(RED_LASER_PIN, OUTPUT);
  pinMode(UV_DIODE_PIN, OUTPUT);
  digitalWrite(UV_DIODE_PIN, LOW);
  digitalWrite(RED_LASER_PIN, LOW);
  
  Serial.print("Инициализация SD карты...");
  if (!sdCard.begin(SD_CS_PIN)) {
    Serial.println("Ошибка инициализации!");
    isSdError = true;
    return;
  }
  Serial.println("Готово.");
}

/**
 * Главный цикл программы.
 */
void loop() {
  processIncomingCommands();
  runMeasurementCycle();
}