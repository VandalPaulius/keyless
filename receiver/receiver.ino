// moto-alarm Receiver
// Used example from http://forum.arduino.cc/index.php?topic=421081

#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"
#include "lowPower.h"
#include "Time.h"

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
//#define CHECK_BEACON_LOCKED 500
#define CHECK_BEACON_LOCKED SLEEP_500MS
#define CHECK_BEACON_UNLOCKED SLEEP_250MS

#define CHECK_BEACON_UNLOCKED_DISCONNECTED SLEEP_60MS
#define DISCONNECTED_BEACON_LOCK_THRESHOLD 60

const uint64_t pipe = 0xE8E8F0F0E1LL;
char secret[32] = "77da4ba6-fdf2-11e7-8be5-0ed5ffff";

bool locked = true;
bool preparingToLock = false;
uint8_t signalLostCounter = 0; // TODO check for overflow values

RF24 radio(CE, CSN);
Time time;

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
    //time.Timeout_manager();
    // toggleLocks();

    if (locked) {
        // time.Timer(
        //     []() {
        //         checkBeacon(signalLostCounter);
        //     },
        //     0,
        //     SLEEP_TIME_LOCKED_MS,
        //     1000
        // );
        checkBeacon(signalLostCounter, locked, secret);

        if (locked) {
            powerDown(CHECK_BEACON_LOCKED);
            loop();
        } else {

        }
        // printf("Signal lost. Retries: %i . Is locked: %i\r\n",
        //     signalLostCounter,
        //     locked
        // );
        
    }
    
    if (!locked) {
        bool connected = checkBeacon(signalLostCounter, locked, secret);

        // if (hasConnection) {
            
        // } else {
        //     // blink
        // }

        if (!connected) {
            // blink
            if (signalLostCounter >= DISCONNECTED_BEACON_LOCK_THRESHOLD) {
                locked = true;
                powerDown(CHECK_BEACON_LOCKED);
            } else {
                powerDown(CHECK_BEACON_UNLOCKED_DISCONNECTED);
            }
            signalLostCounter++;
            loop();
        }

        powerDown(CHECK_BEACON_UNLOCKED);
        loop();
    }
    
    // else {
    //     #ifdef DEBUG
    //         printf("Signal lost. Retries: %i . Is locked: %i\r\n",
    //             signalLost,
    //             locked
    //         );
    //     #endif  
        
    //     if (signalLost >= SIGNAL_LOST_RETRIES){
    //         locked = true;
    //     } else {
    //         signalLost++;
    //     }
    // }

    // if (locked){
    //     digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH);
    // } else if (!locked){
    //     digitalWrite(SIGNAL_LOSS_INDICATOR, LOW);
    // }

    // delay(SLEEP_TIME_MS);
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

void indicateSignalLoss() {

}

// void TimerDrivercheckRadio() {
//      // else {
//     //     #ifdef DEBUG
//     //         printf("Signal lost. Retries: %i . Is locked: %i\r\n",
//     //             signalLost,
//     //             locked
//     //         );
//     //     #endif  
        
//     //     if (!locked) {
//     //         preparingToLock = true;
//     //     } else {
//     //         locked = true;
//     //         preparingToLock = false;   
//     //     }
//     // }
// }

bool checkBeacon(uint8_t &signalLostCounter, bool &locked, char secret[]) {
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

            signalLostCounter = 0;
            locked = false;
            return true;
        }
        return false;
    } else {
        return false;
    }
}

void powerDown(period_t period) {
    LowPower.powerDown(period, ADC_OFF, BOD_OFF);
}