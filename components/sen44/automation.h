#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "sen44.h"

namespace esphome {
namespace sen44 {

template<typename... Ts> class StartFanAction : public Action<Ts...> {
 public:
  explicit StartFanAction(SEN44Component *sen44) : sen44_(sen44) {}

  void play(Ts... x) override { this->sen44_->start_fan_cleaning(); }

 protected:
  SEN44Component *sen44_;
};

}  // namespace sen44
}  // namespace esphome
