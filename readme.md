# SHTC3 Sensor Provider

A [Sensor Hub](../../readme.md) provider usermod for the Sensirion SHTC3 -
a small, cheap I2C temperature + humidity sensor at a fixed address (`0x70`,
no address pin, so only one per I2C bus). It registers two sensors with the
hub, `shtc3_temperature` and `shtc3_humidity` by default, and lets the hub
handle MQTT, Home Assistant discovery, the JSON API and the Info tab.

See [`shtc3_sensor_provider.cpp`](shtc3_sensor_provider.cpp) for the full
source and the [Sensor Hub readme](../../readme.md) for how the bus/hub work.

## Hardware

Wire the SHTC3 to the I2C pins configured on WLED's own
**Config > LED Preferences** page (SDA/SCL, shared across all I2C usermods).
WLED itself calls `Wire.begin()` with those pins at boot, before any
usermod's `setup()` runs - this usermod only checks the pins are set and
then uses the already-initialized bus. It does not call `Wire.begin()`
itself, so it plays nicely with other I2C usermods/sensors on the same bus.

If the sensor isn't found at boot (not wired up yet, still powering up,
etc.) it keeps retrying `begin()` every 10s rather than giving up. After 3
consecutive failed reads both sensors are marked unavailable in Home
Assistant; after 10 it re-attempts `begin()` from scratch.

## Usage

This folder is a complete, independent out-of-tree usermod that depends on
the Sensor Hub's `sensor_bus.h` at compile time (see `library.json` for its
`adafruit/Adafruit SHTC3` dependency). Clone/copy it next to your WLED
checkout alongside the hub itself:

```
~/projects/
  WLED/
  wled-sensor-hub/                 <- the hub (usermod_sensor_hub.cpp, sensor_bus.h, ...)
  wled-sensor-hub-shtc3/           <- this folder
```

In `platformio_override.ini`:

```ini
[env:esp32dev]
extends = env:esp32dev
custom_usermods =
  ${env:esp32dev.custom_usermods}
  symlink:///home/you/projects/wled-sensor-hub
  symlink:///home/you/projects/wled-sensor-hub-shtc3
build_flags =
  ${env:esp32dev.build_flags}
  -I../wled-sensor-hub   ; so #include "sensor_bus.h" resolves
```

## Usermod Settings

| Setting | Default | Description |
|---|---|---|
| Enabled | on | Master on/off switch (also auto-disabled if I2C pins aren't configured) |
| Check interval | 30s | How often the sensor is read |
| Name prefix | `shtc3` | Sensor names become `<prefix>_temperature` / `<prefix>_humidity` - must be unique across every provider registered with the hub |
| Precision | 1 | Decimal places published for both readings |
