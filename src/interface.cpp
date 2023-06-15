#include "interface.hpp"

message Interface::convert_to_serial(const CAN_message_t &msg) {
    message serial_msg{};
    serial_msg.header = PKG_HEADER;
    serial_msg.type = msg.id;
    serial_msg.len = msg.len;
    for (int i = 0; i < msg.len; i++) {
        serial_msg.data[i] = msg.buf[i];
    }
    return serial_msg;
}

CAN_message_t Interface::convert_to_can(message *msg) {
    CAN_message_t can_msg;
    can_msg.id = msg->type;
    can_msg.flags.extended = true;
    can_msg.len = msg->len;
    if (msg->len <= 8) {
        for (int i = 0; i < msg->len; i++) {
            can_msg.buf[i] = msg->data[i];
        }
    }
    return can_msg;
}

Interface::Interface() = default;
