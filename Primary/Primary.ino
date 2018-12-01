#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

const uint8_t RF24_CANAL=76;
// variable pour le time out des LEDS
// indiquant une bonne communication des unités

unsigned long tempsValide_PRIMARY = 0;

bool Valide_PRIMARY = false;

#define RF24_TIMEOUT 10000

/* variable principale pour les sorties 
 *  l'information envoyer au remote est seulement un byte encoder par le bit définie pour chaque sortie
 *  donc nous avons le tamis, le cone et le primary
 */

#define PRIMARY_ON 1
#define CONE_ON  2
#define TAMIS_ON 4

unsigned int UnitsOutput= 0; // all off on boot

/* variable retourner par les rf24 satellites
 *  
 *  Le tamis retourne le RPM 
 *  Le cone  retourne le status du boutton avec le delay depuis le dernier changement
 *  
 */


#define RELAIS 2


// set radio pin 8 CE , pin 10 CS

RF24 radio(8, 10); 


 
const uint64_t RF24_REMOTE  =0xF0F0F0F0E1LL;
const uint64_t RF24_TAMIS   =0xF0F0F0F0D1LL;
const uint64_t RF24_CONE    =0xF0F0F0F0D2LL;
const uint64_t RF24_PRIMARY =0xF0F0F0F0D3LL;


unsigned char datapacket[2];
  
 
void setup()
{
  pinMode(RELAIS, OUTPUT);
  digitalWrite(RELAIS,LOW);
    
  
   printf_begin();
  Serial.begin(115200);

  Serial.println("NRF24L01 Transmitter");

  radio.begin();
   radio.setChannel(RF24_CANAL);  // par 76 par defaux 
  radio.setRetries(15,15);
  radio.setPayloadSize(8);
  radio.setPALevel(RF24_PA_HIGH);
  radio.openReadingPipe(1,RF24_PRIMARY);
  radio.openWritingPipe(0);
  radio.startListening();
  radio.printDetails();
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
    //printf("radio disponible\n");
    if(radio.read(datapacket,1))
    {
    UnitsOutput = datapacket[0];
    // juste au cas ou ont clean le datapacket
    while(!radio.read(datapacket,1));
    radio.stopListening();
    radio.openWritingPipe(RF24_REMOTE);
    delay(20);    
//    for(retry=0;retry<5;retry++)
//     if(radio.write(datapacket,2)) break;   
    radio.write(datapacket,2);   
    radio.openWritingPipe(0);  
    printf("recu %d\n",datapacket[0]);
    tempsValide_PRIMARY=millis();
      radio.startListening();
    // envoyons back;
    }
    else
    printf("bad data\n");
  }
  else
  {
    Serial.print(".");
   while(radio.read(datapacket,1));
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
