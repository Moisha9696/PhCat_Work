#include <Wire.h>
#include <BH1750.h>
#include <SPI.h>
#include <SdFat.h> //Why not SD.h ?
//#include <SD.h>
#include <SoftwareSerial.h>
#include <GParser.h>

#define RED_LASER 7
#define UV_DIODE 8
#define SD_CS_PIN 10

//----OBJECTS----//
SoftwareSerial mySerial(3, 2); // RX, TX
File myFile;
BH1750 lightMeter;
SdFat SD;

//----VARIABLES----//
uint32_t timeAd = 6000;
uint32_t timeCat = 10000;
uint32_t timeMsr = 2000;
uint32_t timePrc = 0;
int amountCat = 5;
int amountAd = 3;
int counter = 0;
bool flagCat = false;
bool flagErrorLight = false;
bool flagErrorSD = false;
String fileNameTxt = "";


void setup() 
{

  Serial.begin(9600);
  mySerial.begin(9600);
    while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Wire.begin();
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  pinMode(RED_LASER, OUTPUT);
  pinMode(UV_DIODE, OUTPUT);
  digitalWrite(UV_DIODE, LOW);
  digitalWrite(RED_LASER, LOW);

  Serial.print("Initializing SD card...");
 
  // if (!SD.begin(SD2_CONFIG)) {
  if (!SD.begin(SD_CS_PIN)) {
      Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
}

void loop() 
{

if (mySerial.available())     //---PARSING---//
  {
    char data[200];
    int amount = mySerial.readBytesUntil(';',data,200); //until terminator //возвращает колличество принятых байт
    data[amount] = NULL;
    Serial.println(data);
    GParser dataG(data, ',');
    int am = dataG.split();

    switch (dataG.getInt(0))
    {
      case 369: // SET SETTINGS
        timeAd = dataG.getInt(1); // Адсорбция
        timeCat = dataG.getInt(2); // общее время
        timeMsr = dataG.getInt(3); // измерять каждые n минут
        fileNameTxt = dataG[4];
        amountCat=timeCat/timeMsr;
        amountAd=timeAd/timeMsr;
        mySerial.println("Adsorption: " + String(timeAd) + "; Catalysis: " + String(timeCat) + "; Measure: " + String(timeMsr) + "; File: " + String(fileNameTxt));
        break;

      case 1: // START 
        flagCat = true;
        mySerial.println("Measurements has been started");
        break;
        
      case 2: // STOP
        flagCat = false;
        mySerial.println("Measurements has been breaked");
        break;

      case 3: // Red On
        digitalWrite(RED_LASER, HIGH);
        float measM;
        if (lightMeter.measurementReady()) { measM = lightMeter.readLightLevel(); }
        mySerial.println(measM);
        break;

      case 4: // Red off
        digitalWrite(RED_LASER, LOW);
        break;
    }
  
  }


if (flagCat==true)
{
  if (counter == 0)
  {
    digitalWrite(UV_DIODE, LOW);
    digitalWrite(RED_LASER, HIGH);
    float meas = 0;
    for (int i = 0; i<10; i++) //make 10 measurements
    {
      if (lightMeter.measurementReady()) { meas = meas + lightMeter.readLightLevel(); }
      delay(100);
    }
    digitalWrite(RED_LASER, LOW);
    float measM = meas/10; //average result
      
    myFile = SD.open(fileNameTxt, FILE_WRITE);
    if (myFile) 
    {
      myFile.println(String(counter) + '\t' + String(millis()-timePrc) + '\t' + String(measM));
      myFile.close();
      Serial.println("done.");
    } 
    else 
    {
      // if the file didn't open, print an error:
      Serial.println("error opening test.txt");
    }
    mySerial.println(measM);
    counter++;
  }
  if (counter>amountAd) { digitalWrite(UV_DIODE, HIGH); }
  if (millis() - timePrc >= timeMsr) 
  {
    timePrc = millis();
    digitalWrite(UV_DIODE, LOW);
    digitalWrite(RED_LASER, HIGH);
    float meas = 0;
    for (int i = 0; i<10; i++)
    {
      if (lightMeter.measurementReady()) { meas = meas + lightMeter.readLightLevel(); }
      delay(100);
    }
    digitalWrite(RED_LASER, LOW);
    float measM = meas/10;
      
    myFile = SD.open(fileNameTxt, FILE_WRITE);
    if (myFile) {
    myFile.println(String(counter) + '\t' + String(millis()-timePrc) + '\t' + String(measM));
    myFile.close();
    Serial.println("done.");
    } 
    else 
    {
      // if the file didn't open, print an error:
      Serial.println("error opening test.txt");
    }
    mySerial.println(measM);
    counter++;
  }
  if (counter>(amountCat+amountAd)) { flagCat = false; }

}
  
}


