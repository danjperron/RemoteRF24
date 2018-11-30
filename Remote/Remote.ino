#include <SPI.h>
#include "RF24.h"
#include "printf.h"



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

 
 #define BOUTON_STOP_ALL       2
 #define BOUTON_STOP_CONE      3
 #define BOUTON_START_CONE     4
 #define BOUTON_STOP_PRIMARY   6
 #define BOUTON_START_PRIMARY  7

/* DEBOUNCE  INFO avec status des bouttons dans une seule variable
 *  un byte est utilisé avec les bits recevant l'info des bouttons
 *  currentBoutons => valeurs des bouttons actuelles
 *  LastBoutons    => valeurs passées  des bouttons 
 *  debounceBoutons =>  temps capturé de  millis() depuis un changement de boutton
 *
 *   si pas de changement sur les bouttons depuis  plus de DEBOUNCE_MIN then currentB = LastB
 */


// bit weight de chaque boutton

 #define BIT_STOP_ALL       1
 #define BIT_STOP_CONE      2
 #define BIT_START_CONE     4
 #define BIT_STOP_PRIMARY   8
 #define BIT_START_PRIMARY  16

 // 50 ms debounce pour les bouttons semblent bon
 #define DEBOUNCE_MIN 50


 /*  
 *  Indicateur LED pour indiquer si les RF24 sont visibles.
 */

#define LED_PRIMARY A0
#define LED_TAMIS  A1
#define LED_CONE   A2

   

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

#define RF24_TIMEOUT 10000



/* variable principale pour les sorties 
 *  l'information envoyer au remote est seulement un byte encoder par le bit définie pour chaque sortie
 *  donc nous avons le tamis, le cone et le primary
 */

#define PRIMARY_ON 1
#define CONE_ON  2
#define TAMIS_ON 4
#define FORCE_TAMIS_OFF 8

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
printf("Decode Action  avant = %d",UnitsOutput);
   if(currentBoutons & BIT_START_CONE)
   {
      UnitsOutput |= CONE_ON;
      printf("=== START CONE ===\n");
   }

   if(currentBoutons & BIT_START_PRIMARY)
   {
      UnitsOutput |= PRIMARY_ON;
      printf("=== START PRIMARY ===\n");
   }

   // maintenant les stop;

   if(currentBoutons & BIT_STOP_PRIMARY)
   {
      UnitsOutput &= ~PRIMARY_ON;
      printf("=== STOP_PRIMARY ===\n");
   }

   if(currentBoutons & BIT_STOP_CONE)
   {
      UnitsOutput &= ~CONE_ON;
      printf("=== STOP CONE ===\n");
   }
   
   if(currentBoutons & BIT_STOP_ALL)
   {
      UnitsOutput = FORCE_TAMIS_OFF; 
      printf("=== STOP ALL ===\n");
   }

printf("apres = %d\n",UnitsOutput);      
}


unsigned char datapacket[2];


unsigned char SendInfo( uint64_t RF24_TX, unsigned char value)
{
  delay(5);
   // stop listening
   radio.stopListening();
   //  envoyer UnitsOutput au bon receiver
   printf("radio => %2lx%lx ",(uint32_t)(RF24_TX>>32),(uint32_t) (RF24_TX));
   printf("value = %d  ",value);
   radio.openWritingPipe(RF24_TX);
   bool ok = radio.write(&UnitsOutput,2);
   // start listening
   delay(1);
   radio.startListening();

    if (ok)
      printf("ok.....");
    else
      printf("failed.");
      
   
   // maintenant attentons pour un réponse
   unsigned long waitdelay = millis();
   while(!radio.available())
    {
      if((millis() - waitdelay) > 250)
       {
         printf("return time out\n");
         return(0); // ok time out retourne 0
       }
    }

  // ok nous avons eu une réponse 
  radio.read(datapacket,2);   
  printf("return %d %d \n\r",datapacket[0],datapacket[1]);
   return 1;
}



/*
 * Envoie à chaque unité l'information des sorties
 * 
 */
void syncInformation(void)
{

#ifdef ENABLE_PRIMARY  
   printf("PRIMARY :");
  // Envoi info au PRIMARY CRUSHER
   if( SendInfo(RF24_PRIMARY,UnitsOutput))
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
   printf("CONE :");
  // Envoi info au CONE_CRUSHER
    if(SendInfo(RF24_CONE,UnitsOutput))
     {
         tempsValide_CONE=millis();
         Valide_CONE = true;
         // cone crusher retourne la valeur du boutton et le delay depuis son changement
         // datapacket[0] c'est l'etat du bouton
         // datapacket[1] c'est le temps en secondes
         if(datapacket[0]==0)
           {

               // bouton non actif
               // donc primary OFF
               UnitsOutput &= ~PRIMARY_ON;
               // si ca fait plus de 10 secondes
               if(datapacket[1] > 10)
                {
                 UnitsOutput &= ~TAMIS_ON;
                 UnitsOutput |= FORCE_TAMIS_OFF;
                }
           }
           else
           {
              // bouton ACTIF
              // si ca fait plus de 5 secondes
              if(datapacket[1] > 5)
                 UnitsOutput |= PRIMARY_ON;
           }
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

   printf("TAMIS :");
  // Envoi info au TAMIS  retourne   RPM
    if(SendInfo(RF24_TAMIS,UnitsOutput))
     {
         UnitsOutput &= ~FORCE_TAMIS_OFF;  // une fois le force TAMIS OFF envoyer on le reset
         tempsValide_CONE=millis();
         Valide_TAMIS = true;
         // TAMIS retourne la valeur en RPM boutton 
         // datapacket[0] RPM
         // datapacket[1] 0
         if( datapacket[0] == 0)
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
  radio.setChannel(76);  // par 76 par defaux 
  radio.setRetries(15,15);
  radio.setPayloadSize(8);
  radio.openReadingPipe(1,RF24_REMOTE);
  radio.openWritingPipe(RF24_PRIMARY);
  radio.startListening();
  radio.printDetails();
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

if( (cTimer - currentLapse) > 1000)
  {
printf("Units Output = %d \n",UnitsOutput);
     
     // 1 seconde écoulée  envoyons.
    syncInformation();
    printf(".");
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
