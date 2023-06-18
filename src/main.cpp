#include "interface.hpp"

constexpr int SERIAL_BAUD = 115200;
constexpr int AUTON_TOGGLE_PIN = 32;
constexpr int ESTOP_DEADMAN_PIN = 31;
constexpr int ESTOP_REFERENCE_PIN = 30;
constexpr int PAUSE_REFERENCE_PIN = 29;
constexpr int LOW_PRIORITY_BAUD = 250000;
constexpr int HIGH_PRIORITY_BAUD = 500000;

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> h_priority;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> l_priority;

bool training_mode = false;
bool auton_kill = false;

message estop_msg{
        .header = PKG_HEADER,
        .type = CanMappings::KillAuton,
        .len = 0x0,
};

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

void estop() {
    //Either estop pin or pause pin has changed so send an auton kill message to the pc
    auton_kill = true;

    //Send an auton kill message to pc to activate software estop
    Serial.write(reinterpret_cast<const char *>(&estop_msg), HEADER_SIZE + estop_msg.len);
    digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(ESTOP_DEADMAN_PIN, OUTPUT);
    pinMode(AUTON_TOGGLE_PIN, INPUT);
    pinMode(ESTOP_REFERENCE_PIN, INPUT);
    pinMode(PAUSE_REFERENCE_PIN, INPUT);

    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);

    //If this ecu crashes, this pin should go low firing the ESTOP
    digitalWrite(ESTOP_DEADMAN_PIN, HIGH);

    //Set up high priority CAN bus
    h_priority.begin();
    h_priority.setBaudRate(HIGH_PRIORITY_BAUD);

    //Set up low priority CAN bus
    l_priority.begin();
    l_priority.setBaudRate(LOW_PRIORITY_BAUD);

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

    /*
     * Setup mailboxes for the low-priority bus
     */
    //Mailbox dedicated to receiving encoder messages
    l_priority.setMB(MB0, RX, EXT);
    l_priority.setMBFilter(REJECT_ALL);
    l_priority.enableMBInterrupts();
    l_priority.onReceive(MB0, serial_send);
    l_priority.setMBFilter(MB0, CanMappings::EncoderTick);
    l_priority.mailboxStatus();

    attachInterrupt(ESTOP_REFERENCE_PIN, estop, FALLING);
    attachInterrupt(PAUSE_REFERENCE_PIN, estop, FALLING);
}

void loop() {
    if (digitalRead(AUTON_TOGGLE_PIN) == HIGH) {
        auton_kill = !auton_kill;
    }
    h_priority.events();

    if (Serial.available() && !auton_kill && !training_mode) {
        noInterrupts()
        serial_receive();
        interrupts()
    }

    l_priority.events();

    // Puts the proc in low power until the next interrupt fires
    asm volatile("wfi":: : "memory");
}
