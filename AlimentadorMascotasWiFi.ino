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
    This sketch demonstrates how to set up a simple HTTP-like server.
    El servidor realizarA una accion dependiendo de la peticiOn
      http://server_ip/gpio/1 rotara el motor a pasos CW,
      http://server_ip/gpio/2 rotara el motor a pasos CCW
    server_ip es la direcciOn IP del mOdulo ESP8266 
    se imprimirA en el monitor serial al inicar conxiOn a Internet.

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
//SE DEBE AGREGAR A PREFERENCIAS https://arduino.esp8266.com/stable/package_esp8266com_index.json
//#define STASSID "your-ssid"
//#define STAPSK  "your-password"

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

/* No ingresar de manera directa la direcciOn IP o no se obtendrán los beneficios de acciOn de pool.
 *  Busque mejor la direcciOn IP */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // la estampa tiempo NTP son los primeros 48 bytes del mensaje

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer para mantener los paquetes salientes

// una instancia UDP nos permite enviar y recibir paquetes sobre UDP
WiFiUDP udp;


const int stepsPerRevolution = 344;  //30 grados V2 funciona a la perfección


Stepper myStepper(stepsPerRevolution, 13, 14, 12, 16);


// Crea una instancia de servidor
// especifica que puerto excuchar como argumento
WiFiServer server(80);

RTC_DS1307 rtc;

char diasSemana[7][12] = {"Sabado", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};

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
  //Serial.print(F("Connecting to "));
  Serial.println(STASSID);
  //Serial.println(ssid);
    WiFi.mode(WIFI_STA);
  //WiFi.begin(ssid, pass);


  WiFi.begin(STASSID,STAPSK);

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
  Serial.print("Puerto local: ");
  Serial.println(udp.localPort());
  

  myStepper.setSpeed(80);//funciona casi a la perfeccion

//rutina de sincronizacion WiFi

  //obtener servidor aleatorio de pool
    WiFi.hostByName(ntpServerName, timeServerIP); 
  
    sendNTPpacket(timeServerIP); // enviar un paquete NTP a un servidor de tiempo
    // esperar si una respuesta estA disponible
    delay(1000);
    
    int cb = udp.parsePacket();
    if (!cb) {
      Serial.println("no hay paquete todavia");
    }
    else {
      Serial.print("paquete recibido, largo=");
      Serial.println(cb);
      //Hemos recibido el paquete, leemos los datos de El
      udp.read(packetBuffer, NTP_PACKET_SIZE); // leer paquete hacia el buffer
  
      //la estampa de tiempo empieza en el byte 40 de los paquetes recibidos y es de 4 bytes,
      // o dos palabras de largo. Primero, se extraen dos palabras:
  
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
      // combina los cuatro bytes (dos palabras) en un entero long
      // este es el NTP de tiempo (segundos desde 1 Enero 1900):
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      Serial.print("Segundos desde 1 Enero 1900 = " );
      Serial.println(secsSince1900);
  
      // ahora convertimos el tiempo NTP en tiempo diario:
      Serial.print("tiempo Unix=");
      // El tiempo Unix empieza en 1 Enero  1970. en seconds, eso es 2208988800:
      const unsigned long seventyYears = 2208988800UL;
      // restamos setenta anyos:
      unsigned long epoch = secsSince1900 - seventyYears;
      // imprimos tiempo Unix:
      Serial.println(epoch);
  
  
      // imprimir la hor minuto y segundo:
      Serial.print("El tiempo UTC-5 es");       // UTC es el tiempo en el meridiano de Greenwich (GMT)
      horaUTC=((epoch  % 86400L) / 3600-5);//para la hora central de Mexico se necesita restar 5 horas al tiempo coordinado universal
      //para ello tambiEn es necesario aislar los casos para cuando la restas dan nUmeros negativos y traducirlas a un fromato legible
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
      
      Serial.print(horaUTC); // imprime la hora (equivale a 86400 segundos a diario)
      Serial.print(':');
      minutoUTC=(epoch % 3600) / 60;
      if ( minutoUTC < 10 ) {
        //En los primeros 10 minutos de cada hora, nosotros querremos que se conduzca a '0'
        Serial.print('0');
        minutoUTC=0;
      }
      Serial.print(minutoUTC); // imprimimos minutos (3600 equivale a segundos por minuto)
      Serial.print(':');
      segundoUTC=epoch % 60;
      if ( segundoUTC < 10 ) {
        // En los primeros 10 segundos de cada minutos querremos que se conduzca a '0'
        Serial.print('0');
        segundoUTC=0;
      }
      Serial.println(segundoUTC); // imprimimos segundos
    }
    //esperar 3 segundos
    delay(3000);

  // comienza el servidor
  server.begin();
  Serial.println(F("Inicia Servidos"));


  if (! rtc.begin()) {
    Serial.println("No se ha encontrado RTC");
    Serial.flush();
    abort();
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC no esta activo, ¡configuremos el tiempo!");
    // Cuando el tiempo necesita ser configurado en un nuevo dispositivo, o despues de un bajo de corriente, la
    // siguiente línea colocarA el RTC a la fecha y tiempo en el que este programa ha sido compilado
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    rtc.adjust(DateTime(2020,9,7,horaUTC,minutoUTC,segundoUTC));//para este caso hemos cargado la hora consultada en internet al mOdulo RTC
    // Esta linea configura el RTC con una fecha y tiempo en especIfico como se muestra en el ejemplot
    // Enero 21, 2014 tiempo lo llamarIamos asI:
     //rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    // rtc.adjust(DateTime(2020, 9, 7, 11, 00, 0));
  }

  //Falta funcion que actualice fecha automaticamente
   rtc.adjust(DateTime(2020,9,10,horaUTC,minutoUTC,segundoUTC));//para este caso hemos cargado la hora consultada en internet al mOdulo RTC
  
}//fin setup

//funcion de PARO motor a pasos
void detener(){
  digitalWrite(13,LOW);
  digitalWrite(14,LOW);
  digitalWrite(12,LOW);
  digitalWrite(16,LOW);
}

//funcion peticion de hora a servidor web
// enviar una peticiOn NTP a un servidor de tiempo dada una direcciOn
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("enviando paquete NTP ...");
  // configurar todos los bytes en el buffer a '0'
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Inizializa valores requeridos de la forma de peticiOn NTP
  // (ver URL arriba para los detalles de los paquetes)
  packetBuffer[0] = 0b11100011;   // LI, Version, Modo
  packetBuffer[1] = 0;     // Stratum, o tipo de reloj
  packetBuffer[2] = 6;     // Intervalo de poleo (Polling)
  packetBuffer[3] = 0xEC;  // Peer Reloj Precision
  // 8 bytes de zero para Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // todos los campos NTP tienen valores dados, ahora
  // es posible enviar paquetes con una peticion de estampa de tiempo(timestamp):
  udp.beginPacket(address, 123); //peticiOn NTP son el puerto 123
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
    Serial.print(diasSemana[now.dayOfTheWeek()]);
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

    // Revisar si un cliente se ha conectado
  WiFiClient client = server.available();
  if (!client) {
    return;
  }
  Serial.println(F("nuevo cliente"));

  client.setTimeout(5000); // predeterminado es 1000

  // Leer la primera linea de la peticiOn
  String req = client.readStringUntil('\r');
  Serial.println(F("peticiOn: "));
  Serial.println(req);

  // Enlazar las peticiones
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

  //realizar accion de acuerdo a la peticiOn

  if(val==1){
    Serial.println("Giro CLW");
    myStepper.step(stepsPerRevolution);
    detener();
  }else if(val==2){
    Serial.println("Giro CCLW");
    myStepper.step(-stepsPerRevolution);
    detener();
  }
//Accion extra no utililzada  
  /*else if(val==3){
     Serial.println("Giro FULL CCLW");
    myStepper.step(rota2);
    detener();
  }*/
  else{
    detener();
  }
  
  // leer/ignorar el resto de la peticiOn
  // do not client.flush(): it is for output only, see below
  while (client.available()) {
    // byte por byte no es muy eficiente
    client.read();
  }

  //Enviar la respuesta al cliente
  // estA BIEN si es para pequenyos multiples client.print/write,
  // porque el algoritmo de nagle los agruparA en un paquete sencillo
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

  // El cliente será descargado (*flushed*) y luego desconectado
  // cuando la funcion regresa y el objeto 'client' es destruido fuera de observaciOn (out-of-scope)
  // flush = asegura que los datos escritos son recibidos del otro lado
  Serial.println(F("Desconectando del cliente"));

    
}//fin void loop
