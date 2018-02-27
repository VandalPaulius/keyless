// keyless-ignition Receiver
// Used example from http://forum.arduino.cc/index.php?topic=421081

#include <SPI.h>
#include <avr/wdt.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "lowPower.h"

//#define DEBUG

// User Interface
#define BEACON_NEARBY_INDICATOR 6
#define SIGNAL_LOSS_INDICATOR   7
#define POWER_TOGGLE_BUTTON     9

#define RELAY                   8

// NRF24l01
#define NRF_IRQ     A0  // NRF IRQ pin (active low)
#define CE          A1  // Toggle between transmit (TX), receive (RX), standby
    // and power-down mode
#define CSN         10 // SPI chip select 

#define PAYLOAD_SIZE        32

#define LOCKED_POWERDOWN_PERIOD                     SLEEP_FOREVER
#define UNLOCKED_POWERDOWN_PERIOD                   SLEEP_2S
#define UNLOCKED_SIGNAL_LOST_POWERDOWN_PERIOD       SLEEP_120MS
#define IGNITION_ON_POWERDOWN_PERIOD                SLEEP_2S
#define IGNITION_ON_SIGNAL_LOST_POWERDOWN_PERIOD    SLEEP_120MS

#define UNLOCKED_SIGNAL_LOST_LIMIT                  20
#define IGNITION_ON_SIGNAL_LOST_LIMIT               100             

const uint64_t pipe = 0xE8E8F0F0E1LL;
char secret[32] = "77da4ba6-fdf2-11e7-8be5-0ed5ffff"; // line ending char
    // missing, thus compiler complaining

RF24 radio(CE, CSN);

enum state_t{
    LOCKED,
    UNLOCKED,
    UNLOCKED_SIGNAL_LOST,
    IGNITION_ON,
    IGNITION_ON_SIGNAL_LOST
};

uint8_t program_state = LOCKED, next_state = LOCKED, powerdown_period = LOCKED_POWERDOWN_PERIOD, signal_loss_counter = 0;
volatile uint8_t nrf24_interrupt_flag = false, power_button_interrupt_flag = false, timeout_interrupt_flag = false, portd_history = 0xFF;

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
    #ifdef DEBUG
        debugStatePrintout();
    #endif

    switch(program_state){
        case LOCKED:          
            if(nrf24_interrupt_flag){
                nrf24_interrupt_flag = false;
                if(checkSecret()){
                    clearTimeout();
                    next_state = UNLOCKED;
                }
            }
            break;
        case UNLOCKED:
            if(nrf24_interrupt_flag){
                nrf24_interrupt_flag = false;
                if(checkSecret()){
                    clearTimeout();
                }
            }
            if(power_button_interrupt_flag){
                power_button_interrupt_flag = false;
                next_state = IGNITION_ON;
            }
            if(timeout_interrupt_flag){
                timeout_interrupt_flag = false;
                next_state = UNLOCKED_SIGNAL_LOST;
            }
            break;
        case UNLOCKED_SIGNAL_LOST:
            if(nrf24_interrupt_flag){
                nrf24_interrupt_flag = false;
                if(checkSecret()){
                    clearTimeout();
                    signal_loss_counter = 0;
                    next_state = UNLOCKED;
                }
            }
            if(timeout_interrupt_flag){
                timeout_interrupt_flag = false;
                if(signal_loss_counter >= UNLOCKED_SIGNAL_LOST_LIMIT){
                    signal_loss_counter = 0;
                    next_state = LOCKED;
                }
                else{
                    digitalWrite(BEACON_NEARBY_INDICATOR, !digitalRead(BEACON_NEARBY_INDICATOR));
                    signal_loss_counter++;
                }  
            }
            break;
        case IGNITION_ON:
            if(nrf24_interrupt_flag){
                nrf24_interrupt_flag = false;
                if(checkSecret()){
                    clearTimeout();
                }
            }
            if(power_button_interrupt_flag){
                power_button_interrupt_flag = false;
                next_state = UNLOCKED;
            }
            if(timeout_interrupt_flag){
                timeout_interrupt_flag = false;
                next_state = IGNITION_ON_SIGNAL_LOST;
            }
            break;
        case IGNITION_ON_SIGNAL_LOST:
            if(nrf24_interrupt_flag){
                nrf24_interrupt_flag = false;
                if(checkSecret()){
                    clearTimeout();
                    next_state = IGNITION_ON;
                }
            }
            if(power_button_interrupt_flag){
                power_button_interrupt_flag = false;
                next_state = LOCKED;
            }
            if(timeout_interrupt_flag){
                timeout_interrupt_flag = false;
                if(signal_loss_counter >= IGNITION_ON_SIGNAL_LOST_LIMIT){
                    signal_loss_counter = 0;
                    next_state = LOCKED;
                }
                else{
                    digitalWrite(SIGNAL_LOSS_INDICATOR, !digitalRead(SIGNAL_LOSS_INDICATOR));
                    signal_loss_counter++;
                }
            }
            break;
    }

    if(program_state != next_state){
        switch(next_state){
            case LOCKED:
                digitalWrite(BEACON_NEARBY_INDICATOR, HIGH); // Turn off beacon nearby indicator 
                digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // Turn off signal lost indicator
                digitalWrite(RELAY, HIGH); // Turn off relay
                pciDetach(POWER_TOGGLE_BUTTON);
                powerdown_period = LOCKED_POWERDOWN_PERIOD;
                program_state = LOCKED;
                break;
            case UNLOCKED:
                digitalWrite(BEACON_NEARBY_INDICATOR, LOW); // Turn on beacon nearby indicator 
                digitalWrite(RELAY, HIGH); // Turn off relay
                pciAttach(POWER_TOGGLE_BUTTON);
                powerdown_period = UNLOCKED_POWERDOWN_PERIOD;
                program_state = UNLOCKED;
                break;
            case UNLOCKED_SIGNAL_LOST:
                digitalWrite(BEACON_NEARBY_INDICATOR, HIGH);
                pciDetach(POWER_TOGGLE_BUTTON);
                powerdown_period = UNLOCKED_SIGNAL_LOST_POWERDOWN_PERIOD;
                program_state = UNLOCKED_SIGNAL_LOST;
                break;
            case IGNITION_ON:
                digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // Turn off signal lost indicator
                digitalWrite(RELAY, LOW); // Turn on relay
                powerdown_period = IGNITION_ON_POWERDOWN_PERIOD;
                program_state = IGNITION_ON;
                break;
            case IGNITION_ON_SIGNAL_LOST:
                digitalWrite(SIGNAL_LOSS_INDICATOR, LOW); // Turn on signal lost indicator
                powerdown_period = IGNITION_ON_SIGNAL_LOST_POWERDOWN_PERIOD;
                program_state = IGNITION_ON_SIGNAL_LOST;
                break;
        }
    }
    
    if(!nrf24_interrupt_flag && !power_button_interrupt_flag && !timeout_interrupt_flag){
        LowPower.powerDown(powerdown_period, BOD_OFF);
    }
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
    PORTB = 0xFF;
    PORTC = 0xFF;
    PORTD = 0xFF;

    PORTB = 0xFF;
    PORTC = 0xFF;
    PORTD = 0xFF;
}

void initializePins() {
    pinMode(SIGNAL_LOSS_INDICATOR, OUTPUT);
    pinMode(BEACON_NEARBY_INDICATOR, OUTPUT);
    pinMode(POWER_TOGGLE_BUTTON, INPUT);
    pinMode(RELAY, OUTPUT);
    pinMode(NRF_IRQ, INPUT);
}

void initializeRadio() {
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.setPayloadSize(PAYLOAD_SIZE);
    radio.openReadingPipe(1, pipe);
    radio.setPALevel(RF24_PA_MIN);
    radio.setLNALevel(RF24_LNA_LOW_GAIN); 
    radio.startListening();
}

void attachInterrupts() {
    pciAttach(NRF_IRQ);
}

void pciAttach(uint8_t pin){
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

void pciDetach(uint8_t pin){
    *digitalPinToPCMSK(pin) &= ~(bit (digitalPinToPCMSKbit(pin)));  // enable pin   
}

ISR(WDT_vect) {
    wdt_disable();
    timeout_interrupt_flag = true;
}

ISR(PCINT2_vect) { // handle pin change interrupt
    uint8_t changed_pins;

    changed_pins = PIND ^ portd_history;
    portd_history = PIND;
    changed_pins &= ~portd_history;

    if(changed_pins & (1 << NRF_IRQ)){
        nrf24_interrupt_flag = true;   
    }

    if(changed_pins & (1 << POWER_TOGGLE_BUTTON)){
        power_button_interrupt_flag = true;   
    }
}

void clearTimeout(){
    wdt_disable();
}

bool checkSecret(){
    if (radio.available()) {
        #ifdef DEBUG
            printf("Radio available\r\n");
            delay(5);
        #endif
        char message[PAYLOAD_SIZE + 1];
        message[PAYLOAD_SIZE] = '\0';

        radio.read(message, PAYLOAD_SIZE);

        #ifdef DEBUG
            printf("Message: %s\r\n", message);
            delay(5);
        #endif

        if (strncmp(secret, message, sizeof(secret)) == 0) { // Secret matches

            #ifdef DEBUG
                printf("Secret matches\r\n");
                delay(5);
            #endif

            return true;
        }
    }
    return false;
}

#ifdef DEBUG
    void debugStatePrintout(){
        switch(program_state){
            case LOCKED:
                printf("LOCKED\t");
                break;
            case UNLOCKED:
                printf("UNLOCKED\t");
                break;
            case UNLOCKED_SIGNAL_LOST:
                printf("UNLOCKED_SIGNAL_LOST\t");
                break;
            case IGNITION_ON:
                printf("IGNITION_ON\t");
                break;
            case IGNITION_ON_SIGNAL_LOST:
                printf("IGNITION_ON_SIGNAL_LOST\t");
                break;
            default:
                printf("UNDEFINED\t");
        }

        if(nrf24_interrupt_flag){
            printf("NRF24_INTERRUPT\r\n");
        }
        
        if(power_button_interrupt_flag){
            printf("POWER_BUTTON_INTERRUPT\r\n");
        }

        if(timeout_interrupt_flag){
            printf("TIMEOUT_INTERRUPT\r\n");
        } 
        delay(10);     
    }
#endif