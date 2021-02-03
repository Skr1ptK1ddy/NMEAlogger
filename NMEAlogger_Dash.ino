
// Projekt NMEA Logger
// Lino Liebegott



//How to upload an image (200KB) from SDcard to FTP server
//https://forum.arduino.cc/index.php?topic=376911.0
// https://github.com/rfetick/MPU6050_light
#include "Wire.h"
#include <MPU6050_light.h>

#include <SD.h>
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <ESPDash.h>

// Read CPU Temp
#ifdef __cplusplus
  extern "C" {
#endif
  uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

//=====================================


MPU6050 mpu(Wire);
long timer = 0;

AsyncWebServer server(80); /* Start Webserver */
ESPDash dashboard(&server);/* Attach ESP-DASH to AsyncWebServer */

/* 
  Dashboard Cards 
  Format - (Dashboard Instance, Card Type, Card Name, Card Symbol(optional) )
*/
Card humidity(&dashboard, HUMIDITY_CARD, "Boat Speed", "kts");
Card tws(&dashboard, GENERIC_CARD, "TWS","kts");
Card twa(&dashboard, GENERIC_CARD, "TWA","°");
Card aws(&dashboard, GENERIC_CARD, "AWS","kts");
Card awa(&dashboard, GENERIC_CARD, "AWA","°");
Card sog(&dashboard, GENERIC_CARD, "SOG","kts");
Card cog(&dashboard, GENERIC_CARD, "COG","°");
Chart power(&dashboard, BAR_CHART, "TWS history (kts)");
Card nmeaStatus(&dashboard, STATUS_CARD, "NMEA Status", "danger");
Card temperature(&dashboard, TEMPERATURE_CARD, "CPU Temperature", "°C");


// Bar Chart Data
String XAxis[] = {"-30 min","-25 min", "-20 min", "-15 min", "-10 min", "-5 min", "-0 min"};
int YAxis[] = {0, 0, 0, 0, 0, 0, 0};
int temp = 0;
// Bar Chart Instance


/* GSM Modul comunikation  */
byte RESET_PIN = 4; // SIM800 Restset pin
HardwareSerial SIM800(2);


/* WiFi Cofig */
const char* ssid = "Esprit Dashboard"; // SSID
const char* password = ""; // Password
unsigned long ulUpLastTime =0;

//=================================
#define SAVEPERIOD 2000 // ms
#define UPDATEPERIOD 1000//ms
#define GYROPERIOD 500//ms

//--DB--
sqlite3 *db;
sqlite3_stmt *res;
const char *tail;


#define bufferLen 14

const int chipSelect = 5;
const int nmeaLen = 15;
unsigned long lastTime = 0;
unsigned long ulGyroLastTime = 0;
int inByte, start=0;
String nmea = "NMEA Logger by Lino Liebegott";
String nmeaSplit[nmeaLen] ;
//Time: hhmmss.ss / Date: YYYYMMDD / Pos. N= + S= - E= + W= - / Wind: m/s

String nmeaBuffer[bufferLen];
//Date,Time,TimeZone,LAT,LON,DPT,SOG,COG,HDG,STW,TWS,TWA,AWS,AWA
//| 0 | 1  |    2   | 3 | 4 | 5 | 6 | 7 | 8 | 9 |10 |11 |12 |13 |

//https://arduino.programmingpedia.net/de/tutorial/4852/zeiteinteilung


void setup() {
 /* Start Access Point */
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  Serial.begin(4800); // 4800 Baud Dateneingang und Konsole
  SIM800.begin(9600, SERIAL_8N1, 16, 17);//init GSM Modul comunikation GPIO 16,17
  
  
  /* Start AsyncWebServer */
  server.begin();
  Serial.println(nmea);
  /* Init MPU6050 */
  Wire.begin();
  byte status = mpu.begin();
  Serial.print(F("MPU6050 status: "));
  Serial.println(status);
  Serial.println(F("Calculating offsets, do not move MPU6050"));
  delay(1000);
  mpu.calcOffsets(true,true); // gyro and accelero
  Serial.println("Done!\n");
 
  
  
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);
  
  Serial.println("Date,Time,TimeZone,LAT,LOG,DPT,SOG,COG,HDG,SOW,TWS,TWA,AWS,AWA");
 //pinMode(LED_BUILTIN, OUTPUT); // digitalWrite(LED_BUILTIN, HIGH);  
  pinMode(10, OUTPUT);// pin I2C
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Karte nicht vorhanden");
    return;
  }
  Serial.println("SD Karte OK.");
sqlite3_initialize();
 
}

void loop() {
  unsigned long now = millis();
  temp = (temprature_sens_read() - 32) / 1.8; // CPU Temp
  mpu.update();


  
  do {
   if (Serial.available()) { // Sind Daten im Eingangspuffer
   
    inByte = Serial.read(); // dan lesen
    if ((start==0) && (inByte == '$')) {start=1; nmea="";} // bei $ starten
    if(start==1) {nmea.concat((char)inByte);} // das Zeichen anhängen
    if((inByte==13) && (start==1)) { // CR > Datensatzende > NMEA ausgeben
      start=0; 
      if (getCheckSum(nmea)){
        schreiben();}
      }
    }}while(start!= 0); 



/// Wird mit der SAVEPERIOD Ausgeführt ///
 if (now - lastTime >= SAVEPERIOD)
  {  
    lastTime = now;
  
  nmea="";
  for(unsigned int i=0; i<bufferLen;i++) {
  nmea.concat(nmeaBuffer[i]+",");
  nmeaBuffer[i] = "";}
  
  Serial.println(nmea);
  schreibeSD();

 int db_open = open_database("/sd/nmea_contributors.db", &db);
   if (db_open != SQLITE_OK)
   {
    Serial.println("open Database faild");
      return;
   }
   create_table();
   insert_data_set("programmers", 777, "New_Set", 0);
   sqlite3_close(db);


  
}
/// Wird mit der UPDATEPERIOD Ausgeführt ///
if (now - ulUpLastTime >= UPDATEPERIOD)
  {  
    ulUpLastTime = now;
/* Update Card Values */
  
  temperature.update(temp);
  humidity.update(nmeaBuffer[9].toFloat());
  tws.update(nmeaBuffer[10].toFloat());
  twa.update(nmeaBuffer[11].toFloat());
  aws.update(nmeaBuffer[12].toFloat());
  awa.update(nmeaBuffer[13].toFloat());
  sog.update(nmeaBuffer[6].toFloat());
  cog.update(nmeaBuffer[7].toFloat());
  nmeaStatus.update("I.O.","success");

 // Randomize YAxis Values ( for demonstration purposes only )
  for(int i=0; i < 7; i++){
    YAxis[i] = (int)random(0, 30);
  }

  /* Update Chart  Axis (axis_array, array_size) */
  power.updateX(XAxis, 7);
  power.updateY(YAxis, 7);

  /* Send Updates to Dashboard (realtime) */
  dashboard.sendUpdates();



  }

/// Wird mit der GYROPERIOD Ausgeführt ///
if (now - ulGyroLastTime >= GYROPERIOD)
  {  ulGyroLastTime = now;
  
/* Gyo and Accelero   */

    Serial.print(millis() - timer); // Time from last report
    Serial.print(F("TEMPERATURE: "));Serial.println(mpu.getTemp()); // Temp
    
    /* ACCELERO */
    Serial.print(F("ACCELERO  X: "));Serial.print(mpu.getAccX());
    Serial.print("\tY: ");Serial.print(mpu.getAccY());
    Serial.print("\tZ: ");Serial.println(mpu.getAccZ());
    /* Gyro */
    Serial.print(F("GYRO      X: "));Serial.print(mpu.getGyroX());
    Serial.print("\tY: ");Serial.print(mpu.getGyroY());
    Serial.print("\tZ: ");Serial.println(mpu.getGyroZ());
    /* Acc Angel*/
    Serial.print(F("ACC ANGLE X: "));Serial.print(mpu.getAccAngleX());
    Serial.print("\tY: ");Serial.println(mpu.getAccAngleY());
    /* Angle */
    Serial.print(F("ANGLE     X: "));Serial.print(mpu.getAngleX());
    Serial.print("\tY: ");Serial.print(mpu.getAngleY());
    Serial.print("\tZ: ");Serial.println(mpu.getAngleZ());
    
    timer = millis();
  
  }


}  



bool getCheckSum(String s) {
// Checksum berechnen und als int ausgeben
// wird als HEX benötigt im NMEA Datensatz
// zwischen $ oder ! und * rechnen


int i, XOR, c;

  for (XOR = 0, i = 0; i < s.length(); i++) {
    c = (unsigned char)s.charAt(i);
    if (c == '*') {
      if (s.substring(i,s.length()).toInt() == XOR){
      return true;}
      else {return false;}
       }
    if ((c!='$') && (c!='!')) XOR ^= c;
  }
 
}




void split( String &toSplit){
  int len = 0;
  int from = toSplit.indexOf(',')+1;
  int to;
  while( toSplit.indexOf(',',from) != -1 ){
    to = toSplit.indexOf(',',from);
    nmeaSplit[len] = toSplit.substring(from,to);
    from = to + 1 ;
    len++;
    }
}
  


void schreiben() { 
// Nur die gewünschten Datensätze rausschreiben.
split(nmea);
if (nmea.substring(3,6) == "ZDA") { // $--ZDE - Time & Date - UTC, day, month, year and local time zone
  
  nmeaBuffer[0] = nmeaSplit[1]+nmeaSplit[2]+nmeaSplit[3]; //Date
  nmeaBuffer[1] = nmeaSplit[0];  //Time
  return;}

   
if (nmea.substring(3,6) == "DPT") { // $--DPT - Depth of water
     
    nmeaBuffer[5] = nmeaSplit[0];//Depth
    return;
}
if (nmea.substring(3,6) == "HDT") { // $--HDT - Heading - True
   nmeaBuffer[8] = nmeaSplit[0];
    return;}

if (nmea.substring(3,6) == "HDG") { // $--HDG - Heading - Magnetic
   nmeaBuffer[8] = nmeaSplit[0];
    return;}
    
if (nmea.substring(3,6) == "GLL") { // $--GLL - Geographic Position - Latitude/Longitude
   if (nmeaSplit[1]=="S"){ nmeaBuffer[3]= "-" + nmeaSplit[0];}//LAT
   else{nmeaBuffer[3]= nmeaSplit[0];}
   
   if (nmeaSplit[3]=="W"){ nmeaBuffer[4]= "-" + nmeaSplit[2];}//LOG
   else{nmeaBuffer[4]= nmeaSplit[2];}
   
  //nmeaBuffer[1] = nmeaSplit[4];// GPS-Time UTC
  return;}

if (nmea.substring(3,6) == "GGA") { // $--GGA - Global Positioning System Fix Data, Time, Position and fix related data fora GPS receiver.
   if (nmeaSplit[2]=="S"){ nmeaBuffer[3]= "-" + nmeaSplit[1];}//LAT
   else{nmeaBuffer[3]= nmeaSplit[1];} 
   
   if (nmeaSplit[4]=="W"){ nmeaBuffer[4]= "-" + nmeaSplit[3];}//LOG
   else{nmeaBuffer[4]= nmeaSplit[3];}
   
  //nmeaBuffer[1] = nmeaSplit[0];// GPS-Time UTC
  return;}

 if (nmea.substring(3,6) == "RMC") { // $--RMC - Recommended Minimum Navigation Information
   if (nmeaSplit[3]=="S"){ nmeaBuffer[3]= "-" + nmeaSplit[2];}//LAT
   else{nmeaBuffer[3]= nmeaSplit[2];} 
   
   if (nmeaSplit[5]=="W"){ nmeaBuffer[4]= "-" + nmeaSplit[4];}//LOG
   else{nmeaBuffer[4]= nmeaSplit[4];}

   nmeaBuffer[6] = nmeaSplit[6];//SOG
  //nmeaBuffer[1] = nmeaSplit[0];// GPS-Time UTC
  //nmeaBuffer[0] = nmeaSplit[8];// GPS-Date
  return;}

if (nmea.substring(3,6) == "VHW") { // $--VHW - Water speed and heading
   nmeaBuffer[9] = nmeaSplit[4];//SOW
    return;}

//TODO Windrichtung anpassen
if (nmea.substring(3,6) == "MWV") { // $--MWV - Wind Speed and Angle
    if (nmeaSplit[1]="R"){ 
      nmeaBuffer[12]= nmeaSplit[2];//AWS
       if (nmeaSplit[0].toFloat() > 180){
         nmeaBuffer[13]= nmeaSplit[0].toFloat() - 360;}//TWA
      nmeaBuffer[13]= nmeaSplit[0];}//AWA
   
   if (nmeaSplit[1]="T"){ 
      nmeaBuffer[10]= nmeaSplit[2];//TWS
      
      if (nmeaSplit[0].toFloat() > 180){
         nmeaBuffer[11]= nmeaSplit[0].toFloat() - 360;}//TWA
      nmeaBuffer[11]= nmeaSplit[0];}//TWA
   return;}  

if (nmea.substring(3,6) == "VWR") { // $--VWR - Relative Wind Speed and Angle
    if (nmeaSplit[1]="R"){ //from Right 
      nmeaBuffer[13]= nmeaSplit[0];}//AWA
    if (nmeaSplit[1]="L"){ //from Left 
      nmeaBuffer[13]= "-" + nmeaSplit[0];}//AWA
   
    nmeaBuffer[12]= nmeaSplit[4];//AWS
     return;}  
}

void schreibeSD() {
  
// Einen Datensatz auf die SD Karte scheiben

  File datei = SD.open("/Log.txt", FILE_WRITE);
  if (datei) {
    datei.println(nmea);
    datei.close();
  }  
  else {
    Serial.println("Fehler: Datei konnte nicht geöffnet werden.");
   
  }
 
  }


//=============DB====================
const char* data = "Result dataset...";
static int callback(void *data, int argc, char **argv, char **column_name)
{
   Serial.println((const char*) data);
   for (int i = 0; i < argc; ++i)
   {
      Serial.printf("%s = %s\n", column_name[i], argv[i] ? argv[i] : "NULL");
   }
   return 0;
}
char *err_msg = 0;

int open_database(const String& filename, sqlite3 **db)
{
   // int db_status = sqlite3_open_v2(filename.c_str(), db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
   int db_status = sqlite3_open(filename.c_str(), db);
   if (db_status == SQLITE_OK) // Testen, ob es mit der Konstanten geht
   {
      Serial.println("Open database succeed");
   }
   else
   {
      Serial.println("Open database failed");
   }
   return db_status;
}

void create_table()
{
  String sql = "CREATE TABLE IF NOT EXISTS programmers (id int NOT NULL PRIMARY KEY, first_name varchar(255), contributions int)";
  int db_status = sqlite3_exec(db, sql.c_str(), callback, (void*)data, &err_msg);
  if (db_status == SQLITE_OK)
  {
    Serial.println("Table programmers created");
  }
  else
  {
    Serial.println("SQL error:");
    Serial.println(err_msg);
    sqlite3_free(err_msg);
  }
}

void print_dataset(const String& table_name,const int& id)
{
   String sql = create_select_statement(table_name, id);
   Serial.println("Execute SQL Statement:");
   Serial.println(sql);
   int db_status = sqlite3_exec(db, sql.c_str(), callback, (void*)data, &err_msg);
   if (db_status == SQLITE_OK)
   {
      Serial.println("SQL statement executed");
   }
   else
   {
      Serial.println("SQL error:");
      Serial.println(err_msg);
      sqlite3_free(err_msg);
   }
}

void insert_data_set(const String& table_name, const int& id, const String& first_name, const int& contributions)
{
  String sql = create_insert_statement();
  int db_status = sqlite3_prepare_v2(db, sql.c_str(), strlen(sql.c_str()), &res, &tail);
  if (db_status == SQLITE_OK)
  {
     Serial.println("SQL statement executed");
     sqlite3_bind_int(res, 1, id);
     sqlite3_bind_text(res, 2, first_name.c_str(), strlen(first_name.c_str()), SQLITE_STATIC);
     sqlite3_bind_int(res, 3, contributions);
     if (sqlite3_step(res) != SQLITE_DONE)
     {
       Serial.printf("ERROR executing stmt: %s\n", sqlite3_errmsg(db));
       sqlite3_close(db);
       return;
     }
     sqlite3_clear_bindings(res);
     db_status = sqlite3_reset(res);
     if (db_status != SQLITE_OK)
     {
       sqlite3_close(db);
       return;
     }
     sqlite3_finalize(res);
  }
  else
  {
     Serial.println("SQL error:");
     Serial.println(err_msg);
     sqlite3_close(db);
  }
}

String create_insert_statement()
{
  String sql_insert;
  sql_insert.concat("INSERT INTO programmers VALUES (?, ?, ?);");
  return sql_insert;
}

String create_select_statement(const String& table_name, const int& id)
{
  String sql_select;
  sql_select.concat("SELECT * FROM ");
  sql_select.concat(table_name);
  sql_select.concat(" WHERE id = ");
  sql_select.concat(id);
  sql_select.concat(";");
  return sql_select;
}













  

//=============GSM===============

bool gsmReset() {
  Serial.println("gsmReset");
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, 0);
  delay(1000);
  digitalWrite(RESET_PIN, 1);
  delay(5000);
  return isSIM800Available();
}

bool isSIM800Available() {
  if (sendATCommand("AT", true) == "_TIMEOUT") {
    return false;
  }
  return true;
}

void setupGPRS() {
  Serial.println("setupGPRS");
  sendATCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"", true);
  sendATCommand("AT+SAPBR=3,1,\"APN\",\"web.vodafone.de\"", true); //TODO APN einstellungen anpassen !!!!!!!!!!!
  openGPRSConn();
  sendATCommand("AT+SAPBR=2,1", true);
}

void openGPRSConn() {
  sendATCommand("AT+SAPBR=1,1", true);
};

String sendATCommand(String cmd, bool waiting) {
  Serial.println("sendATCommand");
  String _resp = "";                                              
  cmd.reserve(500);
  Serial.println(cmd);                                            
  SIM800.println(cmd);                                            
  if (waiting) {                                                 
    _resp = waitResponse();                                       
   
    if (_resp.startsWith(cmd)) {                                
      _resp = _resp.substring(_resp.indexOf("\r", cmd.length()) + 2);
    }
    Serial.println(_resp);                                      
  }
  return _resp;                                                   
}

String waitResponse() {                                           
  Serial.println("waitResponse");
  String _resp = "";                                             
  long _timeout = millis() + 10000;                               
  while (!SIM800.available() && millis() < _timeout)  {};        
  if (SIM800.available()) {                                       
    _resp = SIM800.readString();                                 
  }
  else {                                                         
    Serial.println("Timeout...");                                
    return "_TIMEOUT";
  }
  return _resp;                                                  
}







     
 
