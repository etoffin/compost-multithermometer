// Multithermometer for compost monitoring at different depths
// Tested with ESP32



// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// https://github.com/milesburton/Arduino-Temperature-Control-Library


#include <WiFi.h>
#include <esp_wpa2.h> // example found here https://github.com/JeroenBeemster/ESP32-WPA2-enterprise

#include "InfluxArduino.hpp"
//#include "InfluxCert.hpp"

#include <OneWire.h> // code for DS18B20 data collection here https://github.com/stickbreaker/OneWire/blob/master/examples/DS18x20_Temperature/DS18x20_Temperature.pde
OneWire  ds(25);  // on pin 25 (a 4.7K resistor is necessary) 


/* Constants */

// for connection on Paline-WiFi
const char* ssid = "Plaine-WiFi"; // your ssid
#define EAP_ID "toffin1@ulb.ac.be"
#define EAP_USERNAME "toffin1@ulb.ac.be"
#define EAP_PASSWORD "one53..Y"


// connection/ database stuff that needs configuring
// for connection on Etienne's Laptop WiFi (useless now)
const char WIFI_NAME[] = "Hal Nonante";
const char WIFI_PASS[] = "ProutProut!";


InfluxArduino influx;

const char INFLUX_DATABASE[] = "valuebugs_test";
const char INFLUX_IP[] = "influx.valuebugs.be";
const char INFLUX_USER[] = "etienne";
const char INFLUX_PASS[] = "6xQH6d0SDB6ttYJXGX04";
const char INFLUX_MEASUREMENT[] = "compost_multithermometer-test";

// function to convert array of HEX values into char strings
void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}


void setup(void) {
  Serial.begin(115200);

/*
  // connection with WPA2
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
    
  WiFi.disconnect(true);      
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ID, strlen(EAP_ID));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
//  esp_wifi_sta_wpa2_ent_enable(); //we need to find what should be used here
    // WPA2 enterprise magic ends here

    WiFi.begin(ssid);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
*/

// to be used with ssid and pass configuration

  WiFi.begin(WIFI_NAME, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n WiFi connected! \n");


  // try connection with influxdb
  influx.configure(INFLUX_DATABASE, INFLUX_IP); //third argument (port number) defaults to 8086
  influx.authorize(INFLUX_USER, INFLUX_PASS);   //if you have set the Influxdb .conf variable auth-enabled to true, uncomment this
//  influx.addCertificate(ROOT_CERT);             //uncomment if you have generated a CA cert and copied it into InfluxCert.hpp
//  Serial.print("Using HTTPS: ");
//  Serial.println(influx.isSecure()); //will be true if you've added the InfluxCert.hpp file.
}


void loop(void) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius;

  
  if ( !ds.search(addr)) {
    Serial.println("\n \nNo more addresses. \nScanning for DS12B80 sensors… \n");
    ds.reset_search();
    delay(1000);
    return;
  }

  
/*  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }
    Serial.println();*/

  // use ROM value as sensor_id
  char sensor_id[128]="";
  array_to_string(addr, 8, sensor_id);
  Serial.print("sensor id: ");
  Serial.println(sensor_id);

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad


//  Serial.print("  Data = ");
//  Serial.print(present, HEX);
//  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    //Serial.print(data[i], HEX);
    //Serial.print(" ");
  }
//  Serial.print(" CRC=");
//  Serial.print(OneWire::crc8(data, 8), HEX);
//  Serial.println();


  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  
/*  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.println(" Celsius, ");
*/

  char tags[64];
  char fields[256];
  char formatTags[] = "sensor_id=%s";
  char formatFields[] = "temperature=%0.3f";

  sprintf(tags, formatTags, sensor_id);
  sprintf(fields, formatFields, celsius);

  Serial.println(tags);
  Serial.println(fields);
  Serial.println();


/* Write data to influxDB */
 // influx.write(INFLUX_MEASUREMENT, tags, fields);
  // when getting test results, ds12b80 addresses are lost
  /*if (!influx.write(INFLUX_MEASUREMENT, tags, fields))
  {
    Serial.print("Unable to connect to InfluxDB database. Error: ");
    Serial.println(influx.getResponse());
  }*/
  
  // it is mandatory to add a delay after sending datas on influxdb database, to prevent problems with the ds12b80 addresses
  delay(1000);
}
