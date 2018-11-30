#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"


// variable pour le time out des LEDS
// indiquant une bonne communication des unités

unsigned long tempsValide = 0;
bool Valide = false;
#define RF24_TIMEOUT 10000

/* variable principale pour les sorties 
 *  l'information envoyer au remote est seulement un byte encoder par le bit définie pour chaque sortie
 *  donc nous avons le tamis, le cone et le primary
 */

#define PRIMARY_ON 1
#define CONE_ON  2
#define TAMIS_FORCE_OFF 8

unsigned int UnitsOutput= 0; // all off on boot

/* variable retourner par les rf24 satellites
 *  
 *  Le tamis retourne le RPM 
 *  Le cone  retourne le status du boutton avec le delay depuis le dernier changement
 *  
 */
#define RELAIS_LED 4
#define RELAIS_FEEDER 3
#define BOUTON_LEVIER 7


// premiere chose c'est de debouncer le boutton
#define DEBOUNCE_DELAY  25
bool currentBouton;
bool debounceBouton;   // debounce button value
unsigned long debounceTime; //en millisecond du changement du boutton

unsigned long boutonSteadyTime;  //millis() depuis que le bouton n'a pas changer   
// set radio pin 8 CE , pin 10 CS

RF24 radio(8, 10); 


 
const uint64_t RF24_REMOTE  =0xF0F0F0F0E1LL;
const uint64_t RF24_TAMIS   =0xF0F0F0F0D1LL;
const uint64_t RF24_CONE    =0xF0F0F0F0D2LL;
const uint64_t RF24_PRIMARY =0xF0F0F0F0D3LL;


unsigned char Rcvdatapacket[2];

unsigned char Txmdatapacket[2];
 
void setup()
{
  pinMode(RELAIS_FEEDER, OUTPUT);
  digitalWrite(RELAIS_FEEDER,HIGH);
  pinMode(RELAIS_LED, OUTPUT);
  digitalWrite(RELAIS_LED,HIGH);
  pinMode(BOUTON_LEVIER,INPUT);
  digitalWrite(BOUTON_LEVIER,HIGH);
  
  printf_begin();
  Serial.begin(115200);

  Serial.println("NRF24L01 Transmitter");

  radio.begin();
   radio.setChannel(76);  // par 76 par defaux 
  radio.setRetries(15,15);
  radio.setPayloadSize(8);
  radio.stopListening();
  radio.openWritingPipe(RF24_REMOTE);
  radio.openReadingPipe(1,RF24_CONE);
  radio.startListening();
  radio.printDetails();
  tempsValide=millis();
  
}


void loop() {
uint8_t pipe_number;

// verification du bouton
// avec du debounc pour etre certain de ne pas envoyer
// le rebonsissement des contacts lors du changement 
currentBouton = digitalRead(BOUTON_LEVIER);

if(currentBouton == debounceBouton)
{
   debounceTime= millis();
}
else
{
   if((millis() - debounceTime) > DEBOUNCE_DELAY)
    {
       debounceBouton=currentBouton;
       boutonSteadyTime=debounceTime;
    }
}


// attendons une connection
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
 
    Txmdatapacket[0]= debounceBouton;

    unsigned long tempEnSeconde = (millis() - boutonSteadyTime) / 1000L;

    if(tempEnSeconde > 100)
       tempEnSeconde = 100;
   
    Txmdatapacket[1]= tempEnSeconde;

    delay(6);
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

// mirror relay
digitalWrite(RELAIS_LED, debounceBouton ? LOW : HIGH);
digitalWrite(RELAIS_FEEDER, UnitsOutput & CONE_ON ? LOW : HIGH);
}
