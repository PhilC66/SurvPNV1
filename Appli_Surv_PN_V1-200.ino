/*
	Philippe CORBEL
	24/06/2017
	
	07/05/2020
  Arduino IDE 1.8.10, AVR boards 1.8.1
  Le croquis utilise 24944 octets (77%) PC et Raspberry 24610 76%
	Les variables globales utilisent 1216 octets (59%) PC
  
  IDE 1.8.10 Raspi, AVR boards 1.8.1
	Le croquis utilise 24944 octets (77%)
	Les variables globales utilisent 1216 octets (59%) de mémoire dynamique

	25256,1207
	24800,1207 si suppression message aide ??

  --------------- a faire -----------------

  -----------------------------------------

  V1-202 07/05/2020 installé PN64 14/05/2020
  !!!!! Version carte SIM sans codePIN !!!!!
  suppression verif cnx reseau

  V1-202 19/11/2018 pas encore installé
  modification sms Majheure idem PNV2-1 et Autorail

	V1-201 25/06/2019
	ajout date et heure tous les messages
	
	V1-200 18/04/2018 installé PN64 le 07/05/2018
	1 - Ajout mesure tension Batterie2 8V
	2 - suppression message aide ??
	3 - NE MARCHE PAS bouclage sans arrivé a supprimer SMS ajout commande MAJHEURE, effectue "reset soft" SIM800 et lance mise a l'heure
	
	V1-101 07/12/2017 pas encore installé
	forcer remise à l'heure à chaque reception PSUTTZ sans verif année
	correction bug tension batterie
	
	V1-100 installé PN62 03/07/2017
	
	evolution du V1-22 Arduino UNO avec horloge et Alarme
	
	suppression Per_Ala et Per_Vie remplacé par Ala_Vie
	ici Ala_Vie simplifié en minute 0-59 apres 07h00 en dur
	Ala_Vie toujours consultable et parametrable
	
	
	Application Surveillance PN TPCF
	Philippe Corbel
	31/01/2017
	
	Version Stockage des N°Tel dans le Phone Book Carte SIM
	avec possibilité de changer la liste des N°Tel 9 maxi
	toujours garder la liste continue sans trou
	interdit de supprimer le premier(modif possible)
	Carte ADAFRUIT FONA 800 ID:2468, Module GSM SIM800
	Arduino UNO R3
	Carte proto shield		

	Surveillance des Alarmes Batterie et Secteur provenant du chargeur Batterie
	Mesure de la tension Batterie
	


	EEPROM
	adrr=0	byte Per_Ala plus utilisé 
	adrr=2	byte Ala_Vie (0-59)
	adrr=4	String 10c Id 
	adrr=15	boolean Alarme Intrusion 0/1 (pas utilisé)
		
	Ajout dans la librairie Adafruit_FONA.h
	fonction lire Nom Expediteur du SMS si existe dans Phone Book: 
															fona.getSMSSendername(slot, nameIDbuffer, 14)
	fonction lecture entrée du Phone Book :
															fona.getPhoneBookName(ligne, replybuffer,14) 14 max
															fona.getPhoneBookNumber(ligne, replybuffer,13) 40max
	fonction lire le Nom de l'Operateur
															fona.getNetworkName(replybuffer, 15);
	fonction lire etat carte SIM si Ready
															fona.getetatSIM() = true si Ready

	Modification TimeAlarms.h	reduction mem utilisé
	"TimeAlarms.h" entre guillemets pour utilisé le fichier se trouvant dans le meme repertoire que .ino
		#define dtNBR_ALARMS 3   6 à l'origine nombre d'alarmes RAM*11 max is 255
*/

String ver="V1-202";

#include <Adafruit_FONA.h>
#include <EEPROM.h>							// variable en EEPROM
#include <EEPROMAnything.h>			// variable en EEPROM
#include "Time.h"								// gestion Heure
#include "TimeAlarms.h"					// gestion des Alarmes modifié 3 alarmes
#include "coeff.h"							// coefficient mesure tension


#define FONA_RX 2					//	RX SIM800
#define FONA_TX 3					//	TX SIM800
#define FONA_RST 4				//	Reset SIM800
#define FONA_RI 6					//	Ring SIM800


#define Ip_AlaBatt 9			//	Entrée Alarme Batterie chargeur
#define Ip_AlaSecteur 10	//	Entrée Alarme Secteur

/* 
	Entrée A1 Mesure Tension Batterie 24V R33k/3.3k => 30V/2.73V
	Vref externe 2.889V R5.1k entre 3.3V et Vref
	Entrée A0 Mesure Tension Batterie 8V R33k/3.3k
 */

/*mise en mémoire flash des messages  max 31c membuff[30]*/
/* const char mem_0[] PROGMEM = "NON";								//4											
const char mem_1[] PROGMEM = "OUI";								//4													
//const char mem_2[] PROGMEM = "RING";							//1
const char* const mem_table[] PROGMEM = {mem_0, mem_1}; */

char Telbuff[14];								//	Buffer Numero tel
String fl = "\n";								//	saut de ligne SMS


String Id ;											// Id du PN sera lu dans EEPROM addr=4

char replybuffer[255];					// buffer de reponse FONA

#include <SoftwareSerial.h>			// liaison serie FONA SIM800
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

uint8_t readline(char *buff, uint8_t maxbuff, uint16_t timeout = 0);

String 	message;										//	Texte des SMS envoyé en reponse
char 		fonaInBuffer[64];    				//	for notifications from the FONA
boolean FlagAlarmeBatt = false;			//	Alarme Batterie
boolean FlagAlarmeSect = false;			//	Alarme Secteur
boolean FlagReset = false;					//	Reset demandé=True
long   	VBatterie1;									//	Tension Batterie PN 24Vnom (22-29)
long   	VBatterie2;									//	Tension Batterie PN 8Vnom  (7.3-9.6)
boolean FlagAlarmeTension = false;	//	Alarme tension Batterie
byte 		Ala_Vie = 2;								//	Heure envoie signe de vie(EEPROM) en minute apres 8h00
char 		Id_ecrire[11];							//	buffer ecriture EEPROM nouvel Id
boolean FlagLastAlarmeBatt = false;	//	verification derniere alarme
boolean FlagLastAlarmeSect = false;
boolean FlagLastAlarmeTension = false;
byte    Ntwk_dcx = 0;											//	compteur deconnexion reseau

/* Identification des Alarmes*/
//AlarmId FirstMessage;		// 0 tempo lancement premier message au demarrage
AlarmId loopPrincipale;	// 1 boucle principlae
AlarmId Svie;						// 2 tempo Signal de Vie
AlarmId MajH;						// 3 tempo mise à l'heure régulière
//---------------------------------------------------------------------------
void setup(){
	analogReference(EXTERNAL);// reference Analog 3.3V au travers de 5.1K  Vref=2.85V, 1023=31.32V
  pinMode(Ip_AlaBatt, INPUT_PULLUP);			//	Entrée Alarme Batterie
  pinMode(Ip_AlaSecteur, INPUT_PULLUP);		//	Entrée Alarme Secteur
	
	message.reserve(140);
  while (!Serial);
  Serial.begin(9600);											//	Liaison Serie PC ex 115200
	Serial.print(F("Version Soft : ")),Serial.println(ver);

Serial.print(F("Coeff 1 = ")),Serial.println(CoeffTension1);
Serial.print(F("Coeff 2 = ")),Serial.println(CoeffTension2);
	
  //Serial.println(F("Lancement Application "));
  //Serial.println(F("Initialisation Module GSM....(May take 3 seconds)"));

  fonaSerial->begin(4800);								//	Liaison série FONA SIM800 ex 4800
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
		while (1);
  }
  Serial.println(F("FONA is OK"));
//***************************Lecture EEPROM*********************************
	// EEPROM_readAnything(0,Per_Ala);	//	Lecture valeur en addr=0
	// Alarm.delay(10);
	EEPROM_readAnything(2,Ala_Vie);	//	Lecture valeur en addr=2
	Alarm.delay(10);
	ReadEEPROM(4);									//	Lecture String en adrr=4
	Id += fl;
	Alarm.delay(10);
	// Serial.print(F("lu en 0=")), Serial.println(Per_Ala);
	// Serial.print(F("lu en 2=")), Serial.println(Ala_Vie);
	// Serial.print(F("Id lu=")), Serial.println(Id);	
//***************************--------------*********************************	
  // Print SIM card IMEI number.
  char imei[15] = {0}; // MUST use a 16 character buffer for IMEI!
  uint8_t imeiLen = fona.getIMEI(imei);
  // if (imeiLen > 0) {
    // Serial.print(F("SIM card IMEI: ")); Serial.println(imei);
  // }
  Serial.println(F("FONA Ready"));

  flushSerial();
	if(!fona.getetatSIM()){	// Si carte SIM not READY, Envoyé PIN 
		flushSerial();
		char PIN[5] = "1234";
		byte retries = 1;
		if (! fona.unlockSIM(PIN)) {
			Serial.println(F("Failed to unlock SIM"));
			retries++;
			if (retries == 3) {
				goto sortie;					// 2 tentatives max
			}
		} else {
			Serial.println(F("OK SIM Unlock"));
		}
	sortie:
		Alarm.delay(1000);							//	Attendre cx reseau apres SIM unlock
	}
	byte n = 0;
	byte cpt = 0;
	do {												// boucle tant que reseau pas connecté
		Alarm.delay(2000);
    n = fona.getNetworkStatus();
		cpt ++;	
		if(cpt > 20) break;// sortie si 20 tentatives demarrage sans reseau	
  } while (n!=1 && n!=5);
	Serial.print(F("Network status "));
	Serial.print(n);
	// Serial.print(F(": "));
	if (n == 0) Serial.println(F("Not registered"));
	if (n == 1) Serial.println(F("Registered (home)"));
	if (n == 2) Serial.println(F("Not registered (searching)"));
	if (n == 3) Serial.println(F("Denied"));
	if (n == 4) Serial.println(F("Unknown"));
	if (n == 5) Serial.println(F("Registered roaming"));
		
  flushSerial();
	// Demande Operateur connecté
	fona.getNetworkName(replybuffer, 15);
	Serial.print(F("Operateur :")),Serial.println(replybuffer);

  message = "";
	read_RSSI();	// Niveau
  Serial.println(message);
	message = "";
	
	MajHeure();

  loopPrincipale = Alarm.timerRepeat(15, Acquisition); // boucle principale 15s
  Alarm.enable(loopPrincipale);
  MajH = Alarm.timerRepeat(600, MajHeure);						// toute les heures  type=1
  Alarm.enable(MajH);
  Svie = Alarm.alarmRepeat((25200 + Ala_Vie * 60), SignalVie);// chaque jour à 07h02=25320 type=3
  //Serial.print(F("Alarme vie =")),Serial.println(Alarm.read(Svie));
  Alarm.enable(Svie);	
	
	Serial.print(F("FreeRAM = ")),Serial.println(freeRam());
	
}	//fin setup
//---------------------------------------------------------------------------
void loop(){

  // Attente donnée en provenance ds SIM800
	
	String bufferrcpt;
  char * bufPtr = fonaInBuffer;	//handy buffer pointer
  if (fona.available())      		//any data available from the FONA?
  {
    byte slot = 0;            	//this will be the slot number of the SMS
    unsigned int charCount = 0;
    //Read the notification into fonaInBuffer
    do  {
      *bufPtr = fona.read();
      bufferrcpt += *bufPtr;
      Serial.write(*bufPtr);
      Alarm.delay(1);
    } while ((*bufPtr++ != '\n') && (fona.available()) && (++charCount < (sizeof(fonaInBuffer) - 1)));
    //Add a terminal NULL to the notification string
    *bufPtr = 0;
    // if (charCount > 1) {
      // Serial.print(F("Buffer ="));
      // Serial.println(bufferrcpt);
    // }
    // Si appel entrant on raccroche
    if ((bufferrcpt.indexOf(F("RING"))) == 0) {	// RING, Ca sonne
      //Serial.println(F("Ca sonne!!!!"));
      fona.hangUp();											// on raccroche
    }
		
    if ((bufferrcpt.indexOf(F("PSUTTZ"))) >= 0 ) { // V1-101 rattrapage si erreur mise à la date
      // *PSUTTZ: 2017,5,27,16,19,54,"+8",1
			//Serial.println(F("Relance mise a l'heure !"));
			MajHeure();
			//FlagReset = true;	// on force redemarrage pour prendre la bonne date/time
    }
    // Scan the notification string for an SMS received notification.
    // If it's an SMS message, we'll get the slot number in 'slot'
    if (1 == sscanf(fonaInBuffer, "+CMTI: \"SM\",%d", &slot)) {
      traite_sms(slot);
    }
  }
	Alarm.delay(100);
}
//---------------------------------------------------------------------------
void Acquisition(){
	// verification si toujours connecté au réseau		
	// byte n = fona.getNetworkStatus();
		//Serial.print(F("netwkstatus=")),Serial.println(n);		
	// if(n != 1 && n != 5){
		// Ntwk_dcx++;
		// if (Ntwk_dcx > 20){ // 20x15s=5mn
			// Serial.println(F("Pas de reseau !"));
			// softReset();					//	redemarrage Arduino apres 5mn
		// }
	// }
	// else{
		// if(Ntwk_dcx > 0) Ntwk_dcx --;
	// }
	static byte nalaBatt=0;					// compteur alarme consecutive
	// if (!digitalRead(Ip_AlaBatt)) {// supprimé en V1-200
		// nalaBatt++;
			// if(nalaBatt==4){
				// FlagAlarmeBatt = true;
				// nalaBatt=0;
			// }
		// Serial.println(F("Alarme Batterie"));
	// }
	// else{																	// V1-2
		// FlagAlarmeBatt=false;
		// if(nalaBatt>0)nalaBatt--;						//	efface progressivement le compteur
	// }		
	static byte nalaSecteur=0;
	if (digitalRead(Ip_AlaSecteur)) {// V1=200 entree invesée avec relais presence secteur externe
		nalaSecteur ++;
			if(nalaSecteur==4){
				FlagAlarmeSect = true;
				nalaSecteur=0;
			}			
		Serial.println(F("Alarme Secteur"));
	}
	else{																	// V1-2
		FlagAlarmeSect = false;
		if(nalaSecteur>0)nalaSecteur--;			//	efface progressivement le compteur
	}		
	static byte nalaTension=0;
	VBatterie1 = map(moyenneAnalogique(1),0,1023,0,CoeffTension1);// V1-200
	VBatterie2 = map(moyenneAnalogique(0),0,1023,0,CoeffTension2);// V1-200
	Serial.print(F("VBatterie :")),Serial.print(VBatterie1),Serial.print(";"),Serial.println(VBatterie2);
	if (VBatterie1 < 2200 || VBatterie1 > 2900 || VBatterie2 < 783 || VBatterie2 > 940) { //V1-200
		// Serial.println(F("Alarme batterie"));
		nalaTension ++;
		if(nalaTension==4){
			FlagAlarmeTension = true;
			nalaTension=0;
		}
	}
	else{																	// V1-2
		FlagAlarmeTension= false;
		if(nalaTension>0)nalaTension--;			//	efface progressivement le compteur
	}
	
	// verification nombre SMS en attente(raté en lecture directe)
	int8_t smsnum = fona.getNumSMS();
	Serial.print(F("Sms en attente =")), Serial.println (smsnum);

	if (smsnum > 0) {	// nombre de SMS en attente
		// il faut les traiter
		traite_sms(51);// demande traitement de tous les SMS en attente
	}
	else if (smsnum == 0 && FlagReset) { // on verifie que tous les SMS sont traités avant Reset
		FlagReset = false;
		softReset();					//	redemarrage Arduino
	}
	envoie_alarme();
	displayTime(false);
  Serial.print(F("freeRAM = ")), Serial.println(freeRam());
}
//---------------------------------------------------------------------------
void MajHeure(){
  /* module identique toutes version
		procedure appelée toute les heures
		parametrage du SIM800 a faire une fois
  	AT+CLTS? si retourne 0
  	AT+CLTS=1
		AT+CENG=3
  	AT&W pour sauvegarder ce parametre
  	si AT+CCLK? pas OK
  	avec Fonatest passer en GPRS 'G', envoyer 'Y' la sync doit se faire, couper GPRS 'g'
  	't' ou AT+CCLK? doit donner la date et heure réseau
  	format date retourné par Fona "yy/MM/dd,hh:mm:ss±zz",
  	+CCLK: "14/08/08,02:25:43-16" -16= décalage GMT en n*1/4heures(-4) */
  static boolean First = true;
  //Serial.print(F("getTriggeredAlarmId()MajH="));
  //Serial.println(Alarm.getTriggeredAlarmId());
  int N_Y;	// variable tempo  New_Year
  int N_M;
  int N_D;
  int N_H;
  int N_m;
  int N_S;	
	char buffer[23];
	fona.getTime(buffer, 23);  // demande heure réseau : AT+CCLK?
	String FonaHDtate = buffer;
	//if(First) FonaHDtate = "14/10/20,11:20:00-0   ";// pour test	
	//Serial.print("buffer first="),Serial.println(buffer);
	//Serial.print("String="),Serial.println(FonaHDtate);
	
	// convertir format date time yy/mm/dd,hh:mm:ss
	byte i 	= FonaHDtate.indexOf("/");
	byte j 	= FonaHDtate.indexOf("/", i + 1);
	N_Y			= FonaHDtate.substring(i - 2, i).toInt();
	N_M 		= FonaHDtate.substring(i + 1, j).toInt();
	N_D 		= FonaHDtate.substring(j + 1, j + 3).toInt();
	i 	  	= FonaHDtate.indexOf(":", 6);
	j     	= FonaHDtate.indexOf(":", i + 1);
	N_H 		= FonaHDtate.substring(i - 2, i).toInt();
	N_m 		= FonaHDtate.substring(i + 1, j).toInt();
	N_S 		= FonaHDtate.substring(j + 1, j + 3).toInt();
	
  //Serial.print(N_H),Serial.print(":"),Serial.print(N_m),Serial.print(":"),Serial.print(N_S),Serial.print(" ");
  //Serial.print(N_D),Serial.print("/"),Serial.print(N_M),Serial.print("/"),Serial.println(N_Y);
  Serial.print(F("Mise a l'heure reguliere !, "));

  if (First) {															// premiere fois apres le lancement
    setTime(N_H, N_m, N_S, N_D, N_M, N_Y);	// mise à l'heure de l'Arduino
    First = false;
  }
  else {	
    //  calcul décalage entre H sys et H reseau en s
		int ecart = (N_H - hour()) * 3600;	
		ecart += (N_m - minute()) * 60;
		ecart += N_S - second();
    Serial.print(F("Ecart s= ")), Serial.println(ecart);
		
    if (abs(ecart) > 5) {
			Alarm.disable(loopPrincipale);
			Alarm.disable(Svie);
			Alarm.disable(MajH);
				setTime(N_H, N_m, N_S, N_D, N_M, N_Y);
				
				//adjustTime(ecart);	// correction heure interne Arduino
			Alarm.enable(loopPrincipale);
			Alarm.enable(MajH);
			Alarm.enable(Svie);
      //Serial.print(F("Correction seconde = ")), Serial.println(ecart);
    }
  }
  displayTime(false);
}
//---------------------------------------------------------------------------
void SignalVie(){

	Ntwk_dcx=0;					// reset compteur deconnexion reseau
	
	envoieGroupeSMS();	// envoie groupé V1-2
	fonaSS.println(F("AT+CMGDA=\"DEL ALL\""));// au cas ou, efface tous les SMS envoyé/reçu
}
//---------------------------------------------------------------------------
void traite_sms(byte slot){	// traitement du SMS par slot
  Serial.print(F("slot: ")); Serial.println(slot);
  uint16_t smslen;
  String textesms;
  char callerIDbuffer[13];  //we'll store the SMS sender number in here
	char nameIDbuffer[15];  	//nom expediteur SMS si existe dans Phone Book
  byte i;
  byte j;
  if (slot == 51) { // demande de traitement des SMS en attente (50 nbr slot max)
    i = 1;
    j = 50;
  } else {
    i = slot;
    j = slot;
  }
  for (byte k = i; k <= j; k++) {
    slot = k;
    // Retrieve SMS sender address/phone number.
    if (! fona.getSMSSender(slot, callerIDbuffer, 13)) {
      Serial.println(F("Didn't find SMS message in slot!"));
      continue;	//	Next k
    }
		fona.getSMSSendername(slot, nameIDbuffer, 14);
    //Serial.print(F("Nom appelant:")),Serial.println(nameIDbuffer);		
		fona.readSMS(slot, replybuffer, 250, &smslen);

		textesms = String(replybuffer);
    Serial.print(F("texte du SMS=")), Serial.println(textesms);
			for(byte i=0; i<textesms.length();i++){
				if((int)textesms[i]<0 || (int)textesms[i]>127){// caracteres accentués interdit
					goto sortir;							
				}
			}
      if ( String(nameIDbuffer).length() > 0){// nom appelant existant
				//Envoyer une réponse
				//Serial.println(F("Envoie reponse..."));
				messageId();
				if (!(textesms.indexOf(F("TEL")) ==0 || textesms.indexOf(F("tel")) ==0 || textesms.indexOf(F("Tel"))==0)){
					textesms.toUpperCase();		// passe tout en Maj sauf si "TEL"
					textesms.replace(" ","");	// supp tous les espaces
				}
				else{

				}
				// Serial.print(F("texte du SMS txt =")), Serial.println(textesms);
				// Serial.print(F("texte du SMS char=")), Serial.println(replybuffer);
				// if (textesms.indexOf(F("??")) ==0) {	//	Aide "??"
					// message += F("Liste des commandes :");	
					// message += fl ;
					// message += F("Etat PN : PN");
					// message += fl;									
					// message += F("Etat SYS : SYS");			
					// message += fl;	
					// message += F("Reset Alarme/Sys :RST");
					// message += fl;	
					// message += F("Vie (0-59') >07h00");
					// sendSMSReply(callerIDbuffer);	// SMS n°1
					
					// messageId();
					// message += F("Nouvel Id : Id= (max 10c)");
					// message += fl;				
					// message += F("List Num Tel:LST?");
					// message += fl;
					// message += F("Nouveau Num Tel: ");
					// message += fl;
					// message += F("Tel=+33612345678,") ;
					// message += fl;
					// message += F("Nom(max 14c)");
					// sendSMSReply(callerIDbuffer);	// SMS n°2
				// }
				
				if (textesms.indexOf(F("TEL")) == 0 
					|| textesms.indexOf(F("Tel")) == 0 
					|| textesms.indexOf(F("tel")) == 0){// entrer nouveau num
					boolean FlagOK=true;
					byte j=0;					
					String Send	= F("AT+CPBW=");	// ecriture dans le phone book
					if(textesms.indexOf("=")== 4){// TELn= reserver correction/suppression
						int i = textesms.substring(3).toInt();// recupere n° de ligne
						i=i/1;// important sinon i ne prend pas sa valeur dans les comparaison?
						//Serial.println(i);
						if (i < 1) FlagOK=false;
						Send += i;
						j=5;
						// on efface la ligne sauf la 1 pour toujours garder au moins un numéro
						if( (i!=1) && (textesms.indexOf(F("efface")) == 5 
						|| textesms.indexOf(F("EFFACE")) == 5 )) goto fin_tel;
					}
					else if (textesms.indexOf("=")== 3){// TEL= nouveau numero
						j=4;
					}
					else
					{
						FlagOK=false;
					}
					if(textesms.indexOf("+")== j){			// debut du num tel +
						if(textesms.indexOf(",")== j+12){	// verif si longuer ok
							String numero = textesms.substring(j,j+12);
							String nom    = textesms.substring(j+13,j+27);	// pas de verif si long<>0?							
							Send += F(",\"");
							Send += numero;
							Send += F("\",145,\"");
							Send += nom;
							Send += F("\"");
						}
						else
						{
							FlagOK=false;
						}
					}
					else
					{
						FlagOK=false;
					}
				fin_tel:	
					if(!FlagOK){// erreur de format
						//Serial.println(F("false"));
						messageId();
						message += F("Commande non reconnue ?");// non reconnu
						sendSMSReply(callerIDbuffer);						// SMS non reconnu
					}
					else{
						Serial.println(Send);
						fonaSS.println(Send.c_str());	
						Alarm.delay(500);
						fonaSS.println(F("AT+CMGF=1"));					//pour purger buffer fona
						Alarm.delay(500);
						message  = Id;
						message += F("Nouveau Num Tel: ");
						message += F("OK");
						sendSMSReply(callerIDbuffer);
					}					
				}	
				else if (textesms == F("LST?"))	//	Liste des Num Tel
				{
					messageId();
					for(byte i=1;i<10;i++){
						char name[15];
						char num[14];
						if(!fona.getPhoneBookName(i, name,14)){// si existe pas sortir
						//Serial.println("Failed!");// next i
						goto fin_i;
						}
						fona.getPhoneBookNumber(i, num,13);
						message += String(i) + ":";
						message += String(name);
						message += "," + fl;
						message += String(num);
						message += fl;
						if((i%3)==0){
						sendSMSReply(callerIDbuffer);// envoi sur plusieurs SMS
						//Serial.println(message);
						messageId();
						}
					}
					fin_i:
					if(message.length()>Id.length()+20)sendSMSReply(callerIDbuffer);// SMS final (V1.1)
					//Serial.println(message);
				}
				else if (textesms.indexOf(F("PN")) == 0 || textesms.indexOf(F("ETAT")) == 0 || textesms.indexOf(F("ST")) == 0) {	// "ETAT PN?"
					generationMessage();
					//Serial.println(message);
					sendSMSReply(callerIDbuffer);
				}
				else if (textesms.indexOf(F("SYS")) == 0){				//	Etat Systeme
					messageId();
					flushSerial();
					fona.getNetworkName(replybuffer, 15);		// Operateur
					//Serial.println(replybuffer);
					flushSerial();
					byte n = fona.getNetworkStatus();
						if (n==5){														// Operateur roaming
							message += F("rmg, ");							// roaming
							message += replybuffer + fl;
						}
						 else
						{
							message += replybuffer + fl; 				// Operateur
						}
					read_RSSI();														// info RSSI seront ajoutées à message					
					uint16_t vbat;
					uint16_t vpct;
					flushSerial();					
					fona.getBattVoltage(&vbat);
					fona.getBattPercent(&vpct);
					message += F("Vbat = ");
					message	+= String(vbat);
					message += F(" mV, ");
					message += String(vpct) + "%" + fl;

					message += F("freeRAM=");
					message += String(freeRam()) + fl;
					message += F("Ver:");
					message += ver + fl;
					displayTime(true);
					sendSMSReply(callerIDbuffer);
				}
				else if (textesms.indexOf(F("ID=")) == 0)			//	Id= nouvel Id
				{
					String temp = textesms.substring(3);
					if (temp.length() > 0 && temp.length() < 11) {
						Id="";
						temp.toCharArray(Id_ecrire, 11);
						EEPROM_writeAnything(4,Id_ecrire);	//	ecriture EEPROM
						Alarm.delay(400);
						ReadEEPROM(4);														//	lecture EEPROM adrr=4
						Alarm.delay(10);
						Id += fl;
					}
					messageId();
					message += F("Nouvel Id");
					sendSMSReply(callerIDbuffer);
				}

      else if (textesms.indexOf(F("VIE")) == 0) {			//	Heure Message Vie
        //if ((textesms.indexOf("?") == 3) || (textesms.indexOf(char(61))) == 3) { //char(61) "="
          if ((textesms.indexOf(char(61))) == 3) {
            long i = atol(textesms.substring(4).c_str()); //	Heure message Vie
            if (i > 0 && i <= 59) {										//	ok si entre 0 et 59
              Ala_Vie = i;
              byte n=EEPROM_writeAnything(2,Ala_Vie);		//	ecriture EEPROM
							delay(400);															// sauvegarde en EEPROM
              //Serial.println(25200 + Ala_Vie * 60);
							Svie = Alarm.alarmRepeat((25200 + Ala_Vie * 60), SignalVie);	// init tempo
            }
          }
          message += F("Heure Vie = ");
          message += int((25200 + Ala_Vie * 60)  / 3600);
          message += ":";
          message += int(((25200 + Ala_Vie * 60) % 3600) / 60);
          message += F("(hh:mm)");
          sendSMSReply(callerIDbuffer);
        //}
      }			
       else if (textesms.indexOf(F("MAJHEURE")) == 0) {	//	forcer mise a l'heure
         char datebuffer[21];
         fona.getSMSdate(slot, datebuffer, 20);
         String mytime = String(datebuffer).substring(0,20);
         // Serial.print(F("heure du sms:")),Serial.println(mytime);
         String _temp = F("AT+CCLK=\"");
         _temp += mytime + "\"\r\n";
         // Serial.print(_temp);
         fona.print(_temp);;// mise a l'heure SIM800
         Alarm.delay(100);
         MajHeure();			// mise a l'heure

         sendSMSReply(callerIDbuffer);
      }
			else if (textesms == F("RST")){								// demande RESET
					message += F("Le systeme va etre relance");	// apres envoie du SMS!
					FlagReset = true;														// reset prochaine boucle
					sendSMSReply(callerIDbuffer);
			}
			else{
				message += F("Commande non reconnue ?");		//"Commande non reconnue ?"
				sendSMSReply(callerIDbuffer);
			}
				//Serial.print(message);
		}
		else{
				Serial.print(F("Appelant non reconnu ! ")), Serial.println(callerIDbuffer);
		}
		sortir:	
    // Suppression du SMS
    flushSerial();
		if (fona.deleteSMS(slot)) {
      Serial.print(F("OK message supprime, slot=")), Serial.println(slot);
    } else {
      Serial.print(F("Impossible de supprimer slot=")), Serial.println(slot);
    }
  }
}
//---------------------------------------------------------------------------
void envoie_alarme(){
	static boolean SendEtat = false;	//	V1-2
	//	V1-2
	if(FlagAlarmeSect!=FlagLastAlarmeSect){// si nouvelle alarme on envoie Etat
		SendEtat=true;
		FlagLastAlarmeSect=FlagAlarmeSect;
	}
	if(FlagAlarmeBatt!=FlagLastAlarmeBatt){
		SendEtat=true;
		FlagLastAlarmeBatt=FlagAlarmeBatt;
	}
	if(FlagAlarmeTension!=FlagLastAlarmeTension){
		SendEtat=true;
		FlagLastAlarmeTension=FlagAlarmeTension;
	}
	if(SendEtat){ 					// si envoie Etat demandé
		envoieGroupeSMS();		// envoie groupé V1-2
		SendEtat=false;				// efface demande
	}
}
//---------------------------------------------------------------------------
void read_RSSI(){	// lire valeur RSSI et remplir message
  int r;
	byte n = fona.getRSSI();
  // Serial.print(F("RSSI = ")); Serial.print(n); Serial.print(": ");
  if (n == 0) r = -115;
  if (n == 1) r = -111;
  if (n == 31) r = -52;
  if ((n >= 2) && (n <= 30)) {
    r = map(n, 2, 30, -110, -54);
  }
	message += F("RSSI=");
	message += String(n);
	message += ", ";
	message += String(r);
	message += F("dBm");
	message += fl;
}
//---------------------------------------------------------------------------
void envoieGroupeSMS(){	//	V1-2
	for (byte Index = 1; Index < 10; Index++) {		// Balayage des Num Tel Autorisés=dans Phone Book
	
		if(!fona.getPhoneBookNumber(Index, Telbuff,13)){// lire Phone Book
			//Serial.print(Index),Serial.println(F("Failed!"));
			break;
		}
		//Serial.print(F("Num :  ")), Serial.println(Telbuff);
		if (String(Telbuff).length() > 0)	{	// Numero Tel existant/non vide
			generationMessage();
			sendSMSReply(Telbuff);			
		}
	}
}
//---------------------------------------------------------------------------
void generationMessage() {
	messageId();
	if (FlagAlarmeBatt || FlagAlarmeSect || FlagAlarmeTension
			|| FlagLastAlarmeBatt || FlagLastAlarmeSect	|| FlagLastAlarmeTension){
		message += F("--KO--------KO--");//+= V1-21
	}
	else{
		message += F("-------OK-------");//+= V1-21
	}
	message += fl;
	message += F("Secteur : ");				//"Alarme Secteur : "
	if (FlagAlarmeSect || FlagLastAlarmeSect) {
		message += F("Alarme");
		message += fl;	//"OUI\n"
	}
	else {
		message += F("OK");
		message += fl;	//"NON\n"
	}
	message += F("Batterie : ");				//"Alarme Batterie : "
	if (FlagAlarmeBatt || FlagLastAlarmeBatt || FlagAlarmeTension) {
		message += F("Alarme");
		message += fl;	//"OUI\n"
	}
	else {
		message += F("OK");
		message += fl;	//"NON\n"
	}
	message += F("V Batterie = ");			//"Tension Batterie = "
	messagebatterie(1);	//	V1-200
	message += F(";");	//	V1-200
	messagebatterie(2);	//	V1-200
	message += fl;			//	V1-200
}
//---------------------------------------------------------------------------
void messagebatterie(byte Nbatt){//V1-200
	long V = 0;
	if (Nbatt == 1){
		V = VBatterie1;
	}
	else if (Nbatt == 2){
		V = VBatterie2;
	}
	message += String(V / 100) + ",";				//V1.1
	if((V-(V / 100) * 100) < 10){						//correction bug decimal<10
		message += "0";
	}	
  message += V - ((V / 100) * 100);	//V1.1
  message += "V";
}
//---------------------------------------------------------------------------
void sendSMSReply(char *num ){ //char *cmd  String message
  // Envoie SMS
  Serial.print (F("Message = ")), Serial.println(message);
	if (!fona.sendSMS(num, message.c_str())) {
    Serial.println(F("Envoi SMS Failed"));
  } else {
    Serial.println(F("SMS Sent OK"));
  }
}
//---------------------------------------------------------------------------
void ReadEEPROM(byte adrr){											//	Lecture String en EEPROM
	char mot;
	for(byte i = adrr; i < adrr+10;i++){
		EEPROM_readAnything(i,mot);	
		Alarm.delay(10);	
		Id += String(mot);
	}	
}
//--------------------------------------------------------------------------------//
int moyenneAnalogique(byte Input){	// calcul moyenne 10 mesures consécutives 
	int moyenne = 0;
  for (int j = 0; j < 10; j++) {
    delay(10);		
		moyenne += analogRead(Input);
  }
  moyenne /= 10;
	return moyenne;
}
//---------------------------------------------------------------------------
void softReset(){
  asm volatile ("  jmp 0");	//	Reset Soft
}
//---------------------------------------------------------------------------
int freeRam (){	// lecture RAM free
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
//---------------------------------------------------------------------------
void flushSerial(){
  while (Serial.available())
    Serial.read();
}
//---------------------------------------------------------------------------
void displayTime(boolean m) {
  // m = true ajouter Time à message
  String dt;
  if (day() < 10) {
    dt += "0";
  }
  dt += day();
  dt += ("/");
  if (month() < 10) {
    dt += "0";
  }
  dt += month();
  dt += ("/");
  dt += year();
  dt += (" ");
  if (hour() < 10) {
    dt += "0";
  }
  dt += hour();
  dt += ":";
  if (minute() < 10) {
    dt += "0";
  }
  dt += minute();
  dt += ":";
  if (second() < 10) {
    dt += "0";
  }
  dt += second();
  if (m) message += dt;
  Serial.println(dt);
}
//--------------------------------------------------------------------------------//
void messageId() {
  message = Id;
  displayTime(true);
  message += fl;
	}
//--------------------------------------------------------------------------------//
// void ResetSIM800(){ // V1-200
	// fonaSerial -> println(F("AT+CFUN=1,1"));
	// Alarm.delay(1000);
	// fonaSerial -> println(F("AT+CLTS=1"));
	// fonaSerial -> println(F("AT+CENG=3"));
	// FlagReset = true;
	// if (!fona.getetatSIM()) {	// Si carte SIM not READY, Envoyé PIN
		// flushSerial();
		// char PIN[5] = "1234";
		// byte retries = 1;
		// if (! fona.unlockSIM(PIN)) {
			// Serial.println(F("Failed to unlock SIM"));
			// retries++;
			// Alarm.delay(1000);
			// if (retries == 3) {
				// goto sortie;					// 2 tentatives max
			// }
		// }
		// else {
			// Serial.println(F("OK SIM Unlock"));
		// }
// sortie:
		// Alarm.delay(1000);				//	Attendre cx reseau apres SIM unlock
	// }
// }