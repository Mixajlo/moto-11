// ===== Unit tests for the engine-run supervisor =====
// Runs on the ESP via `pio test -e bench` (no host compiler needed). Sensors are
// in SIM and relays just track state, so no INA3221/relay-box is required. The
// supervisor's timers are shrunk via its public tunables so the millis()-based
// logic (debounces, off-delay, backstop) resolves in ms instead of real seconds.
#include <Arduino.h>
#include <unity.h>

#include "supervisor.h"
#include "sensors.h"
#include "relays.h"

// Advance ~ms of real time, pumping update() like the main loop does.
static void run_for(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) { Engine.update(); delay(5); }
  Engine.update();
}

void setUp(void) {
  Relays.begin();
  Sensor.begin();
  Sensor.setSim(true);
  Sensor.setSimIgn(false);
  Sensor.setSimVbus(12.4f);
  // Shrink the timers so the real-millis logic runs fast and deterministically.
  Engine.runDebounceMs    = 40;
  Engine.stallDebounceMs  = 40;
  Engine.offDelayMs       = 120;
  Engine.backstopMs       = 200;
  Engine.chargeDebounceMs = 60;
  Engine.begin();           // -> SLEEP
  Engine.update();
}
void tearDown(void) {}

static void toRunning(void) {
  Sensor.setSimIgn(true);
  Sensor.setSimVbus(13.5f);
  run_for(80);              // > runDebounce -> RUNNING
}

void test_sleep_to_armed(void) {
  Sensor.setSimIgn(true); Engine.update();
  TEST_ASSERT_EQUAL(SUP_ARMED, Engine.state());
}
void test_armed_to_running_master_on(void) {
  toRunning();
  TEST_ASSERT_EQUAL(SUP_RUNNING, Engine.state());
  TEST_ASSERT_TRUE(Relays.isOn(RELAY_MASTER));
}
void test_stays_armed_below_vrunon(void) {
  Sensor.setSimIgn(true); Sensor.setSimVbus(12.5f); run_for(120);
  TEST_ASSERT_EQUAL(SUP_ARMED, Engine.state());
  TEST_ASSERT_FALSE(Relays.isOn(RELAY_MASTER));
}
void test_running_to_powered_keeps_master(void) {        // stall/kill, key on
  toRunning(); Sensor.setSimVbus(12.5f); run_for(90);
  TEST_ASSERT_EQUAL(SUP_POWERED, Engine.state());
  TEST_ASSERT_TRUE(Relays.isOn(RELAY_MASTER));            // power held
}
void test_powered_to_running_restart(void) {
  toRunning(); Sensor.setSimVbus(12.5f); run_for(90);     // -> POWERED
  Sensor.setSimVbus(13.5f); run_for(80);                  // restart
  TEST_ASSERT_EQUAL(SUP_RUNNING, Engine.state());
}
void test_powered_backstop_to_armed(void) {              // forgotten key
  toRunning(); Sensor.setSimVbus(12.5f); run_for(90);     // -> POWERED
  run_for(Engine.backstopMs + 80);
  TEST_ASSERT_EQUAL(SUP_ARMED, Engine.state());
  TEST_ASSERT_FALSE(Relays.isOn(RELAY_MASTER));           // battery saved
}
void test_keyoff_offdelay_then_sleep(void) {
  toRunning(); Sensor.setSimIgn(false); Engine.update();
  TEST_ASSERT_EQUAL(SUP_OFF_DELAY, Engine.state());
  TEST_ASSERT_TRUE(Relays.isOn(RELAY_MASTER));            // held during delay
  run_for(Engine.offDelayMs + 80);
  TEST_ASSERT_EQUAL(SUP_SLEEP, Engine.state());
  TEST_ASSERT_FALSE(Relays.isOn(RELAY_MASTER));
}
void test_offdelay_resume(void) {
  toRunning(); Sensor.setSimIgn(false); Engine.update();  // -> OFF_DELAY
  Sensor.setSimIgn(true); Sensor.setSimVbus(13.5f); run_for(60);
  TEST_ASSERT_EQUAL(SUP_RUNNING, Engine.state());
}
void test_charge_transient_ignored(void) {
  toRunning();
  Sensor.setSimVbus(13.3f); run_for(Engine.chargeDebounceMs / 2);  // dip < debounce
  Sensor.setSimVbus(13.7f); run_for(40);                           // recover
  TEST_ASSERT_FALSE(Engine.chargeMarginal());
}
void test_charge_sustained_warns(void) {
  toRunning();
  Sensor.setSimVbus(13.3f); run_for(Engine.chargeDebounceMs + 60);
  TEST_ASSERT_TRUE(Engine.chargeMarginal());
}
void test_charge_hysteresis_deadband(void) {
  toRunning();
  Sensor.setSimVbus(13.3f); run_for(Engine.chargeDebounceMs + 60);
  TEST_ASSERT_TRUE(Engine.chargeMarginal());                       // marginal
  Sensor.setSimVbus(13.5f); run_for(Engine.chargeDebounceMs + 60);
  TEST_ASSERT_TRUE(Engine.chargeMarginal());                       // 13.5 deadband holds
  Sensor.setSimVbus(13.7f); run_for(Engine.chargeDebounceMs + 60);
  TEST_ASSERT_FALSE(Engine.chargeMarginal());                      // cleared
}

void setup() {
  delay(2000);              // let USB serial attach before Unity prints
  UNITY_BEGIN();
  RUN_TEST(test_sleep_to_armed);
  RUN_TEST(test_armed_to_running_master_on);
  RUN_TEST(test_stays_armed_below_vrunon);
  RUN_TEST(test_running_to_powered_keeps_master);
  RUN_TEST(test_powered_to_running_restart);
  RUN_TEST(test_powered_backstop_to_armed);
  RUN_TEST(test_keyoff_offdelay_then_sleep);
  RUN_TEST(test_offdelay_resume);
  RUN_TEST(test_charge_transient_ignored);
  RUN_TEST(test_charge_sustained_warns);
  RUN_TEST(test_charge_hysteresis_deadband);
  UNITY_END();
}
void loop() {}
