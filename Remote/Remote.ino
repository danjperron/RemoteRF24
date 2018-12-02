#include <SPI.h>
#include "RF24.h"
#include "printf.h"

const uint8_t RF24_CANAL=83;

// pour tester
// permet d'enlever une unité spécifique pour vérifier le fonctionnement

#define ENABLE_TAMIS
#define ENABLE_CONE
//#undef ENABLE_TAMIS
//#undef ENABLE_CONE

#define ENABLE_PRIMARY
///undef ENABLE_PRIMARY

/*
 *  remote control utilisant un nrf24L01
 *  PUSH BUTTON DEFINITION
 *  
 */

#define SERIAL_DEBUG
//#undef SERIAL_DEBUG

 
const uint8_t BOUTON_STOP_ALL=2;
const uint8_t BOUTON_STOP_CONE=3;
const uint8_t BOUTON_START_CONE=4;
const uint8_t BOUTON_STOP_PRIMARY=6;
const uint8_t BOUTON_START_PRIMARY=7;




/* DEBOUNCE  INFO avec status des bouttons dans une seule variable
 *  un byte est utilisé avec les bits recevant l'info des bouttons
 *  currentBoutons => valeurs des bouttons actuelles
 *  LastBoutons    => valeurs passées  des bouttons 
 *  debounceBoutons =>  temps capturé de  millis() depuis un changement de boutton
 *
 *   si pas de changement sur les bouttons depuis  plus de DEBOUNCE_MIN then currentB = LastB
 */


// bit weight de chaque boutton


const uint8_t BIT_STOP_ALL=1;
const uint8_t BIT_STOP_CONE=2;
const uint8_t BIT_START_CONE=4;
const uint8_t BIT_STOP_PRIMARY=8;
const uint8_t BIT_START_PRIMARY=16;

uint8_t rpm=0;  // valeur en rpm que le tamis retourne
                // si 0 arrête tout


 // 50 ms debounce pour les bouttons semblent bon
const int DEBOUNCE_MIN=50;


 /*  
 *  Indicateur LED pour indiquer si les RF24 sont visibles.
 */

const uint8_t LED_PRIMARY=A0;
const uint8_t LED_TAMIS=A1;
const uint8_t LED_CONE=A2;

   

 // variable pour le debounce

unsigned long tempMilli;     // variable temporaire pour lecture millis()
unsigned char tempBoutons;    // variable temporaire pour lecture boutons
unsigned char currentBoutons= BIT_STOP_ALL;  //par défaux bouton 1 pressé (tout à OFF).
unsigned char lastBoutons=currentBoutons;  
unsigned long debounceBoutons = millis();          // debounce current time en milli sec;
unsigned long currentLapse= debounceBoutons;    // timer pour envoyer le data  toute les 0.1 sec.

// variable pour le time out des LEDS
// indiquant une bonne communication des unités

unsigned long tempsValide_PRIMARY = 0;
unsigned long tempsValide_CONE = 0;
unsigned long tempsValide_TAMIS = 0;

bool Valide_PRIMARY = false;
bool Valide_CONE = false;
bool Valide_TAMIS = false;

const unsigned long RF24_TIMEOUT=10000;

/* variable principale pour les sorties 
 *  l'information envoyer au remote est seulement un byte encoder par le bit définie pour chaque sortie
 *  donc nous avons le tamis, le cone et le primary
 */

uint8_t PRIMARY_ON=1;
uint8_t CONE_ON=2;
uint8_t TAMIS_ON=4;
uint8_t FORCE_TAMIS_OFF=8;

unsigned int UnitsOutput= 0; // all off on boot

/* variable retourner par les rf24 satellites
 *  
 *  Le tamis retourne le RPM 
 *  Le cone  retourne le status du boutton avec le delay depuis le dernier changement
 *  
 */



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


/* LireBoutons
 *  retourne valeur de chaque boutton dans une valeur binaire
 */

unsigned char lireBoutons(void)
{
    unsigned char bValue=0;
    if (digitalRead(BOUTON_STOP_ALL) == LOW)
       bValue = BIT_STOP_ALL;
    if (digitalRead(BOUTON_STOP_CONE) == LOW)
       bValue |= BIT_STOP_CONE;
    if (digitalRead(BOUTON_START_CONE) == LOW)
       bValue |= BIT_START_CONE;
    if (digitalRead(BOUTON_STOP_PRIMARY) == LOW)
       bValue |= BIT_STOP_PRIMARY;
    if (digitalRead(BOUTON_START_PRIMARY) == LOW)
       bValue |= BIT_START_PRIMARY;
    return bValue;
}


// decode les boutons
void decodeAction(void)
{          
#ifdef SERIAL_DEBUG
  printf("Decode Action  avant = %d  rpm=%d",UnitsOutput,rpm);
#endif

   if(currentBoutons & BIT_START_CONE)
   {
     if(rpm>0)
     {
      UnitsOutput |= CONE_ON;
#ifdef SERIAL_DEBUG
      printf("=== START CONE ===\n");
#endif      
   }
        else
        {
#ifdef SERIAL_DEBUG
      printf("=== START CONE IMPOSSIBLE RPM=0 ===\n");
#endif
          
        }

   
   }

   if(currentBoutons & BIT_START_PRIMARY)
   {
      if(rpm>0)
        {
         UnitsOutput |= PRIMARY_ON;
#ifdef SERIAL_DEBUG
      printf("=== START PRIMARY ===\n");
#endif
        }
        else
        {
#ifdef SERIAL_DEBUG
      printf("=== START PRIMARY IMPOSSIBLE RPM=0 ===\n");
#endif
          
        }
   }

   // maintenant les stop;

   if(currentBoutons & BIT_STOP_PRIMARY)
   {
      UnitsOutput &= ~PRIMARY_ON;
#ifdef SERIAL_DEBUG
      printf("=== STOP_PRIMARY ===\n");
#endif      
   }

   if(currentBoutons & BIT_STOP_CONE)
   {
      UnitsOutput &= ~CONE_ON;
#ifdef SERIAL_DEBUG
      printf("=== STOP CONE ===\n");
#endif 
   }
   
   if(currentBoutons & BIT_STOP_ALL)
   {
      UnitsOutput = FORCE_TAMIS_OFF; 
#ifdef SERIAL_DEBUG
      printf("=== STOP ALL ===\n");
#endif      
   }

#ifdef SERIAL_DEBUG
printf("apres = %d\n",UnitsOutput);
#endif      
}


unsigned char datapacket[3];



unsigned char SendInfo( uint64_t RF24_TX,unsigned char ID, unsigned char value)
{
  bool ok;
  uint8_t pipe_number;
  
#ifdef SERIAL_DEBUG
   printf("radio => %2lx%lx ",(uint32_t)(RF24_TX>>32),(uint32_t) (RF24_TX));
   printf("value = %d  ",value);
#endif   
   radio.stopListening(); 
   radio.openWritingPipe(RF24_TX);
   datapacket[0]=ID_REMOTE;
   datapacket[1]=UnitsOutput;
   ok=radio.write(datapacket,2);
 
  radio.startListening();
  // maintenant attentons pour un réponse
  unsigned long waitdelay = millis();
  while(1)
    {
      if(radio.available(&pipe_number))
        {
           if(pipe_number==1)
             {
               if(radio.read(datapacket,3))
                 {
                   if(datapacket[0] != ID)
                   {
                     #ifdef SERIAL_DEBUG
                       printf("wrong packet\n\r");
                     #endif
                   }
                  else
                  {
                    // ok nous avons eu une réponse  
                    #ifdef SERIAL_DEBUG
                    printf("return %d %d \n\r",datapacket[1],datapacket[2]);
                    #endif
                    break;
                  }
                 }

             }
            else
             {
              radio.read(datapacket,3);
             }          
        }
      
      if((millis() - waitdelay) > 50)
       {
        #ifdef SERIAL_DEBUG
         printf("return time out\n");
        #endif 
         radio.stopListening();
         return(0); // ok time out retourne 0
       }
    }
      radio.stopListening();
   return ok;
}



/*
 * Envoie à chaque unité l'information des sorties
 * 
 */
void syncInformation(void)
{
 delay(20);
#ifdef ENABLE_PRIMARY
  #ifdef SERIAL_DEBUG  
   printf("PRIMARY :");
  #endif
  // Envoi info au PRIMARY CRUSHER
   if( SendInfo(RF24_PRIMARY,ID_PRIMARY,UnitsOutput))
    {
      // reset time out
      tempsValide_PRIMARY=millis();
      Valide_PRIMARY=true;
    }
    else
    { // ok je n'ai pas réussi a communiquer avec le RF24_PRIMARY

      if((millis() - tempsValide_PRIMARY) >  RF24_TIMEOUT)
        {
              // ok time out sur la communication allons faire un stop complet
               UnitsOutput = FORCE_TAMIS_OFF;
               Valide_PRIMARY=false;
        }
    }
#endif
 

#ifdef ENABLE_CONE 
  #ifdef SERIAL_DEBUG
   printf("CONE :");
  #endif 
  // Envoi info au CONE_CRUSHER
    if(SendInfo(RF24_CONE,ID_CONE,UnitsOutput))
     {
         tempsValide_CONE=millis();
         Valide_CONE = true;
         // cone crusher retourne la valeur du boutton et le delay depuis son changement
         // datapacket[0] c'est ID_CONE
         // datapacket[1] c'est l'etat du bouton
         // datapacket[2] c'est le temps en secondes
         if(datapacket[1]==0)
           {

               // bouton non actif
               // donc primary OFF
               UnitsOutput &= ~PRIMARY_ON;
               // si ca fait plus de 10 secondes
               if(datapacket[2] > 10)
                {
                 UnitsOutput &= ~TAMIS_ON;
                 UnitsOutput |= FORCE_TAMIS_OFF;
                }
           }
           else
           {
              // bouton ACTIF
              // si ca fait plus de 5 secondes
              if(datapacket[2] > 5)
                 UnitsOutput |= PRIMARY_ON;
           }

           // si RPM == 0 stop tout anyway
           if(rpm == 0)
            UnitsOutput = FORCE_TAMIS_OFF;
     }
     else
     {
      if((millis() - tempsValide_CONE) >  RF24_TIMEOUT)
        {
              // ok time out sur la communication allons faire un stop complet
               UnitsOutput = FORCE_TAMIS_OFF;
               Valide_CONE = false;
         }
     }
#endif


#ifdef ENABLE_TAMIS
  #ifdef SERIAL_DEBUG
   printf("TAMIS :");
  #endif 
  // Envoi info au TAMIS  retourne   RPM
    if(SendInfo(RF24_TAMIS,ID_TAMIS,UnitsOutput))
     {
         UnitsOutput &= ~FORCE_TAMIS_OFF;  // une fois le force TAMIS OFF envoyer on le reset
         tempsValide_CONE=millis();
         Valide_TAMIS = true;
         // TAMIS retourne la valeur en RPM boutton 
         // datapacket[0] ID_TAMIS
         // datapacket[1] RPM
         // datapacket[2] 0
         rpm  = datapacket[1];
         if( rpm == 0)
          {
            // ok all off
            UnitsOutput = FORCE_TAMIS_OFF;
          }
     }
     else
     {
      if((millis() - tempsValide_CONE) >  RF24_TIMEOUT)
        {
              // ok time out sur la communication allons faire un stop complet
               UnitsOutput = 0;
               Valide_TAMIS = false;
        }
     }
#endif
}
     
 
void setup()
{
  pinMode(BOUTON_STOP_ALL, INPUT);
  pinMode(BOUTON_STOP_CONE, INPUT);
  pinMode(BOUTON_START_CONE, INPUT);
  pinMode(BOUTON_STOP_PRIMARY, INPUT);
  pinMode(BOUTON_START_PRIMARY, INPUT);
  digitalWrite(BOUTON_STOP_ALL, HIGH);
  digitalWrite(BOUTON_STOP_CONE, HIGH);
  digitalWrite(BOUTON_START_CONE, HIGH);
  digitalWrite(BOUTON_STOP_PRIMARY, HIGH);
  digitalWrite(BOUTON_START_PRIMARY, HIGH);
    
  pinMode(LED_PRIMARY, OUTPUT);
  pinMode(LED_TAMIS, OUTPUT);
  pinMode(LED_CONE, OUTPUT);
  
  digitalWrite(LED_PRIMARY, LOW);
  digitalWrite(LED_TAMIS, LOW);
  digitalWrite(LED_CONE, LOW);


  
  printf_begin();
  Serial.begin(115200);

  Serial.println("NRF24L01 Transmitter");

  radio.begin();
  radio.setChannel(RF24_CANAL);  // par 76 par defaux 
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(15,15);
  radio.setPayloadSize(8);
  radio.setPALevel(RF24_PA_HIGH);
  radio.openReadingPipe(1,RF24_REMOTE);
  radio.startListening();
  #ifdef SERIAL_DEBUG
  radio.printDetails();
  #endif
}


void loop() {

unsigned long cTimer = millis();

// maintenant vérifions les boutons
unsigned char _tBoutons = lireBoutons();

// est-ce la même valeur
 if(_tBoutons != lastBoutons)
    {
      // ok les boutons ne sont pas stable attendons
      debounceBoutons = cTimer;
      lastBoutons = _tBoutons;
    }
    else
    {
      // est-ce que les bouttons on changé depuis le debounce
      if(_tBoutons != currentBoutons)
      {
        // est-ce que le débounce est fait
        if((debounceBoutons - cTimer) > DEBOUNCE_MIN)
           {
              // ok debounce est fait donc 
              currentBoutons = _tBoutons;
              currentLapse = cTimer;
              // Decode Boutons Action
              decodeAction();
              syncInformation();
              currentLapse = cTimer;
           }
      }
    }





// Vérifier si cela fait plus de 1 seconde
// que nous avons pas envoyer . Si oui envoyons l'info

if( (cTimer - currentLapse) > 100)
  {

    #ifdef SERIAL_DEBUG
      printf("Units Output = %d  rpm=%d\n",UnitsOutput,rpm);
    #endif 
     // 1 seconde écoulée  envoyons.
    syncInformation();
    currentLapse = cTimer;
  }

// status des leds
// allumer veut dire que cela fonctionne
// flash veut dire pas de communication avec 

if(millis() & 256)
{
  digitalWrite(LED_PRIMARY, HIGH);
  digitalWrite(LED_TAMIS, HIGH);
  digitalWrite(LED_CONE, HIGH);  


  
}
else
{
  digitalWrite(LED_PRIMARY,Valide_PRIMARY);
  digitalWrite(LED_TAMIS, Valide_TAMIS);
  digitalWrite(LED_CONE, Valide_CONE);  
}

}
