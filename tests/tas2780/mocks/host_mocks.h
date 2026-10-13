#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
inline uint32_t host_now = 0;
inline uint32_t millis() { return host_now; }
template<typename T> T clamp(T value, T low, T high) { return std::clamp(value, low, high); }
template<typename... Args> void host_log(Args &&...) {}

class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  struct Timer { uint32_t due; std::function<void()> callback; };
  std::map<std::string, Timer> timers;
  bool loop_enabled = true;
  bool failed = false;
  void enable_loop() { loop_enabled = true; }
  void disable_loop() { loop_enabled = false; }
  void mark_failed() { failed = true; }
  void set_timeout(const std::string &name, uint32_t delay, std::function<void()> callback) {
    timers[name] = {millis() + delay, std::move(callback)};
  }
  bool cancel_timeout(const std::string &name) { return timers.erase(name) != 0; }
  void advance(uint32_t elapsed) {
    host_now += elapsed;
    for (;;) {
      auto ready = std::find_if(timers.begin(), timers.end(), [](const auto &entry) {
        return static_cast<int32_t>(millis() - entry.second.due) >= 0;
      });
      if (ready == timers.end()) return;
      auto callback = std::move(ready->second.callback);
      timers.erase(ready);
      callback();
    }
  }
};

namespace audio_dac {
class AudioDac {
 public:
  virtual bool set_mute_off() = 0;
  virtual bool set_mute_on() = 0;
  virtual bool set_volume(float) = 0;
  virtual bool is_muted() = 0;
  virtual float volume() = 0;
 protected:
  bool is_muted_{false};
};
}

namespace i2c {
class I2CDevice {
 public:
  struct Event { bool write; uint8_t page; uint8_t reg; uint8_t value; bool ok; };
  struct Failure { bool write; uint8_t page; uint8_t reg; unsigned skip; };
  std::array<std::array<uint8_t, 256>, 256> registers{};
  std::vector<Event> events;
  std::vector<Failure> failures;
  uint8_t page = 0;
  I2CDevice() { registers[0][0x05] = 0x41; }
  void fail(bool write, uint8_t reg, unsigned skip = 0, uint8_t on_page = 0) {
    failures.push_back({write, on_page, reg, skip});
  }
  bool succeeds(bool write, uint8_t reg) {
    for (auto it = failures.begin(); it != failures.end(); ++it) {
      if (it->write != write || it->page != page || it->reg != reg) continue;
      if (it->skip != 0) { --it->skip; return true; }
      failures.erase(it);
      return false;
    }
    return true;
  }
  bool read_byte(uint8_t reg, uint8_t *value) {
    const bool ok = succeeds(false, reg);
    events.push_back({false, page, reg, registers[page][reg], ok});
    if (ok) *value = registers[page][reg];
    return ok;
  }
  bool write_byte(uint8_t reg, uint8_t value) {
    const bool ok = succeeds(true, reg);
    events.push_back({true, page, reg, value, ok});
    if (!ok) return false;
    if (reg == 0) { page = value; return true; }
    if (page == 0 && reg == 1 && value == 1) {
      // SAR values model physical supplies, so survive a software reset.
      const auto old = registers[0];
      registers[0].fill(0);
      for (uint8_t r = 0x52; r <= 0x55; ++r) registers[0][r] = old[r];
      registers[0][0x05] = 0x41;
      registers[0][0x02] = 0x82;
    }
    registers[page][reg] = value;
    if (page == 0 && reg == 0x5C && (value & 4)) {
      for (uint8_t r : {0x49, 0x4A, 0x4B, 0x4F, 0x50, 0x51}) registers[0][r] = 0;
    }
    return true;
  }
  class Register {
   public:
    Register(I2CDevice *device, uint8_t reg) : device_(device), reg_(reg) {}
    uint8_t get() const { uint8_t value = 0; device_->read_byte(reg_, &value); return value; }
    Register &operator=(uint8_t value) { device_->write_byte(reg_, value); return *this; }
   private:
    I2CDevice *device_;
    uint8_t reg_;
  };
  Register reg(uint8_t address) { return {this, address}; }
};
}
}

#define ESP_LOGD(...) ::esphome::host_log(__VA_ARGS__)
#define ESP_LOGW(...) ::esphome::host_log(__VA_ARGS__)
#define ESP_LOGE(...) ::esphome::host_log(__VA_ARGS__)
#define ESP_LOGCONFIG(...) ::esphome::host_log(__VA_ARGS__)
#define YESNO(value) ((value) ? "YES" : "NO")
#define LOG_I2C_DEVICE(value) ::esphome::host_log(value)
