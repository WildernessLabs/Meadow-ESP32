# Add Access Point Functionality

The purpose of this task is to reinstate the Access Point functionality to the WiFi modules.

The current project has a partial implementation of beta test functionality for the ESP32 to act as an access point.  Key files in the implementation of this functionality are:

- SharedEnums.hpp
- WiFiRequestHandler.cpp/hpp
- AccessPoint.cpp
- ProgressiveDelay.cpp

The project uses C++

## Current State of the Access Point Functionality

The access point functionality was prototyped using ESP-IDF 4.1.  The current system uses ESP-IDF version 5.5.  Some of the functionas used will need to be changed as will some parameters to the ESP-IDF API.

## Communication and Control

The system is controlled through a message queue.  The WiFiRequestHandler task receives messages through a message queue and dispatches the messages the appropriate class methods.

Tasks that are synchronous are normally processed by the WiFiRequestHandler task itself.  Tasks that can be completed asynchronously are passed to the ThreadPool.

A new response message is created at the end of a task.  These messages are added to the communication system message queue.

## Events

The system should generate event messages for:

- Station connected to access point
- Station disconnected to access point
- Maximum number of connections reached

## Tests

Some unit tests are available in the tests directories.  The WiFi tests can be found in the file test_wifi.cpp.

---

## Implementation Plan

### Analysis of Current State

The following summarises what has already been implemented and what remains to be done.

#### Fully Implemented

- `LockConnectedNodesMap()` / `UnlockConnectedNodesMap()` — mutex helpers for the connected-nodes map.
- `ExtractAndValidateAccessPointInformation()` — decodes and validates the access point configuration from the message payload.
- `AccessPointStartedEvent()` — retrieves IP information from the soft-AP interface, encodes it, and raises a `WiFiFunction::AccessPointStartedEvent` message to the STM32.  Sets `WIFI_ACCESS_POINT_STARTED_BIT`.
- `AccessPointStoppedEvent()` — clears the connected-nodes map, raises a `WiFiFunction::AccessPointStoppedEvent` message to the STM32, and clears `WIFI_ACCESS_POINT_STARTED_BIT`.
- `RaiseNodeConnectionChangeEvent()` — encodes node MAC / IP information and raises a `NodeConnectedEvent` or `NodeDisconnectedEvent` message to the STM32.
- `NodeDisconnectedEvent()` — looks up the disconnecting station in the connected-nodes map by MAC address, removes it, and calls `RaiseNodeConnectionChangeEvent()`.
- `DispatchRequest()` routing — `StartAccessPoint` and `StopAccessPoint` function codes are already dispatched to the correct methods.
- Event handler cases — `WIFI_EVENT_AP_START`, `WIFI_EVENT_AP_STOP`, and `WIFI_EVENT_AP_STADISCONNECTED` are already handled and route to the correct methods.

#### Partially Implemented / Commented Out

- `StartAccessPoint()` — the complete logic is commented out; the method currently returns `StatusCodes::NotImplemented`.
- `StopAccessPoint()` — the complete logic is commented out; the method currently returns `StatusCodes::NotImplemented`.
- `NodeConnectedEvent()` — the logic is commented out and uses the deprecated `tcpip_adapter_*` API from ESP-IDF 4.x.
- `StartWiFiInterface()` — the call to `esp_netif_create_default_wifi_ap()` for `WIFI_MODE_AP` and `WIFI_MODE_APSTA` is commented out.
- `WIFI_EVENT_AP_STACONNECTED` event case — logs the event but does not call `NodeConnectedEvent()`.
- Maximum connections event — not yet raised anywhere.

### ESP-IDF API Migration Notes (4.1 → 5.5)

The following API changes must be applied when reinstating the commented-out code.

| ESP-IDF 4.1 | ESP-IDF 5.5 | Notes |
|---|---|---|
| `tcpip_adapter_sta_list_t` | `esp_netif_sta_list_t` | New struct in `esp_netif.h` |
| `tcpip_adapter_get_sta_list()` | `esp_netif_get_sta_list()` | Requires `esp_netif_t *` AP handle |
| `tcpip_adapter_sta_info_t` | `esp_netif_sta_info_t` | Members `mac` and `ip` remain |
| `ESP_IF_WIFI_AP` | `WIFI_IF_AP` | Already used correctly elsewhere in this codebase |
| `lwip/apps/sntp.h` | `esp_sntp.h` | Already updated in the existing include list |

### Task List

Each item in the task list should be added to its own branch.  These branches can be merged into the Access Point branch.  

They cannot be merged into the develop branch.

All code should remain local - no code may be pushed to GitHub

#### 1. Reinstate Soft-AP Network Interface Creation in `StartWiFiInterface()`

**File:** `components/F7Services/WiFi/WiFiRequestHandler.cpp`

Uncomment the block that calls `esp_netif_create_default_wifi_ap()` and restore the guard condition for `WIFI_MODE_AP` and `WIFI_MODE_APSTA`.  Add a `nullptr` check on `_softApNetIfHandle` consistent with the existing check on `_stationNetIfHandle`.

#### 2. Implement `StartAccessPoint()`

**File:** `components/F7Services/WiFi/AccessPoint.cpp`

Remove the `NotImplemented` stub and restore the commented-out logic with the following changes.

- Replace `ESP_IF_WIFI_AP` with `WIFI_IF_AP` in the `esp_wifi_set_config()` call.
- When the WiFi interface has not yet been started, call `StartWiFiInterface(WIFI_MODE_APSTA, ...)` so that station mode remains available alongside the soft-AP.
- When the interface is already started in station mode only (`WIFI_MODE_STA`), call `esp_wifi_set_mode(WIFI_MODE_APSTA)` before configuring the AP, then call `esp_netif_create_default_wifi_ap()` if `_softApNetIfHandle` is `nullptr`.
- Retain the optional static-IP configuration path using `esp_netif_dhcps_stop()` / `esp_netif_set_ip_info()` / `esp_netif_dhcps_start()` on `_softApNetIfHandle`.
- Wait on `WIFI_ACCESS_POINT_STARTED_BIT | WIFI_ERROR_BIT` before returning the response, consistent with the existing station connect pattern.
- Free `accessPointInformation` using `vPortFree()` before returning.

#### 3. Implement `StopAccessPoint()`

**File:** `components/F7Services/WiFi/AccessPoint.cpp`

Remove the `NotImplemented` stub and implement the following.

- Clear the AP configuration by calling `esp_wifi_set_config(WIFI_IF_AP, &config)` with a zeroed `wifi_config_t`.
- If the station interface is still active (`WIFI_CONNECTED_BIT` or `WIFI_HAVE_IP_BIT` is set), switch mode back to `WIFI_MODE_STA` via `esp_wifi_set_mode()`.
- Destroy the soft-AP network interface with `esp_netif_destroy_default_wifi()` or set `_softApNetIfHandle` to `nullptr` after the interface is stopped.
- Wait on `WIFI_EVENT_AP_STOP` (cleared `WIFI_ACCESS_POINT_STARTED_BIT`) before sending the response, or send the response synchronously after the configuration change if waiting is not required.

#### 4. Implement `NodeConnectedEvent()` using ESP-IDF 5.5 API

**File:** `components/F7Services/WiFi/AccessPoint.cpp`

Replace the commented-out `tcpip_adapter_*` code with the ESP-IDF 5.x equivalent.

- Call `esp_wifi_ap_get_sta_list()` to obtain a `wifi_sta_list_t`.
- Call `esp_netif_get_sta_list()` with `_softApNetIfHandle` and the `wifi_sta_list_t` to obtain an `esp_netif_sta_list_t` containing MAC addresses and IP addresses.
- Iterate over `esp_netif_sta_list_t::sta[]` (up to `esp_netif_sta_list_t::num` entries).
- Derive the map key from the six-byte MAC address using `memcpy` into a `uint64_t` as before.
- For each station whose IP address differs from the stored value (or is new), update the map and call `RaiseNodeConnectionChangeEvent(WiFiFunction::NodeConnectedEvent, &data)`.

#### 5. Wire `WIFI_EVENT_AP_STACONNECTED` to `NodeConnectedEvent()`

**File:** `components/F7Services/WiFi/WiFiRequestHandler.cpp`

In `EventHandlerHelper()`, add a call to `NodeConnectedEvent()` in the `WIFI_EVENT_AP_STACONNECTED` case, replacing the current log-only stub.

#### 6. Implement the Maximum Connections Reached Event

**File:** `components/F7Services/WiFi/AccessPoint.cpp` and `components/F7Services/WiFi/WiFiRequestHandler.cpp`

- In `NodeConnectedEvent()`, after updating the connected-nodes map, compare the map size against `ESP_WIFI_MAX_CONN_NUM` (or the value stored in the AP configuration).
- If the count equals the maximum, raise an event message to the STM32 using `RaiseEvent()` with the appropriate `WiFiFunction` code.
- A new `WiFiFunction` enum value (e.g., `MaximumConnectionsReachedEvent`) will need to be added to `SharedEnums.hpp`.

#### 7. Add Unit Tests

**File:** `components/F7Services/test/test_wifi.cpp`

Add the following test cases, following the existing pattern (set up `MockMessageDispatcher`, send a request message, wait for response, verify status code and payload).

- `Start access point` — start a soft-AP with a known SSID and password; verify `StatusCodes::CompletedOk` and that the response payload contains a valid IP address.
- `Start access point with static IP` — start a soft-AP with a static IP configuration; verify the returned IP address matches the configured value.
- `Stop access point` — start a soft-AP then stop it; verify `StatusCodes::CompletedOk`.
- `Start access point — invalid configuration` — send a `StartAccessPoint` request with a missing SSID; verify `StatusCodes::CannotStartAccessPoint`.
- `Stop access point when not running` — send a `StopAccessPoint` request without first starting the AP; verify an appropriate error status code.

---

## Implementation Notes

### Completed: 23 May 2026

All seven tasks were implemented on individual feature branches and merged into the `AccessPoint` branch.

### Files Changed

| File | Changes |
|---|---|
| `components/F7Services/WiFi/AccessPoint.cpp` | Implemented `StartAccessPoint()`, `StopAccessPoint()`, and `NodeConnectedEvent()`. Added `#include "esp_netif.h"`. |
| `components/F7Services/WiFi/WiFiRequestHandler.cpp` | Uncommented `esp_netif_create_default_wifi_ap()` block in `StartWiFiInterface()`. Added `NodeConnectedEvent()` dispatch in `IP_EVENT_AP_STAIPASSIGNED`. |
| `components/F7Services/WiFi/WiFiRequestHandler.hpp` | Updated `NodeConnectedEvent()` declaration to accept `ip_event_ap_staipassigned_t *` parameter. |
| `components/F7Services/Messaging/SharedEnums.hpp` | Added `MaximumConnectionsReachedEvent = 49` to the `WiFiFunction` enum. |
| `components/F7Services/test/WiFiRequestHandlerTestFriend.hpp` | Added `StartAccessPoint()` and `StopAccessPoint()` method declarations. |
| `components/F7Services/test/WiFiRequestHandlerTestFriend.cpp` | Implemented `StartAccessPoint()` and `StopAccessPoint()` delegates. |
| `components/F7Services/test/test_wifi.cpp` | Added five access point test cases. |

### Deviations from the Implementation Plan

#### Task 4 — `NodeConnectedEvent()` API

The migration table in the plan specified:

| ESP-IDF 4.1 | ESP-IDF 5.5 |
|---|---|
| `tcpip_adapter_sta_list_t` | `esp_netif_sta_list_t` |
| `tcpip_adapter_get_sta_list()` | `esp_netif_get_sta_list()` |
| `tcpip_adapter_sta_info_t` | `esp_netif_sta_info_t` |

`esp_netif_sta_list_t`, `esp_netif_get_sta_list()`, and `esp_netif_sta_info_t` are **not present** in the ESP-IDF 5.5.3 build used by this project.

The `NodeConnectedEvent()` function was therefore rewritten to accept the `IP_EVENT_AP_STAIPASSIGNED` event data directly as `ip_event_ap_staipassigned_t *eventData`. This struct provides both the station MAC address (`eventData->mac`) and the DHCP-assigned IP address (`eventData->ip`) without requiring a secondary STA-list query.

The function signature changed from `NodeConnectedEvent()` to `NodeConnectedEvent(ip_event_ap_staipassigned_t *eventData)`.

#### Task 5 — `WIFI_EVENT_AP_STACONNECTED` wiring

The plan called for wiring `WIFI_EVENT_AP_STACONNECTED` to `NodeConnectedEvent()`. Because the revised `NodeConnectedEvent()` requires the DHCP event data (which is not available at `WIFI_EVENT_AP_STACONNECTED` time), no function call is made from that event case. The notification to the STM32 is raised correctly once DHCP assignment completes via `IP_EVENT_AP_STAIPASSIGNED`.

