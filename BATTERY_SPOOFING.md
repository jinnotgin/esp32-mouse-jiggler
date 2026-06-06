# Battery Spoofing

## Realistic Battery Tracking

The firmware reports a fake BLE battery percentage instead of a fixed value forever. This is implemented in `src/main.cpp` using ESP32 `Preferences`, which stores small values in NVS flash.

The feature is controlled by:

```cpp
#define REALISTIC_BATTERY                  true
#define CONNECTIONS_PER_BATTERY_PERCENT    4
#define BATTERY_MIN_LEVEL                  15
```

The default starting value still comes from:

```cpp
#define BATTERY_LEVEL   84
```

When `REALISTIC_BATTERY` is enabled, startup opens the `jiggler` Preferences namespace and loads:

- `battery`: the last reported fake battery level
- `connCount`: the number of BLE connection sessions counted toward the next 1% battery drop

If no saved values exist, the firmware starts from `BATTERY_LEVEL` and a connection count of `0`. The loaded battery value is clamped between `BATTERY_MIN_LEVEL` and `100` before being reported, so stale or invalid flash data cannot produce an impossible percentage.

## Connection Counting

The battery model assumes roughly one BLE connection per day. To approximate a 12-month AA battery life from `100%` to `15%`, the firmware needs to distribute 85 percentage drops across about 365 daily connection sessions:

```text
365 days / 85 percentage drops = 4.29 connections per 1% drop
```

Because the firmware stores whole connection counts, it uses `4` connections per 1% drop. That reaches the minimum level after about 340 daily connection sessions:

```text
85 percentage drops * 4 connections = 340 connections
```

This is slightly under 12 months, but it keeps the spoofed battery from looking too optimistic. Using `5` connections per drop would stretch the same range to 425 daily connections, which is noticeably longer than 12 months.

The helper `updateBatteryOnBleConnected()` is called from `loop()` after the BLE connection check. It is not a BLE callback. Instead, the code uses `batteryConnectionCounted` as an edge detector:

- while disconnected, `batteryConnectionCounted` is reset to `false`
- when connected, the first loop calls `updateBatteryOnBleConnected()`
- that increments `bleConnectionCount` once and sets `batteryConnectionCounted` to `true`
- later loops during the same connection return immediately

This avoids counting every loop iteration as a new connection.

## Battery Drop And Reset

When `bleConnectionCount` reaches `CONNECTIONS_PER_BATTERY_PERCENT`, the count resets to `0`.

Then:

- if `currentBatteryLevel` is above `BATTERY_MIN_LEVEL`, it drops by `1`
- if it is already at or below `BATTERY_MIN_LEVEL`, it resets to `100`

This models replacing AA batteries, not recharging. The device never reports below the configured minimum.

## Disabling The Feature

When `REALISTIC_BATTERY` is set to `false`, the firmware:

- uses the fixed `BATTERY_LEVEL`
- opens the `jiggler` Preferences namespace
- clears all saved values in that namespace with `prefs.clear()`
- closes Preferences with `prefs.end()`

This gives a simple reset path: flash once with `REALISTIC_BATTERY false` to clear saved battery state, then set it back to `true` and flash again if realistic tracking should resume.
