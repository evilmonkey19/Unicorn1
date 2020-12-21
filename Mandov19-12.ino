/***      LIBRARIES     ***/

#include <RF24.h>
#include <nRF24L01.h>
#include <SPI.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <printf.h>   // Per comprovar els moduls de RF



/***      STRUCTS       ***/

struct information {
  int left;
  int right;
  byte control = 0x00;
  int sensors[6] = {0, 0, 0, 0, 0, 0}; //0 humitat; 1 temp; 2 ultrasons; 3 MQ2; 4 FUMS; 5 LASER
};

struct xyzPositions {
  int x;
  int y;
  int z;
};


/***    Variables ESTÁTICAS y CONSTANTES    ***/

const int pinCSN = 9;
const int pinCE  = 10;
const int pinZ   = A3; //cable groc
const int pinY   = A1;
const int pinX   = A0;

const int sensibilityButton = 150;  // Sensibilitat per ficar els potenciometres com botons
const unsigned long canviPantalla = 1000 * 5;
const int refrescLCD = 500;

/***    Variables GLOBALES   ***/

// Objects
RF24 radio(pinCSN, pinCE);
LiquidCrystal_I2C lcd(0x3F, 16, 2);
byte addresses[][6] = {"1Node", "2Node"};   // It isnt an object but see as an object

// Variables
int success, paquets;
information info;
xyzPositions pos;
float alfa, beta, pendent;

String up = "";   // Fila superior LCD
String down = ""; // Fila inferior LCD
int potRadio, pant;  // Valor de potencia por defecto de la radio
boolean backlightLCD;   // Si backlight ences o no
unsigned long tempoCanviPantalla = 0, tempoRefrescLCD = 0;    // Tempos del LCD
boolean pantActive[6];   // Per tenir la pantalla activa
int pantDeixar;   // Per si volem deixar una pantalla únicament

/***    CABECERAS funciones ***/
void initUNO(void);     // Inicio Arduino UNO (Mando)
void readPosition(void);    // Leemos posicion Joystick
void radioTest(void);     // Per comprovar que el modul i els cables estan ben connectats
int txInfo(void);       // Enviamos por RF la posicion del Joystick
void positionToVelocities(void);    // Fiquem les velocitats dels motors segons la posicio
void pantalla(void);    // Pantalla LCD
void pantConf(void);    // Pantalla config
void pantConfLCD(void);   // Pantalla config del LCD
void pantConfRadio(void);   // Pantalla config de la radio
void pantUnactive(void);  // Pantalla para desactivar algún módulo

// Utilitzar el mando com botons
boolean botonZ(void);    // Eix Z
boolean arribaY(void);   // mejor arriba España
boolean abajoY(void);
boolean adelanteX(void);
boolean atrasX(void);
boolean accion(void);   // para saber si se ha tocado algun boton

/***      FUNCIONES setup y loop    ***/

void setup() {
  Serial.begin(115200);
  //
  //radioTest();
  initUNO();
}

void loop() {
  readPosition();
  positionToVelocities();
  paquets++;
  if (txInfo())  success++;
  //Serial.print(success); Serial.print("/"); Serial.println(paquets);
  pantalla();
  //Ser,0,,,,,,,,,,,,,,,,,,,,,,,,,,,,00222ial.println(info.hum);
}


/***    FUNCIONES     ***/

/* Inici */
void initUNO(void) {
  /* Joystick */
  pinMode(pinX, INPUT);
  pinMode(pinY, INPUT);

  /* Radio */
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  potRadio = 1;
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1, addresses[1]);
  radio.startListening();
  radio.setAutoAck(false);

  /* LCD */
  lcd.init();
  lcd.backlight();
  backlightLCD = true;
  tempoRefrescLCD = millis();
  tempoCanviPantalla = millis();
  for (int i = 0; i<6; i++){ pantActive[i] = true;}
  pantDeixar = 6;
}

void radioTest(void) {
  printf_begin();
  //Serial.println("CheckConnection Starting");
  //Serial.println();
  //Serial.println("FIRST WITH THE DEFAULT ADDRESSES after power on");
  Serial.println("  Note that RF24 does NOT reset when Arduino resets - only when power is removed");
  Serial.println("  If the numbers are mostly 0x00 or 0xff it means that the Arduino is not");
  Serial.println("     communicating with the nRF24");
  Serial.println();
  radio.begin();
  radio.printDetails();
  Serial.println();
  Serial.println();
  Serial.println("AND NOW WITH ADDRESS AAAxR  0x65 64 6f 4e 32 ON P1");
  Serial.println(" and 250KBPS data rate");
  Serial.println();
  radio.openReadingPipe(1, addresses[1]);
  radio.setDataRate( RF24_250KBPS );
  radio.printDetails();
  Serial.println();
  Serial.println();

}
/* Radio */

int txInfo(void) {
  
  radio.stopListening();
  //enviadoRecibido();

  if (!radio.write((byte *)&info, sizeof(info))) {
    Serial.println(F("Fail sending (Not sended)"));
    return 0;
  }


  // Esperamos un ACK
  radio.startListening();
  delay(100);
  unsigned long started_waiting_at = micros();
  boolean timeout = false;

  while (!radio.available()) {  // NO recibir nada
    if (micros() - started_waiting_at > 30000) { //TEMPS ESPERA
      timeout = true;
      Serial.println(F("Failed, response time out"));
      return 0;
    }
  }

  //boolean ack = false;
  radio.read((byte *)&info, sizeof(info));
  //enviadoRecibido();
  //if(ack)   Serial.println(F("Succesful, ack received"));
  //else      Serial.println(F("Unsuccesful, not Ack"));
  return 1;
}

/* Joystick  */
void readPosition(void) {
  // Read values from analogic port and Remap (0, 1023) -> (-255, 255) :
  pos.x = map(analogRead(pinX), 0, 1023, -255, 255);
  pos.y = map(analogRead(pinY), 0, 1023, -255, 255);
  pos.z = map(analogRead(pinZ), 0, 1023, -255, 255);
  pos.z+=200;
  //Serial.println(pos.z);
}

void positionToVelocities(void) {
  if ((pos.y <= 30 && pos.y >= -30) && (pos.x <= 30 && pos.x >= -30)) {
    //PARA
    info.left = 0;
    info.right = 0;
  }
  else if (pos.y > 30 && (pos.x <= 30 && pos.x >= -30)) {
    //ENDAVANT
    alfa = 0.95;
    info.right = alfa * pos.y;
    info.left = alfa * pos.y;
  }
  else if (pos.y < 30 && (pos.x <= 30 && pos.x >= -30)) {
    //ENDARRERA
    alfa = 0.95;
    info.right = alfa * pos.y;
    info.left = alfa * pos.y;
  }
  else if (pos.x < 30 && (pos.y <= 30 && pos.y >= -30)) {
    //RODA DRETA ENDAVANT RODA ESQ ENDARRERA (GIR SOBRE SI MATEIX)
    alfa = 0.6;
    info.right = alfa * -pos.x;
    info.left = alfa * pos.x;
  }
  else if (pos.x > 30 && (pos.y <= 30 && pos.y >= -30)) {
    //RODA ESQ ENDAVANT RODA DRETA ENDARRERA (GIR SOBRE SI MATEIX)
    alfa = 0.6;
    info.right = -alfa * pos.x;
    info.left = alfa * pos.x;
  }
  else if (pos.x > 30 && pos.y > 30) {
    //QUADRANT 1 --> ENDAVANT I CAP A LA DRETA -----> llindar a y=100, x=234.57  ---> y=(100/234.57)*x
    pendent = 100 / 234.57;
    if (pos.y >= pendent * pos.x) {
      //ZONA SUPERIOR DEL LLINDAR (ZONA VARIABLE)
      alfa = 0.80;
      beta = 0.20;
      info.right = alfa * pos.y - beta * pos.x; //provar +35
      info.left = alfa * pos.y + beta * pos.x;
    }
    else {
      //ZONA INFERIOR DEL LLINDAR (ZONA CONSTANT)
      info.right = 80;
      info.left = 80 + 30;
    }
  }
  else if (pos.x < -30 && pos.y > 30) {
    //QUADRANT 2 --> ENDAVANT I CAP A L'ESQUERRA -----> llindar a y=100, x=-234.57  ---> y=(100/-234.57)*x
    pendent = 100 / -234.57;
    pos.x = -pos.x;
    if (pos.y >= pendent * pos.x) {
      //ZONA SUPERIOR DEL LLINDAR (ZONA VARIABLE)
      alfa = 0.80;
      beta = 0.20;
      info.right = alfa * pos.y + beta * pos.x; //provar +35
      info.left = alfa * pos.y - beta * pos.x;
    }
    else {
      //ZONA INFERIOR DEL LLINDAR (ZONA CONSTANT)
      info.right = 80 + 30;
      info.left = 80;
    }
  }
  else if (pos.x < -30 && pos.y < -30) {
    //QUADRANT 3 --> ENDARRERA I CAP A L'ESQUERRA -----> llindar a y=-100, x=-234.57  ---> y=(-100/-234.57)*x
    pendent = -100 / -234.57;
    pos.x = -pos.x;
    pos.y = -pos.y;
    if (pos.y >= pendent * pos.x) {
      //ZONA INFERIOR DEL LLINDAR (ZONA VARIABLE)
      alfa = 0.80;
      beta = 0.20;
      info.right = -(alfa * pos.y + beta * pos.x); //provar +35
      info.left = -(alfa * pos.y - beta * pos.x);
    }
    else {
      //ZONA SUPERIOR DEL LLINDAR (ZONA CONSTANT)
      info.right = -(80+30);
      info.left = -80;
    }
  }
  else if (pos.x > 30 && pos.y < -30) {
    //QUADRANT 4 --> ENDARRERA I CAP A LA DRETA -----> llindar a y=-100, x=234.57  ---> y=(-100/234.57)*x
    pendent = -100 / 234.57;
    pos.y = -pos.y;
    if (pos.y >= pendent * pos.x) {
      //ZONA INFERIOR DEL LLINDAR (ZONA VARIABLE)
      alfa = 0.80;
      beta = 0.20;
      info.right = -(alfa * pos.y - beta * pos.x); //provar +35
      info.left = -(alfa * pos.y + beta * pos.x);
    }
    else {
      //ZONA SUPERIOR DEL LLINDAR (ZONA CONSTANT)
      info.right = -80;
      info.left = -(80 + 30);
    }
  }
}


/* LCD */
void pantalla(void) {
  if (botonZ()) { // Utilitzem el potenciometre del eix z com un boto
    pantConf();
  }

  if (millis() - tempoRefrescLCD > refrescLCD) {
    lcd.clear();
    if(pantDeixar != 6){pant = pantDeixar;}   // No actualitzem a la següent pantalla
    if (millis() - tempoRefrescLCD + 200 > refrescLCD) {
      if(!pantActive[pant]){ pant = ++pant%6;}
      switch (pant) {
        case 0: up = "Left"; down = "Right"; break;
        case 1: up = "Humidity:"; down = "Temp:"; break;
        case 2: up = "Ultrasounds:"; down = ""; break;
        case 3: up = "CO2"; down = "Smoke:"; break;
        case 4: up = "Distance:"; down = "LIDAR"; break;
        case 5: up = "Tasa RF:"; down = ""; break;
      }
      lcd.setCursor(0, 0);
      lcd.print(up);
      lcd.setCursor(0, 1);
      lcd.print(down);
      switch (pant) {
        case 0: // motors
          lcd.setCursor(12, 0);
          lcd.print(info.left);
          lcd.setCursor(12, 1);
          lcd.print(info.right);
          break;
        case 1: // Temperatura - Humitat
          lcd.setCursor(13, 0);
          lcd.print(info.sensors[0]);
          lcd.setCursor(13, 1);
          lcd.print(info.sensors[1]);
          break;
        case 2:  // Ultrasons
          lcd.setCursor(13, 0);
          lcd.print(info.sensors[2]);
          break;
        case 3: // Dioxid de carboni - Fum
          lcd.setCursor(13, 0);
          lcd.print(info.sensors[3]);
          lcd.setCursor(13, 1);
          lcd.print(info.sensors[4]);
          break;
        case 4: // MiniLiDaR
          lcd.setCursor(13, 0);
          lcd.print(info.sensors[5]);
          break;

        case 5: // RadioFrecuencia
          lcd.setCursor(13, 0);
          lcd.print((float) success / paquets);
          break;
      }
      tempoRefrescLCD = millis(); // reiniciem el tempo
    }
    /* Cambia los datos que se ven por pantalla */
    if (millis() - tempoCanviPantalla > canviPantalla) {
      pant = ++pant % 6;
      tempoCanviPantalla = millis();
    }
  }


}

void pantConf(void) {
  boolean finish = false;
  int conf = 0;

  info.control = 0x01;   // Parada del cotxe
  txInfo();

  while (!finish) {
    lcd.clear();
    delay(100);
    switch (conf) {
      case 0: up = "--> Radio"; down = "LCD"; break;
      case 1: up = "--> LCD"; down = "Atras"; break;
      case 2: up = "--> Atras"; down = "Radio"; break;
    }
    lcd.setCursor(0, 0);
    lcd.print(up);
    lcd.setCursor(0, 1);
    lcd.print(down);
    delay(100);
    while (accion()) {};
    if (botonZ() || adelanteX()) {
      switch (conf) {
        case 0: pantConfRadio(); break;
        case 1: pantConfLCD();  break;
        case 2: finish = true; break;
      }
    }
    if (atrasX()) {
      finish = true;
    }
    if (arribaY()) {
      if (--conf < 0)  conf = 2;
    }
    if (abajoY()) conf = ++conf % 3;
  }

  info.control = 0x00;  // Tornem a encendre el cotxet
  txInfo();
}

void pantConfRadio(void) {
  boolean finish = false;
  int conf = 0;
  while (!finish) {
    lcd.clear();
    delay(100);
    if (conf == 0) {
      switch (potRadio) {
        case 0: up = "--> Power: MIN"; break;
        case 1: up = "--> Power: LOW"; break;
        case 2: up = "--> Power: HIGH"; break;
        case 3: up = "--> Power: MAX"; break;
      }
      down = "Atras";
    }
    else if (conf == 1) {
      up = "-->Atras";
      switch (potRadio) {
        case 0: down = "Power: MIN"; break;
        case 1: down = "Power: LOW"; break;
        case 2: down = "Power: HIGH"; break;
        case 3: down = "Power: MAX"; break;
      }
    }

    lcd.setCursor(0, 0);
    lcd.print(up);
    lcd.setCursor(0, 1);
    lcd.print(down);
    delay(100);
    while (accion()) {};
    if (botonZ()  || adelanteX()) {
      switch (conf) {
        case 0:
          potRadio = ++potRadio % 4;
          switch (potRadio) {
            case 0: radio.setPALevel(RF24_PA_MIN); break;
            case 1: radio.setPALevel(RF24_PA_LOW); break;
            case 2: radio.setPALevel(RF24_PA_HIGH); break;
            case 3: radio.setPALevel(RF24_PA_MAX); break;
          }
          break;
        case 1:
          finish = true;
          break;
      }

    }
    if (atrasX())  finish = true;
    if (arribaY()) {
      if (--conf < 0)  conf = 1;
    }
    if (abajoY()) conf = ++conf % 2;
  }
}

void pantConfLCD(void) {
  boolean finish = false;
  int conf = 0;
  while (!finish) {
    lcd.clear();
    delay(100);
    switch (conf) {
      case 0:
        if (backlightLCD)  up = "--> Light: ON";
        else up = "--> Light: OFF";
        switch(pantDeixar){
            case 0: down = "Pant: Motors"; break; 
            case 1: down = "Pant: Hum/Tmp"; break;
            case 2: down = "Pant: USounds"; break;
            case 3: down = "Pant: C02/Fum"; break;
            case 4: down = "Pant: LASER"; break;
            case 5: down = "Pant: RF"; break;
            default: down = "Pant: Multi"; break;
        }
        break;
      case 1:
        switch(pantDeixar){
            case 0: up = "-->Pant: Motors"; break;
            case 1: up = "-->Pant: Hum/Tmp"; break;
            case 2: up = "-->Pant: USounds"; break;
            case 3: up = "-->Pant: C02/Fum"; break;
            case 4: up = "-->Pant: LASER"; break;
            case 5: up = "-->Pant: RF"; break;
            default: up = "-->Pant: Multi"; break;
        }
        down = "Disable item";
        break;
      case 2:
        up = "-->Disable item";
        down = "Atras";
        break;
      case 3:
        up = "--> Atras";
        if (backlightLCD) down = "Light: ON";
        else down = "Light: OFF";
        break;
    }
    lcd.setCursor(0, 0);
    lcd.print(up);
    lcd.setCursor(0, 1);
    lcd.print(down);
    delay(100);
    while (accion()) {};
    if (botonZ() || adelanteX()) {
      switch (conf) {
        case 0:
          if (backlightLCD) lcd.noBacklight();
          else lcd.backlight();
          backlightLCD = !backlightLCD;
          break; 
        case 1:
          pantDeixar = ++pantDeixar%7;
          break;
        case 2: pantUnactive();       
        case 3: finish = true; break;
      }
    }
    if (atrasX()) {
      finish = true;
    }
    if (arribaY()) {
      if (--conf < 0)  conf = 3;
    }
    if (abajoY()) conf = ++conf % 4;
  }
}

void pantUnactive(void){
  boolean finish = false;
  int conf = 0;
  while (!finish) {
    lcd.clear();
    delay(100);
    switch (conf) {
      case 0:
        if(pantActive[0]) up = "-->Motors: ON";
        else up = "-->Motors: OFF";
        if(pantActive[1]) down = "Hum/Tmp: ON";
        else down = "Hum/Tmp: OFF";
        break;
      case 1:
        if(pantActive[1]) up = "-->Hum/Tmp: ON";
        else up = "-->Hum/Tmp: OFF";
        if(pantActive[2]) down = "USounds: ON";
        else down = "USounds: OFF";
        break;
      case 2:
        if(pantActive[2]) up = "-->USounds: ON";
        else up = "-->USounds: OFF";
        if(pantActive[3]) down = "C02/Fum: ON";
        else down = "-->C02/Fum: OFF";
        break;
      case 3:
        if(pantActive[3]) up = "-->C02/Fum: ON";
        else up = "-->C02/Fum: OFF";
        if(pantActive[4]) down = "LASER: ON";
        else down = "LASER: OFF";
        break;
      case 4:
        if(pantActive[4]) up = "-->LASER: ON";
        else up = "-->LASER: OFF";
        if(pantActive[5]) down = "RF: ON";
        else down = "RF: OFF";
        break;
      case 5:
        if(pantActive[5]) up = "-->RF: ON";
        else up = "-->RF: OFF";
        down ="Atras";
        break;
      case 6:
        up ="-->Atras";
        if(pantActive[0]) down = "Motors: ON";
        else down = "Motors: OFF";
    }
    lcd.setCursor(0, 0);
    lcd.print(up);
    lcd.setCursor(0, 1);
    lcd.print(down);
    delay(100);
    while (accion()) {};
    if (botonZ() || adelanteX()) {
      if(conf == 6){ finish = true;}
      else pantActive[conf] = !pantActive[conf];
    }
    if (atrasX()) {
      finish = true;
    }
    if (arribaY()) {
      if (--conf < 0)  conf = 6;
    }
    if (abajoY()) conf = ++conf % 7;
  }
}

boolean accion(void) {
  return !(botonZ() || abajoY() || arribaY() || adelanteX() || atrasX());
}
boolean botonZ(void) {
  readPosition();
  return (pos.z > sensibilityButton);
}

boolean abajoY(void) {
  readPosition();
  return pos.y < -sensibilityButton;
}

boolean arribaY(void) {
  readPosition();
  return pos.y > sensibilityButton;
}

boolean adelanteX(void) {
  readPosition();
  return pos.x < -sensibilityButton;
}

boolean atrasX(void) {
  readPosition();
  return pos.x > sensibilityButton;
}
