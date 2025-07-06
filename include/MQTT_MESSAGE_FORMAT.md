# MQTT Message Format Specification

This document defines the MQTT message formats used by the Green Game (Funger) IoT devices based on the `receiveEvents()` function in `GreenGame.cpp`.

## Message Overview

All MQTT messages are JSON formatted and published to specific topic patterns. The device receives events through the `receiveEvents()` function which parses JSON messages and handles different event types.

## Topic Structure

### Inbound Topics (Device Subscriptions)
- `funger/events/` - Global events for all devices in the game
- `funger/device/{deviceID}` - Device-specific commands and events

### Outbound Topics (Device Publications)
- `funger/events/` - Game events (touch, sync)
- `funger/device/{deviceID}` - Device-specific events
- `funger/device/{deviceID}/status` - Device status updates
- `funger/device/{deviceID}/logs` - Device log messages

## Message Types

### 1. Touch Event

**Topic:** `funger/events/` and `funger/device/{deviceID}`

**Direction:** Device → Backend

**Purpose:** Published when a player touches their device button

```json
{
  "event": "touch",
  "device": "deviceID",
  "delta": 1234,
  "time": 1672531200000,
  "H": 109,
  "S": 63,
  "V": 98,
  "position": 1
}
```

**Fields:**
- `event`: Always "touch"
- `device`: Unique device identifier
- `delta`: Time delta from last sync (milliseconds)
- `time`: Unix timestamp in milliseconds when touch occurred
- `H`: Hue value (0-360)
- `S`: Saturation value (0-100)
- `V`: Value/Brightness (0-100)
- `position`: Player position (1=FIRST, 2=SECOND)

### 2. Sync Event

**Topic:** `funger/events/`

**Direction:** Device → Backend

**Purpose:** Published to synchronize game state after determining win/lose condition

```json
{
  "event": "sync",
  "device": "deviceID"
}
```

**Fields:**
- `event`: Always "sync"
- `device`: Unique device identifier of the device initiating sync

### 3. Connected Event

**Topic:** `funger/device/{deviceID}`

**Direction:** Device → Backend

**Purpose:** Published when device establishes MQTT connection

```json
{
  "event": "connected",
  "device": "deviceID",
  "userName": "Device Name",
  "ipaddr": "192.168.1.100",
  "FW_Ver": "1.0.0",
  "HW_Ver": "1.0"
}
```

**Fields:**
- `event`: Always "connected"
- `device`: Unique device identifier
- `userName`: Human-readable device name
- `ipaddr`: Device IP address
- `FW_Ver`: Firmware version
- `HW_Ver`: Hardware version

### 4. Status Event

**Topic:** `funger/device/{deviceID}/status`

**Direction:** Device → Backend

**Purpose:** Automatic status updates when device state changes

```json
{
  "event": "status",
  "device": "deviceID",
  "H": 120,
  "S": 85,
  "V": 200,
  "position": 1,
  "mqttLogLevel": 3,
  "firmwareVersion": "1.1.0",
  "hardwareVersion": "1",
  "isConnected": true,
  "lastUpdate": 1672531200000
}
```

**Fields:**
- `event`: Always "status"
- `device`: Unique device identifier
- `H`: Current hue value (0-360)
- `S`: Current saturation value (0-100)
- `V`: Current value/brightness (0-100)
- `position`: Current game position (1=FIRST, 2=SECOND, 3=OTHER, 4=ENTICE)
- `mqttLogLevel`: Current MQTT log level (1-5)
- `firmwareVersion`: Device firmware version
- `hardwareVersion`: Device hardware version
- `isConnected`: MQTT connection status
- `lastUpdate`: Timestamp of last status update (milliseconds)

**Automatic Triggers:**
Status messages are automatically sent when:
- Device connects to MQTT
- Player position changes (touch events, game state changes)
- Color values change (HSV updates)
- Display settings are modified via MQTT commands

### 5. Log Messages

**Topic:** `funger/device/{deviceID}/logs`

**Direction:** Device → Backend

**Purpose:** Device log entries for debugging and monitoring

```json
{
  "level": 3,
  "entry": "Touch event detected",
  "time": 1672531200
}
```

**Fields:**
- `level`: Log level (1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE)
- `entry`: Log message text
- `time`: Unix timestamp (seconds)

### 6. OTA Update Command

**Topic:** `funger/device/{deviceID}`

**Direction:** Backend → Device

**Purpose:** Trigger over-the-air firmware update

```json
{
  "event": "OTA",
  "url": "https://example.com/firmware.bin",
  "persist": true
}
```

**Fields:**
- `event`: Always "OTA"
- `url`: URL to download firmware binary
- `persist`: Boolean indicating if update should persist across reboots (optional, defaults to true)

### 7. Display Configuration

**Topic:** `funger/device/{deviceID}`

**Direction:** Backend → Device

**Purpose:** Configure display settings and LED behavior

```json
{
  "event": "display",
  "maxBrightness": 255,
  "nightBrightness": 50,
  "nightStart": 22,
  "nightEnd": 7,
  "RGBW_EN": true,
  "R": 128,
  "G": 255,
  "B": 64,
  "W": 0
}
```

**Fields:**
- `event`: Always "display"
- `maxBrightness`: Maximum LED brightness (0-255)
- `nightBrightness`: Brightness during night mode (0-255)
- `nightStart`: Hour when night mode begins (0-23)
- `nightEnd`: Hour when night mode ends (0-23)
- `RGBW_EN`: Enable RGBW override mode (boolean)
- `R`: Red channel brightness override (0-255)
- `G`: Green channel brightness override (0-255)
- `B`: Blue channel brightness override (0-255)
- `W`: White channel brightness override (0-255)

### 8. Factory Reset Command

**Topic:** `funger/device/{deviceID}`

**Direction:** Backend → Device

**Purpose:** Reset device to factory defaults

```json
{
  "event": "reset"
}
```

**Fields:**
- `event`: Always "reset"

## Event Processing Logic

### Touch Event Processing
1. Device receives touch event from another device
2. Compares event timing with local touch event
3. Determines winner/loser based on timing
4. Updates player position (FIRST=1, SECOND=2)
5. Publishes sync event
6. Updates LED color based on result

### Game State
- **FIRST (1)**: Player who touched first (LED shows green)
- **SECOND (2)**: Player who touched second (LED shows opponent's color)

### Color System
- Uses HSV color space (Hue, Saturation, Value)
- Hue: 0-360 degrees
- Saturation: 0-100%
- Value/Brightness: 0-100%

## Error Handling

- Non-JSON messages generate error logs
- Unknown event types generate warning logs
- Missing required fields are handled gracefully with defaults
- Events from the same device are ignored to prevent self-triggering

## Implementation Notes

- All timestamps are in milliseconds (for `time` field) or seconds (for log `time` field)
- Device ignores its own events to prevent loops
- Delta timing is used for game synchronization
- Persist flag in OTA events defaults to `true` if not provided
- Display settings are stored in device preferences and persist across reboots

## Device Status Structure

The device firmware maintains a `DeviceStatus` structure that contains:

```cpp
struct DeviceStatus {
  uint16_t H, S, V;           // Current HSV color values
  uint8_t position;           // Current game position (FIRST=1, SECOND=2, OTHER=3, ENTICE=4)  
  String deviceID;            // Unique device identifier
  uint8_t mqttLogLevel;       // Current MQTT logging level (1-5)
  String firmwareVersion;     // Firmware version string
  String hardwareVersion;     // Hardware version string
  bool isConnected;           // MQTT connection status
  unsigned long lastUpdate;   // Last update timestamp
};
```

This structure provides a centralized way to track device state and can be used to generate status messages and respond to configuration commands.
