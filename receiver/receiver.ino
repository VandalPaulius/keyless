// keyless-ignition Receiver
// Used example from http://forum.arduino.cc/index.php?topic=421081

#include <SPI.h>
#include <avr/wdt.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "lowPower.h"

// #define DEBUG

// User Interface
#define BEACON_NEARBY_INDICATOR 5
#define SIGNAL_LOSS_INDICATOR   6
#define POWER_TOGGLE_BUTTON     7

#define RELAY                   8

// NRF24l01
#define NRF_IRQ     4  // NRF IRQ pin (active low)
#define CE          9  // Toggle between transmit (TX), receive (RX), standby
    // and power-down mode
#define CSN         10 // SPI chip select 

#define PAYLOAD_SIZE        32

#define LOCKED_POWERDOWN_PERIOD         SLEEP_FOREVER
#define UNLOCKED_POWERDOWN_PERIOD       SLEEP_2S
#define IGNITION_ON_POWERDOWN_PERIOD    SLEEP_2S
#define SIGNAL_LOST_POWERDOWN_PERIOD    SLEEP_8S

const uint64_t pipe = 0xE8E8F0F0E1LL;
char secret[32] = "77da4ba6-fdf2-11e7-8be5-0ed5ffff"; // line ending char
    // missing, thus compiler complaining

RF24 radio(CE, CSN);

enum state_t{
    LOCKED,
    UNLOCKED,
    IGNITION_ON,
    SIGNAL_LOST
};

enum interrupt_causes_t{
    NRF24_INTERRUPT,
    POWER_BUTTON_INTERRUPT,
    TIMEOUT_INTERRUPT,
    UNDEFINED
};

uint8_t program_state = LOCKED, next_state = LOCKED, powerdown_period = LOCKED_POWERDOWN_PERIOD;
volatile uint8_t interrupt_cause = TIMEOUT_INTERRUPT;

void setup(void) {
    #ifdef DEBUG
        Serial.begin(115200);
        printf_begin();
    #endif

    systemInit();

    initializePins();
    initializeRadio();
    attachInterrupts();
}

void loop(void) {
    switch(program_state){
        case LOCKED:
            if(interrupt_cause == NRF24_INTERRUPT){
                if(checkSecret()){
                    clearTimeout();
                    next_state = UNLOCKED;
                }
            }
            break;
        case UNLOCKED:
            if(interrupt_cause == NRF24_INTERRUPT){
                if(checkSecret()){
                    clearTimeout();
                }
            }
            else if(interrupt_cause == POWER_BUTTON_INTERRUPT){
                next_state = IGNITION_ON;
            }
            else if(interrupt_cause == TIMEOUT_INTERRUPT){
                next_state = LOCKED;
            }
            break;
        case IGNITION_ON:
            if(interrupt_cause == NRF24_INTERRUPT){
                if(checkSecret()){
                    clearTimeout();
                }
            }
            else if(interrupt_cause == POWER_BUTTON_INTERRUPT){
                next_state = UNLOCKED;
            }
            else if(interrupt_cause == TIMEOUT_INTERRUPT){
                next_state = SIGNAL_LOST;
            }
            break;
        case SIGNAL_LOST:
            if(interrupt_cause == NRF24_INTERRUPT){
                if(checkSecret()){
                    clearTimeout();
                    next_state = IGNITION_ON;
                }
            }
            else if(interrupt_cause == POWER_BUTTON_INTERRUPT){
                next_state = LOCKED;
            }
            else if(interrupt_cause == TIMEOUT_INTERRUPT){
                next_state = LOCKED;
            }
            break;
    }

    if(program_state != next_state){
        switch(next_state){
            case LOCKED:
                digitalWrite(BEACON_NEARBY_INDICATOR, HIGH); // Turn off beacon nearby indicator 
                digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // Turn off signal lost indicator
                digitalWrite(RELAY, HIGH); // Turn off relay
                powerdown_period = LOCKED_POWERDOWN_PERIOD;
                program_state = LOCKED;
                break;
            case UNLOCKED:
                digitalWrite(BEACON_NEARBY_INDICATOR, LOW); // Turn on beacon nearby indicator 
                digitalWrite(RELAY, HIGH); // Turn off relay
                powerdown_period = UNLOCKED_POWERDOWN_PERIOD;
                program_state = UNLOCKED;
                break;
            case IGNITION_ON:
                digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // Turn off signal lost indicator
                digitalWrite(RELAY, LOW); // Turn on relay
                powerdown_period = IGNITION_ON_POWERDOWN_PERIOD;
                program_state = IGNITION_ON;
                break;
            case SIGNAL_LOST:
                digitalWrite(SIGNAL_LOSS_INDICATOR, LOW); // Turn on signal lost indicator
                powerdown_period = SIGNAL_LOST_POWERDOWN_PERIOD;
                program_state = SIGNAL_LOST;
                break;
        }
    }
    
    interrupt_cause = UNDEFINED;
    LowPower.powerDown(powerdown_period, BOD_OFF);
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

    PORTB = 0xff;
    PORTC = 0xff;
    PORTD = 0xff;
}

void initializePins() {
    pinMode(SIGNAL_LOSS_INDICATOR, OUTPUT);
    pinMode(BEACON_NEARBY_INDICATOR, OUTPUT);
    pinMode(POWER_TOGGLE_BUTTON, INPUT);
    pinMode(RELAY, OUTPUT);
}

void initializeRadio() {
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.setPayloadSize(PAYLOAD_SIZE);
    radio.openReadingPipe(1, pipe);
    radio.setPALevel(RF24_PA_HIGH);
    radio.startListening();
}

void attachInterrupts() {
    pciSetup(POWER_TOGGLE_BUTTON);
    pciSetup(NRF_IRQ);
}

void pciSetup(byte pin){
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

ISR(WDT_vect) {
    wdt_disable();
    interrupt_cause = TIMEOUT_INTERRUPT;
}

ISR(PCINT2_vect) { // handle pin change interrupt
    if(!digitalRead(NRF_IRQ)){
        interrupt_cause = NRF24_INTERRUPT;    
    }
    else if (!digitalRead(POWER_TOGGLE_BUTTON)) {
        interrupt_cause = POWER_BUTTON_INTERRUPT;
    }
}

void clearTimeout(){
    wdt_disable();
}

bool checkSecret(){
    if (radio.available()) {
        #ifdef DEBUG
            printf("Radio available\r\n");
        #endif
        char message[PAYLOAD_SIZE + 1];
        message[PAYLOAD_SIZE] = '\0';

        radio.read(message, PAYLOAD_SIZE);

        #ifdef DEBUG
            printf("Secret: %s Message: %s\r\n", secret, message);
        #endif

        if (strncmp(secret, message, sizeof(secret)) == 0) { // Secret matches

            #ifdef DEBUG
                printf("Secret matches\r\n");
            #endif

            return true;
        }
    }
    return false;
}