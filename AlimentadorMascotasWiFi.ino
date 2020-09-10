/******************
 -www.idea161.org
 - Alimentador de mascotas WiFi con ESP8266
 - ElectrOnica utilizada:
 -- ESP8266
 -- ULN2003a driver para motor a pasos
 -- Motor a pasos BYJ
 -- Reloj en tiempo real RTC DS1307

 Para la documentación se dejarán los comentarios originales de los cOdigos de los ejemplos utilizados
 Se utilizaron ejemplos para:
 -DS1307 RTC para actulizar la fecha
 -ESP8266 como cliente de servicio para la hora UTC
 -ESP8266 como servidor para tener acciOn manual del motor a pasos
 -movimiento de motor a pasos con biblioteca especializada
*/

/*
    This sketch demonstrates how to set up a simple HTTP-like server.
    The server will set a GPIO pin depending on the request
      http://server_ip/gpio/0 will set the GPIO2 low,
      http://server_ip/gpio/1 will set the GPIO2 high
    server_ip is the IP address of the ESP8266 module, will be
    printed to Serial when the module is connected.

//Mapeo de pines para plca de desarrollo ESP8266
PIN       Arduino IDE

D0->RX       -> digital  0
D1<-TX       -> digital  1
TX1/D9       -> digital  2
NotConnected -> digital  3
D14/SDA/D4   -> digital  4
D15/SCL/D3   -> digital  5
NotConnected -> digital  6
NotConnected -> digital  7
NotConnected -> digital  8
NotConnected -> digital  9
NotConnected -> digital 10
NotConnected -> digital 11
D12/MOSI/D7  -> digital 12
D11/MISI     -> digital 13
D13/SCK/D5   -> digital 14
D10/SS       -> digital 15
D2           -> digital 16

*/

#include <ESP8266WiFi.h>

#ifndef STASSID
#define STASSID "your-ssid" //  your network SSID (name)
#define STAPSK  "your-password" // your network password

#endif
#include <Stepper.h>
#include "RTClib.h" // Date and time functions using a DS1307 RTC connected via I2C and Wire lib

/********************
- www.geekstips.com
- Arduino Time Sync from NTP Server using ESP8266 WiFi module 
- Arduino code example
 ********************/
#include <WiFiUdp.h>

unsigned int localPort = 2390;      // local port to listen for UDP packets
int hora, minuto, segundo, horaUTC, minutoUTC, segundoUTC, a,rota2;

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

//const int stepsPerRevolution = 341;  //30 grados

const int stepsPerRevolution = 344;  //30 grados V2 funciona a la perfección


// for your motor

// initialize the stepper library on pins 8 through 11:
//Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);
//Stepper myStepper(stepsPerRevolution, 13, 14, 12, 4);
Stepper myStepper(stepsPerRevolution, 13, 14, 12, 16);


const char* ssid = STASSID;
const char* password = STAPSK;

// Create an instance of the server
// specify the port to listen on as an argument
WiFiServer server(80);

RTC_DS1307 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup () {
  //Serial.begin(57600);(ver conflicto con tasa de baudio)

#ifndef ESP8266
  while (!Serial); // wait for serial port to connect. Needed for native USB
#endif

Serial.begin(115200);

  // prepare LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0);

  // Connect to WiFi network
  Serial.println();
  Serial.println();
  Serial.print(F("Connecting to "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println();
  Serial.println(F("WiFi connected"));

  // Imprime direcciOn IP
  Serial.println("direccion IP: ");
  Serial.println(WiFi.localIP());


  //Rutina de cliente
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  

  myStepper.setSpeed(80);//funciona casi a la perfeccion

//rutina de sincronizacion WiFi

  //get a random server from the pool
    WiFi.hostByName(ntpServerName, timeServerIP); 
  
    sendNTPpacket(timeServerIP); // send an NTP packet to a time server
    // wait to see if a reply is available
    delay(1000);
    
    int cb = udp.parsePacket();
    if (!cb) {
      Serial.println("no packet yet");
    }
    else {
      Serial.print("packet received, length=");
      Serial.println(cb);
      // We've received a packet, read the data from it
      udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  
      //the timestamp starts at byte 40 of the received packet and is four bytes,
      // or two words, long. First, esxtract the two words:
  
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combine the four bytes (two words) into a long integer
      // this is NTP time (seconds since Jan 1 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      Serial.print("Seconds since Jan 1 1900 = " );
      Serial.println(secsSince1900);
  
      // now convert NTP time into everyday time:
      Serial.print("Unix time = ");
      // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // subtract seventy years:
      unsigned long epoch = secsSince1900 - seventyYears;
      // print Unix time:
      Serial.println(epoch);
  
  
      // print the hour, minute and second:
      Serial.print("The UTC-5 time is ");       // UTC is the time at Greenwich Meridian (GMT)
      horaUTC=((epoch  % 86400L) / 3600-5);
      if(horaUTC==-1){
        horaUTC=23;
      }else if(horaUTC==-2){
        horaUTC=22;
      }else if(horaUTC==-3){
        horaUTC=21;
      }else if(horaUTC==-4){
        horaUTC=20;
      }else if(horaUTC==-5){
        horaUTC=19;
      }
      Serial.print(horaUTC); // print the hour (86400 equals secs per day)
      Serial.print(':');
      minutoUTC=(epoch % 3600) / 60;
      if ( minutoUTC < 10 ) {
        // In the first 10 minutes of each hour, we'll want a leading '0'
        Serial.print('0');
        minutoUTC=0;
      }
      Serial.print(minutoUTC); // print the minute (3600 equals secs per minute)
      Serial.print(':');
      segundoUTC=epoch % 60;
      if ( segundoUTC < 10 ) {
        // In the first 10 seconds of each minute, we'll want a leading '0'
        Serial.print('0');
        segundoUTC=0;
      }
      Serial.println(segundoUTC); // print the second
    }
    //esperar 3 segundos
    delay(3000);

  // comienza el servidor
  server.begin();
  Serial.println(F("Server started"));


  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtc.adjust(DateTime(2020,9,7,horaUTC,minutoUTC,segundoUTC));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
     //rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    // rtc.adjust(DateTime(2020, 9, 7, 11, 00, 0));
  }

  // When time needs to be re-set on a previously configured device, the
  // following line sets the RTC to the date & time this sketch was compiled
   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   rtc.adjust(DateTime(2020,9,7,horaUTC,minutoUTC,segundoUTC));
  // This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2014 at 3am you would call:
  // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
 // rtc.adjust(DateTime(2020, 9, 7, 11, 00, 0));  

  
}//fin setup

//funcion de paro motor a pasos
void detener(){
  digitalWrite(13,LOW);
  digitalWrite(14,LOW);
  digitalWrite(12,LOW);
  digitalWrite(16,LOW);
}

//funcion peticion de hora a servidor web
// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void loop () {
    DateTime now = rtc.now();
      hora=now.hour();
      minuto=now.minute();
      segundo=now.second();

    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" (");
    Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
    Serial.print(") ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    //Serial.print(now.minute(), DEC);
    Serial.print(now.minute());//si se despliegan los datos en decimal
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();

 
    //Para el uso del aparato se tiene un espacio de 6:30 a 22:30, lo que da un total de 16 horas
    //en espacios equitativos se tiene que cada intervalo debe ser de 2 horas con 42 min

     //####CONTROL DE ACCIONES CON RELOJ RTC#####
       //8:12
        if(hora==8){
          if(minuto==12){
            if(segundo<=3){
              Serial.println("Giro CLW");
              myStepper.step(stepsPerRevolution); 
              detener(); 
            }
          } 
        }
        //10:54
        if(hora==10){
          if(minuto==54){
             if(segundo<=3){
              Serial.println("Giro CLW");
              myStepper.step(stepsPerRevolution);
               detener();
            }
          } 
        }
        //13:46
        if(hora==13){
          if(minuto==46){
              if(segundo<=3){
              Serial.println("Giro CLW");
              myStepper.step(stepsPerRevolution);
               detener(); 
            }
          } 
        }
        //16:28
        if(hora==16){
          if(minuto==28){
             if(segundo<=3){
              Serial.println("Giro CLW");
              myStepper.step(stepsPerRevolution);
               detener();
            } 
          } 
        }
        //19:10
        if(hora==19){
          if(minuto==10){
             if(segundo<=3){
              Serial.println("Giro CLW");
              myStepper.step(stepsPerRevolution); 
               detener(); 
            }
          } 
        }
        //21:52
        if(hora==21){
          if(minuto==52){
              if(segundo<=3){
              Serial.println("Giro CLW");
              myStepper.step(stepsPerRevolution);
               detener(); 
            }
          } 
        }
   
        else{
          detener();
        }

    delay(3000);
    //fin rutina de RTC

    // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  Serial.println(F("new client"));

  client.setTimeout(5000); // default is 1000

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(F("request: "));
  Serial.println(req);

  // Match the request
  int val;
  if (req.indexOf(F("/gpio/1")) != -1) {
    val = 1;
  } 
    else if (req.indexOf(F("/gpio/2")) != -1) {
    val = 2;
  }  
  /*else if (req.indexOf(F("/gpio/3")) != -1) {
    val = 3;
  }*/
  else {
    Serial.println(F("peticion Invalida"));
    val = digitalRead(LED_BUILTIN);
  }


 rota2=stepsPerRevolution*6;
  // Set LED according to the request
  //digitalWrite(LED_BUILTIN, val);

  if(val==1){
    Serial.println("Giro CLW");
    myStepper.step(stepsPerRevolution);
    detener();
  }else if(val==2){
    Serial.println("Giro CCLW");
    myStepper.step(-stepsPerRevolution);
    detener();
  }
  
  /*else if(val==3){
     Serial.println("Giro FULL CCLW");
    myStepper.step(rota2);
    detener();
  }*/
  else{
    detener();
  }
  
  // read/ignore the rest of the request
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte by byte is not very efficient
    client.read();
  }

  // Send the response to the client
  // it is OK for multiple small client.print/write,
  // because nagle algorithm will group them into one single packet
  client.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<!DOCTYPE HTML>\r\n<html>\r\n Hora "));
  //client.print((val) ? F("detenido") : F("moviendose"));
  client.print(hora);
   client.print(":");
   client.print(minuto);
   client.print(":");
   client.print(segundo);
  client.print(F("<br><br>Presiona <a href='http://"));
  client.print(WiFi.localIP());
  client.print(F("/gpio/1'>aqui</a> para mover 60 grados CW, <a href='http://"));
   client.print(WiFi.localIP());
   client.print(F("/gpio/2'>aqui</a> para mover 60 grados CCW.</html>"));
  //client.print(F("/gpio/2'>aqui</a> para mover 60 grados CCW, <a href='http://"));
  //client.print(WiFi.localIP());
  //client.print(F("/gpio/3'>aqui</a> para rotacion Completa.</html>"));

  // The client will actually be *flushed* then disconnected
  // when the function returns and 'client' object is destroyed (out-of-scope)
  // flush = ensure written data are received by the other side
  Serial.println(F("Desconectando del cliente"));

    
}//fin void loop
