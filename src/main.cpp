#include "interface.hpp"

constexpr int AUTON_TOGGLE_PIN = 32;
constexpr int ESTOP_DEADMAN_PIN = 31;
constexpr int ESTOP_REFERENCE_PIN = 30;
constexpr int PAUSE_REFERENCE_PIN = 29;

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> h_priority;
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> l_priority;

enum CanMappings {
    KillAuton = 0x0,
    TrainingMode = 0x8,
};

bool training_mode = false;
bool auton_kill = false;

Interface interface_ecu{};

void can_receive(const CAN_message_t &msg) {
    message smsg = interface_ecu.convert_to_serial(msg);
    if (msg.id == KillAuton) {
        auton_kill = true;
        digitalWrite(LED_BUILTIN, HIGH);
    }
    Serial.write(reinterpret_cast<const char *>(&smsg), sizeof(message));
}

void can_send(message &msg) {
    if (!auton_kill && !training_mode) {
        CAN_message_t cmsg = interface_ecu.convert_to_can(&msg);
        if (cmsg.id == TrainingMode) {
            training_mode = true;
        }
        h_priority.write(cmsg);
    }
}

void receive_pc_data() {
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
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(ESTOP_DEADMAN_PIN, OUTPUT);
    pinMode(AUTON_TOGGLE_PIN, INPUT);
    pinMode(ESTOP_REFERENCE_PIN, INPUT);
    pinMode(PAUSE_REFERENCE_PIN, INPUT);

    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);

    //If this ecu crashes this pin should go low firing the ESTOP
    digitalWrite(ESTOP_DEADMAN_PIN, HIGH);

    //Set up low priority CAN bus
    l_priority.begin();
    l_priority.setBaudRate(250000);

    //Set up high priority CAN bus
    h_priority.begin();
    h_priority.setBaudRate(500000);

    /*
     * Setup mailboxes for high priority bus
     */
    //Mailbox dedicated to receiving control messages
    h_priority.setMB(MB0, RX, EXT);

    //Mailbox dedicated to sending control messages onto high priority bus
    h_priority.setMB(MB2, TX, EXT);
    h_priority.setMBFilter(REJECT_ALL);
    h_priority.enableMBInterrupts();
    h_priority.onReceive(MB0, can_receive);
    h_priority.setMBFilter(MB0, 0x00, 0x1, 0x4, 0x5);
    h_priority.mailboxStatus();

    /*
     * Setup mailboxes for low priority bus
     */
    //Mailbox dedicated to receiving encoder messages
    l_priority.setMB(MB0, RX, EXT);
    l_priority.setMBFilter(REJECT_ALL);
    l_priority.enableMBInterrupts();
    l_priority.onReceive(MB0, can_receive);
    l_priority.setMBFilter(MB0, 0x6);
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
        noInterrupts();
        receive_pc_data();
        interrupts();
    }

    l_priority.events();

    // Puts the proc in low power until the next interrupt fires
    asm volatile("wfi":: : "memory");
}
