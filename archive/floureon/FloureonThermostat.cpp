#include "FloureonThermostat.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {

static const char *TAG = "floureon";
static const uint8_t MESSAGE_SIZE = 16;

void FloureonThermostat::setup() {
    /* nothing at the moment, initialization of members is done in the constructor */
}

void FloureonThermostat::loop() {
    /* handle received data */
    while (this->available()) {
        uint8_t c;

        /* read a single byte from UART and process it */
        this->read_byte(&c);
        this->handle_char_(c);
    }

    /* handle data to be sent */
    uint32_t timeNow = millis();
    if (time_next_send_ < timeNow) {
        /* process the first queries faster */
        if (first_queries_done_) {
            time_next_send_ = timeNow + SEND_DELAY_MS;
        } else {
            time_next_send_ = timeNow + FIRST_DELAY_MS;
        }
        
        /* send the new settings if required */
        if (settings_changed_) {
            settings_changed_ = false;

            /* compose the message */
            uint8_t settingsMsg[MESSAGE_SIZE] = { 0 };
            settingsMsg[0] = 0xAA;  /* header byte 1 */
            settingsMsg[1] = 0x55;  /* header byte 2 */
            settingsMsg[2] = (uint8_t)FloureonCommand::SETTINGS1;  /* SETTINGS1 contains all the settings changeable by the thermostat interface */
            settingsMsg[3] = (uint8_t)FloureonWifiState::CONNECTED;  /* always reported connected, otherwise settings are not accepted */

            /* check if time is available */
            if (this->time_id_.has_value()) {
                auto time_id = *this->time_id_;
                auto now = time_id->now();

                /* set the day, hour and minute bytes */
                if (now.is_valid()) {
                    settingsMsg[4] = now.day_of_week - 1;
                    if (settingsMsg[4] == 0) {
                        settingsMsg[4] = 7;
                    }
                    settingsMsg[5] = now.hour;
                    settingsMsg[6] = now.minute;
                }
            } else {
                /* send 0xFF-s when no time is available */
                settingsMsg[4] = 0xFF;
                settingsMsg[5] = 0xFF;
                settingsMsg[6] = 0xFF;
            }
            
            settingsMsg[7] = 0xFF;  /* unknown byte, set to 0xFF */
            settingsMsg[8] = settings1_temperature_set_;  /* desired temperature */
            settingsMsg[9] = 0xFF;  /* unknown byte, set to 0xFF */
            settingsMsg[10] = 0xFF;  /* unknown byte, set to 0xFF */
            settingsMsg[11] = 0xFF;  /* unknown byte, set to 0xFF */
            settingsMsg[12] = (uint8_t)settings1_thermostat_locked_;  /* lock status */
            settingsMsg[13] = (uint8_t)settings1_thermostat_manual_;  /* mode selection */
            settingsMsg[14] = (uint8_t)settings1_thermostat_power_;  /* power */

            /* calculate the checksum and send the message */
            this->send_message_(settingsMsg, sizeof(settingsMsg));
        } else {
            /* otherwise query a thermostat register */
            uint8_t queryMsg[MESSAGE_SIZE] = { 0 };
            queryMsg[0] = 0xAA;
            queryMsg[1] = 0x55;
            queryMsg[2] = (uint8_t)FloureonCommand::QUERY;
            queryMsg[3] = (uint8_t)next_command_;

            /* calculate the checksum and send the message */
            this->send_message_(queryMsg, sizeof(queryMsg));

            /* set the register to query next */
            if (next_command_ == FloureonCommand::SCHEDULE_DAY7) {
                next_command_ = FloureonCommand::SETTINGS1;
                first_queries_done_ = true;
            } else {
                next_command_ = (FloureonCommand)((uint8_t)next_command_ + 1);
            }
        }
    } else if (settings_changed_ && (timeNow + FIRST_DELAY_MS < time_next_send_)) {
        /* bring forward the time when the settings are sent */
        time_next_send_ = timeNow + FIRST_DELAY_MS;
    }

    /* check the relay state (if available) and update the thermostat action (only when changed) */
    if (this->relay_id_.has_value()) {
        auto relay_id = *this->relay_id_;
        auto state = relay_id->state;

        /* only when the state has changed */
        if (state != relay_state_) {
            relay_state_ = state;

            if (state) {
                /* when the relay is on, the action is 'heating' */
                this->action = climate::ClimateAction::CLIMATE_ACTION_HEATING;
            } else if (this->mode == climate::ClimateMode::CLIMATE_MODE_HEAT) {
                /* when the relay is off, but the thermostat is in heating mode, the action is 'idle' */
                this->action = climate::ClimateAction::CLIMATE_ACTION_IDLE;
            } else {
                /* otherwise, the relay is off and the thermostat is in off mode, de the action is 'off' */
                this->action = climate::ClimateAction::CLIMATE_ACTION_OFF;
            }

            /* publish the new state */
            this->publish_state();
        }
    }
}

void FloureonThermostat::send_message_(uint8_t *data, uint8_t length) {
    /* calculate the checksum of the message */
    for (uint8_t i = 0; i < length - 1; i++) {
        data[length - 1] += data[i];
    }

    /* send the data over */
    ESP_LOGV(TAG, "Sending message:  [%s]", hexencode(data, length).c_str());
    this->write_array(data, length);
}

void FloureonThermostat::handle_char_(uint8_t c) {
    this->rx_message_.push_back(c);
    if (!this->validate_message_()) {
        this->rx_message_.clear();
    }
}

bool FloureonThermostat::validate_message_() {
    uint32_t at = this->rx_message_.size() - 1;
    auto *data = &this->rx_message_[0];
    uint8_t last_byte = data[at];

    /* byte 0: header (always 0xAA) */
    if (at == 0) {
        return (last_byte == 0xAA);
    }
  
    /* byte 1: header (always 0x55) */
    if (at == 1) {
        return (last_byte == 0x55);
    }

    /* byte 2: message type (0xC0 to 0xC9) */
    if (at == 2) {
        return (last_byte >= 0xC0 && last_byte <= 0xC9);
    }

    /* bytes 3-14: payload, no verification is required */
    if (at >= 3 && at <= 14)
    {
        return true;
    }

    /* byte 15: checksum, which is excluded from the calculation */
    uint8_t rx_checksum = last_byte;
    uint8_t calc_checksum = 0;
    for (uint32_t i = 0; i < at; i++) {
        calc_checksum += data[i];
    }

    if (rx_checksum != calc_checksum) {
        ESP_LOGW(TAG, "Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
        ESP_LOGV(TAG, "  Data: %s", hexencode(data, at + 1).c_str());
        return false;
    }

    const uint8_t *message_data = data + 0;
    ESP_LOGV(TAG, "Received message: [%s]", hexencode(data, at + 1).c_str());
    this->handle_message_(message_data, at + 1);

    /* return false to reset the RX buffer */
    return false;
}

void FloureonThermostat::handle_message_(const uint8_t *buffer, size_t len) {
    uint8_t cmd = buffer[2];
    bool update = false;

    switch ((FloureonCommand)cmd)
    {
        case FloureonCommand::QUERY:
            ESP_LOGVV(TAG, "Command: QUERY");
            break;
        case FloureonCommand::SETTINGS1: {
            /* store SETTINGS1 data */
            settings1_wifi_state_ = (FloureonWifiState)buffer[3];
            settings1_temperature_internal_ = ((uint16_t)buffer[4] << 8) | buffer[5];
            settings1_temperature_external_ = ((uint16_t)buffer[6] << 8) | buffer[7];
            settings1_temperature_set_ = buffer[8];
            settings1_thermostat_locked_ = (FloureonBoolean)buffer[12];
            settings1_thermostat_manual_ = (FloureonBoolean)buffer[13];
            settings1_thermostat_power_ = (bool)buffer[14];

            /* update the climate current temperature */
            if (settings2_sensor_mode_ == FloureonSensorMode::EXTERNAL_SENSOR) {
                /* check the external sensor value */
                if ((settings1_temperature_external_ / 10.0f) != this->current_temperature) {
                    this->current_temperature = settings1_temperature_external_ / 10.0f;
                    update = true;
                }
            } else {
                /* check the internal sensor value */
                if ((settings1_temperature_internal_ / 10.0f) != this->current_temperature) {
                    this->current_temperature = settings1_temperature_internal_ / 10.0f;
                    update = true;
                }
            }

            /* update the climate target temperature */
            if ((settings1_temperature_set_ / 2.0f) != this->target_temperature) {
                this->target_temperature = settings1_temperature_set_ / 2.0f;
                update = true;
            }

            /* update the climate mode */
            if (settings1_thermostat_power_ == false && this->mode != climate::ClimateMode::CLIMATE_MODE_OFF)
            {
                this->mode = climate::ClimateMode::CLIMATE_MODE_OFF;
                update = true;
            } else if (settings1_thermostat_power_ == true && this->mode != climate::ClimateMode::CLIMATE_MODE_HEAT) {
                this->mode = climate::ClimateMode::CLIMATE_MODE_HEAT;
                update = true;
            }

            /* update the internal sensor value (if exists) */
            if (this->internal_temp_id_.has_value()) {
                auto int_temp_id = *this->internal_temp_id_;
                int_temp_id->publish_state(settings1_temperature_internal_ / 10.0f);
            }

            /* update the external sensor value (if exists) */
            if (this->external_temp_id_.has_value()) {
                auto ext_temp_id = *this->external_temp_id_;
                ext_temp_id->publish_state(settings1_temperature_external_ / 10.0f);
            }

            ESP_LOGV(TAG, "Command: SETTINGS1, WiFi State: %#04x, T_int: %1.1f, T_ext: %1.1f, T_set: %1.1f, Lock: %s, Manual: %s, Power: %s",
                (uint8_t)settings1_wifi_state_,
                settings1_temperature_internal_ / 10.0f,
                settings1_temperature_external_ / 10.0f,
                settings1_temperature_set_ / 2.0f,
                (settings1_thermostat_locked_ == FloureonBoolean::DISABLED ? "OFF" : "ON"),
                (settings1_thermostat_manual_ == FloureonBoolean::DISABLED ? "OFF" : "ON"),
                (settings1_thermostat_power_ == false ? "OFF" : "ON"));

            break;
        }
        case FloureonCommand::SETTINGS2: {
            /* store SETTINGS2 data */
            settings2_backlight_ = (FloureonBoolean)buffer[3];
            settings2_restore_ = (FloureonBoolean)buffer[4];
            settings2_antifreeze_ = (FloureonBoolean)buffer[5];
            settings2_temperature_correction_ = buffer[6];
            settings2_hysteresis_internal_ = buffer[7];
            settings2_hysteresis_external_ = buffer[8];
            settings2_sensor_mode_ = (FloureonSensorMode)buffer[10];
            settings2_external_limit_ = buffer[11];

            ESP_LOGV(TAG, "Command: SETTINGS2, Backlight: %s, State restore: %s, Antifreeze: %s, T_corr: %1.1f, int Hyst: %1.1f, ext Hyst: %1.1f, Sensor mode: %s, ext limit: %d",
                (settings2_backlight_ == FloureonBoolean::DISABLED ? "OFF" : "ON"),
                (settings2_restore_ == FloureonBoolean::DISABLED ? "OFF" : "ON"),
                (settings2_antifreeze_ == FloureonBoolean::DISABLED ? "OFF" : "ON"),
                (int8_t)settings2_temperature_correction_ / 10.0f,
                settings2_hysteresis_internal_ / 10.0f,
                settings2_hysteresis_external_ / 10.0f,
                (settings2_sensor_mode_ == FloureonSensorMode::INTERNAL_SENSOR ? "internal" : (settings2_sensor_mode_ == FloureonSensorMode::EXTERNAL_SENSOR ? "external" : "both")),
                settings2_external_limit_);
            break;
        }
        case FloureonCommand::SCHEDULE_DAY1:
        case FloureonCommand::SCHEDULE_DAY2:
        case FloureonCommand::SCHEDULE_DAY3:
        case FloureonCommand::SCHEDULE_DAY4:
        case FloureonCommand::SCHEDULE_DAY5:
        case FloureonCommand::SCHEDULE_DAY6:
        case FloureonCommand::SCHEDULE_DAY7: {
            ESP_LOGVV(TAG, "Command: SCHEDULE_DAY%d", (cmd - 0xC2));
            for (uint8_t i = 0; i < 6; i++)
            {
                ESP_LOGVV(TAG, "P%d: %02d:%02d - %1.1f",
                    (i + 1),
                    (buffer[3 + i * 2] / 10),
                    (buffer[3 + i * 2] % 10 * 10),
                    (buffer[4 + i * 2] / 2.0f));
            }
            break;
        }

    }

    /* update the climate state if required */
    if (update) {
        this->publish_state();
    }
}

climate::ClimateTraits FloureonThermostat::traits() {
    auto traits = climate::ClimateTraits();

    /* the thermostat supports the current temperature trait */
    traits.set_supports_current_temperature(true);

    /* the thermostat supports the heat mode trait */
    traits.set_supports_heat_mode(true);

    /* the thermostat supports the action trait */
    traits.set_supports_action(true);

    /* the thermostat also defines the supported minimum and maximum temperatures
     * and the temperature step */
    traits.set_visual_min_temperature(5.0f);
    traits.set_visual_max_temperature(35.0f);
    traits.set_visual_temperature_step(0.5f);

    /* all other traits are not supported, set to false by ESPHome */ 
    return traits;
}

void FloureonThermostat::control(const climate::ClimateCall &call) {
    /* check if the climate mode is available */
    if (call.get_mode().has_value()) {
        this->mode = *call.get_mode();
        switch (*call.get_mode()) {
            case climate::CLIMATE_MODE_HEAT:
                ESP_LOGV(TAG, "Turning on the thermostat");
                this->settings1_thermostat_power_ = true;
                this->settings_changed_ = true;

                /* set the thermostat action depending on the relay state */
                if (this->relay_state_) {
                    this->action = climate::ClimateAction::CLIMATE_ACTION_HEATING;
                } else {
                    this->action = climate::ClimateAction::CLIMATE_ACTION_IDLE;
                }

                break;
            case climate::CLIMATE_MODE_OFF:
                ESP_LOGV(TAG, "Turning off the thermostat");
                this->settings1_thermostat_power_ = false;
                this->settings_changed_ = true;
                this->action = climate::ClimateAction::CLIMATE_ACTION_OFF;
                break;
            default:
                ESP_LOGD(TAG, "Unsupported thermostat mode: %d", (uint8_t)(*call.get_mode()));
                /* other modes are not supported */
                break;
        }
    }

    /* check if the desired temperature is available */
    if (call.get_target_temperature().has_value()) {
        this->target_temperature = *call.get_target_temperature();
        ESP_LOGV(TAG, "Target temperature set to %f", *call.get_target_temperature());
        
        /* set the target temperature in SETTINGS1 */
        this->settings1_temperature_set_ = (uint16_t)(this->target_temperature * 2);
        this->settings_changed_ = true;
    }

    this->publish_state();
}

}  // namespace esphome
