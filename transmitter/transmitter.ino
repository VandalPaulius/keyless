// keyless ignition transmitter
// Used example from http://forum.arduino.cc/index.php?topic=421081

#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "lowPower.h"

//#define DEBUG

#define LED         8

// NRF24l01
#define CE          9  // Toggle between transmit (TX), receive (RX), standby, and power-down mode
#define CSN         10 // SPI chip select 

#define TRANSMIT_DELAY 140
#define PAYLOAD_SIZE   32

// Constants
const char secret[32] = "77da4ba6-fdf2-11e7-8be5-0ed5ffff"; // must be unique for every TX-RX pair
const uint64_t pipe = 0xE8E8F0F0E1LL;

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
    radio.powerDown();
    LowPower.powerDown(SLEEP_1S, BOD_OFF);
    digitalWrite(LED, LOW);
    radio.powerUp(); // go to normal radio operation mode (takes ~5ms)
    delay(5);
    radio.write(secret, sizeof(secret));
    digitalWrite(LED, HIGH);
}

void initializePins() {
    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
}

void initializeRadio() {
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.setRetries(3,5); // delay, count
    radio.setPayloadSize(PAYLOAD_SIZE);
    radio.setPALevel(RF24_PA_HIGH);
    radio.openWritingPipe(pipe);
    radio.stopListening();
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

void goToSleep() {
    SMCR |= (1 << 2); // power down mode
    SMCR |= 1; // enable sleep

    // BOD DISABLE
    MCUCR |= (3 << 5); // set both BODS and BODSE at the same time
    MCUCR = (MCUCR & ~(1 << 5)) | (1 << 6); // then set the BODS bit and clear the BODSE bit at the same time

    __asm__ __volatile__("sleep");
} 
