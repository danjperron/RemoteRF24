#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"


// definition de l'activation des relays
#define RELAY_NC  HIGH
#define RELAY_NO  LOW 

const uint8_t RF24_CANAL=83;

#define SERIAL_DEBUG
//#undef SERIAL_DEBUG

// verification si le rf24 recoit encore
unsigned long tempsValide = 0;
bool Valide = false;
const unsigned long RF24_TIMEOUT=10000;
const unsigned long HALL_EFFECT_DEBOUNCE=100;  //100ms debounce

const uint64_t RF24_REMOTE  =0xF0F0F0F0E1LL;
const uint64_t RF24_TAMIS   =0xF0F0F0F0D1LL;
const uint64_t RF24_CONE    =0xF0F0F0F0D2LL;
const uint64_t RF24_PRIMARY =0xF0F0F0F0D3LL;



const uint8_t ID_REMOTE=0xE1;
const uint8_t ID_TAMIS=0xD1;
const uint8_t ID_CONE=0xD2;
const uint8_t ID_PRIMARY=0xD3;

const unsigned int nb_Aimant = 6;
int Relay1 = 4;
int Relay2 = 3;
int DataMsg[1];
int sensorState = 0;
unsigned int rpm=0;
unsigned long pulseEcart;  // le temps entre deux impulsions
unsigned long timeold;
volatile int rpmcount=0;
unsigned long rpmTime;


bool DrumRun=false;
bool DrumEnStop=false;
unsigned long DrumDelay;

RF24 radio(8, 10);

// buffer pour transmission receptio
unsigned char Rcvdatapacket[2];
unsigned char Txmdatapacket[3];




/* variable principale pour les sorties 
 *  l'information envoyer au remote est seulement un byte encoder par le bit définie pour chaque sortie
 *  donc nous avons le tamis, le cone et le primary
 */

uint8_t PRIMARY_ON=1;
uint8_t CONE_ON=2;
uint8_t TAMIS_ON=4;
uint8_t FORCE_TAMIS_OFF=8;
// output mirroir
unsigned int UnitsOutput= 0; // all off on boot



int cycle=0;
unsigned long cycle_delay;

void rpm_fun()
{
  rpmcount=1;

}

void setup()
{
  printf_begin();
// hall effect transistor doit etre sur la pin 2 pour interrupt 0
  pinMode(2,INPUT);
  digitalWrite(2,HIGH);
  attachInterrupt(0, rpm_fun, FALLING);
  pinMode(Relay1, OUTPUT);
  pinMode(Relay2, OUTPUT);
  Serial.begin(115200);
  radio.begin();
  radio.setChannel(RF24_CANAL);  // par 76 par defaux 
  radio.setRetries(15,15);
   radio.setPALevel(RF24_PA_HIGH);
  radio.setPayloadSize(8);
  radio.stopListening();
  radio.openWritingPipe(0);
  radio.openReadingPipe(1,RF24_TAMIS);
  radio.startListening();
  #ifdef SERIAL_DEBUG
    radio.printDetails();
  #endif
  tempsValide=millis();
  timeold=millis()-1;
  rpmcount=0;
  rpmTime=0;
}




void CylindreCycle(void)
{
  switch(cycle)
  {
    case 0 : break;  // ne fait rien
    case 1 : //sort cylindre
            digitalWrite(Relay1, RELAY_NO);
            cycle_delay = millis();
            cycle = 2;
            break;
    case 2  : // attends 1 seconde
            if((millis() - cycle_delay) > 1000)
              {
                // ok coupe signal
                cycle_delay=millis();
                digitalWrite(Relay1, RELAY_NC);
                cycle = 3;                
              }
              break;
   case 3:   // attends 10 ms
            if((millis() - cycle_delay) > 10)
              {
                // ok rentre cylindre
                cycle_delay=millis();
                digitalWrite(Relay2, RELAY_NO);
                cycle = 4;                
              }
              break;
    case 4  : // attends 1 seconde
            if((millis() - cycle_delay) > 1000)
              {
                // ok coupe signal
                cycle_delay=millis();
                digitalWrite(Relay2, RELAY_NC);
                cycle = 5;                
              }
              break;
   case 5:   // attends 10 ms
            if((millis() - cycle_delay) > 10)
              {
                cycle = 0; // c'est fini                
                DrumEnStop=true;
                DrumDelay=millis();
              }
              break;
  default:    cycle = 0;
              
  }
   
}


void loop() {
uint8_t pipe_number;



  CylindreCycle();
  if (rpmcount >= 1) {

     pulseEcart = (millis() - timeold);
     timeold = millis();
    // est-ce que l'écart entre les deux impulsion est trop longue
    if(pulseEcart < (60000 / nb_Aimant))  // pour empêcher une compte d'amorcer le ON
    if(pulseEcart > HALL_EFFECT_DEBOUNCE)
    {
      // temps écoulé avec le dernier interrup ok donc c'est valide
       rpm = (60000 * rpmcount) / (nb_Aimant * (millis() - timeold)); //6 a changer si le nombre daimant est different
#ifdef SERIAL_DEBUG
       printf("irq => rpm %d\n\r",rpm);
#endif
    }
    rpmcount = 0;
  }
  else
  {
// ok derniere impulsion remonte a quand
// si pas d'impulsion pour moins de 1 rpm set rpm=0
   rpmTime = millis() - timeold;

   if(rpmTime > (60000 / nb_Aimant))
     rpm = 0;

   // ajuste rpm time en seconde
   // pour raporter le temps en seconde depuis le dernier aimant
   if(rpmTime > 255000)
      rpmTime= 255000;  // ne peut pas depasser 255 secondes
    rpmTime/=1000;
   
  }


 // verifions si le tambour est en train de s'arrêter
  if(DrumEnStop)
    {
       // un petit delay d'une seconde pour assurer que mécaniquement
       // le drum est en arrêt
       if((millis()-DrumDelay) > 1000)
        {
           DrumEnStop=false;
           DrumRun=false;
           rpm=0;
        }
    }


  // si le tambour est en arret et que j'ai un RPM donc je roule
  
  if(rpm > 0)
    DrumRun= true;

  
  if(radio.available(&pipe_number))
  {
  if(pipe_number==1)
  {
    // ok nous avons une connection
    // lisons l'info 
    if(radio.read(Rcvdatapacket,2))
    {
    if(Rcvdatapacket[0]==ID_REMOTE)
    {  
    // envoyons bac1k;
    UnitsOutput = Rcvdatapacket[1];
    if(UnitsOutput & FORCE_TAMIS_OFF)
     if(DrumRun)  // est-ce que le drum roule
     {
       if(!DrumEnStop) // est-ce que le délais pour arrêt est 
         if(cycle==0) // est-ce que le cylindre est en standby
           cycle=1;  // ok fait l cycle
       DrumRun=false;
     }

    // ok encore besoin de faire la logique avec le rpm 

    Txmdatapacket[0]=ID_TAMIS;
    Txmdatapacket[1]= rpm;
    Txmdatapacket[2]= DrumRun;
    radio.stopListening();
    delay(5);
    radio.openWritingPipe(RF24_REMOTE);
    radio.write(Txmdatapacket,3);
    delay(1);
    radio.openWritingPipe(0);
    radio.startListening();
    #ifdef SERIAL_DEBUG
      printf("recu UnitsOutput= %d\n",Rcvdatapacket[1]);
      printf("transmet %d %d\n",Txmdatapacket[1],Txmdatapacket[2]);
    #endif
    tempsValide=millis();
    }
    }
  }
  else
  {
    radio.read(Rcvdatapacket,1);
  }
  }
  else
  {
    // pas de connection
    // time out
    if((millis()-tempsValide) > RF24_TIMEOUT)
    {
       // ok time out tout mettre a zero
       UnitsOutput =0;
   }
  
  }

  }
