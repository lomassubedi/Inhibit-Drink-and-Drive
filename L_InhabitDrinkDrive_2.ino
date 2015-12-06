#include <b64.h>
#include <HttpClient.h>

#include <ArduinoJson.h>

#include <LGPS.h>
#include <LBattery.h>
#include <LGSM.h>

#include<string.h>
#include <LWiFi.h>
#include <LWiFiClient.h>

//#include <LGPRS.h>
//#include <LGPRSClient.h>
//#include <LGPRSServer.h>

#define WIFI_AP "DIGICOM"
#define WIFI_PASSWORD "n3p@lnet"
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP.

int port = 80;

LWiFiClient client;
//LGPRSClient client;

String myData;
String vehicleLocation;
double latitude;
double longitude;

int hour, minute, second;

boolean SMSSent = false;

char alcoholSensor = A0;
int bodyAlc = 0;

gpsSentenceInfoStruct info;

char buff[256];

//---------- Function to send SMS --------
void sendSMS(){
    while(!LSMS.ready());
    delay(1000);
    Serial.println("SIM ready for work!");
    LSMS.beginSMS("9841784514");
    LSMS.print("Some one in the vahichle XXXX found drunk.");
    LSMS.print("Ignition of the vehicle is technically disabled and the driver is safe.");
    LSMS.println("Current Location of the vahicle is : ");
//    LSMS.print("Lat: ");
    LSMS.print(vehicleLocation);
//    LSMS.print(", Lng: ");
//    LSMS.println(longitude, 10);
    LSMS.print(" and the Alcohol level of drunk person is: ");
    LSMS.print(bodyAlc); 
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
 
static unsigned char getComma(unsigned char num,const char *str)
{
  unsigned char i,j = 0;
  int len=strlen(str);
  for(i = 0;i < len;i ++)
  {
     if(str[i] == ',')
      j++;
     if(j == num)
      return i + 1; 
  }
  return 0; 
}

static double getDoubleNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;
  
  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atof(buf);
  return rev; 
}

static double getIntNumber(const char *s)
{
  char buf[10];
  unsigned char i;
  double rev;  
  i=getComma(1, s);
  i = i - 1;
  strncpy(buf, s, i);
  buf[i] = 0;
  rev=atoi(buf);
  return rev; 
}

void parseGPGGA(const char* GPGGAstr)
{
  /* Refer to http://www.gpsinformation.org/dale/nmea.htm#GGA
   * Sample data: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
   * Where:
   *  GGA          Global Positioning System Fix Data
   *  123519       Fix taken at 12:35:19 UTC
   *  4807.038,N   Latitude 48 deg 07.038' N
   *  01131.000,E  Longitude 11 deg 31.000' E
   *  1            Fix quality: 0 = invalid
   *                            1 = GPS fix (SPS)
   *                            2 = DGPS fix
   *                            3 = PPS fix
   *                            4 = Real Time Kinematic
   *                            5 = Float RTK
   *                            6 = estimated (dead reckoning) (2.3 feature)
   *                            7 = Manual input mode
   *                            8 = Simulation mode
   *  08           Number of satellites being tracked
   *  0.9          Horizontal dilution of position
   *  545.4,M      Altitude, Meters, above mean sea level
   *  46.9,M       Height of geoid (mean sea level) above WGS84
   *                   ellipsoid
   *  (empty field) time in seconds since last DGPS update
   *  (empty field) DGPS station ID number
   *  *47          the checksum data, always begins with *
   */  
  int tmp, num ;
  if(GPGGAstr[0] == '$')
  {
    tmp = getComma(1, GPGGAstr);
    hour     = (GPGGAstr[tmp + 0] - '0') * 10 + (GPGGAstr[tmp + 1] - '0');
    minute   = (GPGGAstr[tmp + 2] - '0') * 10 + (GPGGAstr[tmp + 3] - '0');
    second    = (GPGGAstr[tmp + 4] - '0') * 10 + (GPGGAstr[tmp + 5] - '0');
    
//    sprintf(buff, "UTC timer %2d-%2d-%2d", hour, minute, second);
//    Serial.println(buff);
    
    tmp = getComma(2, GPGGAstr);
    latitude = getDoubleNumber(&GPGGAstr[tmp]);
    latitude /= 100;
    tmp = getComma(4, GPGGAstr);
    longitude = getDoubleNumber(&GPGGAstr[tmp]);
    longitude /= 100;
    
    tmp = getComma(7, GPGGAstr);
    num = getIntNumber(&GPGGAstr[tmp]);    
    sprintf(buff, "satellites number = %d", num);
    Serial.println(buff); 
  }
  else
  {
    Serial.println("Not get data"); 
  }
}
void readJSONAddress(double latitude, double longitude) {
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
//        long bodyLen = http.contentLength();
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
  LWiFi.begin();
  // put your setup code here, to run once:
  Serial.begin(115200);
  LGPS.powerOn();
  Serial.println("LGPS Power on, and waiting ..."); 
  delay(3000);  
//  Serial.println("LGPRS Power on, and waiting ..."); 
//  while (!LGPRS.attachGPRS("web","ncellgprs","ncellgprs"))
//  {
//    delay(500);
//  }
  Serial.println("Connecting to AP");
  while (0 == LWiFi.connect(WIFI_AP, LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)))
  {
    delay(1000);
  }  
}
void loop() {  
  // -------- Alcohol Sensor Operations ------
  bodyAlc = analogRead(alcoholSensor);
  Serial.print("Sensor Value: ");
  Serial.println(bodyAlc);
  //------------------------------------------
  //------------- GPS Operations ------------   
  Serial.println("Reading GPS locations...");
  LGPS.getData(&info);  
  parseGPGGA((const char*)info.GPGGA); 
  Serial.print("Latitude: ");
  Serial.println(latitude);
  Serial.print("Longitude: ");
  Serial.println(longitude);
  //------------------------------------------  
  Serial.println("Reading JSON file from Google Map API. It may take a minute...");
  readJSONAddress(latitude,longitude);  
  DynamicJsonBuffer jsonBuffer;
  Serial.println("Reading JSON file from Google Map API Done.");
  //------- Parse The JSON obtained JSON File ---------------------
  JsonObject& root = jsonBuffer.parseObject(myData);
  JsonArray& results = root["results"];
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
//  Serial.print(myData);
  const char* addressExact = root["results"][0]["formatted_address"];
  vehicleLocation = addressExact;
  //---- Done Parsing JSON and obtained the address...
  Serial.print("Obtained Location : ");
  Serial.println(addressExact);
  while(1); 
  
  delay(500);
  if(!SMSSent) {  
    SMSSent = true; 
  // sendSMS();       
  }
}
