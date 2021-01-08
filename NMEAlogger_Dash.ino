#include <SD.h>
// Projekt NMEA Logger
// Lino Liebegott

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




/* WiFi Cofig */
const char* ssid = "Esprit Dashboard"; // SSID
const char* password = ""; // Password
unsigned long ulUpLastTime =0;

//=================================
#define SAVEPERIOD 2000 // ms
#define UPDATEPERIOD 1000//ms
#define bufferLen 14
const int chipSelect = 4;
const int nmeaLen = 15;
unsigned long lastTime = 0;
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
  
  /* Start AsyncWebServer */
  server.begin();




  
  Serial.begin(4800); // 4800 Baud Dateneingang und Konsole
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("SSID: ");
  Serial.print(ssid);
  Serial.print(" Password: ");
  Serial.println(password);
  Serial.println(nmea);
  Serial.println("Date,Time,TimeZone,LAT,LOG,DPT,SOG,COG,HDG,SOW,TWS,TWA,AWS,AWA");
 //pinMode(LED_BUILTIN, OUTPUT); // digitalWrite(LED_BUILTIN, HIGH);  
  pinMode(10, OUTPUT);// pin I2C
  if (!SD.begin(chipSelect)) {
    Serial.println("SD Karte nicht vorhanden");
    return;
  }
  Serial.println("SD Karte OK.");

 
}

void loop() {
  unsigned long now = millis();
  temp = (temprature_sens_read() - 32) / 1.8; // CPU Temp
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



/// Wird mit der PERIOD Ausgeführt ///
 if (now - lastTime >= SAVEPERIOD)
  {  
    lastTime = now;
  
  nmea="";
  for(unsigned int i=0; i<bufferLen;i++) {
  nmea.concat(nmeaBuffer[i]+",");
  nmeaBuffer[i] = "";}
  
  Serial.println(nmea);
  schreibeSD();
}

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

  File datei = SD.open("Log.csv", FILE_WRITE);
  if (datei) {
    datei.println(nmea);
    datei.close();
    
  }  
  else {
    Serial.println("Fehler: Datei konnte nicht geöffnet werden.");
   
  }}
  







     
 
