#include "wled.h"
#include "sensor_bus.h"
#include <Adafruit_SHTC3.h>

/*
 * SHTC3 temperature + humidity sensor provider.
 *
 * Reads a Sensirion SHTC3 over I2C (fixed address 0x70, no address pin) and
 * pushes the two readings into the Sensor Hub (see ../../usermod_sensor_hub.cpp
 * and ../../sensor_bus.h) as two sensors, "<prefix>_temperature" and
 * "<prefix>_humidity". This usermod never talks to MQTT, the JSON API or
 * the Info tab itself - the hub takes care of all of that once a sensor is
 * registered here.
 *
 * Wiring: SHTC3 SDA/SCL go to the I2C pins configured on WLED's own
 * Config > LED Preferences page (the shared "i2c_sda"/"i2c_scl" globals).
 * WLED core already calls Wire.begin() with those pins while loading cfg.json
 * at boot (wled00/cfg.cpp), before any usermod's setup() runs - so this
 * usermod only needs to confirm the pins are set, then use the shared Wire
 * bus. It must NOT call Wire.begin() itself.
 *
 * This folder is a self-contained usermod (see library.json for its
 * "adafruit/Adafruit SHTC3" dependency - PlatformIO resolves its
 * Adafruit_BusIO / Adafruit Unified Sensor dependencies automatically) - see
 * readme.md for wiring it into a WLED build.
 */
class SHTC3SensorUsermod : public Usermod {
  private:
    Adafruit_SHTC3 shtc3;
    SensorHub* hub = nullptr;
    uint8_t tempHandle = SENSOR_HANDLE_INVALID;
    uint8_t humidityHandle = SENSOR_HANDLE_INVALID;

    bool enabled = true;
    bool sensorFound = false;
    bool initDone = false;

    unsigned long lastRead = 0;
    unsigned long lastBeginAttempt = 0;
    uint8_t consecutiveFailures = 0;

    // config
    uint16_t checkIntervalS = 30; // how often to read the sensor
    String namePrefix = "shtc3";  // sensor names become "<prefix>_temperature" / "<prefix>_humidity"
    uint8_t precision = 1;        // decimal places published for both readings

    static const char _name[];
    static const char _enabled[];
    static const char _checkInterval[];
    static const char _namePrefix[];
    static const char _precision[];

    void registerSensors() {
      if (!hub || tempHandle != SENSOR_HANDLE_INVALID) return; // already registered
      // registerSensor() copies the name internally, so a temporary is fine here.
      tempHandle     = hub->registerSensor((namePrefix + "_temperature").c_str(), SensorType::Temperature, nullptr, nullptr, precision);
      humidityHandle = hub->registerSensor((namePrefix + "_humidity").c_str(),    SensorType::Humidity,    nullptr, nullptr, precision);
    }

  public:
    void setup() override {
      // I2C bus is configured (and Wire.begin() already called) via WLED's
      // own Config > LED Preferences page - nothing to do here if it's unset.
      if (i2c_sda < 0 || i2c_scl < 0) { enabled = false; return; }
      sensorFound = shtc3.begin();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !initDone) return;

      if (!hub) hub = getSensorHub(); // Sensor Hub usermod may finish init after us
      if (hub) registerSensors();

      unsigned long now = millis();

      if (!sensorFound) {
        // sensor missing at boot (or lost) - keep retrying rather than giving up forever
        if (now - lastBeginAttempt < 10000) return;
        lastBeginAttempt = now;
        sensorFound = shtc3.begin();
        if (!sensorFound) return;
      }

      if (now - lastRead < (unsigned long)checkIntervalS * 1000UL) return;
      lastRead = now;

      sensors_event_t humidityEvt, tempEvt;
      if (!shtc3.getEvent(&humidityEvt, &tempEvt)) {
        consecutiveFailures++;
        if (hub && consecutiveFailures >= 3) {
          if (tempHandle != SENSOR_HANDLE_INVALID)     hub->setSensorAvailable(tempHandle, false);
          if (humidityHandle != SENSOR_HANDLE_INVALID) hub->setSensorAvailable(humidityHandle, false);
        }
        if (consecutiveFailures >= 10) sensorFound = false; // force a fresh begin() next loop
        return;
      }

      consecutiveFailures = 0;
      if (hub) {
        if (tempHandle != SENSOR_HANDLE_INVALID) {
          hub->setSensorAvailable(tempHandle, true);
          hub->updateSensor(tempHandle, tempEvt.temperature);
        }
        if (humidityHandle != SENSOR_HANDLE_INVALID) {
          hub->setSensorAvailable(humidityHandle, true);
          hub->updateSensor(humidityHandle, humidityEvt.relative_humidity);
        }
      }
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_checkInterval)] = checkIntervalS;
      top[FPSTR(_namePrefix)] = namePrefix;
      top[FPSTR(_precision)] = precision;
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_checkInterval)], checkIntervalS);
      configComplete &= getJsonValue(top[FPSTR(_namePrefix)], namePrefix);
      configComplete &= getJsonValue(top[FPSTR(_precision)], precision);
      return configComplete;
    }

    void appendConfigData(Print& settingsScript) override {
      settingsScript.print(F("addInfo('SHTC3Sensor:checkInterval',1,'seconds between sensor reads');"));
      settingsScript.print(F("addInfo('SHTC3Sensor:namePrefix',1,'sensor names become &lt;prefix&gt;_temperature / &lt;prefix&gt;_humidity - must be unique across all sensor providers');"));
      settingsScript.print(F("addInfo('SHTC3Sensor:precision',1,'decimal places published for both readings');"));
    }
};

const char SHTC3SensorUsermod::_name[]          PROGMEM = "SHTC3Sensor";
const char SHTC3SensorUsermod::_enabled[]       PROGMEM = "enabled";
const char SHTC3SensorUsermod::_checkInterval[] PROGMEM = "checkInterval";
const char SHTC3SensorUsermod::_namePrefix[]    PROGMEM = "namePrefix";
const char SHTC3SensorUsermod::_precision[]     PROGMEM = "precision";

static SHTC3SensorUsermod shtc3_sensor;
REGISTER_USERMOD(shtc3_sensor);
