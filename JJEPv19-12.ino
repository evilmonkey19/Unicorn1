//  MODIFICAT EL 19/12/2020 A LES 13:27

/***      LIBRARIES     ***/

#include <RF24.h>  // radio
#include <SPI.h>    // radio
#include <DHT.h>    // sensors humitat i temperatura

#include <printf.h>

#include <DFRobot_TFmini.h>   // sensor Lidar

#include <MQ2.h>      // Sensor fum i co2
#include <Wire.h>     



/***      STRUCTS       ***/

struct information{
    int left;
    int right;
    byte control = 0x00;
    int sensors[6] = {0,0,0,0,0,0}; //0 humitat; 1 temp; 2 ultrasons; 3 MQ2; 4 FUMS; 5 LASER
};


/***    Variables ESTÁTICAS y CONSTANTES    ***/


//  AIN -> RODA ESQUERRA
//  BIN -> RODA DRETA
//  AIN1 / BIN1 --> ENDAVANT
//  AIN2 / BIN2 --> ENDARRERA

const int pinCSN = 46;      // radio
const int pinCE  = 45;      // radio
const int BIN_1  = 7;       // motors
const int BIN_2  = 6;       // motors
const int AIN_1  = 4;       // motors
const int AIN_2  = 5;       // motors
const int pinLED = A14;       // LED module
const int pinLDR=A3;         //Photoresistor LDR
const int pinDHT = 13;       // Humidity and temperature sensor
const int pinLidar1 = 10;   // Lidar TX
const int pinLidar2 = 11;   // Lidar RX
const int pinFumC02 = A0;   // Smoke and CO2
const int pinTrig = 15;    // Ultrasons
const int pinEcho = 14;     // Ultrasons

/***    Variables GLOBALES   ***/
// Objects
RF24 radio(pinCSN, pinCE);
byte addresses[][6] = {"1Node", "2Node"};
DHT dht(pinDHT, DHT11);           // Humidity and temperature
information info;
SoftwareSerial mySerial(pinLidar1, pinLidar2);   // Lidar
DFRobot_TFmini TFmini;            // Lidar
MQ2 mq2(pinFumC02);

// variables
//byte stateLED = LOW;
int lecturaLDR=0;


/***    CABECERAS funciones ***/
void initMEGA(void);     // Inicio Arduino Mega (Cotxe)
int rxInfo(void);       // Leemos por RF la posicion del Joystick
void motors(void);      // motors
void sensorTemp(void);     // Lectura sensor temperatura
void sensorHum(void);      // Lectura sensor humedad
void led(void);             // activa o desactiva el led segons el valor del LDR
void lidar(void);         // llegeix la distancia del lidar
void fum(void);           // Llegeix el sensor de fum
void c02(void);           // Llegeix el sensor de C02
void ultrasons(void);     // Llegeix el sensor d'ultrasons

/***      FUNCIONES setup y loop    ***/

void setup() {
      Serial.begin(115200);
      initMEGA();
      pinMode(pinLED, OUTPUT);
      //printf_begin();
      //radio.printDetails();
      
}

void loop() {

    rxInfo();
    while(info.control != 0x00){
      info.right=0;
      info.left=0;
      rxInfo();
    }

//    Funcions que no envien info a la COMMAND BOX
    motors();
    led();
}


/***    FUNCIONES     ***/

/* Inici */ 
void initMEGA(void){
    /* Motors */ 
    pinMode(BIN_1, OUTPUT);
    pinMode(BIN_2, OUTPUT);
    pinMode(AIN_1, OUTPUT);
    pinMode(AIN_2, OUTPUT);

    digitalWrite(BIN_1, LOW);
    digitalWrite(BIN_2, LOW);
    digitalWrite(AIN_1, LOW);
    digitalWrite(AIN_2, LOW);

    /* Radio */
    radio.begin();
    // Set the PA Level low to prevent power supply related issues since this is a
    // getting_started sketch, and the likelihood of close proximity of the devices. RF24_PA_MAX is default.
    radio.setPALevel(RF24_PA_LOW);
    // Open a writing and reading pipe on each radio, with opposite addresses
    radio.openWritingPipe(addresses[1]);
    radio.openReadingPipe(1,addresses[0]);
    // Start the radio listening for data
    radio.startListening();
    radio.setAutoAck(false);

    /* Humidity and temperature sensors */
    dht.begin();

    /* LED */
    analogWrite(pinLED, 0);

    /* LIDAR */
    TFmini.begin(mySerial);

    /* Smoke and C02 sensor */
    mq2.read(false); //set it false it you don't want to print the values in the Serial

    /* Ultrasounds */
    pinMode(pinTrig, OUTPUT);
    pinMode(pinEcho, INPUT);
}

/* Radio */


int rxInfo(void){
      if(radio.available()){
        Serial.println("hey");
        radio.read((byte *) &info, sizeof(info));
      }
      else{return 0;}   
    
    
    //Enviamos un ACK
    radio.stopListening();
    //boolean ack = true;
    //if(!radio.write(&ack, sizeof(boolean)))   Serial.println("Fail");
    sensorTemp();
    sensorHum();
    //c02();
    //fum();
    lidar();
    //ultrasons();
    
    if(!radio.write((byte *) &info, sizeof(info))){
        Serial.println("Fail sending ACK");
        radio.startListening();
        return 0;
    }
    radio.startListening();
    return 1;
}

/* Motors */
void motors(void){
    
    if (info.left>0) {
        digitalWrite(AIN_2,LOW);
        analogWrite(AIN_1,info.left);
    }
    else {
        digitalWrite(AIN_1,LOW);
        analogWrite(AIN_2,-info.left);
    }
    if (info.right>0) {
        digitalWrite(BIN_2,LOW);
        analogWrite(BIN_1,info.right);
    }
    else {
        digitalWrite(BIN_1,LOW);
        analogWrite(BIN_2,-info.right);
    }
}

/* Sensor */
void sensorTemp(void){
    info.sensors[1] = (int)dht.readTemperature();
}

void sensorHum(){
    info.sensors[0] = (int) dht.readHumidity();
    //info.hum=(int) dht.readHumidity();
    //Serial.println(info.sensors[0]);
}

void led(void){
    //stateLED = !stateLED;
    lecturaLDR= analogRead(pinLDR)/93; //veiem que 950 ha set mes o menys el valor maxim
                                       // [el normalitzem per a que el tinguem entre el 1 i el 10]
    if(lecturaLDR<2){  //El nombre en el que volem que s'encenguin
      analogWrite(pinLED,255);
    } else{
      analogWrite(pinLED,0);
    }
}

void lidar(void){
    if(TFmini.measure()){
      info.sensors[5] = (int) TFmini.getDistance();
    }
}

void fum(void){
    info.sensors[4] = (int) mq2.readSmoke();
}

void c02(void){
    info.sensors[3] = (int) mq2.readCO();
}

void ultrasons(void){
    long duration;
    int distance;

    digitalWrite(pinTrig, LOW);
    delay(2);
    digitalWrite(pinTrig, HIGH);
    delay(20);
    digitalWrite(pinTrig, LOW);
    duration = pulseIn(pinEcho, HIGH);
    info.sensors[2] = (int) duration/57.8;
}
