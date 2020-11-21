//  ESP32 web radio player. 
//	Based on educ8s/ESP32-Web-Radio-Simple on github
//  Customization and further dev by limbo666 


#include <VS1053.h>  //https://github.com/baldram/ESP_VS1053_Library
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>
#include <EEPROM.h>

#define VS1053_CS    32
#define VS1053_DCS   33
#define VS1053_DREQ  35

#define VOLUME  70 // volume level 0-100
#define EEPROM_SIZE 64

//UDP
#include <AsyncUDP.h>
AsyncUDP udp;
//

////OLED
#include <Arduino.h>
#include <U8g2lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);  // Please check your OLED screen type there are lots of different supported modules by u8g2 library
////

long interval = 1000;
int SECONDS_TO_AUTOSAVE = 30;
long seconds = 0;
long previousMillis = 0;

int radioStation = 0;
int previousRadioStation = -1;
const int previousButton = 12;
const int nextButton = 14; //This changed! it was 13 on previous code release

char ssid[] = "YOURSSID";            //  your network SSID (name)
char pass[] = "YOURPASS";            // your network password

// 8 radio stations (must fill:  host, path and port)
char *host[8] = {"best.live24.gr", "pepper966.live24.gr", "loveradio.live24.gr", "iphone.live24.gr", "netradio.live24.gr", "radiostreaming.ert.gr", "ice.onestreaming.com", "nitro.live24.gr"};
char *path[8] = {"/best1222", "/pepperorigin", "/loveradio-1000", "/radio998", "/mousikos986", "/ert-kosmos", "/velvetfm", "/nitro4555"};
int   port[8] = {80, 80, 80, 80, 80, 80, 80, 80};
// Radio station names will be dispalyed on OLED screen, serial communication and UDP broadcast
// Recommended up to 13 chars per name to get displayed correctly on the screen.
char *radioname[8] = {"  Best 92.6" , " Pepper 96.6", " Love Radio",  " Smooth 99.8" , "Mousikos 98.6", " Kosmos 93.6", "  Velvet FM", "*AthensVoice*"};


int status = WL_IDLE_STATUS;
WiFiClient  client;
uint8_t mp3buff[32];   // vs1053 likes 32 bytes at a time

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

void IRAM_ATTR previousButtonInterrupt() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200)
  {
    if (radioStation > 0)
      radioStation--;
    else
      radioStation = 7;
  }
  last_interrupt_time = interrupt_time;
}

void IRAM_ATTR nextButtonInterrupt() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  if (interrupt_time - last_interrupt_time > 200)
  {
    if (radioStation < 8)
      radioStation++;
    else
      radioStation = 0;
  }
  last_interrupt_time = interrupt_time;
}

void setup () {
  Serial.begin(115200); // no need to go o 9600 baud rate, stay to 115200 to read all messages coming from ESP32
  Serial.println("initializing oled screen");
  delay(500);
  // initialize Screen
  u8g2.begin();
  u8g2.clearBuffer();          // clear the internal memory
  u8g2.setFont(u8g2_font_amstrad_cpc_extended_8f);
  u8g2.drawStr(18, 22, "Starting up!"); // Srartup messges on screen
  u8g2.sendBuffer();
  delay(100);
  //

  SPI.begin();
  Serial.println("setting up eeprom");
  EEPROM.begin(EEPROM_SIZE);
  delay(2000); // Some delay to help some EEPROMs
  Serial.println("setting input pins");
  pinMode(previousButton, INPUT_PULLUP);
  pinMode(nextButton, INPUT_PULLUP);
  Serial.println("setting interrupts");
  attachInterrupt(digitalPinToInterrupt(previousButton), previousButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(nextButton), nextButtonInterrupt, FALLING);
  Serial.println("initialising decoder module");
  initMP3Decoder();
  Serial.println("connecting to wifi");
  connectToWIFI();
  Serial.println("reading last station from eeprom");
  radioStation = readStationFromEEPROM();
  Serial.println("last radio station:" + String(radioStation));
  Serial.println("starting UDP server");
  // UDP Receiver below
  if (udp.listen(4009)) {
    Serial.print("UDP listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet) {
      String inText = "";
      inText = String( (char*) packet.data());
      //  Serial.write(packet.data(), packet.length());
      Serial.println(inText);
      if ( inText == "0" )    {
        Serial.println("station 0 selected by UDP command");
        radioStation = 0;
      }
      else if ( inText == "1" ) {
        Serial.println("station 1 selected by UDP command");
        radioStation = 1;
      }
      else if ( inText == "2" ) {
        Serial.println("station 2 selected by UDP command");
        radioStation = 2;
      }
      else if ( inText == "3" ) {
        Serial.println("station 3 selected by UDP command");
        radioStation = 3;
      }
      else if ( inText == "4" ) {
        Serial.println("station 4 selected by UDP command");
        radioStation = 4;
      }
      else if ( inText == "5" ) {
        Serial.println("station 5 selected by UDP command");
        radioStation = 5;
      }
      else if ( inText == "6" ) {
        Serial.println("station 6 selected by UDP command");
        radioStation = 6;
      }
      else if ( inText == "7" ) {
        Serial.println("station 7 selected by UDP command");
        radioStation = 7;
      }
    });
  }
  //
  Serial.println("setup done");
}

void UDPSend(String txtBroadcast) {
  String builtString = String("");
  builtString = String(txtBroadcast);
  udp.broadcastTo(builtString.c_str(), 4009);
}


void loop() {

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis > interval)
  {
    if (radioStation != previousRadioStation)
    {
      station_connect(radioStation);
      previousRadioStation = radioStation;
      seconds = 0;
    } else
    {
      seconds++;
      if (seconds == SECONDS_TO_AUTOSAVE)
      {
        int readStation = readStationFromEEPROM();
        if (readStation != radioStation)
        {
          writeStationToEEPROM();
        }
      }
    }
    previousMillis = currentMillis;
    String myReport = String(radioname[radioStation]) +  " - ID:" + String(radioStation) + " - Playing time:" + String(seconds) + " secs";
    Serial.println(myReport);
    UDPSend(myReport);
    u8g2.clearBuffer();          // clear the internal memory
    u8g2.setFont(u8g2_font_profont17_mr);
    u8g2.setCursor(7, 22);
    // u8g2.print ("Playing");
    u8g2.print (String(radioname[radioStation]));
    u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();          // send to display

  }

  if (client.available() > 0)
  {
    uint8_t bytesread = client.read(mp3buff, 32);
    player.playChunk(mp3buff, bytesread);
  }
}

void station_connect (int station_no ) {
  if (client.connect(host[station_no], port[station_no]) ) Serial.println("station connected :-)");
  client.print(String("GET ") + path[station_no] + " HTTP/1.1\r\n" +
               "Host: " + host[station_no] + "\r\n" +
               "Connection: close\r\n\r\n");
}

void connectToWIFI()
{
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("connected to WiFi");
  Serial.println(WiFi.localIP());

  u8g2.clearBuffer();                     // clear the internal memory
  u8g2.setFont(u8g2_font_amstrad_cpc_extended_8f);  // choose a suitable font
  u8g2.setCursor(6, 20);
  u8g2.print ("Connected");
  u8g2.sendBuffer();          // transfer internal memory to the display
}

void initMP3Decoder()
{
  player.begin();
  player.switchToMp3Mode(); // optional, some boards require this
  player.setVolume(VOLUME);
}

int readStationFromEEPROM()
{
  int aa;
  aa = EEPROM.read(0);
  return aa;
}

void writeStationToEEPROM()
{
  Serial.println("saving radio station: " + String(radioStation));
  EEPROM.write(0, radioStation);
  EEPROM.commit();
  // just for verificaion
  int ab;
  ab = EEPROM.read(0);
  Serial.println("eeprom value verification: " + String(ab)); // display back to serial port what exists on EEPROM

}

