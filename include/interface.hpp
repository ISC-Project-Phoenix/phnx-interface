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
    /// \param msg reference to CAN frame to pull data from
    /// \return message struct with properly configured serial message data
    static message convert_to_serial(const CAN_message_t &msg);

    /// Convert serial data to CAN frames
    /// \param msg pointer to message to pull data from
    /// \return CAN frame with properly configured CAN frame data
    static CAN_message_t convert_to_can(message *msg);

};
