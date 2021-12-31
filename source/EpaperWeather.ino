/*
Small internet weather station with long battery life based on
LilyGO TTGO T5 V2.3 ESP32 - with 2.13 inch E-paper E-ink
In Arduino IDE: Boards ESP32 Arduino / ESP32 Dev Module

inCollects internet time & weather (temp, min temp, max temp, humidity, wind speed and direction)
every 15 minutes. A full info screen is displyed for 30 seconds then fewer info is displayed in bigger font
and the module goes to sleep for 15 minutes (display still shows last info) 
Some info is displayed in Dutch but can easyly be translated

inspired by https://github.com/Xinyuan-LilyGO/T5-Ink-Screen-Series
and https://www.youtube.com/watch?v=av-w0U8UZEs
and https://bestofcpp.com/repo/piotr-kubica-weather-tiny-cpp-miscellaneous
*/

#define LILYGO_T5_V213
#include <boards.h>
#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h>    // 2.13" b/w  form DKE GROUP
#include <SD.h>
#include <FS.h>
#include GxEPD_BitmapExamples
#include <U8g2_for_Adafruit_GFX.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
#include <WiFi.h>

GxIO_Class io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
GxEPD_Class display(io, EPD_RSET, EPD_BUSY);
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
int               sleepmins     = 15;
int               sleeptime     = sleepmins * 60; // 60 mins * 60 seconds

//from ESP32_Client_TTGO_8.4
bool      debug=true;
#define   rightbutton 39
#include  <ESP32Ping.h>
#include  <HTTPClient.h>
#include  "gewoon_secrets.h"
/*
// gewoon_secrets.h: wifi passwords and weather.api get yours at api.openweathermap.org
const char* ssid     = "mySSID";        
const char* password = "myWIFIpassword";
String town="Apeldoorn";//weather api           
String Country="NL";               
const String key = "095e789fe1a290c29b29bbb364346bcd";
*/

//RTC
#include <ArduinoJson.h>        //https://github.com/bblanchon/ArduinoJson.git
#include <NTPClient.h>          //https://github.com/taranais/NTPClient
#include <WiFiUdp.h>
//HTTP
const String endpoint = "http://api.openweathermap.org/data/2.5/weather?q="+town+","+Country+"&units=metric&APPID=";
String payload= "";              //whole json 
String tmp    = "";              //temperatur
String hum    = "";              //humidity
String tmpmi  = "";              //temp max
String tmpma  = "";              //temp min
String tmps   = "";              //windspeed
String tmpr   = "";              //wind direction
String tmpf   = "";              //wind direction
String tmpp   = "";              //wind direction
String tmpd   = "";
String tmpm   = "";              //main description
String tmpv   = "";              //visibility

StaticJsonDocument<1000> doc;
WiFiUDP ntpUDP;                  // Define NTP Client to get time
NTPClient timeClient(ntpUDP);    // Variables to save date and time
String formattedDate;
String dayStamp;
String LastdayStamp;
String TimeStamp;
String LastTimeStamp;
String Ftemp;                    //formatted temp sensor1
String Fhum;                     //formatted hum  sensor1
String Fbar;                     //formatted bar  sensor1
String Fsoil;
String Ftemp2;                   //formatted temp sensor2
String Fhum2;                    //formatted hum  sensor2
String Fsoil2;
String Ftempi;                   //formatted internet temp
String Fhumi;                    //formatted internet hum
String Fbari;                    //formatted bar
String Fmin;
String Fmax;
float  windspeed;
int    beauf;
int    winddeg;
String winddirection;
String windtxt[]={"stil","zeer zwak","zwak","vrij matig","matig","vrij krachtig","krachtig","hard","stormachtig","storm","zware storm","zeer zwarestorm","orkaan"};
String winddir[]={"N","N","NNO","NNO","NO","NO","ONO","ONO","O","O","O","O","OZO","OZO","ZO","ZO","ZZO","ZZO","Z","Z","Z","Z","ZZW","ZZW","ZW","ZW","WZW","WZW","W","W","W","W","WNW","WNW","NW","NW","NNW","NNW","N","N"};
long sunrise; 
long sunset; 

void setup(void)
{
  Serial.begin      (115200);
  pinMode(rightbutton,INPUT);
  unsigned long     sleepingtime  = (sleeptime * uS_TO_S_FACTOR);
  esp_sleep_enable_timer_wakeup (sleepingtime);
  Serial.println    ("setup");
  SPI.begin         (EPD_SCLK, EPD_MISO, EPD_MOSI);    
  display.init      (); // enable diagnostic output on Serial
   
  u8g2Fonts.begin               (display);             // 122x250pixels
  u8g2Fonts.setFontMode         (1);                   // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection    (1);                   // landscape
  u8g2Fonts.setForegroundColor  (GxEPD_BLACK);         // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor  (GxEPD_WHITE);         // apply Adafruit GFX color
//u8g2Fonts.setFont             (u8g2_font_helvR14_tf);// select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
//u8g2Fonts.setFont             (u8g2_font_courB10_tr);// 
//u8g2Fonts.setFont             (u8g2_font_10x20_mr);//not
  u8g2Fonts.setFont             (u8g2_font_VCR_OSD_tf);
  display.fillScreen            (GxEPD_WHITE);

  //u8g2Fonts.setCursor  (item1data[boxx],item1data[boxy]);
  WiFi.begin           (ssid, password);
  Serial.println       ("Connecting");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500);}
    
  Serial.println         ("Connected to ");
  Serial.println         (ssid);
  Serial.println         ("with IP Address: ");
  Serial.println         (WiFi.localIP());
  display.update         ();

//RTC Initialize a NTPClient to get time
  timeClient.begin(); 
  timeClient.setTimeOffset(3600);  
  //TIME
  while(!timeClient.update()) {timeClient.forceUpdate();}
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate   = timeClient.getFormattedDate();
  int splitT      = formattedDate.indexOf("T");
  dayStamp        = formattedDate.substring(0, splitT);
  TimeStamp       = formattedDate.substring(splitT+1, formattedDate.length()-1);
  
  getWeatherData();  
  
//fill display
///01234567890123456789012345678901234567890123456789012345678901234567890
 //Het is vandaag 12-12-2021 in Apeldoorn
 //Buiten is het  12.3 graden 1024 mb.
 //dat voelt als   5.0 graden Min. 12.1 Max. 12.0
 //Wind ZZO 5 matig
 //verwachting voor morgen 10.0 graden ZO 2
 //Laatste info   12-12-2021 om 17:53 uur
 //
display.fillScreen    (GxEPD_WHITE);   
u8g2Fonts.setCursor   ( 120,0);//falls off
u8g2Fonts.print       ("0123456789012345678901234567890123456789012345");
u8g2Fonts.setCursor   ( 100,0);
//u8g2Fonts.print     ("12-12-2021 17:53");
u8g2Fonts.print       (dayStamp);
u8g2Fonts.print       (" ");
u8g2Fonts.print       (TimeStamp);
u8g2Fonts.setCursor   ( 80,0);
//u8g2Fonts.print     ("12.3 C 1024 Mb.");
u8g2Fonts.print       ("Temp ");
u8g2Fonts.print       (Ftempi);
u8g2Fonts.print       ("°C ");
u8g2Fonts.print       (tmpp);
u8g2Fonts.print       (" Mb.");
u8g2Fonts.setCursor   ( 60,0);
//u8g2Fonts.print     ("Min. 12.1 Max. 15.0");
u8g2Fonts.print       ("Min. ");
u8g2Fonts.print       (Fmin);
u8g2Fonts.print       ("°C Max.");
u8g2Fonts.print       (Fmax);
u8g2Fonts.print       ("°C");
u8g2Fonts.setCursor   ( 40,0);
//u8g2Fonts.print     ("Wind ZZO 5 matig");
u8g2Fonts.print       ("Wind ");
u8g2Fonts.print       (winddirection);
u8g2Fonts.print       (beauf);
u8g2Fonts.print       (" ");
u8g2Fonts.print       (windtxt[beauf]);

u8g2Fonts.setCursor   ( 20,0);
u8g2Fonts.print       ("Chill");
u8g2Fonts.print       (tmpf);
u8g2Fonts.print       ("°C");
u8g2Fonts.print       (" hum.");
u8g2Fonts.print       (Fhumi);
u8g2Fonts.print       ("%");
u8g2Fonts.setCursor   ( 0,0);
//u8g2Fonts.print     (tmpm);////not working
u8g2Fonts.print       ("Zicht");
u8g2Fonts.print       (tmpv);
u8g2Fonts.print       ("mtr.");
display.update();

delay (30000);
display.fillScreen    (GxEPD_WHITE);   
//u8g2Fonts.setFont     (u8g2_font_fub42_tf);//46
u8g2Fonts.setFont     (u8g2_font_fub35_tf);//35
u8g2Fonts.setCursor   (80,10);
u8g2Fonts.print       (Ftempi);
u8g2Fonts.print       ("° ");
u8g2Fonts.print       (winddirection);
u8g2Fonts.print       (beauf);
u8g2Fonts.setCursor   (30,10);
//u8g2Fonts.print       (windtxt[beauf]);
u8g2Fonts.print       ("Min.");
u8g2Fonts.print       (Fmin);
u8g2Fonts.print       ("°C");
u8g2Fonts.setFont     (u8g2_font_helvR14_tf);
u8g2Fonts.setCursor   (0,10);
u8g2Fonts.print       ("last update:");
u8g2Fonts.print       (TimeStamp);
display.update();
}

void loop()
{
Serial.println      ("Going to sleep for  " + String(sleeptime/60) + " Minutes");
delay               (200);
esp_deep_sleep_start();
//keypressed();
}

void keypressed(){
if(digitalRead         (rightbutton)==0){
   //previousMillis4 = millis();
   int presr=0;
   while (digitalRead  (rightbutton)==0&&presr<75){delay(10);presr++;}
   if (presr>=75){      //longpress
   } else {  
    //  display.fillScreen            (GxEPD_WHITE);
    display.update(); //shortpress
   }
   }
}// end of rightbutton

void getWeatherData(){  
   if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http; 
    http.begin(endpoint + key); //Specify the URL
    int httpCode = http.GET();  //Make the request 
    if (httpCode > 0) { //Check for the returning code 
         payload = http.getString();
         if (debug==true){
         Serial.println(httpCode);
         Serial.println();
         Serial.println(payload);}        
     } else { Serial.println("Error on HTTP request"); }
    http.end(); //Free the resources
   
/*//{"coord":{"lon":5.9694,"lat":52.21},
"weather":[{"id":803,"main":"Clouds","description":"broken clouds","icon":"04d"}],
"base":"stations",
"main":{"temp":2.7,"feels_like":-1.04,"temp_min":1.57,"temp_max":4.36,"pressure":1001,"humidity":96},
"visibility":10000,
"wind":{"speed":4.12,"deg":270},
"clouds":{"all":75},
"dt":1638438824,
"sys":{"type":2,"id":2010138,"country":"NL","sunrise":1638429867,"sunset":1638458825},"timezone":3600,"id":2759706,"name":"Apeldoorn","cod":200}
*/
    char inp[1000];
    payload.toCharArray(inp,1000);
    deserializeJson(doc,inp);
    
//    String tmp_main = doc["weather"]["main"];
    String tmp_main = doc["main"];//not working
//    Serial.println(tmp_main);
//    String tmp_desc = doc["weather"]["description"];
    String tmp_desc = doc["description"];//not working
//    Serial.println(tmp_desc);
    String tmp_icon = doc["weather"]["icon"];
    
    String tmp_temp = doc["main"]["temp"];
    String tmp_feel = doc["main"]["feels_like"];
    String tmp_min  = doc["main"]["temp_min"];
    String tmp_max  = doc["main"]["temp_max"];
    String tmp_pres = doc["main"]["pressure"];
    String tmp_humi = doc["main"]["humidity"];       
    String tmp_visi = doc["visibility"];
 
    String tmp_spee = doc["wind"]["speed"];
    String tmp_degr = doc["wind"]["deg"];

    String tmp_sunr = doc["sys"]["sunrise"];
    String tmp_suns = doc["sys"]["sunset"];

    tmp   = tmp_temp;
    hum   = tmp_humi;
    tmpmi = tmp_min;
    tmpma = tmp_max;
    tmps  = tmp_spee;
    tmpr  = tmp_degr;
    tmpf  = tmp_feel;
    tmpp  = tmp_pres;  
    tmpd  = tmp_desc;
    tmpm  = tmp_main;
    tmpv  = tmp_visi;
     
    //convert to float
    windspeed       = tmps.toFloat();
    Beaufort          (windspeed);      // returs int beauf
    float windtemp  = tmpr.toFloat();
    int   windtemp2 = windtemp/10;
    winddirection   =  winddir[windtemp2];
    //text formatting    
    int splitTempi = tmp.indexOf    (".");
    Ftempi =         tmp.substring  (0, splitTempi+2);
    int humTempi =   hum.indexOf    (".");
    Fhumi =          hum.substring  (0, humTempi+3);
    int tmin     =   tmpmi.indexOf  (".");
    Fmin         =   tmpmi.substring(0,tmin+2);
    int tmax     =   tmpma.indexOf  (".");
    Fmax         =   tmpma.substring(0,tmax+2);

 } else {Serial.println("WiFi Disconnected");}  
}

void Beaufort(float wspeed){
  Serial.print  ("windspeed");
  Serial.println(wspeed);
  if (wspeed<  0.3)               {beauf= 0;}
  if (wspeed>= 0.3 && wspeed< 1.6){beauf= 1;}
  if (wspeed>= 1.6 && wspeed< 3.4){beauf= 2;}
  if (wspeed>= 3.4 && wspeed< 5.5){beauf= 3;}
  if (wspeed>= 5.5 && wspeed< 8.0){beauf= 4;}
  if (wspeed>= 8.0 && wspeed<10.8){beauf= 5;}
  if (wspeed>=10.8 && wspeed<13.9){beauf= 6;}
  if (wspeed>=13.9 && wspeed<17.2){beauf= 7;}
  if (wspeed>=17.2 && wspeed<20.8){beauf= 8;}
  if (wspeed>=20.8 && wspeed<24.5){beauf= 9;}
  if (wspeed>=24.5 && wspeed<28.5){beauf=10;}
  if (wspeed>=28.5 && wspeed<32.7){beauf=11;}
  if (wspeed> 32.7)               {beauf=12;}
  Serial.print  ("Beaufort");
  Serial.println(beauf);
}
