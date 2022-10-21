#include <Arduino.h>
#include <FlexCAN_T4.h>

#define AUTON_TOGGLE_PIN 32
#define ESTOP_PIN 31

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> h_priority;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> l_priority;
bool auton_disable;

struct message{
    uint8_t type;
    uint16_t len;
    uint8_t data[512];
} __attribute__((packed));

void send_encoder_data(const CAN_message_t &msg){
    //Send data back to the PC
    struct message enc_msg;
    enc_msg.type = msg.id;
    enc_msg.len = msg.len;
    for(uint8_t i = 0; i<msg.len; i++){
        enc_msg.data[i] = msg.buf[i];
    }
    Serial.write(reinterpret_cast<const char *>(&enc_msg), sizeof(message));
}

void kill_auton(const CAN_message_t &msg){
    //callback for filtered mailbox MB0 which only receives kill auton messages
    if(msg.id == 0x0){
        digitalWrite(LED_BUILTIN, HIGH);
        auton_disable = true;
    }
    else{
        Serial.write("This callback has fired with an incorrect message! :(");
    }

}

void get_control_messages(const CAN_message_t &msg){
    //callback for getting back messages from the bus
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(ESTOP_PIN, OUTPUT);
    pinMode(AUTON_TOGGLE_PIN, INPUT);

    //If this ecu crashes this pin should go low firing the ESTOP
    digitalWrite(ESTOP_PIN, HIGH);

    //Start in autonomous mode
    auton_disable = false;

    //Set up low priority CAN bus
    l_priority.begin();
    l_priority.setBaudRate(250000);

    //Set up high priority CAN bus
    h_priority.begin();
    h_priority.setBaudRate(250000);

    /*
     * Setup mailboxes for high priority bus
     */
    h_priority.setMB(MB0, RX, EXT);
    h_priority.setMB(MB1, RX, EXT);
    h_priority.setMB(MB2, TX, EXT);
    h_priority.setMBFilter(REJECT_ALL);
    h_priority.enableMBInterrupts();
    h_priority.onReceive(MB0, kill_auton);
    h_priority.onReceive(MB1, get_control_messages);
    h_priority.setMBFilter(MB0, 0x00);
    h_priority.setMBFilter(MB1, 0x1, 0x4, 0x5);
    h_priority.mailboxStatus();

    /*
     * Setup mailboxes for low priority bus
     */
    l_priority.setMB(MB0, RX, EXT);
    l_priority.setMBFilter(REJECT_ALL);
    l_priority.enableMBInterrupts();
    l_priority.onReceive(MB0, send_encoder_data);
    l_priority.setMBFilter(MB0, 0x6);
    l_priority.mailboxStatus();
}

void publish_data(message* msg){
    //Publish received control data onto the CAN bus
    CAN_message_t cmsg;
    switch(msg->type){
        case 1:
            //Send brake message out
            cmsg.id = msg->type;
            cmsg.len = msg->len;
            if(msg->len <= 8){
                //Make sure that we don't try to put 512 bytes of data into an 8 byte message
                for(uint8_t i = 0; i < msg->len; i++){
                    cmsg.buf[i] = msg->data[i];
                }
                h_priority.write(MB2, cmsg);
            }
            break;
        case 4:
            //Send steer message out
            //TODO: Change this to match spec of steering motor
            cmsg.id = msg->type;
            cmsg.len = msg->len;
            if(msg->len <= 8){
                for(uint8_t i = 0; i < msg->len; i++){
                    cmsg.buf[i] = msg->data[i];
                }
                h_priority.write(MB2, cmsg);
            }
            break;
        case 5:
            //Send throttle message out
            cmsg.id = msg->type;
            cmsg.len = msg->len;
            if(msg->len <= 8){
                for(uint8_t i = 0; i < msg->len; i++){
                    cmsg.buf[i] = msg->data[i];
                }
                h_priority.write(MB2, cmsg);
            }
            break;
    }
}

void receive_pc_data(){
    //Get new commands from PC
    struct message *msg;
    int count = 0;
    uint8_t buf[515];
    while(count < 515 && Serial.available()){
        buf[count++] = Serial.read();
    }
    if(count > 2){
        msg = reinterpret_cast<message*>(buf);
        if(!auton_disable){
            publish_data(msg);
        }
    }
}

void loop() {
    if(digitalRead(AUTON_TOGGLE_PIN) == HIGH){
        auton_disable = !auton_disable;
    }
    h_priority.events();
    if(Serial.available()){
        receive_pc_data();
    }
    l_priority.events();
}