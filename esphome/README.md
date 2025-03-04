# ESPHome projects

This directory contains current and older (archived) ESPHome device configurations.

## Contents

### TODO
placeholder

### Archive
* bedroom_sensor_node - ESP8266 and HTU21D based sensor node. Has an additional analog and digital input for a microphone module that never worked. Uses MQTT instead of the native API.
* bedside_lcd_sensor - ESP8266, MQTT. HTU21D and SSD1306 modules connected via I2C. The OLED screen displays the current date and temperature. Time/date is acquired via SNTP. Requires "Roboto-Regular.ttf".
* lr_rgbstrib - ESP8266, MQTT. Drives an RGB light strip using PWM. Has support for a button to toggle the lights.
* micro_sensor_node - ESP8266, MQTT. BMP280 connected via I2C, PIR and ultrasonic sensor modules connected via GPIO.
* oldsensor - first ESPHome project, uses an ESP8266 and MQTT. AM2303 provides temperature and humidity readings, an LDR connected to the ADC measures illuminance.
* tempsensor - ESP8266, MQTT. HTU21D, BMP280 and BH1750 connected via I2C.
* thermostat_byc17gh3 - ESP8266 project using the [FloureonThermostat](../archive/floureon) custom component (thermostat MCU connected via UART). Additional SHT3xD and BH1750 modules connected via I2C, relay control via GPIO.