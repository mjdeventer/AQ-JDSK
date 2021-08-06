#include <Wire.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"

#include <Seeed_BME280.h>
#include "SdsDustSensor.h"
#include <SPI.h>
#include <SD.h>
#include "RTClib.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define BME280_ADDRESS   0x76 // define BMP280 adress
#define DEVICE "ESP8266"
#define MESSPUNKT "MP_3" //tag of measurement site
#define NODEMCU_ID "Node_3"  //tag of microcontroller used
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Define Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET   LED_BUILTIN // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1); //LED_BUILTIN

// Define SD Card Pin
const int chipSelect = D8;  
// Initialize RTC
RTC_DS1307 RTC;  
             
float Temperature;
float Humidity;
float Pressure;

// Initialize SDS sensor
int rxPin = D3;
int txPin = D4;
SdsDustSensor sds(rxPin, txPin);

// Initialize BME280
BME280 bme280;

// Initialize WIFI
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Initialize InfluxDB
#define INFLUXDB_URL "https://westeurope-1.azure.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "" //  1kn0mBd9TSX9ZFSjAC-U4SpZUFPrl7kYja9sPiGVAxSEnU7lu_r6oxDv2dkRO-eWrz1fpTeY1O78HmpbFlRUtA==
#define INFLUXDB_ORG ""
#define INFLUXDB_BUCKET "" // AQ-JDMB
// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Data points to write into DB bucket, each sensor gets individual datapoints
Point bmpdp("BMP280");
Point sdsdp("SDS011");

void setup() {
  Serial.begin(9600);
   if(!bme280.init()){
  Serial.println("Device error!");}
  Wire.begin(); // Start the I2C
  RTC.begin();  // Init RTC
  RTC.adjust(DateTime(__DATE__, __TIME__));  // Time and date is expanded to date and time on your computer at compiletime
  delay(1000);
  Serial.println("Time and date set");
  delay(500);
  if (!RTC.isrunning()) {
  Serial.println("RTC lost power");
  RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  if (!SD.begin(chipSelect)) {
  Serial.println("Initialization failed!");
  while (1);
  }
 
  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
  Serial.print(".");
  delay(500);
  }
  Serial.println("wifi established");
 
  // Setup Influx DB
  // Add tags to datapoints for DB
  bmpdp.addTag("device", NODEMCU_ID);
  bmpdp.addTag("Messpunkt", MESSPUNKT);
  sdsdp.addTag("device", NODEMCU_ID);
  sdsdp.addTag("Messpunkt", MESSPUNKT);
 
  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  timeSync(TZ_INFO, "pool.ntp.org", "de.pool.ntp.org");

  // Check server connection
  if (client.validateConnection()) {
  Serial.print("Connected to InfluxDB: ");
  Serial.println(client.getServerUrl());
  } else {
  Serial.print("InfluxDB connection failed: ");
  Serial.println(client.getLastErrorMessage());
  }
 
  //SDS SETUP
  sds.begin();
  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version
  Serial.println(sds.setActiveReportingMode().toString()); // ensures sensor is in 'active' reporting mode
  Serial.println(sds.setContinuousWorkingPeriod().toString()); // ensures sensor has continuous working period - default but not recommended

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
   Serial.println(("SSD1306 allocation failed"));
   for(;;);
  }
  delay(500);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  display.println("ANECO NodeMCU Ready");
  display.display();
  delay(2500);
}

void loop() {
  // preallocate constants and datapoints
  String dataString = "";
  bmpdp.clearFields();
  sdsdp.clearFields();
 
  PmResult pm = sds.readPm();
  if (pm.isOk()) {
  DateTime now = RTC.now();
  Temperature = bme280.getTemperature(); // Gets the values of the temperature;
  Humidity = bme280.getHumidity(); // Gets the values of the humidity;
  Pressure = bme280.getPressure(); // Gets the values of the pressure in hPa;
  bmpdp.addField("Temperature",Temperature);
  bmpdp.addField("RH",Humidity);
  bmpdp.addField("hPa",Pressure);
  bmpdp.addField("rssi", WiFi.RSSI());
  sdsdp.addField("PM_10",pm.pm10);
  sdsdp.addField("PM_25",pm.pm25);
    
  Serial.print(". received all sensor data: PM2.5 = ");
  Serial.print(pm.pm25);
  Serial.print(", PM10 = ");
  Serial.print(pm.pm10);
  Serial.print(", Tair = ");
  Serial.print(Temperature);
  Serial.print(", RH = ");
  Serial.println(Humidity);
  Serial.print(", hPa = ");
  Serial.println(Pressure);

  // Write Data to SD
  dataString += String(now.timestamp());
  dataString += ",";
  dataString += String(Temperature);
  dataString += ",";
  dataString += String(Humidity);
  dataString += ",";
  dataString += String(Pressure);
  dataString += ",";
  dataString += String(pm.pm10);
  dataString += ",";
  dataString += String(pm.pm25);
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
 
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    Serial.print("...Writing data to SD card: ");
    Serial.println(dataString);
  }
  else {
    Serial.println("error opening datalog.txt");
  }
    
  // Send complete Data to LED Display
  display.clearDisplay();
  display.setTextSize(1);           // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);      // Draw white text
  display.setCursor(0,0);           // Start at top-left corner
  display.print(("Datum: "));
  display.println(now.timestamp());
  display.print(("Temperatur: "));
  display.println(Temperature);
  display.print(("Luftfeuchte: "));
  display.println(Humidity);
  display.print(("Luftdruck: "));
  display.println((Pressure));
  display.print(("PM10: "));
  display.println(pm.pm10);
  display.print(("PM25: "));
  display.println(pm.pm25);
  display.display();

  // Print what are we exactly writing to InfluxDB
  Serial.print("......Uploading to DB: ");
  Serial.println(client.pointToLineProtocol(bmpdp));
  Serial.print(".........Uploading to DB: ");
  Serial.println(client.pointToLineProtocol(sdsdp));
 
  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }
  // Write point
  if (!client.writePoint(bmpdp)) {
    Serial.print("InfluxDB write failed DHT datapoint: ");
    Serial.println(client.getLastErrorMessage());
  }
  if (!client.writePoint(sdsdp)) {
    Serial.print("InfluxDB write failed SDS datapoint: ");
    Serial.println(client.getLastErrorMessage());
  }
    
  } else {
  Serial.print("Could not read values from SDS sensor, reason: ");
  Serial.println(pm.statusToString());
  }

  //Wait
  Serial.println("............ Waiting 5s for next poll");
  delay(5000);
}
