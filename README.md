Equipment:
- Arduino Uno, with Power supply (USB)
- Grove HAT
- Grove Display LCD 16x2 RGB Backlight v5.0, connected to I2C port
- Grove Temperature & Humidity Sensor v2.0, connected to I2C port
- Grove Relay v1.3, connected to digital pin 4
- Power supply for pump, 12V 2A DC
- BrushlessDC-1238B 12V 4.8W IP68 Submersible pump (240l/h)
  connected to power supply (with one wire through the Relay)
- Tube for water flow (3/8" inner diameter, 2m length)

Requirements:
- Upon startup, pump turns on for 20s
- Every 30 minutes, turns on for 20s
- Display shows:
    - First line: temperature & humidity
    - Second line: 
        - timer in minutes counting down to next pump activation (when off)
          (e.g. "Pump off 29m")
        - timer in seconds counting down to next pump deactivation (when on)
          (e.g. "Pump on 15s")
    - RGB backlight color changes based on pump state:
        - Green when pump is off and will be activated in less than 5 minutes
        - Red when pump is on

Non-functional requirements:
- Code should be well-structured and commented for readability
- Use of functions to encapsulate logic for pump control, sensor reading, and display updates
- Efficient use of resources, avoiding unnecessary delays or blocking code
- Use of constants and variables to allow easy adjustments of timing and PIN configurations
- Logging of pump activation and deactivation events, temperature, and humidity readings to
  the serial monitor for debugging purposes
- Use #ifdef blocks to allow turning off:
    - Serial logging ("#define ENABLE_SERIAL_LOGGING" at the top of the code)
    - Use of the display ("#define ENABLE_DISPLAY" at the top of the code)
    - Use of the temperature and humidity sensor ("#define ENABLE_TEMP_HUMIDITY_SENSOR" at the 
      top of the code)

Pricing overview (estimated):

| Component                                | Setup 1<br>(basic) | Setup 2<br>(+ display) | Setup 3<br>(+Temp/Hum) |
|------------------------------------------|-----------:|------------:|------------:|
| Arduino Uno                              |     €25.00 |      €25.00 |      €25.00 |
| Grove Base Shield (HAT)                  |     €10.00 |      €10.00 |      €10.00 |
| Grove Relay v1.3                         |      €5.00 |       €5.00 |       €5.00 |
| Submersible Pump (12V DC)                |     €15.00 |      €15.00 |      €15.00 |
| Tube (3/8", 2m)                          |      €5.00 |       €5.00 |       €5.00 |
| Power Supply USB (Arduino)               |      €8.00 |       €8.00 |       €8.00 |
| Power Supply 12V 2A DC (Pump)            |     €10.00 |      €10.00 |      €10.00 |
| Grove LCD 16x2 RGB Backlight v5.0        |          — |      €15.00 |      €15.00 |
| Grove Temperature & Humidity Sensor v2.0 |          — |           — |      €12.00 |
| **Total**                                | **€78.00** |  **€93.00** | **€105.00** |
