#include <Arduino.h>
#include <FlexCAN_T4.h>

struct message {
    uint8_t type;
    uint16_t len;
    uint8_t data[512];
} __attribute__((packed));

class Interface {
public:
    Interface();

    /// Convert CAN frames to serial data
    message convert_to_serial(const CAN_message_t &msg);

    /// Convert serial data to CAN frames
    CAN_message_t convert_to_can(message *msg);

};
