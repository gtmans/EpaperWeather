/*
 * Small weatherstation based on LILYGO T5 e-paper screen and weeronline.nl (dutch) weather api
 * multi version for 2.13 2.8 and 2.9 inch version
 * 
 * Mans Rademaker september 2022 feel free to use any way
 */

//select your screen
//#define LILYGO_T5_V213   
//#define LILYGO_T5_V28
#define LILYGO_T5_V29

#include    <GxEPD.h>
#if defined LILYGO_T5_V213
  #include  "boards.h"
  #include  <GxDEPG0213BN/GxDEPG0213BN.h>    // 2.13" b/w  form DKE GROUP
  #define   maxx 250
  #define   maxy 122
#elif       defined LILYGO_T5_V28
  #include  "boards.h"
  #include  <GxGDEW027W3/GxGDEW027W3.h>      // 2.7" b/w  264x176
  #define   maxx 264
  #define   maxy 176
#elif       defined LILYGO_T5_V29
  #include  <GxDEPG0290BS/GxDEPG0290BS.h>    // 2.9" b/w Waveshare variant, TTGO T5 V2.4.1 2.9"
  #define   maxx 296
  #define   maxy 128
#endif
#include    <SD.h>
#include    <FS.h>
#include    GxEPD_BitmapExamples
#include    <U8g2_for_Adafruit_GFX.h>
#include    <GxIO/GxIO_SPI/GxIO_SPI.h>
#include    <GxIO/GxIO.h>
#include    <WiFi.h>

int fonth;  //fontheigth
int fontw;  //fontwidth
int fontc;  //height of capital
int curx;
int cury;
int pos;
int bewolkt     = 64;//124 in all-font https://github.com/olikraus/u8g2/wiki/fntgrpiconic#open_iconic_weather_4x
int halfbewolkt = 65;
int regen       = 67;
int zon         = 69;//259
int drop        = 152;
int up          = 76;
int down        = 73;
int ok          = 120;
int nok         = 121;

#ifdef LILYGO_T5_V29
    GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16);   // arbitrary selection of 17, 16
    GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4);          // arbitrary selection of (16), 4
#else
    GxIO_Class io(SPI,  EPD_CS, EPD_DC,  EPD_RSET);
    GxEPD_Class display(io, EPD_RSET, EPD_BUSY);
#endif

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

//from https://randomnerdtutorials.com/esp32-timer-wake-up-deep-sleep/
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
int               sleepmins     = 15;
int               sleeptime     = sleepmins * 60; // 60 mins * 60 seconds

//from ESP32_Client_TTGO_8.4
bool        debug=true;
#define     rightbutton 39
#include    <ESP32Ping.h>
#include    <HTTPClient.h>

#include    "gewoon_secrets.h"  // or fill in your values below
//const     char* ssid     =  "myssid";        
//const     char* password =  "mypass";
//const     String KNMIkey =  "c123j5djk6"; // get your code on weeronline.nl
//String    town           =  "Apeldoorn"; 

#include    <ArduinoJson.h>   //https://github.com/bblanchon/ArduinoJson.git
#include    <NTPClient.h>     //https://github.com/taranais/NTPClient
#include    <WiFiUdp.h>

const String KNMI   = "http://weerlive.nl/api/json-data-10min.php?key="+KNMIkey+"&locatie="+town;
String payload= ""; //whole json 
String temp;        // actuele temperatuur
String gtemp;       // gevoelstemperatuur
String samenv;      // omschrijving weersgesteldheid
String lv;          // relatieve luchtvochtigheid
String windr;       // windrichting
String winds;       // windkracht (Beaufort)
String windrgr;     // windrichting (graden)
String luchtd;      // luchtdruk
String zicht;       // zicht in km
String verw;        // korte dagverwachting
String sup;         // zon op
String sunder;      // zon onder
String image;       // afbeeldingsnaam
String d0weer;      // Weericoon vandaag
String d0tmax;      // Maxtemp vandaag
String d0tmin;      // Mintemp vandaag
String d0windk;     // Windkracht vandaag   (Bft)
String d0windr;     // Windrichting vandaag
String d0windrgr;   // Windrichting vandaag (graden)
String d0neerslag;  // Neerslagkans vandaag (%)
String d0zon;       // Zonkans vandaag (%)
String d1weer;      // Weericoon morgen
String d1tmax;      // Maxtemp morgen
String d1tmin;      // Mintemp morgen
String d1windk;     // Windkracht morgen   (Bft)
String d1windr;     // Windrichting morgen
String d1windrgr;   // Windrichting morgen (graden)
String d1neerslag;  // Neerslagkans morgen (%)
String d1zon;       // Zonkans morgen (%)
String myalarm;     // Geldt er een weerwaarschuwing voor deze regio of niet? "1" of "0"
String myalarmtxt;  // Dde omschrijving van de weersituatie.
String formattedDate;
String dayStamp;
String LastdayStamp;
String TimeStamp;
String LastTimeStamp;
String monthStamp;
String daysStamp;
int    Tmonth;
int    Tdays;
bool   DST;
String windtxt[]={"stil","zeer zwak","zwak","vrij matig","matig","vrij krachtig","krachtig","hard","stormachtig","storm","zware storm","zeer zwarestorm","orkaan"};

StaticJsonDocument<2048> doc;
WiFiUDP ntpUDP;                  // Define NTP Client to get time
NTPClient timeClient(ntpUDP);    // Variables to save date and time

void setup(void)
{
  Serial.begin      (115200);
  while(!Serial);   // time to get serial running
  Serial.println    (__FILE__);//name of this doc
  pinMode(rightbutton,INPUT);
  unsigned long     sleepingtime  = (sleeptime * uS_TO_S_FACTOR);
  esp_sleep_enable_timer_wakeup (sleepingtime);
  Serial.println    ("setup");
  #if not defined   LILYGO_T5_V29
    SPI.begin       (EPD_SCLK, EPD_MISO, EPD_MOSI);    
  #endif
  display.init      (); // enable diagnostic output on Serial
   
  u8g2Fonts.begin               (display);             // 122x250pixels
  u8g2Fonts.setFontMode         (1);                   // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection    (1);                   // landscape
  u8g2Fonts.setForegroundColor  (GxEPD_BLACK);         // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor  (GxEPD_WHITE);         // apply Adafruit GFX color
  u8g2Fonts.setFont             (u8g2_font_VCR_OSD_tf);
  display.fillScreen            (GxEPD_WHITE);

  WiFi.begin             (ssid, password);
  Serial.println         ("Connecting");
  while (WiFi.status()   != WL_CONNECTED) {delay(500);}
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
  pos             = formattedDate.indexOf("T");
  dayStamp        = formattedDate.substring(0, pos);
  TimeStamp       = formattedDate.substring(pos+1, formattedDate.length()-1);  
  Serial.println(dayStamp);
  Serial.println(TimeStamp);

  //DST DUTCH TIMEZONE
  monthStamp      = dayStamp.substring(5,7);
  daysStamp       = dayStamp.substring(8,10);
  Tmonth          = monthStamp.toInt();
  Tdays           = daysStamp.toInt();
  int today = (Tmonth-1)*30 + Tdays;
  if  (today < ((2*30)+26) || today > ((9*30)+26)){DST=false;} else {DST=true;}
  Serial.print      ("vandaag is dagnr ");
  Serial.print      (today);
  if (DST) {Serial.println("DST");}else{Serial.println("WT");}
  if (DST){
    timeClient.setTimeOffset(7200);  
    while(!timeClient.update()) {timeClient.forceUpdate();}
    formattedDate  = timeClient.getFormattedDate();
    pos            = formattedDate.indexOf("T");
    TimeStamp      = formattedDate.substring(pos+1, formattedDate.length()-1);
   }
  getWeatherData();  

  display.fillScreen    (GxEPD_WHITE);   

#ifdef LILYGO_T5_V213
  u8g2Fonts.setCursor   ( 120,0);//falls off
  u8g2Fonts.print       ("0123456789012345678901234567890123456789012345");
  u8g2Fonts.setCursor   ( 100,0);
  u8g2Fonts.print       (dayStamp);
  u8g2Fonts.print       (" ");
  u8g2Fonts.print       (TimeStamp);
  u8g2Fonts.setCursor   ( 80,0);
  u8g2Fonts.print       ("Temp ");
  u8g2Fonts.print       (temp);
  u8g2Fonts.print       ("°C ");
  u8g2Fonts.print       (luchtd);
  u8g2Fonts.print       (" Mb.");
  u8g2Fonts.setCursor   ( 60,0);
  u8g2Fonts.print       ("Min. ");
  u8g2Fonts.print       (d0tmin);
  u8g2Fonts.print       ("°C Max.");
  u8g2Fonts.print       (d0tmax);
  u8g2Fonts.print       ("°C");
  u8g2Fonts.setCursor   ( 40,0);
  u8g2Fonts.print       ("Wind ");
  u8g2Fonts.print       (windr);
  u8g2Fonts.print       (winds);
  u8g2Fonts.print       (" ");
  u8g2Fonts.print       (windtxt[winds.toInt()]);
  u8g2Fonts.setCursor   ( 20,0);
  u8g2Fonts.print       ("Chill");
  u8g2Fonts.print       (gtemp);
  u8g2Fonts.print       ("°C");
  u8g2Fonts.print       (" hum.");
  u8g2Fonts.print       (lv);
  u8g2Fonts.print       ("%");
  u8g2Fonts.setCursor   ( 0,0);
  u8g2Fonts.print       ("Zicht");
  u8g2Fonts.print       (zicht);
  u8g2Fonts.print       ("km.");
#endif

#ifdef LILYGO_T5_V213
  u8g2Fonts.setFont     (u8g2_font_fub35_tf);//35  
  u8g2Fonts.setCursor   (80,0);
  u8g2Fonts.print       (temp);
  if (windr.length()==1){u8g2Fonts.print("°");
  u8g2Fonts.setFont     (u8g2_font_open_iconic_weather_4x_t);
  //http://weerlive.nl/delen.php 
  if (image=="buien"||image=="regen"||image=="hagel"){u8g2Fonts.print(char(regen));}
  if (image=="bewolkt"||image=="zwaarbewolkt")       {u8g2Fonts.print(char(bewolkt));}
  if (image=="lichtbewolkt"||image=="halfbewolkt")   {u8g2Fonts.print(char(halfbewolkt));}
  if (image=="zonnig")                               {u8g2Fonts.print(char(zon));}
  u8g2Fonts.setFont     (u8g2_font_fub35_tf);//35
  //  u8g2Fonts.print   ("°");
  if (windr.length()==1){u8g2Fonts.print(" ");}
  u8g2Fonts.print       (windr);
  u8g2Fonts.print       (winds);
  u8g2Fonts.setCursor   (52,0);
  u8g2Fonts.setFont     (u8g2_font_logisoso22_tf);//33x27
  u8g2Fonts.print       (samenv);
  u8g2Fonts.setCursor   (22,0);
  u8g2Fonts.print       ("zon ");
  u8g2Fonts.print       (d0zon);
  u8g2Fonts.print       ("% ");
  if (d0neerslag=="0"){
    u8g2Fonts.print   ("droog ");
  } else {
    u8g2Fonts.print   ("regen");
    u8g2Fonts.print   (d0neerslag);
    u8g2Fonts.print   ("% ");
  }
  u8g2Fonts.print       (d0tmax);
  u8g2Fonts.print       ("°max");
  u8g2Fonts.setFont     (u8g2_font_helvR14_tf);
  u8g2Fonts.setCursor   (0,0);
  u8g2Fonts.print       ("Updated:");
  u8g2Fonts.print       (dayStamp.substring(5,10));
  u8g2Fonts.print       (" ");
  u8g2Fonts.print       (TimeStamp);
#endif

#ifdef LILYGO_T5_V29
  u8g2Fonts.setFont     (u8g2_font_fub35_tf);fontw=51;fonth=65;fontc=35;  
  cury  =               (maxy-fontc-5);
  u8g2Fonts.setCursor   (cury,0);
  u8g2Fonts.print       (temp);
  u8g2Fonts.print       ("° ");
  u8g2Fonts.print       (windr);
  u8g2Fonts.print       (winds);
  u8g2Fonts.print       (" ");
  u8g2Fonts.setFont     (u8g2_font_open_iconic_weather_4x_t);
  if (image=="buien"||image=="regen"||image=="hagel"){u8g2Fonts.print(char(regen));}
  if (image=="bewolkt"||image=="zwaarbewolkt"||image=="wolkennacht   ")       {u8g2Fonts.print(char(bewolkt));}
  if (image=="lichtbewolkt"||image=="halfbewolkt")   {u8g2Fonts.print(char(halfbewolkt));}
  if (image=="zonnig")                               {u8g2Fonts.print(char(zon));}
  u8g2Fonts.setFont     (u8g2_font_logisoso18_tr);fontw=18;fonth=27;fontc=18;
  u8g2Fonts.print       (" ");
  u8g2Fonts.setFont     (u8g2_font_open_iconic_all_4x_t);fontw=32;fonth=32;fontc=32;
  pos      = random(1, 10);if (pos<6){u8g2Fonts.print(char(ok));}else{u8g2Fonts.print(char(nok));}//for future use
  u8g2Fonts.setFont     (u8g2_font_logisoso18_tr);fontw=18;fonth=27;fontc=18;
  cury  =               (cury-fontc-8);
  u8g2Fonts.setCursor   (cury,0+3);
  u8g2Fonts.print       (samenv);
  cury  =               (cury-fontc-5-2);  
  u8g2Fonts.setFont     (u8g2_font_open_iconic_weather_4x_t);fontw=32;fonth=32;fontc=32;
  u8g2Fonts.setCursor   (2+cury-(fontw/2),0); 
  u8g2Fonts.print       (char(zon));
  u8g2Fonts.setFont     (u8g2_font_logisoso20_tf);fontw=20;fonth=31;fontc=21;
  u8g2Fonts.print       (d0zon);
  u8g2Fonts.print       (" ");
  u8g2Fonts.setFont     (u8g2_font_open_iconic_weather_4x_t);fontw=32;fonth=32;fontc=32;
  u8g2Fonts.print       (char(regen));
  u8g2Fonts.setFont     (u8g2_font_logisoso20_tf);fontw=20;fonth=31;fontc=21;
  u8g2Fonts.print       (d0neerslag);
  u8g2Fonts.print       (" ");
  u8g2Fonts.setFont     (u8g2_font_open_iconic_all_4x_t);fontw=32;fonth=32;fontc=32;
  u8g2Fonts.print       (char(up));
  u8g2Fonts.setFont     (u8g2_font_logisoso20_tf);fontw=20;fonth=31;fontc=21;
  u8g2Fonts.print       (d0tmax);
  u8g2Fonts.print       (" ");  
  u8g2Fonts.setFont     (u8g2_font_open_iconic_all_4x_t);fontw=32;fonth=32;fontc=32;
  u8g2Fonts.print       (char(down));
  u8g2Fonts.setFont     (u8g2_font_logisoso20_tf);fontw=20;fonth=31;fontc=21;
  u8g2Fonts.print       (d0tmin);
  u8g2Fonts.setFont     (u8g2_font_helvR14_tf);
  u8g2Fonts.setCursor   (0,0);
  u8g2Fonts.print       ("bron:weerlive.nl om ");
  u8g2Fonts.print       (TimeStamp.substring(0,5));
#endif

#ifdef LILYGO_T5_V28 
  u8g2Fonts.setFont     (u8g2_font_maniac_tf);fontw=39;fonth=39;fontc=23;  
  cury  =               (maxy-fonth+5);
  u8g2Fonts.setCursor   (cury,0);
  u8g2Fonts.print       ("Apeldoorn  ");
  u8g2Fonts.setFont     (u8g2_font_open_iconic_weather_4x_t);
  if (image=="buien"||image=="regen"||image=="hagel"){u8g2Fonts.print(char(regen));}
  if (image=="bewolkt"||image=="zwaarbewolkt")       {u8g2Fonts.print(char(bewolkt));}
  if (image=="lichtbewolkt"||image=="halfbewolkt")   {u8g2Fonts.print(char(halfbewolkt));}
  if (image=="zonnig")                               {u8g2Fonts.print(char(zon));}
  
  u8g2Fonts.setFont     (u8g2_font_fub35_tf);fontw=51;fonth=65;fontc=35;  
  cury  =               (cury-fontc-10-5);
  u8g2Fonts.setCursor   (cury,0);
  u8g2Fonts.print       (temp);
  u8g2Fonts.print       ("°");
  u8g2Fonts.setCursor   (cury,u8g2Fonts.tx+(fontw/2));//half a space
  u8g2Fonts.print       (windr);
  u8g2Fonts.print       (winds);
  u8g2Fonts.setFont     (u8g2_font_logisoso18_tr);fontw=18;fonth=27;fontc=18;
  cury  =               (cury-fontc-10);
  u8g2Fonts.setCursor   (cury,0);
  u8g2Fonts.print       (samenv);
  cury  =               (cury-fontc-10);  
  u8g2Fonts.setFont     (u8g2_font_open_iconic_weather_4x_t);fontw=32;fonth=32;fontc=32;
  u8g2Fonts.setCursor   (cury-(fontw/2),0); 
  u8g2Fonts.print       (char(zon));
  u8g2Fonts.setCursor   (cury-(fontw/2),fontw*2.5); 
  u8g2Fonts.print       (char(regen));
  u8g2Fonts.setCursor   (cury-(fontw/2),fontw*5); 
  u8g2Fonts.setFont     (u8g2_font_open_iconic_all_4x_t);fontw=32;fonth=32;fontc=32;
  u8g2Fonts.print       (char(up)); 
  u8g2Fonts.setFont     (u8g2_font_logisoso20_tf);fontw=20;fonth=31;fontc=21;
  cury = cury - 10;
  u8g2Fonts.setCursor   (cury,32+2); 
  u8g2Fonts.print       (d0zon);
  u8g2Fonts.print       ("%");
  u8g2Fonts.setCursor   (cury,96+18); 
  u8g2Fonts.print       (d0neerslag);
  u8g2Fonts.print       ("%");
  u8g2Fonts.setCursor   (cury,164+32);
  u8g2Fonts.print       (d0tmax);
  u8g2Fonts.print       ("°c"); 
  u8g2Fonts.setFont     (u8g2_font_helvR14_tf);
  u8g2Fonts.setCursor   (0,0);
  u8g2Fonts.print       ("Updated:");
  u8g2Fonts.print       (dayStamp.substring(5,10));
  u8g2Fonts.print       (" ");
  u8g2Fonts.print       (TimeStamp);
#endif

  display.update();

}

void loop()
{
  Serial.println      ("Going to sleep for  " + String(sleeptime/60) + " Minutes");
  delay               (200);
  esp_deep_sleep_start();
}

void getWeatherData(){  
   if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status
    HTTPClient http; 
    http.begin(KNMI);           //Specify the URL for KNMI
    int httpCode = http.GET();  //Make the request 
    if (httpCode > 0) { //Check for the returning code 
         payload = http.getString();
         if (debug==true){
         Serial.println(httpCode);
         Serial.println();
         Serial.println(payload);}        
     } else { Serial.println("Error on HTTP request"); }
    http.end(); //Free the resources  
     
    /* KNMI example http://weerlive.nl/delen.php
     * { "liveweer": [{"plaats": "Apeldoorn", "temp": "20.0", "gtemp": "19.5", "samenv": "Geheel bewolkt", "lv": "63", 
     * "windr": "ZW", "windrgr": "225", "windms": "2", "winds": "2", "windk": "3.9", "windkmh": "7.2", "luchtd": "1016.2", 
     * "ldmmhg": "762", "dauwp": "13", "zicht": "35", "verw": "Wisselend bewolkt en enkele buien. Morgen zonnig", "sup": "05:15", 
     * "sunder": "22:03", "image": "bewolkt", "d0weer": "halfbewolkt", "d0tmax": "19", "d0tmin": "11", "d0windk": "2", 
     * "d0windknp": "6", "d0windms": "3", "d0windkmh": "11", "d0windr": "Z", "d0windrgr": "180", "d0neerslag": "17", 
     * "d0zon": "31", "d1weer": "zonnig", "d1tmax": "24", "d1tmin": "10", "d1windk": "2", "d1windknp": "6", "d1windms": "3", 
     * "d1windkmh": "11", "d1windr": "Z", "d1windrgr": "180", "d1neerslag": "10", "d1zon": "80", "d2weer": "halfbewolkt", 
     * "d2tmax": "26", "d2tmin": "14", "d2windk": "2", "d2windknp": "6", "d2windms": "3", "d2windkmh": "11", "d2windr": "Z", 
     * "d2windrgr": "180", "d2neerslag": "20", "d2zon": "60", "alarm": "1", 
     * "alarmtxt": "Er trekken buien over het land. In het zuidwesten en midden komen hierbij plaatselijk onweer, hagel en windstoten tot 60 km/uur voor. Hiervan kunnen verkeer en buitenactiviteiten hinder ondervinden  Bij Brouwershaven is een waterhoos waargenomen. Deze trekt naar het oost-noordoosten."}]}
    */
    
    char inp[2048];
    payload.toCharArray (inp,2048);
    deserializeJson     (doc,inp); 
    temp      = doc["liveweer"][0]["temp"].as<String>();//https://arduinojson.org/v5/faq/how-to-fix-error-ambiguous-overload-for-operator/
    gtemp     = doc["liveweer"][0]["gtemp"].as<String>();
    samenv    = doc["liveweer"][0]["samenv"].as<String>();
    lv        = doc["liveweer"][0]["lv"].as<String>();  
    windr     = doc["liveweer"][0]["windr"].as<String>();
    winds     = doc["liveweer"][0]["winds"].as<String>();
    windrgr   = doc["liveweer"][0]["windrgr"].as<String>();
    luchtd    = doc["liveweer"][0]["luchtd"].as<String>();
    zicht     = doc["liveweer"][0]["zicht"].as<String>();
    verw      = doc["liveweer"][0]["verw"].as<String>();    
    sup       = doc["liveweer"][0]["sup"].as<String>();
    sunder    = doc["liveweer"][0]["sunder"].as<String>();
    image     = doc["liveweer"][0]["image"].as<String>();
    d0weer    = doc["liveweer"][0]["d0weer"].as<String>();
    d0tmax    = doc["liveweer"][0]["d0tmax"].as<String>();
    d0tmin    = doc["liveweer"][0]["d0tmin"].as<String>();
    d0windk   = doc["liveweer"][0]["d0windk"].as<String>();
    d0windr   = doc["liveweer"][0]["d0windr"].as<String>();
    d0windrgr = doc["liveweer"][0]["d0windrgr"].as<String>();
    d0neerslag= doc["liveweer"][0]["d0neerslag"].as<String>();
    d0zon     = doc["liveweer"][0]["d0zon"].as<String>();
    d1weer    = doc["liveweer"][0]["d1weer"].as<String>();
    d1tmax    = doc["liveweer"][0]["d1tmax"].as<String>();
    d1tmin    = doc["liveweer"][0]["d1tmin"].as<String>();
    d1windk   = doc["liveweer"][0]["d1windk"].as<String>();
    d1windr   = doc["liveweer"][0]["d1windr"].as<String>();
    d1neerslag= doc["liveweer"][0]["d1neerslag"].as<String>();
    d1zon     = doc["liveweer"][0]["d1zon"].as<String>();
    myalarm   = doc["liveweer"][0]["alarm"].as<String>();
    myalarmtxt= doc["liveweer"][0]["alarmtxt"].as<String>();
    if (windr=="NNO")   {windr="N";}
    if (windr=="ONO")   {windr="O";}
    if (windr=="OZO")   {windr="O";}
    if (windr=="ZZO")   {windr="Z";}
    if (windr=="ZZW")   {windr="Z";}
    if (windr=="WZW")   {windr="W";}
    if (windr=="WNW")   {windr="W";}
    if (windr=="NNW")   {windr="N";}
    if (windr=="Noord") {windr="N";}
    if (windr=="Oost")  {windr="O";}
    if (windr=="Zuid")  {windr="Z";}
    if (windr=="West")  {windr="W";}
    //TEMPCONV decimal to int rounded
    // temp="25.3"->25   25.5->26??
    float x = temp.toFloat();
    int   y = round(x); 
    Serial.print  (temp);
    Serial.print  ("->");
    Serial.println(y);
    temp = String(y);
 } else {Serial.println("WiFi Disconnected");}  
}
