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
#define CE          9  // Toggle between transmit (TX), receive (RX), standby
    // and power-down mode
#define CSN         10 // SPI chip select 

#define PAYLOAD_SIZE        32

#define CHECK_BEACON_LOCKED SLEEP_500MS
#define CHECK_BEACON_UNLOCKED SLEEP_120MS
// 1 = how many times transmitter sends per second
#define DISCONNECTED_BEACON_RETRY_THRESHOLD (1 * 1000 / 120) * 5 // = 8.333 * 5
    // ( 1 second / CHECK_BEACON_UNLOCKED sleep time ) * how many seconds
#define DISCONNECTED_BEACON_RETRY_THRESHOLD_TOLERANCE (1 * 1000 / 120) * 1.09
    // 8.333 -> 9

const uint64_t pipe = 0xE8E8F0F0E1LL;
char secret[32] = "77da4ba6-fdf2-11e7-8be5-0ed5ffff"; // line ending char
    // missing, thus compiler complaining

bool lock = true;
bool prepareToLock = false;
bool powerUp = false;
unsigned int signalLostCounter = DISCONNECTED_BEACON_RETRY_THRESHOLD + 1;

RF24 radio(CE, CSN);

#ifdef DEBUG
    int convB(bool value) {
        if (value == true) {
            return 1;
        } else {
            return 0;
        }
    }
#endif

void initializePins() {
    pinMode(SIGNAL_LOSS_INDICATOR, OUTPUT);
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

void pciSetup(byte pin){
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}

void attachInterrupts() {
    pciSetup(POWER_TOGGLE_BUTTON);
}

ISR(PCINT2_vect) { // handle pin change interrupt
    if (digitalRead(POWER_TOGGLE_BUTTON)) {
        powerUp = !powerUp;
    }
}

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
    }

    retryCounter++;
}

void powerDown(period_t period) {
    LowPower.powerDown(period, ADC_OFF, BOD_OFF);
}


void checkRetryOverflow(unsigned int &counter) {
    if (counter == 65534) {
        counter = DISCONNECTED_BEACON_RETRY_THRESHOLD + 1;
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
    attachInterrupts();
}

void loop(void) {
    if (lock && !prepareToLock) {
        powerDown(CHECK_BEACON_LOCKED);
    } else {
        powerDown(CHECK_BEACON_UNLOCKED); // check more frequently
    }

    bool gotSignal = checkBeacon(signalLostCounter, lock, secret);

    if (gotSignal) {
        prepareToLock = false;
    }

    if (!lock) {
        if (signalLostCounter > DISCONNECTED_BEACON_RETRY_THRESHOLD_TOLERANCE) {
            prepareToLock = true;
        }

        if (signalLostCounter >= DISCONNECTED_BEACON_RETRY_THRESHOLD) {
            lock = true;
            prepareToLock = false;
            powerUp = false;
        }

        if (prepareToLock) {
            toggleSignalLossIndicator();
        }
    }

    #ifdef DEBUG
        printf("Signal lost. Retries: %i. Is locked: %i. prepareToLock: %i. powerUp: %i\r\n",
            signalLostCounter,
            convB(lock),
            convB(prepareToLock),
            convB(powerUp)
        );
        delay(10);
    #endif

    if (!prepareToLock) {
        toggleSignalLossIndicator(false);
    }

    if (powerUp && !lock) {
        toggleLocks(false);  
    } else {
        toggleLocks(true);
    }
    //toggleLocks(lock);
    checkRetryOverflow(signalLostCounter);
}
