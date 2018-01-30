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
#define CHECK_BEACON_UNLOCKED SLEEP_120MS
// 1 = how many times transmitter sends per second
#define DISCONNECTED_BEACON_LOCK_THRESHOLD (1 * 1000 / 120) * 5 // = 8.333 * 5  // ( 1 second / CHECK_BEACON_UNLOCKED sleep time ) * how many seconds
#define DISCONNECTED_BEACON_LOCK_THRESHOLD_TOLERANCE (1 * 1000 / 120) * 1.09 // 8.333 -> 9

const uint64_t pipe = 0xE8E8F0F0E1LL;
char secret[32] = "77da4ba6-fdf2-11e7-8be5-0ed5ffff"; // line ending char missing, thus compiler complaining

bool lock = true;
bool prepareToLock = false;
unsigned int signalLostCounter = DISCONNECTED_BEACON_LOCK_THRESHOLD + 1;
//int signalLostCounter = DISCONNECTED_BEACON_LOCK_THRESHOLD + 1;
//bool signalLossIndicatorState = false;

//bool signalLossIndicatorState = false;

RF24 radio(CE, CSN);
//Time time;

#ifdef DEBUG
    int convB(bool value) {
        if (value == true) {
            return 1;
        } else {
            return 0;
        }
    }
#endif

void checkRetryOverflow(unsigned int &counter) {
    if (counter == 65534) {
        counter = DISCONNECTED_BEACON_LOCK_THRESHOLD + 1;
    }
}

void toggleSignalLossIndicator(bool leaveOn = true) {
    static bool isOnPrev = false;
    if (!leaveOn) {
        isOnPrev = leaveOn;
        digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // HIGH = off
    } else {
        if (!isOnPrev) {
            digitalWrite(SIGNAL_LOSS_INDICATOR, LOW); // LOW = on
        } else {
            digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // HIGH = off
        }

        isOnPrev = !isOnPrev;
    }
}

void toggleLocks(bool lock, bool init = false) {
    static bool lockPrevious = true;

    #ifdef DEBUG
        printf("In Relay %i\r\n", lock);
    #endif

    if (lockPrevious != lock || init) {
        if (lock) {
            digitalWrite(RELAY, HIGH); // HIGH = off
        } else {
            digitalWrite(RELAY, LOW); // LOW = on
        }

        lockPrevious = lock;
    }
}

void setup(void) {
    #ifdef DEBUG
        Serial.begin(115200);
        printf_begin();
    #endif

    systemInit();

    initializePins();
    initializeRadio();

    // #ifdef DEBUG
    //     printf("setup Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
    //         signalLostCounter,
    //         convB(lock),
    //         convB(prepareToLock)
    //     );
    //     delay(10);
    // #endif
    // Serial.print("lock: ");
    // Serial.write(lock);
}

// void toggleLocks(bool lock = true) {
//     static bool lockPrevious = true;

//     #ifdef DEBUG
//         printf("In Relay %i\r\n", lock);
//     #endif

//     if (lockPrevious != lock) {
//         if (lock) {
//             digitalWrite(RELAY, HIGH); // HIGH = off
//         } else {
//             digitalWrite(RELAY, LOW); // LOW = on
//         }

//         lockPrevious = lock;
//     }
// }


//void loop() {};
void loop(void) {
    // #ifdef DEBUG
    //     printf("0 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
    //         signalLostCounter,
    //         convB(lock),
    //         convB(prepareToLock)
    //     );
    //     delay(10);
    // #endif

    if (lock && !prepareToLock) {
        powerDown(CHECK_BEACON_LOCKED);
    } else {
        powerDown(CHECK_BEACON_UNLOCKED); //check more frequently
    }

    // #ifdef DEBUG
    //     printf("1 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
    //         signalLostCounter,
    //         lock,
    //         prepareToLock
    //     );
    //     delay(10);
    // #endif


    bool gotSignal = checkBeacon(signalLostCounter, lock, secret);


    // #ifdef DEBUG
    //     printf("2 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
    //         signalLostCounter,
    //         convB(lock),
    //         convB(prepareToLock)
    //     );
    //     delay(10);
    // #endif

    if (gotSignal) {
        prepareToLock = false;
    }

    if (!lock) {
        // #ifdef DEBUG
        //     printf("3 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
        //         signalLostCounter,
        //         convB(lock),
        //         convB(prepareToLock)
        //     );
        //     delay(10);
        // #endif
        if (signalLostCounter > DISCONNECTED_BEACON_LOCK_THRESHOLD_TOLERANCE) {
            prepareToLock = true;
        }
        
        // #ifdef DEBUG
        //     printf("4 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
        //         signalLostCounter,
        //         convB(lock),
        //         convB(prepareToLock)
        //     );
        //     delay(10);
        // #endif

        if (signalLostCounter >= DISCONNECTED_BEACON_LOCK_THRESHOLD) {
            lock = true;
            prepareToLock = false;
        }

        // #ifdef DEBUG
        //     printf("5 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
        //         signalLostCounter,
        //         convB(lock),
        //         convB(prepareToLock)
        //     );
        //     delay(10);
        // #endif
    }

    // #ifdef DEBUG
    //     printf("6 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
    //         signalLostCounter,
    //         convB(lock),
    //         convB(prepareToLock)
    //     );
    //     delay(10);
    // #endif
    // if (!lock) {
    //     preparingToLock = false;

    //     if (signalLostCounter > DISCONNECTED_BEACON_LOCK_THRESHOLD_TOLERANCE) {
    //         preparingToLock = true;
    //     } // else {
    //     //     preparingToLock = false;
    //     // }

    //     if (signalLostCounter >= DISCONNECTED_BEACON_LOCK_THRESHOLD) {
    //         preparingToLock = false;
    //         lock = true;
    //     }

    //     if (preparingToLock) {
    //         toggleSignalLossIndicator();
    //     }
    // }

    // #ifdef DEBUG
    //     printf("Signal lost. Retries: %i. Is locked: %i. PreparingToLock: %i\r\n",
    //         signalLostCounter,
    //         lock,
    //         preparingToLock
    //     );
    //     delay(5);
    // #endif
    
    // if (!preparingToLock) {
    //     toggleLocks(lock); // ERROR: turns relay all the time
    //     toggleSignalLossIndicator(false);
    // }

    #ifdef DEBUG
        printf("Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
            signalLostCounter,
            convB(lock),
            convB(prepareToLock)
        );
        delay(10);
    #endif

    toggleLocks(lock);
    checkRetryOverflow(signalLostCounter);

    // #ifdef DEBUG
    //         printf("7 Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i\r\n",
    //             signalLostCounter,
    //             convB(lock),
    //             convB(prepareToLock)
    //         );
    //         delay(10);
    //     #endif
    // end





    

    // if (locked) {
    //     // time.Timer(
    //     //     []() {
    //     //         checkBeacon(signalLostCounter);
    //     //     },
    //     //     0,
    //     //     SLEEP_TIME_LOCKED_MS,
    //     //     1000
    //     // );
    //     checkBeacon(signalLostCounter, locked, secret);

    //     if (locked) {
    //         powerDown(CHECK_BEACON_LOCKED);
    //         loop();
    //     } else {

    //     }
    //     // printf("Signal lost. Retries: %i . Is locked: %i\r\n",
    //     //     signalLostCounter,
    //     //     locked
    //     // );
        
    // }
    
    // if (!locked) {
    //     bool connected = checkBeacon(signalLostCounter, locked, secret);

    //     if (!connected) {
    //         // blink
    //         #ifdef DEBUG
    //             printf("!connected\r\n");
    //             delay(5);
    //         #endif  
    //         if (signalLostCounter >= DISCONNECTED_BEACON_LOCK_THRESHOLD) {
    //             locked = true;
    //             powerDown(CHECK_BEACON_LOCKED);
    //         } else {
    //             powerDown(CHECK_BEACON_UNLOCKED_DISCONNECTED);
    //         }
    //         signalLostCounter++;
    //         loop();
    //     }

    //     powerDown(CHECK_BEACON_UNLOCKED);
    //     loop();
    // }
    
//     // else {
//     //     #ifdef DEBUG
//     //         printf("Signal lost. Retries: %i . Is locked: %i\r\n",
//     //             signalLost,
//     //             locked
//     //         );
//     //     #endif  
        
//     //     if (signalLost >= SIGNAL_LOST_RETRIES){
//     //         locked = true;
//     //     } else {
//     //         signalLost++;
//     //     }
//     // }

//     // if (locked){
//     //     digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH);
//     // } else if (!locked){
//     //     digitalWrite(SIGNAL_LOSS_INDICATOR, LOW);
//     // }

//     // delay(SLEEP_TIME_MS);
}

void initializePins() {
    pinMode(SIGNAL_LOSS_INDICATOR, OUTPUT);
    //digitalWrite(SIGNAL_LOSS_INDICATOR, HIGH); // On is LOW

    pinMode(POWER_TOGGLE_BUTTON, INPUT);

    pinMode(RELAY, OUTPUT);
    //digitalWrite(RELAY, HIGH); // LOW = on, HIGH = off
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

bool checkBeacon(unsigned int &retryCounter, bool &lock, char secret[]) {
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

            retryCounter = 0;
            lock = false;
            return true;
        }
        //return false;
    } //else {
       // return false;
    //}

    retryCounter++;
}

void powerDown(period_t period) {
    LowPower.powerDown(period, ADC_OFF, BOD_OFF);
}

// void toggleSignalLossIndicator(bool isOff = false) {

// }
