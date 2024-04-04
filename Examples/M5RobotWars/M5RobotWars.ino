
#define DS4 4
#define DS3 3
#define CONTROLLER DS4   // IMPORTANTE!!! Scegliere quale joypad voglio usare: 
                         //               DualShock 3 -> DS3
                         //               DualShock 4 -> DS4


#include "Cdrv8833.h"
#include "ESP32Servo.h"
#include "FastLED.h"

#if CONTROLLER == DS4
#include "PS4Controller.h"
#elif CONTROLLER == DS3
#include "Ps3Controller.h"
#endif

#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include"esp_gap_bt_api.h"
#include "esp_err.h"

// N.B.: per poter programmare la scheda M5Stack Stamp Pico, bisogna aggiungere la definizione delle 
// schede M5Stack, (File -> Preferences -> Additional Board Manager) inserendo il seguente URL:
// https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/arduino/package_m5stack_index.json
// Una volta installato, la scheda da selezionare è (Tools -> Boards -> M5Stack Arduino -> STAMP-PICO

// Per poter programmare la scheda con il programmatore presente nel kit, seguire la seguente piedinatura
// seguendo PEDISSEQUAMENTE la piedinatura, senza invertire nulla.
// USB2UART   ESP32
//   3V3       3V3
//   GND       G
//   RX        G1
//   TX        G3
//   RTS       EN
//   DTR       G0

// nella cartella "Libraries" sono presenti tutte le librerie necessarie per poter compilare lo sketch.
// Vanno copiate nella cartella "libraries" di Arduino.
// ATTENZIONE: anche se l'avete già, sovrascrivete la libreria per la gestione del joypad PS4 
// con quella fornita (ho fato delle modifiche perchè nella originale c'erano degli errori)

// configurazione motore DC sinistro
#define LEFT_MOTOR_PIN_A                18 // pin al quale è connesso il pin IN1/IN3 del DRV8833
#define LEFT_MOTOR_PIN_B                19 // pin al quale è connesso il pin IN2/IN4 del DRV8833
#define LEFT_MOTOR_CHANNEL               1 // canale utilizzato per la gestione del PWM. 
                                           // Se non sai cosa stai facendo, lascia questo valore
#define LEFT_MOTOR_ROTATION_INVERTED false // se vero, inverte il senso di rotazione del motore.
                                           // Utile se si è invertita la cablatura del motore/se il 
							               // motore è in posizione inversa rispetto all'altro 
							               // (coppia motore sinistro/destro)

// configurazione motore DC destro
#define RIGHT_MOTOR_PIN_A               21 // pin al quale è connesso il pin IN3/IN1 del DRV8833
#define RIGHT_MOTOR_PIN_B               22 // pin al quale è connesso il pin IN4/IN2 del DRV8833
#define RIGHT_MOTOR_CHANNEL              2 // canale utilizzato per la gestione del PWM. 
                                           // Se non sai cosa stai facendo, lascia questo valore
#define RIGHT_MOTOR_ROTATION_INVERTED true // se vero, inverte il senso di rotazione del motore.
                                           // Utile se si è invertita la cablatura del motore/se il 
									       // motore è in posizione inversa rispetto all'altro 
									       // (coppia motore sinistro/destro)

// configurazione motore brushless/servo
#define BRUSHLESS_PIN                   25 // pin al quale è collegato il segnale del brushless/servo
#define BRUSHLESS_CHANNEL                4 // canale utilizzato per la gestione del PWM.
                                           // se non stai cosa stai facendo, lascia questo valoe
#define BRUSHLESS_MIN_ANGLE              0 // angolo minimo del servo. 
                                           // Se non sai cosa stai facendo, lascia questo valore
#define BRUSHLESS_MAX_ANGLE            180 // angolo massimo del servo.
                                           // Se non sai cosa stai facendo, lascia questo valore
#define BRUSHLESS_MIN_USEC            1000 // minimo valore in microsecondi.
                                           // Se non sai cosa stai facendo, lascia questo valore
#define BRUSHLESS_MAX_USEC 			  2000 // massimo valore in microsecondi
                                           // Se non sai cosa stai facendo, lascia questo valore
#define BRUSHLESS_DEAD_ZONE           1100 // valore minimo in microsecondi sotto il quale il brushless
                                           // non viene attivato. Serve perchè sotto certi valori il 
										   // brushless non riesce a spuntare. Se non sai cosa stai facendo,
										   //lascia questo valore

#define MY_BT_ADDRESS  "AA:BB:CC:DD:EE:FF" // indirizzo bluetooth utilizzato per l'accoppiamento
                                           // tra l'ESP32 ed il joypad DS4.
										   // Inserisci il tuo valore definendo sei byte in formato 
										   // esadecimale, separati da due punti 
										   // Questo indirizzo deve essere uguale a quello settato 
										   // sul joypad DS4 mediante l'utility sixaxispairtool

// configurazione del led NEOPIXEL della scheda M5Stack Stamp Pico
#define NEOPIXEL_PIN                    27 // pin al quale è connesso il NEOPIXEL della scheda
                                           // M5Stack Stamp Pico
#define NUM_NEOPIXELS                    1 // quanti NEOPIXEL sono collegati in serie.
										   // Nel caso del M5Stack Stamp Pico c'è un solo NEOPIXEL

// oggetto per la gestione del NEOPIXEL
CRGB leds[NUM_NEOPIXELS];

// oggetti per la gestione dei due motori DC (motore sinistro/destro)
Cdrv8833 leftMotor(LEFT_MOTOR_PIN_A, LEFT_MOTOR_PIN_B, LEFT_MOTOR_CHANNEL, LEFT_MOTOR_ROTATION_INVERTED);
Cdrv8833 rightMotor(RIGHT_MOTOR_PIN_A, RIGHT_MOTOR_PIN_B, RIGHT_MOTOR_CHANNEL,RIGHT_MOTOR_ROTATION_INVERTED);

// oggetto per la gestione del brushless/servo
Servo brushless;

// restituisce una stringa contenente l'indirizzo bluetooth della scheda ESP32
String getDeviceAddress(void) {
	String btAddr;
	btAddr = "";
	const uint8_t* point = esp_bt_dev_get_address();

	if (point == NULL)
		return "BT ERROR";

	for (int i = 0; i < 6; i++) {
		char str[3];
		sprintf(str, "%02X", (uint8_t)point[i]);
		btAddr += str;
		if (i < 5) {
			btAddr += ":";
		}
	}
	return(btAddr);
}

// elimina eventuali dispositivi bluetooth memorizzati. Evita che il joypad non riesca a connettersi
void clearBondedBTDevices(void) {
	esp_err_t tError;
	int totalBondedDevices;
   esp_bd_addr_t *pPairedDeviceAddress;
	totalBondedDevices = esp_bt_gap_get_bond_device_num();
	if (0 == totalBondedDevices) {
//      Serial.println("No bonded device(s) found.");
		return;
   }
   pPairedDeviceAddress = (esp_bd_addr_t *)malloc(sizeof(esp_bd_addr_t) * totalBondedDevices);
   if (NULL == pPairedDeviceAddress) {
//      Serial.println("Unable to allocate memory.");
      return;
   }
//	Serial.printf("\n--->Found %d bonded devices\n", totalBondedDevices);
   tError = esp_bt_gap_get_bond_device_list(&totalBondedDevices, pPairedDeviceAddress);
   if(ESP_OK != tError) {
//		Serial.printf("esp_bt_gap_get_bond_device_list error: %03Xh\n", tError);
		free(pPairedDeviceAddress);
      return;
   }
   for (int i = 0; i < totalBondedDevices; i++) {
 		tError = esp_bt_gap_remove_bond_device(pPairedDeviceAddress[i]);
		if (ESP_OK == tError) {
//			Serial.printf(" |->Removed bonded device #%d - %02X:%02X:%02X:%02X:%02X:%02X\n", i, 
//   	      pPairedDeviceAddress[i][0], pPairedDeviceAddress[i][1], pPairedDeviceAddress[i][2], 
//	         pPairedDeviceAddress[i][3], pPairedDeviceAddress[i][4], pPairedDeviceAddress[i][5]);
		}
		else {
//			Serial.printf(" |->Failed to remove bonded device #%d\n", i);
		}
   }
   free(pPairedDeviceAddress);
}

// callback che verrà chiamata ogni qualvolta che il joypad si connette
void onConnect(void) {
	Serial.println("onConnect");
}

// callback che verrà chiamata goni qualvolta che il joypad si disconnette
void onDisconnect(void) {
	Serial.println("onDisconnect");
}

// callback che verrà chiamata ogni qualvolta che ci sarèa un nuovo pacchetto dati inviato dal joypad
void onPacket(void) {

	int32_t lTrigger, ESC;

	int8_t lMotor, rMotor;
	float lx, ly, value;

	// leggo i valori dello stick sinistro del joypad
#if CONTROLLER == DS4
	lx = PS4.data.analog.stick.lx;
	ly = PS4.data.analog.stick.ly;
#elif CONTROLLER == DS3
	lx = Ps3.data.analog.stick.lx;
	ly = Ps3.data.analog.stick.ly;
#endif

	// trasformo i valori letti (destra/sinistra, alto/basso) da -128..127 a -1..1 e ne calcolo il quadrato
	// serve per poter applicare il teorema di pitagora
	if (lx > 0) {
		lx = lx / 127.0;
		lx = lx * lx;
	}
	else {
		lx = lx / 128.0;
		lx = -(lx * lx);
	}
	if (ly > 0) {
		ly = ly / 127.0;
		ly = ly * ly;
	}
	else {
		ly = ly / 128.0;
		ly = -(ly * ly);
	}

	// calcolo il valore da impostare sul motore sinistro (-100..100)
	value = (ly + lx) * 100.0;
	if (value > 100)
		value = 100;
	else if (value < -100)
		value = -100;
	lMotor = value;
	if (abs(value - (float)lMotor) >= 0.5)
		lMotor++;

	// calcolo il valore da impostare sul motore destro (-100..100)
	value = (ly - lx) * 100.0;
	if (value > 100)
		value = 100;
	else if (value < -100)
		value = -100;
	rMotor = value;
	if (abs(value - (float)rMotor) >= 0.5)
		rMotor++;

	// aggiorno la velocità di rotazione dei modori sinistro e destro
	leftMotor.move(lMotor);
	rightMotor.move(rMotor);

	// leggo il valore del grilletto sinistro (utilizzato per definire la velocità di rotazione del brushless)
#if CONTROLLER == DS4
	lTrigger = PS4.data.analog.button.l2;
#elif CONTROLLER == DS3
	lTrigger = Ps3.data.analog.button.l2;
#endif

	// mappo il valore letto (0..255) nei valori possibili del brushless (1000...2000)
	ESC = map(lTrigger, 0, 255, 1000, 2000);

	// se il valore letto è sotto la soglia di attivazione del brushless, fermo il motore
	if (ESC < BRUSHLESS_DEAD_ZONE)
		ESC = 1000;

	// BRUSHLESS PRESET ----------------------------

	// se tengo premuto cerchio, imposto a 1100 microsecondi (un decimo della velocità massima) il segnale sul brushless
#if CONTROLLER == DS4
	if (PS4.data.button.circle) {
#elif CONTROLLER == DS3
	if (Ps3.data.button.circle) {
#endif
		ESC = 1100;
	}

	// se tengo premuto triangolo, imposto a 1200 microsecondi (due decimi della velocità massima) il segnale sul brushless
#if CONTROLLER == DS4
	if (PS4.data.button.triangle) {
#elif CONTROLLER == DS3
if (Ps3.data.button.triangle) {
#endif
		ESC = 1200;
	}

	// se tengo premuto quadrato, imposto a 1300 microsecondi (tre decimi della velocità massima) il segnale sul brushless
#if CONTROLLER == DS4
	if (PS4.data.button.square) {
#elif CONTROLLER == DS3
if (Ps3.data.button.square) {
#endif
		ESC = 1300;
	}

	// se tengo premuto croce, imposto a 1500 microsecondi (la metà della velocità massima) il segnale sul brushless
#if CONTROLLER == DS4
	if (PS4.data.button.cross) {
#elif CONTROLLER == DS3
if (Ps3.data.button.cross) {
#endif
		ESC = 1500;
	}

	// aggiorno la velocità di rotazione del brushless/posizione del servo
	brushless.writeMicroseconds(ESC);
}

void setup() {
	// inizializza la seriale e stampa un messagio 
	Serial.begin(115200);
	Serial.printf("\n\n---| ESP32 BattleBots controller |---\n\n");

	// inizializza la gestione del NEOPIXEL della scheda M5Stack Stamp Pico
	FastLED.addLeds<SK6812, NEOPIXEL_PIN, RGB>(leds, NUM_NEOPIXELS);  // GRB ordering is typical

	// elimina eventuali dispositivi bluetooth precedentemente memorizzati.
    // Serve per evitare che il joypad non riesca a collegarsi
	clearBondedBTDevices();

	// inizializza la libreria per la gestione della comunicazione tra il joypad DS4 e la M5Stack Stamp Pico
#if CONTROLLER == DS4
	if (!PS4.begin(MY_BT_ADDRESS)) {
#elif CONTROLLER == DS3
		if (!Ps3.begin(MY_BT_ADDRESS)) {
#endif
		Serial.println("PS4 Library NOT initialized.");
		while (1);
	}
	else
		Serial.println("PS4 Library initialization done.");

	// inizializzo l'oggetto per la gestione del brushless/servo
	brushless.attach(BRUSHLESS_PIN, BRUSHLESS_CHANNEL, BRUSHLESS_MIN_ANGLE, BRUSHLESS_MAX_ANGLE, BRUSHLESS_MIN_USEC, BRUSHLESS_MAX_USEC);
	// per poter attivare l'ESC che gestisce il brushless, è necessario inviare il valore di 1000 microsecondi
	// (corrisponde alla manetta a zero)
	brushless.writeMicroseconds(1000);

	// setto la modalità slow decay per i motori DC. Questo permette di avere una coppia maggiore sull'asse del motore rispetto
	// a quanto potremmo ottenere con la modalità fast decay
	leftMotor.setDecayMode(drv8833DecaySlow);
	rightMotor.setDecayMode(drv8833DecaySlow);

	//setto le callback che verranno chiamate ogni volta che il joypad si connette/si disconnette/c'è un nuovo pacchetto dati
#if CONTROLLER == DS4
	PS4.attachOnConnect(onConnect);
	PS4.attachOnDisconnect(onDisconnect);
	PS4.attach(onPacket);
#elif CONTROLLER == DS3
	Ps3.attachOnConnect(onConnect);
	Ps3.attachOnDisconnect(onDisconnect);
	Ps3.attach(onPacket);
#endif

	// stampo l'indirizzo bluetooth della scheda
	Serial.printf("\nBT Address: %s\n\n", getDeviceAddress().c_str());
}

void loop() {
	// se il joypad è connesso alla scheda...
#if CONTROLLER == DS4
	if (PS4.isConnected()) {
#elif CONTROLLER == DS3
	if (Ps3.isConnected()) {
#endif
		// ...visualizzo il colore ROSSO sul neopixel. In questa maniera capisco che il battlebot
		// è armato.
		leds[0] = 0x008000; // rosso - NB l'ordine è GRB
		FastLED.show();
	}
	// altrimenti...
	else {
		// ...il joypad non è connesso e visualizzo il colore blu sul neopixel. In questa manieta capisco
		// che il battlebot NON è armato.

		leds[0] = 0x000080; // blu - NB l'ordine è GRB
		FastLED.show();

		// non essendo connesso il joypad, metto in stop i motori DC...
		leftMotor.stop();
		rightMotor.stop();
		// ...ed il brushless. In questa maniera gestiamo anche il FAILSAFE, ossia se perdo la connessione
		// con il joypad, blocco il battlebot
		brushless.writeMicroseconds(1000);
	}
}
