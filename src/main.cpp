#include <Arduino.h>
#include <FlexCAN_T4.h>

#define AUTON_TOGGLE_PIN 32

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> h_priority;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> l_priority;
bool auton_disable;

struct message{
    uint8_t type;
    uint16_t len;
    uint8_t data[512];
} __attribute__((packed));

void send_pc_data(const CAN_message_t &msg){
    //Send data back to the PC
}

void kill_auton(const CAN_message_t &msg){
    //Only thing that should be coming off high priority bus will be auto disable messages for now
}

void setup() {
    //We want to boot into autonomous mode
    Serial.begin(115200);
    auton_disable = false;

    //Set up low priority CAN bus
    l_priority.begin();
    l_priority.setBaudRate(250000);
    l_priority.enableFIFO();
    l_priority.enableFIFOInterrupt();
    l_priority.onReceive(send_pc_data);

    //Set up high priority CAN bus
    h_priority.begin();
    h_priority.setBaudRate(250000);
    h_priority.enableFIFO();
    h_priority.enableFIFOInterrupt();
    h_priority.onReceive(kill_auton);
    //pinMode(AUTON_TOGGLE_PIN, INPUT);
}

void publish_data(message* msg){
    //Publish received data onto the CAN bus
    switch(msg->type){
        case 1:
            //Send brake message out
            break;
        case 4:
            //Send steer message out
            break;
        case 5:
            //Send throttle message out
            break;
    }
}

void recieve_pc_data(){
    //Get new commands from PC
    struct message *msg = nullptr;
    int count = 0;
    uint8_t buf[515];
    while(count < 515 && Serial.available()){
        //Serial.write("Got data from PC!");
        buf[count++] = Serial.read();
    }
    Serial.print("\n");
    Serial.print("Got data from PC, Converting...\n");
    if(count > 2){
        msg = reinterpret_cast<message*>(buf);
        if(!auton_disable){
            publish_data(msg);
        }
        /*switch(msg->type){
            case 1:
                Serial.print("Got Brake Message!");
                publish_data(msg);
                break;
            case 4:
                Serial.print("Got Steering Message!");
                for(uint8_t i = 0; i < msg->len; i++){
                    Serial.print(msg->data[i]);
                    Serial.print("\n");
                }
                break;
            default:
                Serial.print("Got unknown message!");
                break;
        }*/
    }
}

void loop() {
    //Buffer for serial data
    if(Serial.available()){
        recieve_pc_data();
    }
    h_priority.events();
    l_priority.events();
}