#include "interface.hpp"

message Interface::convert_to_serial(const CAN_message_t &msg) {
    struct message smsg;
    smsg.type = msg.id;
    smsg.len = msg.len;
    for(int i = 0; i < msg.len; i++){
        smsg.data[i] = msg.buf[i];
    }
    return smsg;
}

CAN_message_t Interface::convert_to_can(message *msg) {
    CAN_message_t cmsg;
    cmsg.id = msg->type;
    cmsg.flags.extended = true;
    cmsg.len = msg->len;
    if(msg->len <= 8){
        for(int i = 0; i < msg->len; i++){
            cmsg.buf[i] = msg->data[i];
        }
    }
    return cmsg;
}

Interface::Interface() = default;
