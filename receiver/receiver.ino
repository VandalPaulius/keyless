// moto-alarm Receiver
// Used example from http://forum.arduino.cc/index.php?topic=421081

#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define DEBUG

// User Interface
#define POWER_TOGGLE_BUTTON   6
#define SIGNAL_LOSS_INDICATOR 8

#define RELAY                 7

// NRF24l01
#define CE          9  // Toggle between transmit (TX), receive (RX), standby, and power-down mode
#define CSN         10 // SPI chip select 

#define PAYLOAD_SIZE        32
#define SIGNAL_LOST_RETRIES 10
#define SLEEP_TIME_MS       500

const uint64_t pipe = 0xE8E8F0F0E1LL;
char secret[32] = "77da4ba6-fdf2-11e7-8be5-0ed5ffff";

uint8_t locked = true; // 0- locked, 1- unlocked
uint8_t signalLost = 0;

RF24 radio(CE, CSN);

void setup(void) {
    #ifdef DEBUG
        Serial.begin(115200);
        printf_begin();
    #endif

    systemInit();

    initializePins();
    initializeRadio();
}

void loop(void) {
    if (radio.available()) {
        #ifdef DEBUG
            printf("Radio available\r\n");
        #endif
        char message[PAYLOAD_SIZE + 1];
        message[PAYLOAD_SIZE] = '\0';

        radio.read(message, PAYLOAD_SIZE);

        #ifdef DEBUG
            printf("%s\r\n", message);
        #endif

        if (strncmp(secret, message, sizeof(secret)) == 0) { // Secret matches

            #ifdef DEBUG
                printf("Secret matches\r\n");
            #endif

            signalLost = 0;
            locked = false;
        }
    } else {
        #ifdef DEBUG
            printf("Signal lost. Retries: %i . Is locked: %i\r\n", signalLost, locked);
        #endif  
        
        if (signalLost >= SIGNAL_LOST_RETRIES){
            locked = true;
        } else {
            signalLost++;
        }
    }

    if (locked){
        digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH);
    } else if (!locked){
        digitalWrite(SIGNAL_LOSS_INDICATOR, LOW);
    }

    delay(SLEEP_TIME_MS);
}

void initializePins() {
    pinMode(SIGNAL_LOSS_INDICATOR, OUTPUT);
    digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // On is LOW

    pinMode(POWER_TOGGLE_BUTTON, INPUT);

    pinMode(RELAY, OUTPUT);
    digitalWrite(RELAY, HIGH); // On is LOW
}

void initializeRadio() {
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.setPayloadSize(PAYLOAD_SIZE);
    radio.openReadingPipe(1, pipe);
    radio.setPALevel(RF24_PA_HIGH);
    radio.startListening();
}

void systemInit() {    
    ADCSRA &= ~(1 << 7); // Disable ADC
    ACSR |= (1 << 7); // Disable comparator

    PRR |= (1 << 7) | // Disable TWI
        (1 << 6) | // Disable Timer2
        (1 << 3) | // Disable Timer1
        #ifndef DEBUG
            (1 << 1) | // Disable UART
        #endif
        1; // Disable ADC

    // Enable pull-ups on all port inputs
    PORTB = 0xff;
    PORTC = 0xff;
    PORTD = 0xff;
}