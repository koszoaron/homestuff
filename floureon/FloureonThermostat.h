#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
//namespace floureon {

static const uint32_t SEND_DELAY_MS = 10000;
static const uint32_t FIRST_DELAY_MS = 500;

enum class FloureonCommand : uint8_t {
    QUERY = 0xC0,
    SETTINGS1 = 0xC1,
    SETTINGS2 = 0xC2,
    SCHEDULE_DAY1 = 0xC3,
    SCHEDULE_DAY2 = 0xC4,
    SCHEDULE_DAY3 = 0xC5,
    SCHEDULE_DAY4 = 0xC6,
    SCHEDULE_DAY5 = 0xC7,
    SCHEDULE_DAY6 = 0xC8,
    SCHEDULE_DAY7 = 0xC9,
};

enum class FloureonWifiState : uint8_t {
    CONFIG = 0x00,
    UNKNOWN1 = 0x01,
    DISCONNECTED = 0x02,
    CONNECTING = 0x03,
    DISCONNECTING = 0x04,
    CONNECTED = 0x05,
    UNKNOWN2 = 0x06
};

enum class FloureonBoolean : uint8_t {
    DISABLED = 0x00,
    ENABLED = 0xFF
};

enum class FloureonSensorMode : uint8_t {
    INTERNAL_SENSOR = 0x00,
    EXTERNAL_SENSOR = 0x01,
    BOTH_SENSORS = 0x02
};

class FloureonThermostat : public Component, public uart::UARTDevice, public climate::Climate {
  public:
    FloureonThermostat(uart::UARTComponent *parent) : UARTDevice(parent) { 
        time_next_send_ = FIRST_DELAY_MS + millis();
        next_command_ = FloureonCommand::SETTINGS1;
        first_queries_done_ = false;
        settings_changed_ = false;
        settings1_wifi_state_ = FloureonWifiState::CONFIG;
        settings1_temperature_internal_ = 0;
        settings1_temperature_external_ = 0;
        settings1_temperature_set_ = 0;
        settings1_thermostat_locked_ = FloureonBoolean::DISABLED;
        settings1_thermostat_manual_ = FloureonBoolean::DISABLED;
        settings1_thermostat_power_ = false;
        settings2_backlight_ = FloureonBoolean::DISABLED;
        settings2_restore_ = FloureonBoolean::DISABLED;
        settings2_antifreeze_ = FloureonBoolean::DISABLED;
        settings2_temperature_correction_ = 0;
        settings2_hysteresis_internal_ = 0;
        settings2_hysteresis_external_ = 0;
        settings2_sensor_mode_ = FloureonSensorMode::INTERNAL_SENSOR;
        settings2_external_limit_ = 0;
        relay_state_ = false;
    }
    
    float get_setup_priority() const override { return setup_priority::LATE; }
    void setup() override;
    void loop() override;
    void set_time_identifier(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
    void set_relay_identifier(binary_sensor::BinarySensor *relay_id) { this->relay_id_ = relay_id; };

  protected:
    void send_message_(uint8_t *data, uint8_t length);
    void handle_char_(uint8_t c);
    bool validate_message_();
    void handle_message_(const uint8_t *buffer, size_t len);
    climate::ClimateTraits traits() override;
    void control(const climate::ClimateCall &call) override;

    std::vector<uint8_t> rx_message_;
    optional<time::RealTimeClock *> time_id_{};
    optional<binary_sensor::BinarySensor *> relay_id_{};

  private:
    uint32_t time_next_send_;
    FloureonCommand next_command_;
    bool first_queries_done_;
    bool settings_changed_;
    FloureonWifiState settings1_wifi_state_;  /* SETTINGS1 byte 3 */
    uint16_t settings1_temperature_internal_;  /* SETTINGS1 byte 4-5 */
    uint16_t settings1_temperature_external_;  /* SETTINGS1 byte 6-7 */
    uint8_t settings1_temperature_set_;  /* SETTINGS1 byte 8, divide by 2 for ESPHome */
    FloureonBoolean settings1_thermostat_locked_;  /* SETTINGS1 byte 12 */
    FloureonBoolean settings1_thermostat_manual_;  /* SETTINGS1 byte 13 */
    bool settings1_thermostat_power_;  /* SETTINGS1 byte 14, 0 is false, 1 is true */
    FloureonBoolean settings2_backlight_;  /* SETTINGS2 byte 3 */
    FloureonBoolean settings2_restore_;  /* SETTINGS2 byte 4 */
    FloureonBoolean settings2_antifreeze_;  /* SETTINGS2 byte 5 */
    uint8_t settings2_temperature_correction_;  /* SETTINGS2 byte 6 */
    uint8_t settings2_hysteresis_internal_;  /* SETTINGS2 byte 7 */
    uint8_t settings2_hysteresis_external_;  /* SETTINGS2 byte 8 */
    FloureonSensorMode settings2_sensor_mode_;  /* SETTINGS2 byte 10 */
    uint8_t settings2_external_limit_;  /* SETTINGS2 byte 11 */
    bool relay_state_;
};

//}  // namespace floureon
}  // namespace esphome
