#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace sen44 {

enum ERRORCODE {
  COMMUNICATION_FAILED,
  SERIAL_NUMBER_IDENTIFICATION_FAILED,
  MEASUREMENT_INIT_FAILED,
  FIRMWARE_FAILED,
  UNKNOWN
};

// Shortest time interval of 3H for storing baseline values.
// Prevents wear of the flash because of too many write operations
const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 10800;
// Store anyway if the baseline difference exceeds the max storage diff value
const uint32_t MAXIMUM_STORAGE_DIFF = 50;

struct Sen5xBaselines {
  int32_t state0;
  int32_t state1;
} PACKED;  // NOLINT

struct TemperatureCompensation {
  int16_t offset;
};

class SEN44Component : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0) { pm_1_0_sensor_ = pm_1_0; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5) { pm_2_5_sensor_ = pm_2_5; }
  void set_pm_4_0_sensor(sensor::Sensor *pm_4_0) { pm_4_0_sensor_ = pm_4_0; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0) { pm_10_0_sensor_ = pm_10_0; }

  void set_voc_sensor(sensor::Sensor *voc_sensor) { voc_sensor_ = voc_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_auto_cleaning_interval(uint32_t auto_cleaning_interval) { auto_cleaning_interval_ = auto_cleaning_interval; }
  void set_temperature_compensation(float offset) {
    TemperatureCompensation temp_comp;
    temp_comp.offset = offset * 200;
    temperature_compensation_ = temp_comp;
  }
  bool start_fan_cleaning();
  void get_temperature_offset();

 protected:
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  ERRORCODE error_code_;
  bool initialized_{false};
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  // SEN54 and SEN55 only
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};

  std::string product_name_;
  uint8_t serial_number_[4];
  uint16_t firmware_version_;
  ESPPreferenceObject pref_;
  optional<uint32_t> auto_cleaning_interval_;
  optional<TemperatureCompensation> temperature_compensation_;
  float temperature_offset_;  
};

}  // namespace sen44
}  // namespace esphome
