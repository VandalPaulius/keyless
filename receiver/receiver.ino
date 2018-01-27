// moto-alarm Receiver
//Used example from http://forum.arduino.cc/index.php?topic=421081

#include  <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include "printf.h"

#define DEBUG

#define LED 2

//NRF24l01
#define CE          9  //Toggle between transmit (TX), receive (RX), standby, and power-down mode
#define CSN         10 //SPI chip select 

#define PAYLOAD_SIZE 31

const uint64_t pipe = 0xE8E8F0F0E1LL;
char secret[30] = "77da4ba6-fdf2-11e7-8be5-0ed5ff";

uint8_t state = 0;

RF24 radio(CE, CSN);

void setup(void) {
    #ifdef DEBUG
        Serial.begin(115200);
        printf_begin();
    #endif

    initializeRadio();
    
    pinMode(LED, OUTPUT);
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

        if(strncmp(secret, message, sizeof(secret)) == 0){ //Secret matches

            #ifdef DEBUG
                printf("Secret matches\r\n");
            #endif

            if(message[PAYLOAD_SIZE - 1] == '0'){ //LOCK
                state = 0;
                #ifdef DEBUG
                    printf("State 0\r\n");
                #endif
            }
            else if(message[PAYLOAD_SIZE - 1] == '1'){ //UNLOCK
                state = 1;
                #ifdef DEBUG
                    printf("State 1\r\n");
                #endif
            }
        }
    }

    if(state == 0){
        digitalWrite(LED, HIGH);
    }
    else if(state == 1){
        digitalWrite(LED, LOW);
    }

    delay(100);
}

void initializeRadio(){
    radio.begin();
    radio.setDataRate(RF24_250KBPS);
    radio.setPayloadSize(PAYLOAD_SIZE);
    radio.openReadingPipe(1, pipe);
    radio.setPALevel(RF24_PA_HIGH);
    radio.startListening();
}