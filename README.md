# sen44-esphome
External component for the sen44 sensor to integrate into esphome. This is used in the amazon air quality sensor. 

This is based on a fork of esphomes sen5x. I changed some i2c commands based on what i figured from this library: https://github.com/Sensirion/arduino-i2c-sen44

## Usage
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/sharkyy/sen44-esphome
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
      id: voc
    address: 0x69
    update_interval: 10s
    temperature_compensation:
      offset: -1.0
  - platform: template
    name: "Luftgüte Score"
    id: aqi
    icon: "mdi:air-filter"
    lambda: |-
      auto pm25_sensor = id(pm_2_5);
      auto pm10_sensor = id(pm_10_0);
      auto voc_sensor = id(voc);
      
      float pm25_epa_aqi = 0.0;
      float pm10_epa_aqi = 0.0;
      float voc_epa_aqi = 0.0;

      // 1. Calculate standard EPA AQI for PM2.5 (lower is better)
      if (pm25_sensor->has_state()) {
        float pm25 = pm25_sensor->state;
        if (pm25 <= 12.0) pm25_epa_aqi = (50.0/12.0)*pm25;
        else if (pm25 <= 35.4) pm25_epa_aqi = (49.0/23.3)*(pm25-12.1)+51.0;
        else if (pm25 <= 55.4) pm25_epa_aqi = (49.0/19.9)*(pm25-35.5)+101.0;
        else if (pm25 <= 150.4) pm25_epa_aqi = (49.0/94.9)*(pm25-55.5)+151.0;
        else if (pm25 <= 250.4) pm25_epa_aqi = (99.0/99.9)*(pm25-150.5)+201.0;
        else if (pm25 <= 350.4) pm25_epa_aqi = (99.0/99.9)*(pm25-250.5)+301.0;
        else pm25_epa_aqi = (99.0/149.9)*(pm25-350.5)+401.0;
      }

      // 2. Calculate standard EPA AQI for PM10 (lower is better)
      if (pm10_sensor->has_state()) {
        float pm10 = pm10_sensor->state;
        if (pm10 <= 54.0) pm10_epa_aqi = (50.0/54.0)*pm10;
        else if (pm10 <= 154.0) pm10_epa_aqi = (49.0/99.0)*(pm10-55.0)+51.0;
        else if (pm10 <= 254.0) pm10_epa_aqi = (49.0/99.0)*(pm10-155.0)+101.0;
        else if (pm10 <= 354.0) pm10_epa_aqi = (49.0/99.0)*(pm10-255.0)+151.0;
        else if (pm10 <= 424.0) pm10_epa_aqi = (99.0/69.0)*(pm10-355.0)+201.0;
        else pm10_epa_aqi = (99.0/79.0)*(pm10-425.0)+301.0;
      }
      
      // 3. Calculate standard EPA AQI for VOC (lower is better)
      if (voc_sensor->has_state()) {
        float voc_index = voc_sensor->state;
        if (voc_index <= 100) {
          voc_epa_aqi = voc_index * 0.5;
        } else if (voc_index <= 250) {
          voc_epa_aqi = (49.0/150.0)*(voc_index - 101.0) + 51.0;
        } else {
          voc_epa_aqi = (49.0/250.0)*(voc_index - 251.0) + 101.0;
        }
      }
      
      // Find the single worst EPA AQI value (highest number)
      float worst_epa_aqi = std::max({pm25_epa_aqi, pm10_epa_aqi, voc_epa_aqi});

      // Convert the worst EPA AQI to the new scale (0=worst, 100=best).
      // We cap the EPA AQI at 300 for the conversion to get a reasonable range.
      float capped_epa_aqi = std::min(300.0f, worst_epa_aqi);
      float quality_score = 100.0 - (capped_epa_aqi / 300.0) * 100.0;

      // Return the final score, ensuring it's not negative.
      return std::max(0.0f, quality_score);
    unit_of_measurement: ""
    accuracy_decimals: 0
    update_interval: 10s
```
