Need to implement a flow: 

PC or Raspberry Pi 5 orchestrator that will have USB to RS-485 adapter connected to it. 
Then CAT5 or CAT6 cable connected to A+ B-. Orange and White-Orange wires;
Then It goes straign to MAX485 converter (aka RS485 to TTL converter). It has DI,DE,RE,RO (first side), VCC, A, B, GND (second side)
Then it is connected to Arduino Uno;
Then Arduino Uno is connected to DWM3001CDK board (GPIO14=pin8 and GPIO15=pin10);

Setup must have TX firmware and RX firmware;

Orchestrator will have list of commands to send: 

1. Ping = PNG
2. Start = STRT
3. Stop = STOP
4. Set Config = SET_CFG
5. Get Stats = STAT
6. Node Type = NT

Both nodes received these commands (no exception), firmware for arduino in `Arduino` folder and firmware for DWM3001CDK should cover this. Firware for DWM3001CDK is already written in `Src\examples\ex_22_orchestrator_v2` folder.

Both RS485 to TTL converter has connections with arduino uno:

D2 = RE
D3 = DE
D11 = DI
D10 = RO
GND (near D2) = GND
5V = VCC

A+ B- is connected to USB RS485 adapter (it will be from 1m to 50m)

Choose what ever free D pins to communicate with DWM3001CDK

The idea is pretty simple: 
    Orchestrator sends start command to both nodes and they begin TX sending, RX receiving
    Very important is to send packages via UWB (crucial)
    MAX485 Arduino Uno are just middlewares in between orchestrator and DWM3001CDK