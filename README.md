# sen44-esphome
External component for the sen44 sensor to integrate into esphome. This is used in the amazon air quality sensor. 

This is based on a fork of esphomes sen5x. I changed some i2c commands based on what i figured from this library: https://github.com/Sensirion/arduino-i2c-sen44

## Usage
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/axhe/sen44-esphome
      ref: main
    refresh: 0s
    components: [ sen44 ]


i2c:
  sda: GPIO8
  scl: GPIO9
  scan: true
  id: bus_a

sensor:
  - platform: sen44
    id: sen44_sensor
    pm_1_0:
      name: " PM <1µm Weight concentration"
      id: pm_1_0
      accuracy_decimals: 1
    pm_2_5:
      name: " PM <2.5µm Weight concentration"
      id: pm_2_5
      accuracy_decimals: 1
    pm_4_0:
      name: " PM <4µm Weight concentration"
      id: pm_4_0
      accuracy_decimals: 1
    pm_10_0:
      name: " PM <10µm Weight concentration"
      id: pm_10_0
      accuracy_decimals: 1
    temperature:
      name: "Temperature"
      accuracy_decimals: 1
    humidity:
      name: "Humidity"
      accuracy_decimals: 0
    voc:
      name: "VOC"
    address: 0x69
    update_interval: 10s
    temperature_compensation:
      offset: -1.0
  - platform: template
    name: "Air Quality Index"
    id: aqi
    lambda: |-
      if (id(pm_2_5).state > 100) {
        return 0;
      } else {
        return 100 - id(pm_2_5).state;
      }
    unit_of_measurement: "AQI"
    accuracy_decimals: 0
    update_interval: 10s
```