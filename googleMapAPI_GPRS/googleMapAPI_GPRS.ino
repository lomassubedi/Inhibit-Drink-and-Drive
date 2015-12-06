#include <LGPRS.h>
#include <LGPRSClient.h>
#include <LGPRSServer.h>

int port = 80;

LGPRSClient client;
void setup()
{
  // setup Serial port
  Serial.begin(115200);
  
//  Serial.println("Attach to Ncell GPRS network.");  
  while (!LGPRS.attachGPRS("web","ncellgprs","ncellgprs"))
  {
    delay(500);
  }      
  // if you get a connection, report back via serial:
  Serial.println("Connect to https://maps.googleapis.com/maps/api/geocode/json?latlng=27.668453,85.320082");
  if (client.connect("maps.googleapis.com", port))
  {
    Serial.println("connected");
    // Make a HTTP request:    
    client.println("GET /maps/api/geocode/json?latlng=27.668453,85.320082 HTTP/1.1");
    client.println("Host: maps.googleapis.com");
    client.println("Connection: close");
    client.println();
  }
  else
  {
    // if you didn't get a connection to the server:
    Serial.println("connection failed");
  }   
}
void loop() {
  int v;
  while(client.available())
  {
  v = client.read();
  if (v < 0)
  break;
  Serial.write(v);
  }
  delay(500);
}
