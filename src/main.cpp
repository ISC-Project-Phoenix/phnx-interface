#include "interface.hpp"

constexpr int SERIAL_BAUD = 115200;
constexpr int HIGH_PRIORITY_BAUD = 500000;

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> h_priority;

//Send received CAN messages to the pc via the serial connection
void serial_send(const CAN_message_t &can_msg) {
    message serial_msg = Interface::convert_to_serial(can_msg);

    Serial.write(reinterpret_cast<const char *>(&serial_msg), HEADER_SIZE + serial_msg.len);
}

//Send received serial messages onto the CAN bus
void can_send(message &msg) {
    CAN_message_t cmsg = Interface::convert_to_can(&msg);

    h_priority.write(cmsg);
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

/// On estop activated, we set the brake to brake the kart, and signal the transition to ROS.
void estop_stopped() {
    message brake_full{
            0x54,
            CanMappings::SetBrake,
            1,
            {0x37}
    };

    message pc_kill{
            0x54,
            CanMappings::KillAuton,
            0,
            {}
    };

    can_send(brake_full);
    serial_send(Interface::convert_to_can(&pc_kill));
}

/// On estop released, we set the brake to 0, and signal the transition to ROS.
void estop_released() {
    message brake_zero{
            0x54,
            CanMappings::SetBrake,
            1,
            {0}
    };

    message pc_enable{
            0x54,
            CanMappings::EnableAuton,
            0,
            {}
    };

    can_send(brake_zero);
    serial_send(Interface::convert_to_can(&pc_enable));
}

volatile bool estopped = true;
volatile uint32_t last_stop = 0;

void estop_cb() {
    // Debounce
    if (millis() - last_stop > 700) {
        estopped = !estopped;

        if (estopped) {
            estop_stopped();
        } else {
            estop_released();
        }

        last_stop = millis();
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);
    pinMode(LED_BUILTIN, OUTPUT);

    // Estop state. Will be LOW when ESTOP is ESTOPed, HIGH when vehicle is active.
    pinMode(ESTOP_QUERY_PIN, INPUT_PULLDOWN);

    attachInterrupt(ESTOP_QUERY_PIN, &estop_cb, CHANGE);

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
    h_priority.setMBFilter(ACCEPT_ALL);
    h_priority.enableMBInterrupts();
    h_priority.onReceive(MB0, serial_send);
    h_priority.mailboxStatus();

    // Zero brake when boot, as we start stopped
    message brake_zero{
            0x54,
            CanMappings::SetBrake,
            1,
            {0}
    };
    can_send(brake_zero);
}

void loop() {
    h_priority.events();

    if (Serial.available()) {
        noInterrupts()
        serial_receive();
        interrupts()
    }
}
