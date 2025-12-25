# AutoPump
Standalone control system for an air compressor. Intended use is for inflating and deflating tyres when offroading. Can operate in automatic and manual modes.

Auto mode: Allows the user to specify a pressure setpoint that the controller will strive to reach by switching on an air compressor or opening a solenoid.
Manual mode: Gives the user direct control over the air compressor and solenoid.

It pains me that the raw calcs are in imperial units... but ceebs re-writing using metric for the raw calcs._

Parts (high level):
- ESP32 (denky32/wroom)
- PM pneumatic connectors
- SH pnematic connectors
- 12mm/8mm (OD/ID) pneumatic tubing
- Relay board (control: 3.3VDC, supply: 5VDC)
- Buck converter (12VDC input, 5VDC output)
- ADS1115 (external ADC used due to non-linearity of onboard ADCs of the ESP32)
- Pressure transmitter (0-150psi => 0.5-4.5VDC)
- Solenoid (12VDC)
- Pressure relief valve (sized so tyres don't explode, keep in mind transient pressures will exceed setpoints)
- ESP32 breakout board
- Standard header breakout board
- Fly-wiring
- Manifold (1/4 BSP)
- Bistable switches
- Pushbutton switch
