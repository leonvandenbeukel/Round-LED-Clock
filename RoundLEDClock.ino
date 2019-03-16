/*
  WiFi connected round LED Clock. It gets NTP time from the internet and translates to a 60 RGB WS2812B LED strip.

  If you have another orientation where the wire comes out then change the methods getLEDHour and getLEDMinuteOrSecond

  Happy programming, Leon van den Beukel, march 2019

  ---  
  NTP and summer time code based on:
  https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html 
  https://github.com/SensorsIot/NTPtimeESP/blob/master/NTPtimeESP.cpp (for US summer time support check this link)
  
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#define DEBUG_ON

ESP8266WiFiMulti wifiMulti;                     
WiFiUDP UDP;                                    
IPAddress timeServerIP;                         
const char* NTPServerName = "time.nist.gov";    
const int NTP_PACKET_SIZE = 48;                 
byte NTPBuffer[NTP_PACKET_SIZE];                

const char ssid[] = "*";                        // Your network SSID name here
const char pass[] = "*";                        // Your network password here

unsigned long intervalNTP = 60000;              // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;
unsigned long prevActualTime = 0;

#define LEAP_YEAR(Y) ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )
static const uint8_t _monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

#define NUM_LEDS 60     
#define DATA_PIN D6
CRGB LEDs[NUM_LEDS];

int year;
int month;
int day;
int hour;
int minute;
int second;
int dayofweek;

void setup() {

  FastLED.delay(3000);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);  

  Serial.begin(115200);          
  delay(10);
  Serial.println("\r\n");

  startWiFi();
  startUDP();

  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { 
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
  
  Serial.println("\r\nSending NTP request ...");
  sendNTPpacket(timeServerIP);  
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // If a minute has passed since last NTP request
    prevNTP = currentMillis;
    Serial.println("\r\nSending NTP request ...");
    sendNTPpacket(timeServerIP);               // Send an NTP request
  }

  uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
  if (time) {                                  // If a new timestamp has been received
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
  } else if ((currentMillis - lastNTPResponse) > 3600000) {
    Serial.println("More than 1 hour since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse)/1000;
  if (actualTime != prevActualTime && timeUNIX != 0) { // If a second has passed since last update
    prevActualTime = actualTime;
    ConvertUnixTimestamp(actualTime);

    for (int i=0; i<NUM_LEDS; i++) {
      LEDs[i] = CRGB::Black;
    }

    LEDs[getLEDMinuteOrSecond(second)] = CRGB::Blue;
    LEDs[getLEDHour(hour)] = CRGB::Red;
    LEDs[getLEDMinuteOrSecond(minute)] = CRGB::Green;    

    FastLED.show();
  }  
}

byte getLEDHour(byte hours) {
  if (hours > 12)
    hours = hours - 12;

  if (hours <= 5) 
    return (hours * 5) + 30;
  else
    return (hours * 5) - 30;
}

byte getLEDMinuteOrSecond(byte minuteOrSecond) {
  if (minuteOrSecond < 30) 
    return minuteOrSecond + 30;
  else 
    return minuteOrSecond - 30;
}

void startWiFi() { 
  wifiMulti.addAP(ssid, pass);   

  Serial.println("Connecting");
  byte i = 0;
  while (wifiMulti.run() != WL_CONNECTED) {  
    delay(250);
    Serial.print('.');
    LEDs[i++] = CRGB::Green;
    FastLED.show();    
  }
  Serial.println("\r\n");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());             
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());            
  Serial.println("\r\n");
}

void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages on port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();
}

uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}

void ConvertUnixTimestamp(uint32_t _time) {
  second = _time % 60;
  minute = _time / 60 % 60;
  hour   = _time / 3600 % 24;
  _time /= 60; // To minutes
  _time /= 60; // To hours
  _time /= 24; // To days
  dayofweek = ((_time + 4) % 7) + 1;
  int _year = 0;
  int _days = 0;
  while ((unsigned)(_days += (LEAP_YEAR(_year) ? 366 : 365)) <= _time) {
    _year++;
  }
  _days -= LEAP_YEAR(_year) ? 366 : 365;
  _time  -= _days; // To days in this year, starting at 0  
  _days = 0;
  int  _month = 0;
  int  _monthLength = 0;
  for (_month = 0; _month < 12; _month++) {
    if (_month == 1) { // february
      if (LEAP_YEAR(_year)) {
        _monthLength = 29;
      } else {
        _monthLength = 28;
      }
    } else {
      _monthLength = _monthDays[_month];
    }
  
    if (_time >= _monthLength) {
      _time -= _monthLength;
    } else {
      break;
    }
  }
 
  day = _time + 1;
  year = _year + 1970;
  month = _month + 1;  

  if (!summerTime()) 
    hour += 1;

#ifdef DEBUG_ON
  Serial.print(year);
  Serial.print(" ");
  Serial.print(month);
  Serial.print(" ");
  Serial.print(day);
  Serial.print(" ");
  Serial.print(hour);
  Serial.print(" ");
  Serial.print(minute);
  Serial.print(" ");
  Serial.print(second);
  Serial.print(" day of week: ");
  Serial.print(dayofweek);
  Serial.print(" summer time: ");
  Serial.print(summerTime());
  Serial.println();
#endif  
}

boolean summerTime() {

  if (month < 3 || month > 10) return false;  // No summer time in Jan, Feb, Nov, Dec
  if (month > 3 && month < 10) return true;   // Summer time in Apr, May, Jun, Jul, Aug, Sep
  if (month == 3 && (hour + 24 * day) >= (3 +  24 * (31 - (5 * year / 4 + 4) % 7)) || month == 10 && (hour + 24 * day) < (3 +  24 * (31 - (5 * year / 4 + 1) % 7)))
  return true;
    else
  return false;
}
