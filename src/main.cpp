#include "interface.hpp"

constexpr int SERIAL_BAUD = 115200;
constexpr int HIGH_PRIORITY_BAUD = 500000;

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> h_priority;

bool training_mode = false;
bool auton_kill = false;

//Send received CAN messages to the pc via the serial connection
void serial_send(const CAN_message_t &can_msg) {
    message serial_msg = Interface::convert_to_serial(can_msg);
    if (can_msg.id == CanMappings::KillAuton) {
        auton_kill = true;
        digitalWrite(LED_BUILTIN, HIGH);
    }
    Serial.write(reinterpret_cast<const char *>(&serial_msg), HEADER_SIZE + serial_msg.len);
}

//Send received serial messages onto the CAN bus
void can_send(message &msg) {
    if (!auton_kill && !training_mode) {
        CAN_message_t cmsg = Interface::convert_to_can(&msg);
        if (cmsg.id == CanMappings::TrainingMode) {
            training_mode = true;
        }

        h_priority.write(cmsg);
    }
}

void serial_receive() {
    //Get new commands from PC
    struct message *msg;
    int count = 0;
    uint8_t buf[515];
    while (count < 515 && Serial.available()) {
        buf[count++] = Serial.read();
    }
    if (count > 2) {
        msg = reinterpret_cast<message *>(buf);
        can_send(*msg);
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(LED_BUILTIN, OUTPUT);

    digitalToggle(LED_BUILTIN);

    //Set up high priority CAN bus
    h_priority.begin();
    h_priority.setBaudRate(HIGH_PRIORITY_BAUD);

    /*
     * Setup mailboxes for the high-priority bus
     */
    //Mailbox dedicated to sending control messages onto the high-priority bus
    h_priority.setMB(MB2, TX, EXT);

    //Mailbox dedicated to receiving control messages
    h_priority.setMB(MB0, RX, EXT);
    h_priority.setMBFilter(REJECT_ALL);
    h_priority.enableMBInterrupts();
    h_priority.onReceive(MB0, serial_send);
    h_priority.setMBFilter(MB0, CanMappings::KillAuton, CanMappings::SetBrake, CanMappings::SetAngle,
                           CanMappings::GetAngle, CanMappings::SetThrottle);
    h_priority.mailboxStatus();
}

void loop() {
    h_priority.events();

    if (Serial.available() && !auton_kill && !training_mode) {
        noInterrupts()
        serial_receive();
        interrupts()
    }
}
