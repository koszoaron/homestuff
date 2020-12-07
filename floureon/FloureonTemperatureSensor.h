#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {

class FloureonTemperatureSensor : public Component, public sensor::Sensor {
  public:
    FloureonTemperatureSensor() : Sensor() {
        this->set_accuracy_decimals(1);
        this->set_unit_of_measurement("\302\260C");
        this->set_icon("mdi:thermometer");
    }
};

}  /* namespace esphome */