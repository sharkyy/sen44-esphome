#include "sen44.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sen44 {

static const char *const TAG = "sen44";

static const uint16_t SEN44_CMD_AUTO_CLEANING_INTERVAL = 0x8004;
static const uint16_t SEN44_CMD_GET_DATA_READY_STATUS = 0x0202;
static const uint16_t SEN44_CMD_GET_FIRMWARE_VERSION = 0xD100;
static const uint16_t SEN44_CMD_GET_SERIAL_NUMBER = 0xD033;
static const uint16_t SEN44_CMD_START_CLEANING_FAN = 0x5607;
static const uint16_t SEN44_CMD_START_MEASUREMENTS = 0x0021; 
static const uint16_t SEN44_CMD_STOP_MEASUREMENTS = 0x0104;

static const uint16_t SEN44_CMD_GET_ARTICLE_CODE = 0xD025;
static const uint16_t SEN44_CMD_READ_PM_VALUES = 0x0353;
static const uint16_t SEN44_CMD_READ_MESAUREMENT_TICKS = 0x0374;

static const uint16_t SEN44_CMD_GET_SET_TEMPERATURE_OFFSET = 0x6014;
static const uint16_t SEN44_CMD_WRITE_TEMPERATURE_OFFSET = 0x6002;
static const uint16_t SEN44_DEVICE_STATUS_READ = 0xD206;
static const uint16_t SEN44_DEVICE_STATUS_CLEAR = 0xD210;
static const uint16_t SEN44_DEVICE_RESET = 0xD304;


void SEN44Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sen44...");

  this->start_fan_cleaning();
  // the sensor needs 1000 ms to enter the idle state
  this->set_timeout(1000, [this]() {
    // Check if measurement is ready before reading the value
    if (!this->write_command(SEN44_CMD_GET_DATA_READY_STATUS)) {
      ESP_LOGE(TAG, "Failed to write data ready status command");
      this->mark_failed();
      return;
    }

    uint16_t raw_read_status;
    if (!this->read_data(raw_read_status)) {
      ESP_LOGE(TAG, "Failed to read data ready status");
      this->mark_failed();
      return;
    }

    uint32_t stop_measurement_delay = 0;
    // In order to query the device periodic measurement must be ceased
    if (raw_read_status) {
      ESP_LOGD(TAG, "Sensor has data available, stopping periodic measurement");
      if (!this->write_command(SEN44_CMD_STOP_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Failed to stop measurements");
        this->mark_failed();
        return;
      }
      // According to the SEN5x datasheet the sensor will only respond to other commands after waiting 200 ms after
      // issuing the stop_periodic_measurement command
      stop_measurement_delay = 200;
    }
    this->set_timeout(stop_measurement_delay, [this]() {
      uint16_t raw_serial_number[3];
      if (!this->get_register(SEN44_CMD_GET_SERIAL_NUMBER, raw_serial_number, 3, 20)) {
        ESP_LOGE(TAG, "Failed to read serial number");
        this->error_code_ = SERIAL_NUMBER_IDENTIFICATION_FAILED;
        this->mark_failed();
        return;
      }
      this->serial_number_[0] = static_cast<bool>(uint16_t(raw_serial_number[0]) & 0xFF);
      this->serial_number_[1] = static_cast<uint16_t>(raw_serial_number[0] & 0xFF);
      this->serial_number_[2] = static_cast<uint16_t>(raw_serial_number[1] >> 8);
      ESP_LOGD(TAG, "Serial number %02d.%02d.%02d", serial_number_[0], serial_number_[1], serial_number_[2]);

      if (!this->get_register(SEN44_CMD_GET_FIRMWARE_VERSION, this->firmware_version_, 20)) {
        ESP_LOGE(TAG, "Failed to read firmware version");
        this->error_code_ = FIRMWARE_FAILED;
        this->mark_failed();
        return;
      }
      this->firmware_version_ >>= 8;
      ESP_LOGD(TAG, "Firmware version %d", this->firmware_version_);

      if (!this->auto_cleaning_interval_.has_value()) {
        this->auto_cleaning_interval_ = 14400;
      }

      bool result =
          write_command(SEN44_CMD_AUTO_CLEANING_INTERVAL, this->auto_cleaning_interval_.value());

      if (result) {
        delay(20);
        uint16_t secs[2];
        if (this->read_data(secs, 2)) {
          auto_cleaning_interval_ = secs[0] << 16 | secs[1];
        }
      }

      if (this->temperature_compensation_.has_value()) {
        this->write_temperature_compensation_(this->temperature_compensation_.value());
        delay(20);
      }

      get_temperature_offset();

      // Finally start sensor measurements
      if (!this->write_command(SEN44_CMD_START_MEASUREMENTS)) {
        ESP_LOGE(TAG, "Error starting continuous measurements.");
        this->error_code_ = MEASUREMENT_INIT_FAILED;
        this->mark_failed();
        return;
      }
      initialized_ = true;
      ESP_LOGI(TAG, "Sensor initialized");
    });
  });
}

void SEN44Component::dump_config() {
  ESP_LOGCONFIG(TAG, "sen44:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement Initialization failed!");
        break;
      case SERIAL_NUMBER_IDENTIFICATION_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor serial id");
        break;
      case FIRMWARE_FAILED:
        ESP_LOGW(TAG, "Unable to read sensor firmware version");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  ESP_LOGCONFIG(TAG, "  Productname: %s", this->product_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Firmware version: %d", this->firmware_version_);
  ESP_LOGCONFIG(TAG, "  Serial number %02d.%02d.%02d", serial_number_[0], serial_number_[1], serial_number_[2]);
  ESP_LOGCONFIG(TAG, "  Auto cleaning interval %" PRId32 " seconds", get_auto_cleaning_interval());
  ESP_LOGCONFIG(TAG, "  Temperature offset: %f", this->temperature_offset_);

  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "PM  1.0", this->pm_1_0_sensor_);
  LOG_SENSOR("  ", "PM  2.5", this->pm_2_5_sensor_);
  LOG_SENSOR("  ", "PM  4.0", this->pm_4_0_sensor_);
  LOG_SENSOR("  ", "PM 10.0", this->pm_10_0_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "VOC", this->voc_sensor_);  // SEN54 and SEN55 only
}

void SEN44Component::update() {
  ESP_LOGD(TAG, "update called");

  if (!initialized_) {
    return;
  }

  if (!this->write_command(SEN44_CMD_READ_MESAUREMENT_TICKS)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "write error read measurement (%d)", this->last_error_);
    return;
  }
  this->set_timeout(20, [this]() {
    uint16_t measurements[7];

    if (!this->read_data(measurements, 7)) {
      this->status_set_warning();
      ESP_LOGD(TAG, "read data error (%d)", this->last_error_);
      return;
    }
    float pm_1_0 = measurements[0] / 10.0;
    if (measurements[0] == 0xFFFF)
      pm_1_0 = NAN;
    float pm_2_5 = measurements[1] / 10.0;
    if (measurements[1] == 0xFFFF)
      pm_2_5 = NAN;
    float pm_4_0 = measurements[2] / 10.0;
    if (measurements[2] == 0xFFFF)
      pm_4_0 = NAN;
    float pm_10_0 = measurements[3] / 10.0;
    if (measurements[3] == 0xFFFF)
      pm_10_0 = NAN;
    float voc = (int16_t) measurements[4] / 10.0;
    if (measurements[6] == 0xFFFF)
      voc = NAN;
    float humidity = (int16_t) measurements[5] / 100.0;
    if (measurements[4] == 0xFFFF)
      humidity = NAN;
    float temperature = (int16_t) measurements[6] / 200.0;
    if (measurements[5] == 0xFFFF)
      temperature = NAN;


    if (this->pm_1_0_sensor_ != nullptr)
      this->pm_1_0_sensor_->publish_state(pm_1_0);
    if (this->pm_2_5_sensor_ != nullptr)
      this->pm_2_5_sensor_->publish_state(pm_2_5);
    if (this->pm_4_0_sensor_ != nullptr)
      this->pm_4_0_sensor_->publish_state(pm_4_0);
    if (this->pm_10_0_sensor_ != nullptr)
      this->pm_10_0_sensor_->publish_state(pm_10_0);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    if (this->voc_sensor_ != nullptr)
      this->voc_sensor_->publish_state(voc);
    this->status_clear_warning();
  });
}

bool SEN44Component::start_fan_cleaning() {
  if (!write_command(SEN44_CMD_START_CLEANING_FAN)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error start fan (%d)", this->last_error_);
    return false;
  } else {
    ESP_LOGD(TAG, "Fan auto clean started");
  }
  return true;
}

void SEN44Component::get_temperature_offset() {
  if(!write_command(SEN44_CMD_GET_SET_TEMPERATURE_OFFSET)) {
    this->status_set_warning();
    ESP_LOGE(TAG, "write error get temperature offset (%d)", this->last_error_);
    return;
  }
  uint16_t buffer[1];

  if (!this->read_data(buffer, 1)) {
    this->status_set_warning();
    ESP_LOGD(TAG, "read data error (%d)", this->last_error_);
    return;
  }

  this->temperature_offset_ = (int16_t) buffer[0] / 200.0f;
  delay(20);
}

bool SEN44Component::write_temperature_compensation_(const TemperatureCompensation &compensation) {
  uint16_t params[1];
  params[0] = compensation.offset;
  if (!write_command(SEN44_CMD_GET_SET_TEMPERATURE_OFFSET, params, 1)) {
    ESP_LOGE(TAG, "set temperature_compensation failed. Err=%d", this->last_error_);
    return false;
  }
  return true;
}

}  // namespace sen44
}  // namespace esphome
