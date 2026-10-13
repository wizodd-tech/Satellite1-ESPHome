#include "esphome/components/tas2780/tas2780.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error( \
  std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

namespace {
class Amp : public esphome::tas2780::TAS2780 {
 public:
  Amp() {
    esphome::host_now = 0;
    supplies(5, 5);
    setup();
    set_vol_range_min(0);
    set_vol_range_max(1);
    CHECK(set_volume(0.8f));
    CHECK(!failed);
    events.clear();
  }
  using TAS2780::finish_activation_;
  using TAS2780::protection_shutdown_;
  bool locked() const { return protection_lockout_; }
  bool blocked() const { return protection_blocked_; }
  bool dvc_valid() const { return applied_dvc_valid_; }
  unsigned dvc() const { return applied_dvc_; }
  unsigned trips() const { return protection_trips_; }
  unsigned power_mode() const { return power_mode_; }
  bool sample_supply() { SupplyVoltages sample{}; return read_supply_voltages_(&sample); }
  using TAS2780::write_mute_;
  void supplies(float vbat, float pvdd) {
    for (auto sample : {std::pair<uint8_t, uint16_t>{0x52, static_cast<uint16_t>(vbat * 128)},
                        std::pair<uint8_t, uint16_t>{0x54, static_cast<uint16_t>(pvdd * 64)}}) {
      registers[0][sample.first] = sample.second >> 4;
      registers[0][sample.first + 1] = (sample.second & 15) << 4;
    }
  }
  void start() {
    activate();
    CHECK(is_activation_pending());
    CHECK(registers[0][2] == 0x81);
    advance(100);
    CHECK(is_active());
    CHECK(!is_activation_pending());
    CHECK(registers[0][2] == 0x80);
  }
  void observe(uint8_t latch = 2, uint8_t mode = 2) {
    registers[0][2] = 0x80 | mode;
    registers[0][0x49] = latch;
    advance(1001);
    loop();
    CHECK(!is_active());
    CHECK(!is_activation_pending());
    CHECK(!loop_enabled);
  }
  void retry() {
    advance(2000);
    CHECK(is_activation_pending());
    advance(100);
    CHECK(is_active());
  }
  void lock() {
    start();
    observe(); retry();
    observe(); retry();
    observe();
    CHECK(locked());
    CHECK(trips() == 3);
    CHECK(!blocked());
  }
};

void cooldown_and_evidence() {
  Amp a;
  a.start();
  a.events.clear();
  a.observe();
  CHECK(a.trips() == 1);
  const uint8_t latches[] = {0x49, 0x4A, 0x4B, 0x4F, 0x50, 0x51};
  size_t cursor = 0;
  for (uint8_t latch : latches) {
    while (cursor < a.events.size() && (a.events[cursor].write || a.events[cursor].reg != latch)) ++cursor;
    CHECK(cursor < a.events.size());
    CHECK(a.events[cursor].ok);
    ++cursor;
  }
  CHECK(std::none_of(a.events.begin(), a.events.begin() + cursor, [](const auto &e) {
    return e.write && ((e.reg == 1 && e.value == 1) || (e.reg == 0x5C && (e.value & 4)));
  }));
  a.protection_shutdown_(true);  // Re-observing the stopped device is not another trip.
  CHECK(a.trips() == 1);
  a.events.clear();
  a.activate(); a.reset(); a.init(); a.activate();
  CHECK(a.events.empty());
  a.advance(1999);
  a.activate(); a.reset();
  CHECK(!a.is_activation_pending());
  CHECK(a.events.empty());
  a.advance(1);
  CHECK(a.is_activation_pending());
  a.advance(100);
  CHECK(a.is_active());
}

void third_trip_and_deactivate() {
  Amp a;
  a.lock();
  CHECK(a.timers.empty());
  a.deactivate();
  CHECK(a.locked());
  CHECK(a.trips() == 3);
  CHECK(a.timers.empty());
  a.events.clear();
  a.activate(); a.reset(); a.init(); a.finish_activation_();
  CHECK(std::none_of(a.events.begin(), a.events.end(), [](const auto &e) {
    return e.write && e.page == 0 && ((e.reg == 2 && e.value != 0x82) || e.reg == 1 || e.reg == 0x5C);
  }));
  CHECK(!a.is_active());
  a.deactivate();
  CHECK(a.set_volume(0.7f));
  CHECK(!a.locked());
  CHECK(a.timers.empty());
  CHECK(!a.is_activation_pending());
  a.activate();
  CHECK(!a.is_activation_pending());
  a.advance(2000);
  CHECK(a.is_activation_pending());
}

void deactivate_cancels_retry() {
  Amp a;
  a.start(); a.observe();
  auto stale = a.timers.at("protection_recovery").callback;
  a.advance(500);
  a.deactivate();
  CHECK(a.timers.empty());
  a.advance(1499);
  stale();
  CHECK(!a.is_activation_pending());
  CHECK(a.trips() == 1);
  a.activate();
  CHECK(!a.is_activation_pending());
  a.advance(1);
  CHECK(a.is_activation_pending());
}

void deactivate_observes_unpolled_shutdown() {
  for (int cause : {2, 1, 0, 3, 7, -1, -2, -0x49, -0x4A, -0x4B, -0x4F, -0x50, -0x51}) {
    Amp a;
    a.start();
    a.advance(1001);
    a.loop();  // Prime the periodic poll before introducing a new hardware shutdown.
    a.registers[0][2] = cause == 7 ? 0x87 : 0x82;
    a.registers[0][0x49] = cause >= 0 && cause != 7 ? static_cast<uint8_t>(cause) : 2;
    if (cause < 0) a.fail(cause == -1, cause == -1 ? 0 : static_cast<uint8_t>(-cause));
    a.events.clear();
    a.deactivate();  // No loop call or elapsed poll interval after the hardware fault.
    CHECK(!a.is_active());
    CHECK(!a.is_activation_pending());
    CHECK(!a.loop_enabled);
    CHECK(a.timers.empty());
    CHECK(a.blocked() == (cause != 2));
    CHECK(a.trips() == (cause == 2 ? 1u : 0u));
    CHECK(a.registers[0][2] == 0x82);
    CHECK(std::none_of(a.events.begin(), a.events.end(), [](const auto &e) {
      return e.write && e.page == 0 &&
             (e.reg == 1 || (e.reg == 0x5C && (e.value & 4)) || (e.reg == 2 && (e.value & 7) < 2));
    }));
    size_t cursor = 0;
    for (uint8_t latch : {0x49, 0x4A, 0x4B, 0x4F, 0x50, 0x51}) {
      while (cursor < a.events.size() && (a.events[cursor].write || a.events[cursor].reg != latch)) ++cursor;
      CHECK(cursor < a.events.size());
      CHECK(a.events[cursor].page == 0);
      ++cursor;
    }
    a.events.clear();
    a.advance(1999);
    CHECK(a.events.empty());
    CHECK(a.timers.empty());
    CHECK(!a.is_activation_pending());
    a.activate();
    CHECK(!a.is_activation_pending());
    CHECK(a.events.empty());
    if (cause == 2) {
      CHECK(a.timers.count("protection_recovery") == 1);
      a.advance(1);  // Only the new explicit activation request schedules the remaining cooldown.
      CHECK(a.is_activation_pending());
      a.advance(100);
      CHECK(a.is_active());
      CHECK(a.trips() == 1);
    } else {
      CHECK(a.timers.empty());
      CHECK(a.set_volume(0.1f));
      a.events.clear();
      a.activate(); a.reset(); a.init(); a.advance(5000);
      CHECK(a.events.empty());
      CHECK(a.timers.empty());
      CHECK(a.blocked());
      CHECK(!a.is_active());
    }
  }
}

void rolling_window_and_wrap() {
  for (uint32_t origin : {uint32_t{0}, UINT32_MAX - 1500}) {
    Amp a;
    esphome::host_now = origin;
    a.start(); a.observe(); a.retry();
    a.advance(55000);
    a.observe(); a.retry();
    a.observe();
    CHECK(a.trips() == 2);  // First trip expired, rather than resetting a fixed window.
    CHECK(!a.locked());
    a.retry(); a.observe();
    CHECK(a.trips() == 3);
    CHECK(a.locked());
  }
  for (uint32_t gap : {uint32_t{60000}, uint32_t{60001}}) {
    Amp a;
    a.start(); a.observe();
    const uint32_t first = esphome::millis();
    a.retry();
    a.advance(gap - (esphome::millis() - first));
    a.registers[0][0x49] = 2;
    a.protection_shutdown_(true);
    CHECK(a.trips() == (gap == 60000 ? 2u : 1u));
  }
}

void lower_dvc_required() {
  Amp a;
  a.lock();
  CHECK(a.set_volume(0.7999f));
  CHECK(a.registers[0][0x1A] == 40);
  CHECK(a.locked());
  a.fail(true, 0);
  CHECK(!a.set_volume(0.7f));
  CHECK(a.registers[0][0x1A] == 40);
  CHECK(a.locked());
  a.fail(true, 0x1A);
  CHECK(!a.set_volume(0.7f));
  CHECK(a.locked());
  a.activate(); a.reset();
  CHECK(!a.is_activation_pending());
  CHECK(a.set_mute_on());
  CHECK(a.registers[0][0x1A] == 0xC9);
  CHECK(a.locked());
  CHECK(a.set_volume(0.6f));
  CHECK(a.registers[0][0x1A] == 0xC9);
  CHECK(a.locked());
  a.events.clear();
  a.fail(true, 0x1A);
  CHECK(!a.set_mute_off());
  CHECK(a.locked());
  CHECK(!a.is_activation_pending());
  CHECK(a.set_mute_off());
  CHECK(a.registers[0][0x1A] == 80);
  CHECK(!a.locked());
  CHECK(a.trips() == 0);
  CHECK(!a.is_activation_pending());
  a.advance(1999);
  CHECK(!a.is_activation_pending());
  a.advance(1);
  CHECK(a.is_activation_pending());
  auto lower = std::find_if(a.events.begin(), a.events.end(), [](const auto &e) {
    return e.write && e.page == 0 && e.reg == 0x1A && e.value == 80 && e.ok;
  });
  auto restart = std::find_if(a.events.begin(), a.events.end(), [](const auto &e) {
    return e.write && e.page == 0 && e.reg == 2 && e.value == 0x81;
  });
  CHECK(lower < restart);
  CHECK(restart != a.events.end());
  CHECK(std::none_of(a.events.begin(), lower, [](const auto &e) {
    return e.write && e.page == 0 && e.reg == 2 && (e.value & 7) < 2;
  }));
  a.advance(100);
  CHECK(a.is_active());
}

void delayed_completion_gates() {
  Amp a;
  a.activate();
  auto stale = a.timers.at("finish_activation").callback;
  a.deactivate();
  a.events.clear();
  stale();
  CHECK(a.events.empty());
  CHECK(!a.is_active());
  a.lock();
  CHECK(a.set_volume(0.7f));
  CHECK(a.set_volume(0.8f));
  a.advance(2000);
  CHECK(a.locked());
  CHECK(!a.is_activation_pending());
  CHECK(a.set_volume(0.7f));
  CHECK(a.is_activation_pending());
  CHECK(a.set_volume(0.8f));
  a.events.clear();
  a.advance(100);
  CHECK(a.locked());
  CHECK(!a.is_active());
  CHECK(!a.is_activation_pending());
  CHECK(a.timers.empty());
  CHECK(a.registers[0][2] == 0x82);
  CHECK(std::none_of(a.events.begin(), a.events.end(), [](const auto &e) {
    return e.write && e.page == 0 && e.reg == 2 && e.value == 0x80;
  }));

  Amp b;
  b.activate();
  auto fault_stale = b.timers.at("finish_activation").callback;
  b.advance(100);
  b.observe(1);  // Thermal blocking is runtime-only, not bootstrap classification.
  CHECK(b.blocked());
  b.events.clear();
  fault_stale();
  CHECK(b.events.empty());
  CHECK(!b.is_active());

  Amp c;
  c.lock();
  CHECK(c.set_volume(0.7f));
  c.advance(2000);
  CHECK(c.is_activation_pending());
  c.fail(true, 0x1A);
  CHECK(!c.set_volume(0.6f));
  c.advance(100);
  CHECK(c.locked());
  CHECK(!c.is_active());
  CHECK(!c.is_activation_pending());
  CHECK(c.registers[0][2] == 0x82);
}

void activation_write_failures() {
  for (uint8_t reg : {0, 0x5C, 0x1A, 2}) {
    Amp a;
    // Avoid init()'s intentionally unchecked legacy writes consuming the injection.
    a.start(); a.deactivate();
    a.fail(true, reg);
    a.activate();
    CHECK(a.blocked());
    CHECK(!a.is_activation_pending());
    CHECK(!a.is_active());
    CHECK(a.timers.empty());
  }
  Amp b;
  b.activate();
  b.fail(true, 2);
  b.advance(100);
  CHECK(b.blocked());
  CHECK(!b.is_active());
  Amp c;
  c.start(); c.observe();
  c.fail(true, 2);
  c.deactivate();
  CHECK(c.blocked());
  CHECK(c.timers.empty());
}

void unsafe_faults_block() {
  for (uint8_t latch : {0, 1, 3, 6, 0x82}) {
    Amp a;
    a.start(); a.observe(latch);
    CHECK(a.blocked());
    a.deactivate();
    CHECK(a.blocked());
    CHECK(a.set_volume(0.1f));
    a.events.clear();
    a.activate(); a.reset(); a.init(); a.advance(5000); a.finish_activation_();
    CHECK(a.events.empty());
    CHECK(!a.is_active());
  }
  for (uint8_t latch_reg : {0x4A, 0x4B, 0x4F, 0x50, 0x51}) {
    Amp a;
    a.start();
    a.registers[0][latch_reg] = 1;
    a.observe();
    CHECK(a.blocked());
  }
  for (uint8_t mode : {3, 4, 7}) {
    Amp a;
    a.start(); a.observe(2, mode);
    CHECK(a.blocked());
  }
  for (uint8_t reg : {2, 0x49, 0x4A, 0x4B, 0x4F, 0x50, 0x51}) {
    Amp a;
    a.start();
    a.fail(false, reg);
    a.observe();
    CHECK(a.blocked());
    CHECK(a.timers.empty());
  }
  for (unsigned skip : {0u, 1u, 2u}) {
    Amp a;
    a.start();
    a.fail(true, 0, skip);  // Mode observation, latch page selection, shutdown page selection.
    a.observe();
    CHECK(a.blocked());
  }
  Amp a;
  a.start();
  a.fail(true, 2);
  a.observe();
  CHECK(a.blocked());
  Amp b;
  b.start(); b.observe(); b.retry(); b.observe(); b.retry();
  b.fail(true, 0x1A);
  CHECK(!b.set_volume(0.8f));
  b.observe();
  CHECK(b.blocked());  // Third trip has no confirmed previous hardware DVC.
}

void startup_failures_preserve_fallback() {
  // -1 models hardware software shutdown; 0 is page-write failure, others read failures.
  for (int failure : {-1, 0, 2, 0x52, 0x53, 0x54, 0x55}) {
    Amp a;
    a.activate();
    CHECK(a.power_mode() == 0);
    if (failure == -1) {
      a.registers[0][2] = 0x82;
      a.registers[0][0x49] = 2;
    } else {
      a.fail(failure == 0, static_cast<uint8_t>(failure));
    }
    a.events.clear();
    CHECK(!a.sample_supply());
    CHECK(a.is_activation_pending());
    CHECK(!a.blocked());
    CHECK(a.trips() == 0);
    CHECK(a.timers.count("finish_activation") == 1);
    CHECK(a.timers.count("protection_recovery") == 0);
    CHECK(std::none_of(a.events.begin(), a.events.end(), [](const auto &e) {
      return e.write && e.reg == 2;
    }));
    if (failure != -1) a.fail(failure == 0, static_cast<uint8_t>(failure));
    a.advance(100);
    CHECK(a.power_mode() == 2);
    CHECK(a.is_activation_pending());
    CHECK(!a.is_active());
    CHECK(!a.blocked());
    CHECK(a.trips() == 0);
    CHECK(a.timers.count("protection_recovery") == 0);
    if (failure == -1) a.registers[0][2] = 0x82;
    else a.fail(failure == 0, static_cast<uint8_t>(failure));
    a.advance(100);
    CHECK(!a.is_activation_pending());
    CHECK(!a.is_active());
    CHECK(!a.loop_enabled);
    CHECK(!a.blocked());
    CHECK(!a.locked());
    CHECK(a.trips() == 0);
    CHECK(a.timers.empty());
    CHECK(a.registers[0][2] == 0x82);
    a.advance(5000);
    CHECK(!a.is_active());
    a.supplies(4.8f, 12);
    a.start();  // Startup failure did not install a permanent runtime block.
    CHECK(a.power_mode() == 2);
  }
  Amp a;
  a.activate();
  a.registers[0][2] = 0x82;
  a.advance(100);
  a.supplies(4.8f, 12);
  a.advance(100);
  CHECK(a.is_active());
  CHECK(a.power_mode() == 2);
  CHECK(a.trips() == 0);
  CHECK(!a.blocked());
  a.observe();
  CHECK(a.trips() == 1);
  a.advance(2000);
  CHECK(a.is_activation_pending());
  a.registers[0][2] = 0x82;
  a.registers[0][0x49] = 2;
  a.advance(100);
  CHECK(a.power_mode() == 2);
  CHECK(a.is_activation_pending());
  a.registers[0][2] = 0x82;
  a.advance(100);
  CHECK(!a.is_active());
  CHECK(!a.is_activation_pending());
  CHECK(a.trips() == 1);  // Failed recovery bootstrap is not a second runtime trip.
  CHECK(!a.blocked());
  CHECK(a.timers.empty());
}

void pending_loop_is_suppressed() {
  Amp a;
  esphome::host_now = 5000;
  a.activate();
  a.registers[0][2] = 0x82;
  a.registers[0][0x49] = 2;
  a.fail(false, 2);
  a.events.clear();
  a.loop();
  CHECK(a.events.empty());
  CHECK(a.failures.size() == 1);
  CHECK(a.is_activation_pending());
  CHECK(!a.blocked());
  CHECK(a.trips() == 0);
  a.advance(100);  // Pending MODE_CTRL read failure follows bootstrap fallback.
  CHECK(a.power_mode() == 2);
  CHECK(a.is_activation_pending());
  a.events.clear();
  a.loop();
  CHECK(a.events.empty());
  a.advance(100);
  CHECK(a.is_active());
  a.registers[0][2] = 0x82;
  a.registers[0][0x49] = 2;
  a.loop();
  CHECK(a.trips() == 1);
  CHECK(!a.is_active());
  a.events.clear();
  a.loop();
  CHECK(a.events.empty());
  CHECK(a.trips() == 1);
}

void runtime_entrypoints_preserve_evidence() {
  using Action = void (esphome::tas2780::TAS2780::*)();
  for (Action action : {&Amp::reset, &Amp::init, &Amp::activate}) {
    // OC permits retry; thermal, unknown, mixed flags and failed observations do not.
    for (int cause : {2, 1, 0, 3, -1, -2, -0x49, -0x4A, -0x4B, -0x4F, -0x50, -0x51, 7}) {
      Amp a;
      a.start();
      a.advance(1001);
      a.loop();  // Poll is now fresh; no new poll interval elapses before the public call.
      a.registers[0][2] = cause == 7 ? 0x87 : 0x82;
      a.registers[0][0x49] = cause >= 0 && cause != 7 ? static_cast<uint8_t>(cause) : 2;
      if (cause < 0) a.fail(cause == -1, cause == -1 ? 0 : static_cast<uint8_t>(-cause));
      a.events.clear();
      (a.*action)();
      CHECK(!a.is_active());
      CHECK(!a.is_activation_pending());
      CHECK(!a.loop_enabled);
      CHECK(a.blocked() == (cause != 2));
      CHECK(a.trips() == (cause == 2 ? 1u : 0u));
      CHECK(a.timers.count("protection_recovery") == (cause == 2 ? 1u : 0u));
      CHECK(std::none_of(a.events.begin(), a.events.end(), [](const auto &e) {
        return e.write && e.page == 0 && (e.reg == 1 || (e.reg == 0x5C && (e.value & 4)));
      }));
      size_t cursor = 0;
      for (uint8_t latch : {0x49, 0x4A, 0x4B, 0x4F, 0x50, 0x51}) {
        while (cursor < a.events.size() && (a.events[cursor].write || a.events[cursor].reg != latch)) ++cursor;
        CHECK(cursor < a.events.size());
        CHECK(a.events[cursor].page == 0);
        ++cursor;
      }
      CHECK(a.registers[0][0x49] == (cause >= 0 && cause != 7 ? cause : 2));
      if (cause == 2) a.retry();
      else {
        CHECK(a.set_volume(0.1f));
        a.events.clear();
        a.init(); a.reset(); a.activate(); a.advance(5000);
        CHECK(a.events.empty());
        CHECK(a.blocked());
      }
    }
  }
  for (int cause : {2, 1, 0, -1, -2}) {
    Amp a;
    a.start();
    a.registers[0][2] = 0x82;
    a.registers[0][0x49] = cause >= 0 ? static_cast<uint8_t>(cause) : 2;
    if (cause < 0) a.fail(cause == -1, cause == -1 ? 0 : 2);
    CHECK(!a.sample_supply());
    CHECK(!a.is_active());
    CHECK(a.blocked() == (cause != 2));
    CHECK(a.trips() == (cause == 2 ? 1u : 0u));
  }
}

void init_invalidates_dvc_and_restore_reconfirms() {
  Amp a;
  a.start();
  CHECK(a.dvc_valid());
  CHECK(a.dvc() == 40);
  a.init();
  CHECK(!a.is_active());
  CHECK(!a.dvc_valid());
  CHECK(a.registers[0][0x1A] == 0);
  CHECK(a.set_mute_on());
  CHECK(!a.dvc_valid());
  CHECK(a.write_mute_());
  CHECK(!a.dvc_valid());
  CHECK(a.set_volume(0.7f));
  CHECK(!a.dvc_valid());
  CHECK(a.set_mute_off());
  CHECK(a.dvc_valid());
  CHECK(a.dvc() == 60);
  a.init();
  CHECK(!a.dvc_valid());
  a.fail(true, 0x1A);
  CHECK(!a.write_mute_());
  CHECK(!a.dvc_valid());
  CHECK(a.write_mute_());
  CHECK(a.dvc_valid());
  CHECK(a.dvc() == 60);
  a.reset();
  CHECK(a.is_activation_pending());
  CHECK(a.dvc_valid());
  CHECK(a.dvc() == 60);
  a.advance(100);
  CHECK(a.is_active());

  a.events.clear();
  a.reset();  // A healthy established-active reset must restore the invalidated baseline.
  CHECK(a.is_activation_pending());
  CHECK(a.dvc_valid());
  CHECK(a.dvc() == 60);
  auto reset = std::find_if(a.events.begin(), a.events.end(), [](const auto &e) {
    return e.write && e.page == 0 && e.reg == 1 && e.value == 1;
  });
  auto restored = std::find_if(reset, a.events.end(), [](const auto &e) {
    return e.write && e.page == 0 && e.reg == 0x1A && e.value == 60 && e.ok;
  });
  CHECK(reset != a.events.end());
  CHECK(restored != a.events.end());
  CHECK(std::any_of(a.events.begin(), reset, [](const auto &e) {
    return !e.write && e.page == 0 && e.reg == 2 && e.ok;
  }));
  a.advance(100);
  CHECK(a.is_active());

  Amp b;
  b.start(); b.observe(); b.retry(); b.observe(); b.retry();
  CHECK(b.dvc_valid());
  CHECK(b.set_mute_on());
  b.init();
  CHECK(!b.dvc_valid());
  b.start();
  CHECK(!b.dvc_valid());  // Muted activation must not reuse the pre-init unmuted baseline.
  b.observe();
  CHECK(b.trips() == 3);
  CHECK(b.locked());
  CHECK(b.blocked());
  CHECK(b.set_mute_off());
  CHECK(b.dvc_valid());
  CHECK(b.blocked());
  CHECK(!b.is_active());
}

void volume_and_mute() {
  Amp a;
  for (const auto &sample : {std::pair<float, uint8_t>{1, 0}, {0.995f, 1}, {0.5f, 100},
                            {0, 200}, {-1, 200}, {2, 0}}) {
    CHECK(a.set_volume(sample.first));
    CHECK(a.registers[0][0x1A] == sample.second);
  }
  CHECK(a.set_volume(0.5f));
  CHECK(a.set_mute_on());
  CHECK(a.is_muted());
  CHECK(a.registers[0][0x1A] == 201);
  CHECK(a.set_volume(0.25f));
  CHECK(a.registers[0][0x1A] == 201);
  CHECK(a.set_mute_off());
  CHECK(!a.is_muted());
  CHECK(a.registers[0][0x1A] == 150);
  a.set_vol_range_min(0.3f);
  CHECK(a.set_volume(0));
  CHECK(a.registers[0][0x1A] == 140);
  a.page = 1;
  CHECK(a.set_volume(1));
  CHECK(a.page == 0);
  CHECK(a.registers[0][0x1A] == 0);
}

void normal_bootstrap_and_fallback() {
  for (float pvdd : {5.f, 12.f}) {
    Amp a;
    a.supplies(5, pvdd);
    a.activate();
    CHECK(a.power_mode() == 0);
    CHECK(!a.is_active());
    CHECK(a.registers[0][0x71] == 3);
    a.advance(99);
    CHECK(!a.is_active());
    a.advance(1);
    CHECK(a.is_active());
    CHECK(a.power_mode() == (pvdd == 5 ? 0u : 2u));
    CHECK(a.registers[0][0x71] == (pvdd == 5 ? 3 : 0x11));
  }
  Amp a;
  a.supplies(0, 0);
  a.activate(); a.advance(100);
  CHECK(a.power_mode() == 2);
  CHECK(a.is_activation_pending());
  CHECK(!a.is_active());
  a.supplies(4.8f, 12);
  a.advance(100);
  CHECK(a.is_active());
  CHECK(a.power_mode() == 2);
  Amp b;
  b.supplies(0, 0);
  b.activate(); b.advance(100); b.advance(100);
  CHECK(!b.is_active());
  CHECK(!b.is_activation_pending());
  CHECK(b.registers[0][2] == 0x82);
  Amp c;
  bool ready = false;
  c.set_activation_guard([&ready]() { return ready; });
  c.activate();
  CHECK(!c.is_activation_pending());
  ready = true;
  c.activate();
  CHECK(c.is_activation_pending());
  ready = false;
  c.advance(100);
  CHECK(!c.is_active());
  CHECK(c.registers[0][2] == 0x82);
}
}

int main() {
  const std::pair<const char *, void (*)()> tests[] = {
    {"cooldown and latch evidence", cooldown_and_evidence},
    {"third trip and persistent deactivation", third_trip_and_deactivate},
    {"deactivate cancels retry", deactivate_cancels_retry},
    {"deactivate observes unpolled shutdown", deactivate_observes_unpolled_shutdown},
    {"rolling window and clock wrap", rolling_window_and_wrap},
    {"confirmed lower DVC before restart", lower_dvc_required},
    {"delayed completion gates", delayed_completion_gates},
    {"activation write failures", activation_write_failures},
    {"startup failures preserve fallback", startup_failures_preserve_fallback},
    {"pending loop is suppressed", pending_loop_is_suppressed},
    {"runtime entrypoints preserve evidence", runtime_entrypoints_preserve_evidence},
    {"init invalidates DVC and restore reconfirms", init_invalidates_dvc_and_restore_reconfirms},
    {"unsafe faults fail closed", unsafe_faults_block},
    {"0.5 dB volume and shared-register mute", volume_and_mute},
    {"normal bootstrap and fallback", normal_bootstrap_and_fallback},
  };
  unsigned failures = 0;
  for (const auto &test : tests) {
    try { test.second(); std::cout << "PASS " << test.first << '\n'; }
    catch (const std::exception &error) {
      ++failures;
      std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}
