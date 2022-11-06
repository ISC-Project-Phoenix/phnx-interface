# Phoenix Interface ECU Board

This ECU board is responisble for taking in data from the PC and transforming that data into CAN 2.0b frames that
are then published on the CAN bus.

See [the design doc](https://github.com/ISC-Project-Phoenix/design/blob/main/software/Interface-ECU.md) for more info.

## Relevant Pins:
- High Priority Bus CAN RX Pin: 23
- High Priority Bus CAN TX Pin: 22
- Low Priority Bus CAN RX Pin: 2
- Low Priority Bus CAN RX Pin: 3