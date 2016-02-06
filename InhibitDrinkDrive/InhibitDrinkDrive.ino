#include <b64.h>
#include <HttpClient.h>

#include <ArduinoJson.h>

#include <LGPS.h>
#include <LBattery.h>
#include <LGSM.h>

#include <LGPRS.h>
#include <LGPRSClient.h>
#include <LGPRSServer.h>

int port = 80;

LGPRSClient client;

// variable for storing JSON response from Google Maps API
String myData;
// String storing Formatted Address of the vehicle
String vehicleLocation;

// ---- GPIO for Switching ------
char alcoholSensor = A0;
const int relayPin = 8;
const int buZZ = 10;
//-------------------------
int bodyAlc = 0;
//--- variables for storing local time --
long localSecs, tmpVal;
int localHr, localMn, localSec;

gpsSentenceInfoStruct info;

char buff[256], secBuff[256];
struct GPSDetails {
  float latitude;
  float longitude;
  int hour, minute, second, tmp, fix, num; 
  char latitude_dir, longitude_dir;
};
struct GPSDetails MyGPSPos;
float finalLng, finalLtd, tmpLng, tmpLtd;

//---------- Function to send SMS --------
void sendSMS(){
    while(!LSMS.ready())
    delay(1000);
    Serial.println("SIM ready for work!");
    LSMS.beginSMS("9841784514");
    // Ignition of vehicle numbered XXX-XXX is disabled. System detected
    // a drunk driver entering the vehicle at time.
    LSMS.print("Ignition of a vehicle numbered XXX-XXX is disabled. We detected a drunk driver entering the vehicle at ");
    LSMS.print(localHr);
    LSMS.print(':');
    LSMS.print(localMn);
    LSMS.print(':');
    LSMS.print(localSec);
    LSMS.println('.');
    LSMS.print("The driver is safe but ");
    if(bodyAlc > 200 && bodyAlc < 400) {
      LSMS.print("little drunk.");
    }
    else if(bodyAlc > 400 && bodyAlc < 600) {
      LSMS.print("just drunk.");
    }
    else if(bodyAlc > 600 && bodyAlc < 900) {
      LSMS.print("severely drunk.");
    }
    LSMS.print('\n');
    LSMS.print("Currently vehicle is stopped at ");  
    LSMS.print(vehicleLocation);
    LSMS.println('.');
    LSMS.print("Corrosponding geo-location i.e. lat, lng =  ");
    LSMS.print(finalLtd, 6);
    LSMS.print(',');
    LSMS.print(finalLng, 6);
    LSMS.println(". ");    
    LSMS.endSMS();    
    if(LSMS.endSMS())
    {
      Serial.println("SMS is sent");
    }
    else
    {
      Serial.println("SMS is not sent");
    }     
 }
 //-------- End of function sending SMS -------
//----------------------------------------------------------------------
//!\brief  return position of the comma number 'num' in the char array 'str'
//!\return  char
//----------------------------------------------------------------------
static unsigned char getComma(unsigned char num,const char *str){
  unsigned char i,j = 0;
  int len=strlen(str);
  for(i = 0;i < len;i ++){
    if(str[i] == ',')
      j++;
    if(j == num)
      return i + 1; 
    }
  return 0; 
}

//----------------------------------------------------------------------
//!\brief convert char buffer to float
//!\return  float
//----------------------------------------------------------------------
static float getFloatNumber(const char *s){
  char buf[10];
  unsigned char i;
  float rev;

  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atof(buf);
  return rev; 
}

//----------------------------------------------------------------------
//!\brief convert char buffer to int
//!\return  float
//----------------------------------------------------------------------
static float getIntNumber(const char *s){
  char buf[10];
  unsigned char i;
  float rev;

  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atoi(buf);
  return rev; 
}

//----------------------------------------------------------------------
//!\brief Parse GPS NMEA buffer for extracting data
//!\return  -
//----------------------------------------------------------------------
void parseGPGGA(const char* GPGGAstr){
  if(GPGGAstr[0] == '$'){
    int tmp;
    tmp = getComma(1, GPGGAstr);
    MyGPSPos.hour     = (GPGGAstr[tmp + 0] - '0') * 10 + (GPGGAstr[tmp + 1] - '0');
    MyGPSPos.minute   = (GPGGAstr[tmp + 2] - '0') * 10 + (GPGGAstr[tmp + 3] - '0');
    MyGPSPos.second    = (GPGGAstr[tmp + 4] - '0') * 10 + (GPGGAstr[tmp + 5] - '0');

    //get time
    sprintf(buff, "UTC timer %2d-%2d-%2d", MyGPSPos.hour, MyGPSPos.minute, MyGPSPos.second);
    Serial.print(buff);
    //get lat/lon coordinates
    float latitudetmp;
    float longitudetmp;
    tmp = getComma(2, GPGGAstr);
    latitudetmp = getFloatNumber(&GPGGAstr[tmp]);
    tmp = getComma(4, GPGGAstr);
    longitudetmp = getFloatNumber(&GPGGAstr[tmp]);
    // need to convert format
    convertCoords(latitudetmp, longitudetmp, MyGPSPos.latitude, MyGPSPos.longitude);
    //get lat/lon direction
    tmp = getComma(3, GPGGAstr);
    MyGPSPos.latitude_dir = (GPGGAstr[tmp]);
    tmp = getComma(5, GPGGAstr);    
    MyGPSPos.longitude_dir = (GPGGAstr[tmp]);
    
    //sprintf(buff, "latitude = %10.4f-%c, longitude = %10.4f-%c", MyGPSPos.latitude, MyGPSPos.latitude_dir, MyGPSPos.longitude, MyGPSPos.longitude_dir);
    //Serial.println(buff); 
    
    //get GPS fix quality
    tmp = getComma(6, GPGGAstr);
    MyGPSPos.fix = getIntNumber(&GPGGAstr[tmp]);    
    sprintf(buff, "  -  GPS fix quality = %d", MyGPSPos.fix);
    Serial.print(buff);   
    //get satellites in view
    tmp = getComma(7, GPGGAstr);
    MyGPSPos.num = getIntNumber(&GPGGAstr[tmp]);    
    sprintf(buff, "  -  %d satellites", MyGPSPos.num);
    Serial.println(buff);

    // Convert UTC to NPT (Nepalese Local Time +5:45)    
    localSecs = MyGPSPos.hour * 3600 + MyGPSPos.minute * 60 + MyGPSPos.second + 20700; // 5hr+45min = 20700sec
    if(localSecs > 86399) {
      localSecs = localSecs - 86400;
    }
    localHr = localSecs / 3600;
    tmpVal = localSecs % 3600;
    localMn = tmpVal / 60;
    localSec = tmpVal % 60;
    sprintf(buff, "Local time %2d:%2d:%2d", localHr, localMn, localSec);
    Serial.println(buff);    
    
  //Convert to google map compatible address
  float finalLng, finalLtd;
  convertCoords(latitudetmp, longitudetmp, finalLtd, finalLng);
  sprintf(buff, "Latitude : %.6f, Longitude: %.6f", finalLtd, finalLng);
  Serial.println(buff);
  }
  else{
    Serial.println("No GPS data"); 
  }
}

//----------------------------------------------------------------------
//!\brief Convert GPGGA coordinates (degrees-mins-secs) to true decimal-degrees
//!\return  -
//----------------------------------------------------------------------
void convertCoords(float latitude, float longitude, float &lat_return, float &lon_return){
  int lat_deg_int = int(latitude/100);    //extract the first 2 chars to get the latitudinal degrees
  int lon_deg_int = int(longitude/100);   //extract first 3 chars to get the longitudinal degrees
    // must now take remainder/60
    //this is to convert from degrees-mins-secs to decimal degrees
    // so the coordinates are "google mappable"
    float latitude_float = latitude - lat_deg_int * 100;    //remove the degrees part of the coordinates - so we are left with only minutes-seconds part of the coordinates
    float longitude_float = longitude - lon_deg_int * 100;     
    lat_return = lat_deg_int + latitude_float / 60 ;      //add back on the degrees part, so it is decimal degrees
    lon_return = lon_deg_int + longitude_float / 60 ;
}
void readJSONAddress(float latitude, float longitude) {
  //Host for Google Map API
  char host[] = "maps.googleapis.com";
  // Constant path for JSON 
  String initPath = "/maps/api/geocode/json?latlng=";
  // Number of milliseconds to wait without receiving any data before we give up
  const int kNetworkTimeout = 30*1000;
  // Number of milliseconds to wait if no data is available before trying again
  const int kNetworkDelay = 1000;
  int err =0;
  //---   Concatenate the GPS location obtained to the URL -----
  String finalPath = "";    
  finalPath = initPath + String(latitude, 6);
  finalPath += ',';
  finalPath += String(longitude, 6); 
  int urlLen = finalPath.length();
  char pathMain[urlLen];
  finalPath.toCharArray(pathMain, urlLen);
  //-----------------------------------------------------------------

  Serial.print("Final Path: ");
  Serial.println(pathMain);
  
  HttpClient http(client);
  
  err = http.get(host, pathMain);
  if (err == 0)
  {
    Serial.println("startedRequest ok");

    err = http.responseStatusCode();
    if (err >= 0)
    {
      Serial.print("Got status code: ");
      Serial.println(err);      

      err = http.skipResponseHeaders();
      if (err >= 0)
      {
        long bodyLen = http.contentLength();
//        Serial.print("Content length is: ");
//        Serial.println(bodyLen);
             
        // Now we've got to the body, so we can print it out
        unsigned long timeoutStart = millis();
        char c;
        // Whilst we haven't timed out & haven't reached the end of the body
        while ( (http.connected() || http.available()) &&
               ((millis() - timeoutStart) < kNetworkTimeout) )
        {
            if (http.available())
            {
                c = http.read();
                 myData += c;          
                // Print out this character
//                Serial.print(c);
               
                bodyLen--;
                // We read something, reset the timeout counter
                timeoutStart = millis();
            }
            else
            {
                // We haven't got any data, so let's pause to allow some to
                // arrive
                delay(kNetworkDelay);
            }
        }
      }
      else
      {
        Serial.print("Failed to skip response headers: ");
        Serial.println(err);
      }
    }
    else
    {    
      Serial.print("Getting response failed: ");
      Serial.println(err);
    }
  }
  else
  {
    Serial.print("Connect failed: ");
    Serial.println(err);
  }
  http.stop();
}
void setup() {    
  Serial.begin(115200);
  
  // Initialize Output Pins
  pinMode(relayPin, OUTPUT);
  pinMode(buZZ, OUTPUT);
  digitalWrite(relayPin, LOW);
  digitalWrite(buZZ, LOW);
  
  //Power ON GPS
  LGPS.powerOn();
  Serial.println("LGPS Power on, and waiting ..."); 
  delay(3000);    
}
void loop() {  
  // -------- Alcohol Sensor Operations ------
  bodyAlc = analogRead(alcoholSensor);  
  //------------- GPS Operations always monitor GPS location ------------   
  Serial.println("GPS locations...");
  LGPS.getData(&info);  
  parseGPGGA((const char*)info.GPGGA); 
  Serial.print("Latitude: ");
  Serial.print(finalLtd, 6);
  Serial.print(", Longitude: ");
  Serial.println(finalLng, 6);
  //------------------------------------------    
  delay(500);  
  // A constant value is compared from practical measurement.
  // Generally sensor value greator than 500 is obtained 
  // if a severely drunk comes near to the sensor
  if(bodyAlc > 200) {
    delay(500);  
    bodyAlc = analogRead(alcoholSensor);
    Serial.println("Driver found drunk.");  
    Serial.println("Ignition will be disabled, location will be retrived and- \nSMS will be sent to a Police Station.");
    // Disable ignition by energizing the relay
    // Vehicle is disabled until RESET button is pressed or the System is rebooted
    digitalWrite(relayPin, HIGH);  
    // Buzzer is turned ON and will be turned OFF after few secs..
    digitalWrite(buZZ, HIGH);    
    //------------------------------------------
    // Configure GPRS Module only if the driver found drunk
    Serial.println("LGPRS Power on, and waiting ..."); 
    while (!LGPRS.attachGPRS("web","ncellgprs","ncellgprs"))
    {
      delay(2000);
    }  
    Serial.println("Reading JSON file from Google Map API. It may take a minute...");
    readJSONAddress(finalLtd,finalLng);  
    
    DynamicJsonBuffer jsonBuffer;    
    Serial.println("Reading JSON file from Google Map API Done.");
    //------- Parse The JSON obtained JSON File --------------------
    JsonObject& root = jsonBuffer.parseObject(myData);
    JsonArray& results = root["results"];
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }
//    Serial.print(myData);
    const char* addressExact = root["results"][0]["formatted_address"];
    vehicleLocation = addressExact;
    //---- Done Parsing JSON and obtained the address...
    Serial.print("Obtained Location : ");
    Serial.println(addressExact);        
    // Now send the SMS to the number registered, 
    sendSMS();      
  } else {
     Serial.print("Sensor Value: ");
    Serial.print(bodyAlc);
    Serial.println(", Alcohol is nill!");
    digitalWrite(buZZ, LOW);    
  }
}
