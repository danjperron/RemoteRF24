#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"


// verification si le rf24 recoit encore
unsigned long tempsValide = 0;
bool Valide = false;
#define RF24_TIMEOUT 10000
#define HALL_EFFECT_DEBOUNCE 100  //100ms debounce

const uint64_t RF24_REMOTE  =0xF0F0F0F0E1LL;
const uint64_t RF24_TAMIS   =0xF0F0F0F0D1LL;
const uint64_t RF24_CONE    =0xF0F0F0F0D2LL;
const uint64_t RF24_PRIMARY =0xF0F0F0F0D3LL;

const unsigned int nb_Aimant = 6;
int Relay1 = 4;
int Relay2 = 3;
int DataMsg[1];
int sensorState = 0;
unsigned int rpm;
unsigned long timeold;
volatile int rpmcount;
unsigned long rpmTime;


RF24 radio(8, 10);

// buffer pour transmission receptio
unsigned char Rcvdatapacket[2];
unsigned char Txmdatapacket[2];

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
  radio.setChannel(76);  // par 76 par defaux 
  radio.setRetries(15,15);
  radio.setPayloadSize(8);
  radio.stopListening();
  radio.openWritingPipe(RF24_REMOTE);
  radio.openReadingPipe(1,RF24_TAMIS);
  radio.startListening();
  radio.printDetails();
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
            digitalWrite(Relay1, HIGH);
            cycle_delay = millis();
            cycle = 2;
            break;
    case 2  : // attends 1 seconde
            if((millis() - cycle_delay) > 1000)
              {
                // ok coupe signal
                cycle_delay=millis();
                digitalWrite(Relay1, LOW);
                cycle = 3;                
              }
              break;
   case 3:   // attends 10 ms
            if((millis() - cycle_delay) > 10)
              {
                // ok rentre cylindre
                cycle_delay=millis();
                digitalWrite(Relay2, HIGH);
                cycle = 4;                
              }
              break;
    case 4  : // attends 1 seconde
            if((millis() - cycle_delay) > 1000)
              {
                // ok coupe signal
                cycle_delay=millis();
                digitalWrite(Relay2, LOW);
                cycle = 5;                
              }
              break;
   case 5:   // attends 10 ms
            if((millis() - cycle_delay) > 10)
              {
                cycle = 0; // c'est fini                
              }
              break;
  default:    cycle = 0;
              
  }
   
}


void loop() {
uint8_t pipe_number;
  if (rpmcount >= 1) {
    if((millis()-timeold) > HALL_EFFECT_DEBOUNCE)
    {
      // temps écoulé avec le dernier interrup ok donc c'est valide
       rpm = (60000 * rpmcount) / (nb_Aimant * (millis() - timeold)); //6 a changer si le nombre daimant est different

       printf("irq => rpm %d\n\r",rpm);
    }
    timeold = millis();
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
  
  
  if(radio.available(&pipe_number))
  {
  if(pipe_number==1)
  {
    // ok nous avons une connection
    // lisons l'info 
    if(radio.read(Rcvdatapacket,1))
    {
    // envoyons bac1k;
    UnitsOutput = Rcvdatapacket[0];
    radio.stopListening();
    radio.openWritingPipe(RF24_REMOTE);
 
    Txmdatapacket[0]= rpm;
    Txmdatapacket[1]= rpmTime;

    delay(10);
    radio.write(Txmdatapacket,2);
    delay(4);
    radio.openReadingPipe(0,RF24_CONE);
    radio.startListening();
    printf("recu %d\n",Rcvdatapacket[0]);
    printf("transmet %d %d\n",Txmdatapacket[0],Txmdatapacket[1]);
    
    tempsValide=millis();
    }
  }
  else
  {
    while(!radio.read(Rcvdatapacket,1));
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
