#define DS4 4
#define DS3 3
#define CONTROLLER DS4                     // quale joypad voglio usare: DualShock 3 o 4

#include "ESP32Servo.h"
#if CONTROLLER == DS4
#include "PS4Controller.h"
#elif CONTROLLER == DS3
#include "Ps3Controller.h"
#endif

// configurazione motore servo
#define SERVO_PIN                   25     // pin al quale è collegato il segnale del servo
#define SERVO_CHANNEL                4     // canale utilizzato per la gestione del PWM.
									       // se non stai cosa stai facendo, lascia questo valoe
										   // DEVE ESSERE DIVERSO PER OGNI MOTORE/SERVO

#define MY_BT_ADDRESS  "00:11:22:33:44:55" // indirizzo bluetooth utilizzato per l'accoppiamento
										   // tra l'ESP32 ed il joypad.
										   // Inserisci il tuo valore definendo sei byte in formato 
										   // esadecimale, separati da due punti 
										   // Questo indirizzo deve essere uguale a quello settato 
										   // sul joypad mediante l'utility sixaxispairtool

Servo myServo;   // oggetto per la gestione del servo

// callback che verrà chiamata ogni qualvolta che il joypad si connette
void onConnect(void) {
	Serial.println("onConnect");
}

// callback che verrà chiamata ogni qualvolta che il joypad si disconnette
void onDisconnect(void) {
	Serial.println("onDisconnect");
}

// callback che verrà chiamata ogni qualvolta che ci sarà un nuovo pacchetto dati inviato dal joypad
void onPacket(void) {

	uint8_t lTrigger, angle;

	// leggo il valore del grilletto sinistro (utilizzato per muovere il servo)
#if CONTROLLER == DS4
	lTrigger = PS4.data.analog.button.l2;
#elif CONTROLLER == DS3
	lTrigger = Ps3.data.analog.button.l2;
#endif

	// il valore dei trigger vanno da 0 (non premuto) a 255 (completamente premuto)
	// mentre il servo si muove tra zero gradi e 180 gradi
	angle = map(lTrigger, 0, 255, 0, 180);

	// aggiorno la posizione del servo
	myServo.write(angle);
}

void setup() {
	// inizializza la seriale e stampa un messaggio 
	Serial.begin(115200);
	delay(1000);
	Serial.printf("\n\n---| ESP32 BattleBots controller |---\n\n");

	// configuro l'oggetto myServo specificando su quale pin dell'ESP32 è collegato il segnale del servo
	// e quale canale PWM utilizzare per la generazione del segnale
	myServo.attach(SERVO_PIN, SERVO_CHANNEL);

	// inizializza la libreria per la gestione della comunicazione tra il joypad DS4 e la M5Stack Stamp Pico
#if CONTROLLER == DS4
	if (!PS4.begin(MY_BT_ADDRESS)) {
#elif CONTROLLER == DS3
	if (!Ps3.begin(MY_BT_ADDRESS)) {
#endif
		Serial.println("PS Library NOT initialized.");
		while (1);
	}
	else
		Serial.println("PS Library initialization done.");

	// setto le callback che verranno chiamate ogni qualvolta che il joypad 
	// si connette/disconnette/ci sono nuovi pacchetti contenenti lo stato del joypad
	// (pulsanti premuti, posizione stick, triggers, etc)
#if CONTROLLER == DS4
	PS4.attachOnConnect(onConnect);
	PS4.attachOnDisconnect(onDisconnect);
	PS4.attach(onPacket);
#elif CONTROLLER == DS3
	Ps3.attachOnConnect(onConnect);
	Ps3.attachOnDisconnect(onDisconnect);
	Ps3.attach(onPacket);
#endif
}

void loop() {
}

