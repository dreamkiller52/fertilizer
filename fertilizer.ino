// Import required libraries
//#include <DS1307RTC.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include <Ticker.h>  // From https://github.com/espressif/arduino-esp32/tree/master/libraries/Ticker
#include <EEPROM.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <NtpClientLib.h>
#include "ESP32_MailClient.h"
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>



#define EEPROM_SIZE 512

////////////Email notification
#define emailSenderAccount    "xxxxxxxxxxxxxxxx"    
#define emailSenderPassword   "xxxxxxxxxxxxxxxxxx"
#define emailRecipient        "xxxxxxxxxxxxxx"
#define smtpServer            "smtp.gmail.com"
#define smtpServerPort        465
#define emailSubject          "pump delivery"

SMTPData smtpData;
///////////////////////////////

///////////////telegram
WiFiClientSecure client_id;
#define BOTtoken "xxxxxxxxxxxxxxxxxxxx"
const String id_user="xxxxxxxxxxxxxxx";
UniversalTelegramBot bot(BOTtoken, client_id);
////////////////////////

////////////////////////wifi
const char* ssid = "xxxxxxxxxxxxxx";
const char* password = "xxxxxxxxxxxxxxxx";
///////////////////////////////////////////////////


Ticker OneSecondWaiting;
Ticker PumpTimer;

float calibPump1=0;
int adresse1=0;
int next_adresse=0;
bool debug_actif=false;
bool log_actif=true;

time_t getNtpTime();

// The Email Sending data object contains config and data to send



// Set LED GPIO
const int ledPin=2;
// Stores LED state
String ledState;

const int ENB=27;
int IN3 = 12; //gpio 12;
int IN4 = 14; //gpi0 14;

const int nombretotalpompe=3;

/*** Debut Fichier ***/
struct PumpSettings {
  unsigned long magic;
  byte struct_version;
  bool is_active;
  String pump_name;
  float calib_pump; // Depuis la struct_version = 1
  bool per_day;
  int nbPerDay;
  float dose;
  int bottle_size;
  String prog_hour;
  String prog_min;
  timeDayOfWeek_t week_day;
  String daily_hour_stop;
  String daily_min_stop;
  float delivery;
  AlarmID_t alarmId;

};


static const unsigned long STRUCT_MAGIC = 1587000013;
static const byte STRUCT_VERSION = 7;
//PumpSettings pump1;

PumpSettings pump[nombretotalpompe];

// Callback function to get the Email sending status
void sendCallback(SendStatus info);

void sendCallback(SendStatus msg) {
  // Print the current status
  Serial.println(msg.info());

  // Do something when complete
  if (msg.success()) {
    Serial.println("----------------");
  }
}

void set_alarm(int nb_pump)
{
  if(pump[nb_pump].is_active)
  {
    if (pump[nb_pump].per_day)
    {
      if(pump[nb_pump].nbPerDay==1)
      {
        int heure=pump[nb_pump].prog_hour.toInt();
        int minut=pump[nb_pump].prog_min.toInt();
        pump[nb_pump].alarmId=Alarm.alarmRepeat(heure, minut, 0, alarm_delivery);
        saveEEPROM(nb_pump);
      }
      else
      {
        int heure_deb=pump[nb_pump].prog_hour.toInt();
        int min_deb=pump[nb_pump].prog_min.toInt();
        int heure_fin=pump[nb_pump].daily_hour_stop.toInt();
        int min_fin=pump[nb_pump].daily_min_stop.toInt();
        int timer_daily=(heure_fin*3600+min_fin*60)-(heure_deb*3600+min_deb*60)/pump[nb_pump].nbPerDay;
        pump[nb_pump].alarmId=Alarm.alarmRepeat(pump[nb_pump].week_day, heure, minut, 0, alarm_delivery);
        saveEEPROM(nb_pump);
      }
    }
    else
    {
      int heure=pump[nb_pump].prog_hour.toInt();
      int minut=pump[nb_pump].prog_min.toInt();
      pump[nb_pump].alarmId=Alarm.alarmRepeat(pump[nb_pump].week_day, heure, minut, 0, alarm_delivery);
      saveEEPROM(nb_pump);
    }    
  }
  else
  {
    pump[nb_pump].alarmId=dtINVALID_ALARM_ID;
  }
}

void notify(int numpump)
{
  time_t t = now();
  String heure=String(hour(t))+":"+String(minute(t))+":"+String(second(t));
  String message="Engrais "+pump[numpump].pump_name+" :dose de "+String(pump[numpump].dose)+"mL delivrée avec succés";
  Serial.println("Message:");
  Serial.println(message);
  bot.sendMessage(id_user, message, "");
  //sendmail_delivery(message);
   Serial.println("Fin d'envoi du message!!!!");
   if(log_actif)
   {
     String message_log=heure+":"+message;
     write_log(message_log);
   }
   
}

void sendmail_delivery(String message)
{
  Serial.println("Debut fonction envoi mail");
  smtpData.setLogin(smtpServer, smtpServerPort, emailSenderAccount, emailSenderPassword);
  
  // Set the sender name and Email
  smtpData.setSender("Pump nano 60L", emailSenderAccount);

  // Set Email priority or importance High, Normal, Low or 1 to 5 (1 is highest)
  smtpData.setPriority("High");

  // Set the subject
  smtpData.setSubject(emailSubject);

  // Set the message with HTML format
  //smtpData.setMessage("<div style=\"color:#2f4468;\"><h1>Nano 60L delivery system</h1><p>-"+message+"</p></div>", true);
  // Set the email message in text format (raw)
  smtpData.setMessage("Hello World! - Sent from ESP32 board", false);

  // Add recipients, you can add more than one recipient
  smtpData.addRecipient(emailRecipient);
  //smtpData.addRecipient("YOUR_OTHER_RECIPIENT_EMAIL_ADDRESS@EXAMPLE.com");

  smtpData.setSendCallback(sendCallback);

  //Start sending Email, can be set callback function to track the status
  if (!MailClient.sendMail(smtpData))
    Serial.println("Error sending Email, " + MailClient.smtpErrorReason());

  //Clear all data from Email object to free memory
  smtpData.empty();
}

void reset_pump(int nbpump)
{
   Serial.println("!!!! Reinitilizing pump value !!!!");
    // Valeurs par défaut pour les variables de la version 0
    pump[nbpump].is_active=false;
    pump[nbpump].pump_name="";
    pump[nbpump].calib_pump=999;
    pump[nbpump].per_day = false;
    pump[nbpump].nbPerDay=1;
    pump[nbpump].dose=0;
    pump[nbpump].bottle_size=0;
    pump[nbpump].delivery=0;
    pump[nbpump].prog_hour="12";
    pump[nbpump].prog_min="30";
    pump[nbpump].week_day=dowMonday;  //lundi
    pump[nbpump].daily_hour_stop="18";
    pump[nbpump].daily_min_stop="30";
    pump[nbpump].struct_version=STRUCT_VERSION;
    pump[nbpump].alarmId=dtINVALID_ALARM_ID;
  
  //DebugSettingPump("In LoadEEPROM Before Save");

  // Sauvegarde les nouvelles données
  saveEEPROM(nbpump);
  
}

void refill_pump(int nbpump)
{
   Serial.println("!!!! Refill pump!!!!");
    // Valeurs par défaut pour les variables de la version 0
    pump[nbpump].delivery=0;
     //DebugSettingPump("In LoadEEPROM Before Save");

  // Sauvegarde les nouvelles données
  saveEEPROM(nbpump);
}

void write_log(String logline)
{
    File file = SPIFFS.open("/log.txt", FILE_WRITE);
 
    if (!file) {
      Serial.println("There was an error opening the file for writing");
      return;
    }

    if (file.print(logline)) {
      Serial.println("File was written");
    } else {
      Serial.println("File write failed");
    }
 
  file.close();
  
}

void loadEEPROM(int num_pump) {
 int address=0;
 for (int i = 0; i<num_pump;i++)
 {
  address+=sizeof(pump[i]);
 }
  
 //DebugSettingPump("In LoadEEPROM at beginning");
  // Lit la mémoire EEPROM
  EEPROM.get(address, pump[num_pump]);
  //DebugSettingPump("After get ");
  // Détection d'une mémoire non initialisée
  byte erreur = pump[num_pump].magic != STRUCT_MAGIC;
  byte change_version  = pump[num_pump].struct_version != STRUCT_VERSION;
  // Valeurs par défaut struct_version == 0
  if ((erreur) || (change_version))  {
  Serial.println("!!!! Reinitilizing pump value !!!!");
    // Valeurs par défaut pour les variables de la version 0
    pump[num_pump].is_active=false;
    pump[num_pump].pump_name="";
    pump[num_pump].calib_pump=999;
    pump[num_pump].per_day = false;
    pump[num_pump].nbPerDay=1;
    pump[num_pump].dose=0;
    pump[num_pump].bottle_size=0;
    pump[num_pump].delivery=0;
    pump[num_pump].prog_hour="12";
    pump[num_pump].prog_min="30";
    pump[num_pump].week_day=dowMonday;  //lundi
    pump[num_pump].daily_hour_stop="18";
    pump[num_pump].daily_min_stop="30";
    pump[num_pump].struct_version=STRUCT_VERSION;
    pump[num_pump].alarmId=dtINVALID_ALARM_ID;
  }
  //DebugSettingPump("In LoadEEPROM Before Save");

  // Sauvegarde les nouvelles données
  saveEEPROM(num_pump);
  
}

/** Sauvegarde en mémoire EEPROM le contenu actuel de la structure */
void saveEEPROM(int num_pump) {
  int address=0;
 for (int i = 0; i<num_pump;i++)
 {
  address+=sizeof(pump[i]);
 }
   Serial.println("adresse memoire:");
   Serial.println(address);

  // Met à jour le nombre magic et le numéro de version avant l'écriture
  pump[num_pump].magic = STRUCT_MAGIC;
  pump[num_pump].struct_version =  STRUCT_VERSION;
  EEPROM.put(address, pump[num_pump]);
  bool return_commit=EEPROM.commit();
  //Serial.println("return commit=");
  //Serial.println(return_commit);

  DebugSettingPump("In SaveEEPROM After Save",1);
}

void alarm_delivery()
{
  AlarmID_t alarmid=Alarm.getTriggeredAlarmId();
  Serial.println("Alarme declenché:");
  Serial.println(alarmid);
    for (int i = 0; i<nombretotalpompe;i++){
        if (pump[i].alarmId == alarmid)
        {
          Serial.println("Alarme trouve");
          delivery(i);
        }
    }
}

void delivery(int num_pump) {
  
  
  if(!pump[num_pump].per_day)
  {
      Serial.println("Lancement dose pour la pompe:");
      Serial.println(num_pump);
      int deliv_time=round(pump[num_pump].dose/(pump[num_pump].calib_pump/60));
      Serial.println("duree:");
      Serial.println(deliv_time);
      pump_on(num_pump);
      PumpTimer.once(deliv_time, end_delivery, num_pump);
  }
  else if (pump[num_pump].nbPerDay ==1)
  {
      Serial.println("Lancement journalier unitaire pour la pompe:");
      Serial.println(num_pump);
      int deliv_time=round(pump[num_pump].dose/(pump[num_pump].calib_pump/60));
      Serial.println("duree:");
      Serial.println(deliv_time);
      Serial.println("dose reel:");
      Serial.println((pump[num_pump].calib_pump/60)*deliv_time);
      pump_on(num_pump);
      PumpTimer.once(deliv_time, end_delivery, num_pump);
  }
  else
  {
    
  }
}


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//const char* PARAM_PUMP1 = "calib_pump1";

// Replaces placeholder with LED state value
String processor(const String& var){
  Serial.println(var);
  if(var == "STATE"){
    if(digitalRead(ledPin)){
      ledState = "ON";
    }
    else{
      ledState = "OFF";
    }
    //
    Serial.print(ledState);
    return ledState;
  }

  if (var== "STATUS_LED1"){
    if(digitalRead(ledPin)){
      ledState = "checked";
    }
    else{
      ledState = "";
    }
    return ledState;
  }

   if(var.indexOf("STATUS_PUMP") >= 0){
      int num_pump=var.substring(var.length()-1).toInt()-1;
      String status_pump;
      loadEEPROM(num_pump);
      if(pump[num_pump].is_active){
        status_pump = "true";
      }
      else{
        status_pump = "false";
      }
    return status_pump;
  }

  if(var.indexOf("NAME_PUMP")>= 0){
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
    return pump[num_pump].pump_name;
  }

   if(var.indexOf("BOTTLE_CAPACITY")>= 0){
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
    return String(pump[num_pump].bottle_size);
  }

   if(var.indexOf("DELIVERY")>= 0){
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
    return String(pump[num_pump].delivery);
  }

    if(var.indexOf("WEEKLY_DAY")>= 0){
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);

    String strWeekday;
        
        if (pump[num_pump].week_day == dowMonday)
        {
          strWeekday="dowMonday";
        }
        else if (pump[num_pump].week_day == dowTuesday)
        {
          strWeekday="dowTuesday";
        }
        else if (pump[num_pump].week_day == dowWednesday)
        {
          strWeekday="dowWednesday";
        } 
        else if (pump[num_pump].week_day == dowThursday)
        {
          strWeekday="dowThursday";
        }
        else if (pump[num_pump].week_day == dowFriday)
        {
          strWeekday="dowFriday";
        }
        else if (pump[num_pump].week_day == dowSaturday)
        {
          strWeekday="dowSaturday";
        }
        else if (pump[num_pump].week_day == dowSunday)
        {
          strWeekday="dowSunday";
        }

    return strWeekday;
  }

    if(var.indexOf("PROG_HOUR")>= 0){
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);

    return pump[num_pump].prog_hour;
  }

   if(var.indexOf("PROG_MIN")>= 0){
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
    return pump[num_pump].prog_min;
  }

  if(var.indexOf("PER_DAY")>= 0){
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
    String type_prog;
    if (pump[num_pump].per_day)
    {
      type_prog="jour";
    }
    else
    {
      type_prog="hebdo";
    }
    return type_prog;
  }

  if(var.indexOf("DOSE_DAY")>= 0)
  {
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
    return String(pump[num_pump].nbPerDay);
  }

    if(var.indexOf("DOSE")>= 0)
  {
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
    return String(pump[num_pump].dose);
  }
  
  if(var.indexOf("calibPump")>= 0){
    //DebugSettingPump("before load EEPROM in processor");
    int num_pump=var.substring(var.length()-1).toInt()-1;
    loadEEPROM(num_pump);
   // DebugSettingPump("after load EEPROM in processor");

    if(pump[num_pump].calib_pump == 999) {
       return "Non calibree !!!!";
    }

    return String(pump[num_pump].calib_pump);
  }

  return String();
}

void DebugSettingPump(String where,int num_pump) {
  if(debug_actif)
  {
    Serial.println("");
    Serial.println(where);
    Serial.println(pump[num_pump].magic);
    Serial.println(pump[num_pump].struct_version);
    Serial.println(pump[num_pump].is_active);
    Serial.println(pump[num_pump].pump_name);
    Serial.println(pump[num_pump].calib_pump);
    Serial.println(pump[num_pump].per_day);
    Serial.println(pump[num_pump].nbPerDay);
    Serial.println(pump[num_pump].dose);
    Serial.println(pump[num_pump].bottle_size);
    Serial.println(pump[num_pump].delivery);
    Serial.println(pump[num_pump].delivery);
    Serial.println("");
  }

}
 
void setup(){
   
  // Serial port for debugging purposes
  Serial.begin(115200);


  
  
  EEPROM.begin(EEPROM_SIZE);
  ledcAttachPin(ENB, 0);
  ledcSetup(0, 30000, 8); // 12 kHz PWM, 8-bit resolution
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());


  NTP.begin("fr.pool.ntp.org", 1, true);
  NTP.setInterval (60, 86400);    //Initial NTP call repeat 1 min, then 24h between calls
  delay(1500);
  time_t t = now();
  
  String heure=String(hour(t))+":"+String(minute(t))+":"+String(second(t));
  Serial.print(heure);

  for (int i = 0; i<nombretotalpompe;i++){
     loadEEPROM(i);
   set_alarm(i);
  }
  
  
//DebugSettingPump("Before load EEPROM in setup",1);
  //loadEEPROM(1);
  //DebugSettingPump("After load EEPROM in setup",1);

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });

  server.on("/justgage.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/justgage.js", "text/javascript");
  });

  server.on("/raphael-2.1.4.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/raphael-2.1.4.min.js", "text/javascript");
  });

    // Route calib.html
  server.on("/calib.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/calib.html", String(), false, processor);
  });



  //Route to calibration
  server.on("/calib_pump", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("pump"))
    {
       calibration(request->getParam("pump")->value().toInt()-1);
    }
    request->send(SPIFFS, "/calib.html", String(), false, processor);
  });

   
    server.on("/gest_pump_1.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/gest_pump_1.html", String(), false, processor);
  });

      server.on("/gest_pump_2.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/gest_pump_2.html", String(), false, processor);
  });

      server.on("/gest_pump_3.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/gest_pump_3.html", String(), false, processor);
  });
    server.on("/status.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/status.html", String(), false, processor);
  });

  /* // Route to set GPIO to HIGH
  server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, HIGH);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to set GPIO to LOW
 server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request){
    digitalWrite(ledPin, LOW);    
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });*/

  server.on("/refill", HTTP_GET, [] (AsyncWebServerRequest *request) {
     Serial.println(request->url());
     int nb_pump;
  
    if(request->hasParam("pump"))
    {

       nb_pump=request->getParam("pump")->value().toInt()-1;
        Serial.println("Refill pompe:");
        Serial.println(nb_pump);
       refill_pump(nb_pump);
    }
    //request->send(200, "text/text", "status");
  });

    server.on("/reset", HTTP_GET, [] (AsyncWebServerRequest *request) {
     Serial.println(request->url());
     int nb_pump;
  
    if(request->hasParam("pump"))
    {

       nb_pump=request->getParam("pump")->value().toInt()-1;
        Serial.println("Reset pour la pompe:");
        Serial.println(nb_pump);
       reset_pump(nb_pump);
    }
    request->send(200, "text/text", "reset");
  });

  server.on("/post", HTTP_GET, [] (AsyncWebServerRequest *request) {
  
  Serial.println(request->url());
  
    String calib_pump;
     int nb_pump;
  
    if(request->hasParam("pump"))
    {
       nb_pump=request->getParam("pump")->value().toInt()-1;
       
       // GET inputString value on <ESP_IP>/post?calib_pump1=<value>
      if (request->hasParam("calib_pump")){
      
        calib_pump = request->getParam("calib_pump")->value();
       DebugSettingPump("/post Before cahnge value",nb_pump);

       pump[nb_pump].calib_pump = calib_pump.toFloat();
       DebugSettingPump("/post after change value",nb_pump);

       saveEEPROM(nb_pump);
       DebugSettingPump("After save EEPROM value",nb_pump);
      }
      else {
       calib_pump = "No message sent";
      }

      if (request->hasParam("name_pump")) {
        pump[nb_pump].pump_name = request->getParam("name_pump")->value();
        Serial.println("name_pump:");
        Serial.println(pump[nb_pump].pump_name);
        saveEEPROM(nb_pump);
      }

      if (request->hasParam("type_prog")) {
        if(request->getParam("type_prog")->value() == "jour")
        {
          pump[nb_pump].per_day=true;
        }
        else
        {
          pump[nb_pump].per_day=false;
        }
        saveEEPROM(nb_pump);
      }

      if (request->hasParam("dose_day")) {
        pump[nb_pump].nbPerDay = request->getParam("dose_day")->value().toInt() ;
        Serial.println("nbPerDay:");
        Serial.println(pump[nb_pump].nbPerDay);
        saveEEPROM(nb_pump);
      }

      if (request->hasParam("daylist")) {
        
        String strWeekday=request->getParam("daylist")->value();
        
        if (strWeekday == "dowMonday")
        {
          pump[nb_pump].week_day=dowMonday;
        }
        else if (strWeekday == "dowTuesday")
        {
          pump[nb_pump].week_day=dowTuesday;
        }
        else if (strWeekday == "dowWednesday")
        {
          pump[nb_pump].week_day=dowWednesday;
        } 
        else if (strWeekday == "dowThursday")
        {
          pump[nb_pump].week_day=dowThursday;
        }
        else if (strWeekday == "dowFriday")
        {
          pump[nb_pump].week_day=dowFriday;
        }
        else if (strWeekday == "dowSaturday")
        {
          pump[nb_pump].week_day=dowSaturday;
        }
        else if (strWeekday == "dowSunday")
        {
          pump[nb_pump].week_day=dowSunday;
        }

        
        Serial.println("week_day:");
        Serial.println(pump[nb_pump].week_day);
        saveEEPROM(nb_pump);
      }


      if (request->hasParam("prog-hour")) {
        pump[nb_pump].prog_hour = request->getParam("prog-hour")->value();
        Serial.println("prog_hour:");
        Serial.println(pump[nb_pump].prog_hour);
        saveEEPROM(nb_pump);
      }

       if (request->hasParam("prog-min")) {
        pump[nb_pump].prog_min = request->getParam("prog-min")->value();
        Serial.println("prog_min:");
        Serial.println(pump[nb_pump].prog_min);
        saveEEPROM(nb_pump);
      }

      if (request->hasParam("bottle_capacity")) {
        pump[nb_pump].bottle_size = request->getParam("bottle_capacity")->value().toInt();
        Serial.println("bottle_capacity:");
        Serial.println(pump[nb_pump].bottle_size);
        saveEEPROM(nb_pump);
      }
      
      if (request->hasParam("dose_value")) {
         pump[nb_pump].dose = request->getParam("dose_value")->value().toFloat();
        Serial.println("dose:");
        Serial.println(pump[nb_pump].dose);
        saveEEPROM(nb_pump);
      }

      if (request->hasParam("state"))
      {
        String state_pump=request->getParam("state")->value();
        if(state_pump=="1"){
          pump[nb_pump].is_active=true;
        }
        else{
          pump[nb_pump].is_active=false;
          Alarm.disable(pump[nb_pump].alarmId);
          
        }
        saveEEPROM(nb_pump);
      }

      set_alarm(nb_pump);
  }

     
    
    if (request->hasParam("led")&& request->hasParam("state"))
    {
      String status_led=request->getParam("state")->value();
      String num_pump=request->getParam("led")->value();
      if(status_led=="1"){
        pump_on(num_pump.toInt());
      }
      else{
        pump_off(num_pump.toInt());
      }
    }
    
    //loadEEPROM();
    //DebugSettingPump("after load EEPROM in /post");
    
    request->send(200, "text/text", "post");
    
 
  });

  // Start server
  server.begin();
}


String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path, "r");
  if(!file || file.isDirectory()){
    Serial.println("- empty file or failed to open file");
    return String();
  }
  Serial.println("- read from file:");
  String fileContent;
  while(file.available()){
    fileContent += String((char)file.read());
  }
  Serial.println(fileContent);
  return fileContent;
}

void calibration(int num_pump) {
    Serial.printf("Debut Calibration:");
    OneSecondWaiting.once(10, calibrate_pump, num_pump);
}

void calibrate_pump(int numPump) {
    Serial.printf("One Second passed");
    Serial.printf("Calibration pour la pompe:",String(numPump));

    pump_on(numPump);
    PumpTimer.once(60, pump_off, numPump);
}

int pump_state(int numPump) {
  return digitalRead(ledPin);
}

void pump_off(int numpump) {
  Serial.printf("stop Pump:",String(numpump)); 
  
  // now turn off motors
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  
}

void end_delivery(int numpump) {
  Serial.printf("stop Pump:",String(numpump)); 
  Serial.println("End send");
  // now turn off motors
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  
  pump[numpump].delivery+=pump[numpump].dose;
  saveEEPROM(numpump);
  Serial.println("Save eeprom ok");
  notify(numpump);
}
      
void pump_on(int numpump) {
  Serial.printf("start Pump:",String(numpump));
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  ledcWrite(0,256);
}

void loop(){
  yield();
  Alarm.delay(1000);
}
