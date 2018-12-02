#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define SERIAL_DEBUG
//#undef SERIAL_DEBUG

const uint8_t RF24_CANAL=83;
// variable pour le time out des LEDS
// indiquant une bonne communication des unités


unsigned long tempsValide_PRIMARY = 0;

bool Valide_PRIMARY = false;

const unsigned long RF24_TIMEOUT=10000;

/* variable principale pour les sorties 
 *  l'information envoyer au remote est seulement un byte encoder par le bit définie pour chaque sortie
 *  donc nous avons le tamis, le cone et le primary
 */

const uint8_t PRIMARY_ON=1;
//const uint8_t CONE_ON=2;
//const uint8_t TAMIS_ON=4;

unsigned int UnitsOutput= 0; // all off on boot

/* variable retourner par les rf24 satellites
 *  
 *  Le tamis retourne le RPM 
 *  Le cone  retourne le status du boutton avec le delay depuis le dernier changement
 *  
 */


const uint8_t RELAIS=2;


// set radio pin 8 CE , pin 10 CS

RF24 radio(8, 10); 


 
const uint64_t RF24_REMOTE  =0xF0F0F0F0E1LL;
const uint64_t RF24_TAMIS   =0xF0F0F0F0D1LL;
const uint64_t RF24_CONE    =0xF0F0F0F0D2LL;
const uint64_t RF24_PRIMARY =0xF0F0F0F0D3LL;



const uint8_t ID_REMOTE=0xE1;
const uint8_t ID_TAMIS=0xD1;
const uint8_t ID_CONE=0xD2;
const uint8_t ID_PRIMARY=0xD3;


unsigned char datapacket[3];
  
 
void setup()
{
  pinMode(RELAIS, OUTPUT);
  digitalWrite(RELAIS,LOW);
    
  
   printf_begin();
  Serial.begin(115200);

#ifdef SERIAL_DEBUG
  Serial.println("PRIMARY MODULE\n\r");
#endif
  radio.begin();
   radio.setChannel(RF24_CANAL);  // par 76 par defaux 
  radio.setRetries(15,15);
  radio.setPayloadSize(8);
  radio.setPALevel(RF24_PA_HIGH);
  radio.openReadingPipe(1,RF24_PRIMARY);
  radio.openWritingPipe(0);
  radio.startListening();
  #ifdef SERIAL_DEBUG
   radio.printDetails();
  #endif
  tempsValide_PRIMARY=millis();
  
}


void loop() {
int retry;
uint8_t pipe_num=1;
unsigned long cTimer = millis();

// attendons une connection
if(radio.available(&pipe_num))
 {
  if(pipe_num ==1)
  {
    
    // ok nous avons une connection
    // lisons l'info
    if(radio.read(datapacket,2))
    {
    if(datapacket[0]==ID_REMOTE)
    {  
    UnitsOutput = datapacket[1];
    radio.stopListening();
    radio.openWritingPipe(RF24_REMOTE);
    delay(5);    
    datapacket[0]=ID_PRIMARY;
    datapacket[1]=0;
    datapacket[2]=0;
    
    radio.write(datapacket,3);
    delay(1);   
    radio.openWritingPipe(0);
    #ifdef SERIAL_DEBUG  
      printf("Recu UnitsOutput =%d\n",UnitsOutput);
    #endif
    tempsValide_PRIMARY=millis();
      radio.startListening();
    // envoyons back;
    }
    }
    else
    {
      #ifdef SERIAL_DEBUG
        printf("bad data\n");
      #endif
    }
  }
  else
  {
   radio.read(datapacket,1);
  }
  }
  else
  {
    // pas de connection
    // time out
    if((millis()-tempsValide_PRIMARY) > RF24_TIMEOUT)
    {
       // ok time out tout mettre a zero
       UnitsOutput =0;
   }
  
  }

// mirror relay

digitalWrite(RELAIS, UnitsOutput & PRIMARY_ON ? LOW : HIGH);
}
