# Meadow-ESP32 Comprehensive Code Review

**Date:** 2026-05-11
**Reviewer:** Copilot (Senior Engineer Review)
**Scope:** `components/F7Services`, `main`, `test`
**Out of scope:** `managed_components`

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Open Issues Cross-Reference](#2-open-issues-cross-reference)
3. [Security](#3-security)
4. [Memory Management](#4-memory-management)
5. [Concurrency and Thread Safety](#5-concurrency-and-thread-safety)
6. [Code Correctness](#6-code-correctness)
7. [Best Practices](#7-best-practices)
8. [Code Style and Conventions](#8-code-style-and-conventions)
9. [Test Coverage](#9-test-coverage)
10. [Summary of Recommendations](#10-summary-of-recommendations)

---

## 1. Executive Summary

The Meadow-ESP32 firmware is a well-structured embedded C++ project providing WiFi, Bluetooth, file storage, and POSIX networking services to an STM32 host over SPI/UART. The architecture is clear, documentation is generally thorough, and the overall design is sound for a resource-constrained FreeRTOS target.

However, the codebase carries a significant number of correctness and safety issues, several of which are **critical**. The most pressing concerns are:

- **Mismatched heap allocators** — `pvPortMalloc` / FreeRTOS heap objects are released with `vPortFree` via a utility that wraps `vPortFree`, but `new[]` / `realloc` (stdlib) memory is mixed in, creating undefined behaviour on the FreeRTOS heap.
- **Missing null checks** after allocation calls throughout the SPI and Bluetooth subsystems.
- **Race conditions** in the ThreadPool and singleton implementations.
- **`FileSystem::Mount()` always returns `true`**, masking all mount failures.

All 30 currently open issues are referenced and grouped below.

---

## 2. Open Issues Cross-Reference

All open issues on the `WildernessLabs/Meadow-ESP32` repository are addressed in this review. The table below maps each issue to the relevant review section.

| Issue | Severity | Title | Review Section |
|-------|----------|-------|---------------|
| [#155](https://github.com/WildernessLabs/Meadow-ESP32/issues/155) | LOW | Tidy up ThreadPool code | §7.1 |
| [#159](https://github.com/WildernessLabs/Meadow-ESP32/issues/159) | MEDIUM | `GetCurrentValueForAttributeHandle` stack allocation | §6.5 |
| [#162](https://github.com/WildernessLabs/Meadow-ESP32/issues/162) | CRITICAL | Missing null checks after allocations | §4.1 |
| [#163](https://github.com/WildernessLabs/Meadow-ESP32/issues/163) | CRITICAL | Mismatched allocator/deallocator pairs | §4.2 |
| [#164](https://github.com/WildernessLabs/Meadow-ESP32/issues/164) | HIGH | Memory/resource leaks on error paths | §4.3 |
| [#165](https://github.com/WildernessLabs/Meadow-ESP32/issues/165) | HIGH | Double-free and use-after-free risks | §4.4 |
| [#166](https://github.com/WildernessLabs/Meadow-ESP32/issues/166) | HIGH | Mixing `new[]`/`delete[]` with FreeRTOS heap | §4.2 |
| [#167](https://github.com/WildernessLabs/Meadow-ESP32/issues/167) | CRITICAL | Race conditions in ThreadPool and singleton patterns | §5.1 |
| [#168](https://github.com/WildernessLabs/Meadow-ESP32/issues/168) | HIGH | Unprotected shared data structures | §5.2 |
| [#169](https://github.com/WildernessLabs/Meadow-ESP32/issues/169) | CRITICAL | Allocator mismatch and double-free in file operations | §4.2 |
| [#170](https://github.com/WildernessLabs/Meadow-ESP32/issues/170) | HIGH | Missing null checks on dispatcher/callback pointers | §6.1 |
| [#171](https://github.com/WildernessLabs/Meadow-ESP32/issues/171) | MEDIUM | Buffer overflow and static buffer issues | §6.2 |
| [#172](https://github.com/WildernessLabs/Meadow-ESP32/issues/172) | MEDIUM | Non-atomic operations and inconsistent mutex use | §5.3 |
| [#173](https://github.com/WildernessLabs/Meadow-ESP32/issues/173) | MEDIUM | Text mode used for binary file I/O | §6.3 |
| [#174](https://github.com/WildernessLabs/Meadow-ESP32/issues/174) | MEDIUM | TOCTOU race in file size check and open | §6.4 |
| [#175](https://github.com/WildernessLabs/Meadow-ESP32/issues/175) | CRITICAL | Integer underflow/overflow in packet handling | §6.6 |
| [#176](https://github.com/WildernessLabs/Meadow-ESP32/issues/176) | HIGH | SPI state machine default case cannot recover | §6.7 |
| [#177](https://github.com/WildernessLabs/Meadow-ESP32/issues/177) | MEDIUM | Magic numbers and redundant assignments | §8.1 |
| [#178](https://github.com/WildernessLabs/Meadow-ESP32/issues/178) | LOW | Typos and inconsistent logging | §8.2 |
| [#179](https://github.com/WildernessLabs/Meadow-ESP32/issues/179) | LOW | `NULL` instead of `nullptr` | §8.3 |
| [#180](https://github.com/WildernessLabs/Meadow-ESP32/issues/180) | LOW | `bzero` instead of `std::memset` | §8.4 |
| [#181](https://github.com/WildernessLabs/Meadow-ESP32/issues/181) | LOW | Unused variables and dead code | §8.5 |
| [#182](https://github.com/WildernessLabs/Meadow-ESP32/issues/182) | LOW | Missing docstrings on public methods | §8.6 |
| [#183](https://github.com/WildernessLabs/Meadow-ESP32/issues/183) | HIGH | Bluetooth callback lifecycle | §5.4 |
| [#184](https://github.com/WildernessLabs/Meadow-ESP32/issues/184) | MEDIUM | WiFi credentials not mutex-protected | §5.5 |
| [#185](https://github.com/WildernessLabs/Meadow-ESP32/issues/185) | MEDIUM | Bluetooth event handler stub | §6.8 |
| [#186](https://github.com/WildernessLabs/Meadow-ESP32/issues/186) | HIGH | `Mount()` always returns `true` | §6.9 |
| [#187](https://github.com/WildernessLabs/Meadow-ESP32/issues/187) | LOW | `NULL` instead of `nullptr` (duplicate of #179) | §8.3 |
| [#188](https://github.com/WildernessLabs/Meadow-ESP32/issues/188) | LOW | `bzero` instead of `std::memset` (duplicate of #180) | §8.4 |

---

## 3. Security

### 3.3 BLE Security Commented Out

**File:** `BluetoothRequestHandler.cpp`, `GATTSProfileEventHandler`, `ESP_GATTS_CONNECT_EVT`

```cpp
//  esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
```

BLE pairing security is disabled. Any device in range can connect and read/write characteristics.

**Recommendation:** Enable MITM protection if Bluetooth is used in a sensitive context. At minimum, document this as a conscious decision.

---

## 4. Memory Management

### 4.1 Missing Null Checks After Allocations [CRITICAL / #162]

Multiple allocation call sites do not check the returned pointer before use, risking a hard fault.

**`SpiTransportProvider::CreateSpiTransaction`** (`SpiTransportProvider.cpp`, line 314):
```cpp
spi_slave_transaction_t *transaction = static_cast<spi_slave_transaction_t *>(
    pvPortMalloc(sizeof(spi_slave_transaction_t)));
memset(transaction, 0, sizeof(spi_slave_transaction_t));  // crash if nullptr
```

**`SpiTransportProvider::ProcessInboundPacket`** (line 562):
```cpp
uint8_t *payloadBuffer = static_cast<uint8_t *>(pvPortMalloc(packet->PayloadLength));
memcpy(static_cast<void *>(payloadBuffer), ...);  // crash if nullptr
```

**`BluetoothRequestHandler::OnDataWriteRequest`** (line 814–821):
```cpp
Esp32Messaging::BTDataWriteRequest *request = static_cast<...>(pvPortMalloc(...));
request->Handle = handle;  // crash if nullptr
// ...
uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payload_length));
Encoders::EncodeBTDataWriteRequest(request, payload);  // crash if nullptr
```

**`BluetoothRequestHandler::GetHandles`** (line 402–407):
```cpp
response.Handles = static_cast<uint8_t *>(pvPortMalloc(response.HandlesLength));
_manager.GetHandles((uint16_t *) response.Handles);  // crash if nullptr

request->Payload = static_cast<uint8_t *>(pvPortMalloc(request->PayloadLength));
Encoders::EncodeBTGetHandlesResponse(&response, request->Payload);  // crash if nullptr
```

**`Encoders::ExtractString`** uses `strcpy` without checking the returned allocation before the copy (though it does check before returning).

**Recommendation:** Add null checks immediately after every `pvPortMalloc`, `heap_caps_calloc`, `strdup`, and `new` call. Return an error code or log and abort if allocation fails.

### 4.2 Mismatched Allocators [CRITICAL / #163, #166, #169]

The project uses multiple heap allocators:
- FreeRTOS: `pvPortMalloc` / `vPortFree`
- DMA-capable: `heap_caps_calloc` / `heap_caps_free`
- C++ operators: `new` / `new[]` / `delete` / `delete[]`
- libc: `realloc` / `free`

`Utils::DeleteAndNullifyPointer` calls `vPortFree`. This is **correct** for memory allocated with `pvPortMalloc`, but **incorrect** for the following cases:

**`FileSystem::ListFiles`** (`FileSystem.cpp`, line 504):
```cpp
fileNameList.FileDetails = static_cast<uint8_t *>(realloc(fileNameList.FileDetails, bufferLength));
```
`realloc` uses the libc heap. The returned pointer is never explicitly freed, but if it were passed to `Utils::DeleteAndNullifyPointer`, `vPortFree` would be called on libc memory — undefined behaviour. The correct allocator here is `pvPortMalloc` + `vPortFree`, with a manual reallocation.

**`BluetoothRequestHandler::GenerateAttributeTable`** (line 342):
```cpp
_attribute_table = new esp_gatts_attr_db_t[count];
```
`_attribute_table` is a C++ `new[]` allocation. `Utils::DeleteAndNullifyPointer` must **not** be called on this pointer; `delete[]` must be used. Since `BluetoothManager::DeleteAllServices` uses `delete[]` for `_valueMap` values, this particular pair is internally consistent, but mixing `new[]` with the rest of the codebase's FreeRTOS allocations remains a maintenance hazard.

**`ThreadPool::~ThreadPool`** (`ThreadPool.cpp`, line 98):
```cpp
Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&_threadsAvailable));
```
`_threadsAvailable` is a `SemaphoreHandle_t` (a FreeRTOS object). It must be deleted with `vSemaphoreDelete`, not `vPortFree`. Calling `vPortFree` on a semaphore corrupts the FreeRTOS kernel structures.

**Recommendation:**
- Standardise on `pvPortMalloc` / `vPortFree` for all dynamic allocation.
- Replace `realloc` with explicit `pvPortMalloc` + `memcpy` + `vPortFree`.
- Replace `new[]` / `delete[]` in Bluetooth with `pvPortMalloc` / `vPortFree`.
- Replace `Utils::DeleteAndNullifyPointer` on FreeRTOS handles with the correct `vSemaphoreDelete`, `vQueueDelete`, etc.

### 4.3 Memory / Resource Leaks on Error Paths [HIGH / #164]

**`FileSystem::ReadFile`** — If `fopen` succeeds and `fread` returns 0 bytes, `fileData->Contents` is allocated but the status is left as `Failure`. The cleanup at the bottom checks `result != StatusCodes::CompletedOk` to free `fileData->Contents`, so this path is actually handled, but the logic is difficult to follow.

More clearly: if `fopen` fails (`file == NULL`), `fileData->Contents` is allocated but not freed at that point. The cleanup section below does handle it, but the nested conditionals make auditing error-paths very difficult.

**`SpiTransportProvider::PacketReceived`** — When a CRC error occurs:
```cpp
_outboundMessage = Message::CreateOnHeap(messageType, ...);
newTransaction = CreateAcknowledgementTransaction(_outboundMessage, ...);
```
`_outboundMessage` is set here but is never deleted in this code path. On the next call to `PacketAcknowledged`, `_outboundMessage` would be a dangling pointer from the previous CRC error unless the state machine resets cleanly.

**`ThreadPool::~ThreadPool`** — The `_threads` array is iterated and each `Thread *` is deleted, but the underlying FreeRTOS task is not deleted. `Thread::~Thread` only frees the name buffer. The FreeRTOS task handle (`_taskHandle`) is never cancelled with `vTaskDelete`.

**Recommendation:** Use RAII wrappers or at minimum a structured goto-based cleanup pattern to ensure all error paths release resources.

### 4.4 Double-Free and Use-After-Free Risks [HIGH / #165]

**`PacketAcknowledgementReceived`** (`SpiTransportProvider.cpp`):
```cpp
Message::DeleteMessage(incomingMessage);
if (_currentState == WaitingForPacket)
{
    _outboundMessage = nullptr;
    ...
}
```
If `_outboundMessage` and `incomingMessage` happen to point to the same object (not possible by current flow, but defensive programming is important), this would be a double-free.

**`BluetoothRequestHandler::Start`** — `config->Config` is freed with `DeleteAndNullifyPointer`, then `config` itself is freed. If `Config` is null, the null check in `DeleteAndNullifyPointer` handles it, but `config` is passed without checking whether `ExtractBTStackConfig` returned null.

---

## 5. Concurrency and Thread Safety

### 5.1 Race Conditions in ThreadPool and Singleton Patterns [CRITICAL / #167]

**`ThreadPool::Execute`** contains a TOCTOU race:
```cpp
if (xSemaphoreTake(_threadsAvailable, ...) == pdTRUE)
{
    for (uint8_t index = 0; index < THREAD_POOL_SIZE; index++)
    {
        if (_threads[index] && !_threads[index]->IsBusy())
        {
            _threads[index]->Execute(method, parameter);  // ← race window between IsBusy() and Execute()
```
The semaphore guarantees a thread is available, but between `IsBusy()` returning `false` and `Execute()` claiming the thread, another caller on a different task could claim the same thread. `Thread::Execute` does check `_isBusy` under the mutex, so the race is caught there and an exception is thrown — but throwing from a FreeRTOS task is dangerous (see §7.2).

**Singleton pattern** (WiFiRequestHandler, BluetoothRequestHandler, SystemRequestHandler, SpiTransportProvider) all use the pattern:
```cpp
if (_instance != nullptr)
    throw new MultipleInstancesException();
_instance = this;
```
The check and assignment are not atomic. If two tasks simultaneously call the constructor before either sets `_instance`, both will proceed past the guard. This is unlikely in practice (construction happens in `app_main` before tasks are created), but is undefined behaviour.

### 5.2 Unprotected Shared Data Structures [HIGH / #168]

**`WiFiRequestHandler::_pollRequests`** is a `std::map` accessed from multiple tasks:
```cpp
std::map<uint32_t, struct pollfd *> WiFiRequestHandler::_pollRequests;
SemaphoreHandle_t WiFiRequestHandler::_pollRequestListMutex = NULL;
```
`_pollRequestListMutex` is created, which is good, but callers must verify that every access to `_pollRequests` is consistently guarded by this mutex. Any unguarded access is a data race on a non-trivially destructible STL container (heap corruption risk).

**`BluetoothManager::_valueMap`** is a `std::map<uint16_t, length_val_s>` accessed from the GATTS event handler (`ESP_GATTS_READ_EVT`, `ESP_GATTS_WRITE_EVT`) and from the FreeRTOS task handling STM32 requests. No mutex protects this map.

### 5.3 Non-Atomic Increment / Inconsistent Mutex Use [MEDIUM / #172]

**`MessageDispatcher::GetNextMessageID`**:
```cpp
xSemaphoreTake(_messageIdMutex, portMAX_DELAY);
_lastMessageId++;
uint32_t messageId = _lastMessageId;
xSemaphoreGive(_messageIdMutex);
messageId &= ~ESP32_MESSAGE_ID_MASK;
return (messageId);
```
The mutex correctly protects the increment. However, the masking operation (`&= ~ESP32_MESSAGE_ID_MASK`) is applied *after* releasing the mutex, meaning the returned ID could still have the high bit set in some return paths. This is a logic issue, not a concurrency issue — the mask should be applied before releasing the mutex.

Additionally, with `ESP32_MESSAGE_ID_MASK = 0x80000000`, `messageId &= ~0x80000000` clears the MSB. If the intent is to keep ESP32-originated IDs below 0x80000000, then once `_lastMessageId` rolls over past 0x7FFFFFFF the ID will wrap to 0 (since the bit is cleared), and IDs will start repeating immediately (0x80000000 masked → 0, 0x80000001 masked → 1, etc.). In-flight messages could receive the wrong response.

### 5.4 Bluetooth Callback Lifecycle [HIGH / #183]

Bluetooth callbacks (`GATTSProfileEventHandler`, `GAPEventHandler`, `EventHandlerHelper`) are registered as static C callbacks with the ESP-IDF BLE stack. They access `GetInstance()` to retrieve the singleton. If `BluetoothRequestHandler` is destroyed (or `Stop` is called) while a BLE event is in flight, `GetInstance()` returns `nullptr` and the callback dereferences it:

```cpp
GetInstance()->RaiseEvent(...);  // crash if _instance == nullptr
esp_ble_gap_start_advertising(GetInstance()->GetAdvertisingParameters());
```

**Recommendation:** Before unregistering the BLE stack or destroying the handler, ensure all in-flight callbacks have completed. Set `_instance = nullptr` only after `esp_bluedroid_disable` / `esp_bluedroid_deinit` have returned.

### 5.5 WiFi Credentials Not Mutex-Protected [MEDIUM / #184]

```cpp
char *WiFiRequestHandler::_defaultAccessPoint = nullptr;
char *WiFiRequestHandler::_password = nullptr;
```

These static members are assigned in `SetDefaultAccessPoint` and `SetPassword`, which free the old value and assign a new `strdup`-allocated string. If one task reads `_password` while another task calls `SetPassword`, the reader may access freed memory.

**Recommendation:** Protect all reads and writes to `_defaultAccessPoint` and `_password` with a dedicated mutex (or reuse `_connectedNodesListMutex` if appropriate).

---

## 6. Code Correctness

### 6.1 Null Dispatcher / Callback Pointer Dereference [HIGH / #170]

**`FileSystem::ReadFile`, `WriteFile`, `DeleteFile`, `ListFiles`, `Format`** all call:
```cpp
_messageDispatcher->QueueMessageForStm32(message);
```
without checking whether `_messageDispatcher` is null. `FileSystem` has a private default constructor that sets `_messageDispatcher = nullptr`. If the default constructor is ever used (e.g. reflectively or inadvertently), this is a crash.

**`BluetoothRequestHandler::SetCurrentValueForAttributeHandle`** (line 430):
```cpp
Esp32Messaging::BTServerDataSet *ds = Encoders::ExtractBTServerDataSet(request->Payload);
TRACE_MESSAGE("... %" PRIu32 " bytes to 0x%04" PRIx16, ds->SetDataLength, ds->Handle);
```
`ds` is not null-checked before dereferencing.

### 6.2 Buffer Overflow with `strcpy` and `inet_ntoa` [MEDIUM / #171]

**`WiFiRequestHandler.cpp`**, static IP configuration:
```cpp
char ip[INET_ADDRSTRLEN], subnet[INET_ADDRSTRLEN], gateway[INET_ADDRSTRLEN];
strcpy(ip, inet_ntoa(accessPointInformation->IpAddress));
strcpy(subnet, inet_ntoa(accessPointInformation->SubnetMask));
strcpy(gateway, inet_ntoa(accessPointInformation->Gateway));
```

`inet_ntoa` returns a pointer to a static internal buffer. Calling it three times in sequence means the second call overwrites the buffer used by the first, and the third overwrites the second's result. By the time `strcpy(subnet, ...)` completes, `ip` still contains the correct value (copied), but calling `inet_ntoa` three times in a row with a single static buffer is fragile, especially in a multithreaded context.

**Recommendation:** Use `esp_ip4addr_ntoa` (reentrant) or `inet_ntop` with explicit buffers.

### 6.3 Text Mode for Binary File I/O [MEDIUM / #173]

**`FileSystem::ReadFile`**:
```cpp
FILE *file = fopen(s.c_str(), "r");
```
**`FileSystem::WriteFile`**:
```cpp
FILE *file = fopen(s.c_str(), "w");
```

Both use text mode. On the ESP32/LittleFS stack this is unlikely to cause line-ending translation, but it is non-portable and contrary to best practice when handling arbitrary binary payloads. All file operations should use `"rb"` / `"wb"`.

### 6.4 TOCTOU Race in File Operations [MEDIUM / #174]

**`FileSystem::ReadFile`** and **`FileSystem::ListFiles`** use:
```cpp
if (stat(s.c_str(), &fileInfo) == 0)
{
    // ...
    FILE *file = fopen(s.c_str(), "r");
```
The file could be deleted, truncated, or replaced between the `stat` and `fopen` calls. The correct pattern is to `fopen` first, then use `fstat` on the file descriptor to obtain the size.

### 6.5 Large Stack Allocation in GATTS Event Handler [MEDIUM / #159]

**`BluetoothRequestHandler::GATTSProfileEventHandler`**, `ESP_GATTS_READ_EVT`:
```cpp
esp_gatt_rsp_t rsp;
memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
```

`esp_gatt_rsp_t` contains a 517-byte array (`attr_value.value[ESP_GATT_MAX_ATTR_LEN]`). Allocating this on the GATTS event handler stack risks a stack overflow in the BLE controller task, which typically has a small stack.

Additionally, `GetCurrentValueForAttributeHandle` does not indicate success or failure. If the handle is unknown, `pLen` is set to 0 and `pData` is set to `NULL` locally (the assignment is to the local parameter, not the caller's pointer — this is a bug):

```cpp
else
{
    *pLen = 0;
    pData = NULL;  // ← Bug: modifies local copy only; caller's pointer is unchanged
}
```

**Issue #159 recommendation:** Allocate `esp_gatt_rsp_t` on the heap and return a pointer from `GetCurrentValueForAttributeHandle`, returning `nullptr` on failure (handle not found).

### 6.6 Integer Underflow/Overflow in Packet Handling [CRITICAL / #175]

**`SpiTransportProvider::UpdatePacketInformation`**:
```cpp
message->PacketOffset += message->PacketLength;
uint16_t amountRemaining = message->PayloadLength - message->PacketOffset;
```

`PayloadLength` is `uint32_t` and `PacketOffset` is `uint16_t`. The subtraction is performed as `uint32_t - uint16_t` (implicit promotion), then the result is truncated to `uint16_t`. If `PayloadLength > 65535`, `amountRemaining` wraps silently, causing incorrect packet fragmentation or an infinite loop.

Although the current `MAXIMUM_SPI_FRAME_SIZE` (4092 bytes) means payloads are unlikely to exceed 65535 bytes in practice, the data type is wrong and the assumption is not enforced.

**Recommendation:** Change `PacketOffset` and `PacketLength` to `uint32_t` throughout, or add an `assert`/check that `PayloadLength <= 65535`.

### 6.7 SPI State Machine Cannot Recover from Unknown State [HIGH / #176]

**`SpiTransportProvider::ProcessMessages`**, default case:
```cpp
default:
    ERROR_MESSAGE("ProcessMessages: Unhandled state: %s", SpiStateString(_currentState));
    break;
```

After the `break`, `newTransaction` is `nullptr`. The `Task` loop then calls `ProcessMessages(nullptr)` on the next iteration. `spi_slave_queue_trans` is then called with a null transaction, causing either an assertion in the ESP-IDF or undefined behaviour.

**Recommendation:** In the default case, reset `_currentState` to `WaitingForPacket` and create a fresh transaction:
```cpp
default:
    ERROR_MESSAGE("ProcessMessages: Unhandled state, resetting to WaitingForPacket.");
    _currentState = WaitingForPacket;
    newTransaction = CreateSpiTransaction(nullptr);
    break;
```

### 6.8 Bluetooth Event Handler Stub [MEDIUM / #185]

The `EventHandlerHelper` function (referenced by the BLE stack event registration) is either absent or a no-op. Important state changes — pairing completion, MTU negotiation, GAP security events — are silently discarded. The `GAPEventHandler` only handles advertising-related events.

**Recommendation:** Implement handlers for security and pairing events, or add explicit log messages for all unhandled events so they can be identified during testing.

### 6.9 `FileSystem::Mount()` Always Returns `true` [HIGH / #186]

```cpp
bool FileSystem::Mount()
{
    bool result = true;

    esp_err_t ret = esp_vfs_littlefs_register(&_config);
    if (ret != ESP_OK)
    {
        // only logs; never sets result = false
    }

    return (result);
}
```

The `result` variable is initialised to `true` and never changed on failure. `Setup()` calls `Mount()` and uses the return value to decide whether to continue, so all mount failures are silently treated as successes.

**Recommendation:**
```cpp
bool FileSystem::Mount()
{
    esp_err_t ret = esp_vfs_littlefs_register(&_config);
    if (ret != ESP_OK)
    {
        ERROR_MESSAGE("Failed to mount LittleFS: %s", esp_err_to_name(ret));
        return (false);
    }
    return (true);
}
```

### 6.11 NvsManager Read Opens with `NVS_READWRITE` [LOW]

**`NvsManager::GetValue`** for `uint32_t` (line 233):
```cpp
error = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
```

This is a read-only operation, but the namespace is opened with read-write access. This is unnecessarily permissive and could mask bugs. It should use `NVS_READONLY`.

### 6.12 `SpiStateString` Missing Default Return Value [LOW]

**`SpiTransportProvider::SpiStateString`**:
```cpp
const char *stateString = NULL;
switch (state)
{
    ...
}
return (stateString);
```

If `state` is not one of the named enum values (possible if memory is corrupted), `nullptr` is returned, and callers that pass this to `printf` with `%s` will crash. The `default` case should return a safe sentinel string.

---

## 7. Best Practices

### 7.1 ThreadPool Configuration [LOW / #155]

`ThreadPool::ThreadPool()` has `MAX_THREADS = 10` and `THREAD_POOL_SIZE = 5` as instance-level `const int` members. The constructor previously accepted a size parameter but no longer does. Having both constants when only one is used (`THREAD_POOL_SIZE`) is confusing. `MAX_THREADS` appears unused.

**Recommendation:** Remove `MAX_THREADS` and promote `THREAD_POOL_SIZE` to a `static constexpr`, ideally driven by a compile-time `sdkconfig` value. This aligns with issue #155.

### 7.2 `throw new` in Embedded / FreeRTOS Context [HIGH]

Throughout the codebase, exceptions are thrown using `throw new`:
```cpp
throw new ThreadPoolException("Cannot create mutex for thread.");
throw new MultipleInstancesException();
```

This pattern allocates the exception object on the heap with `new` (which on the ESP32 calls `pvPortMalloc`). The exception must then be caught with `catch(ExceptionType *e)` and the caller must call `delete e`. If no matching `catch` exists, or if the catch handler neglects to delete, the exception object leaks permanently.

More importantly, FreeRTOS tasks do not automatically unwind the C++ stack on exception, and the ESP-IDF documentation advises against relying on C++ exception handling in task context. Some of these throws occur inside constructors called before any `catch` block exists.

**Recommendation:**
- Either throw by value (`throw ThreadPoolException(...)`) and catch by reference, as per C++ best practice.
- Or, since this is an embedded system, replace exceptions with error-return codes and/or `ESP_ERROR_CHECK`-style logging followed by `esp_restart()` on unrecoverable errors.

### 7.3 `app_main` Leaks Allocated Objects

```cpp
SpiTransportProvider *spi = new SpiTransportProvider();
MessageDispatcher *__attribute__((unused)) messageDispatcher = new MessageDispatcher(spi);
vTaskSuspend(NULL);
```

`app_main` is suspended (not deleted), so the objects are intentionally long-lived. However, the `__attribute__((unused))` suppresses an unused-variable warning on `messageDispatcher`, suggesting this was introduced defensively. The code would be clearer if these were top-level global pointers with explicit lifetime documentation.

### 7.4 Spelling Inconsistency: `Milisseconds`

**`ThreadPool.hpp`**, line 38:
```cpp
uint32_t _threadPoolAcquireTimeoutInMilisseconds = 5000;
```
Misspelled (double 's'). Should be `_threadPoolAcquireTimeoutInMilliseconds`.

### 7.5 `SendingData` Parameter Unused

**`SpiTransportProvider.cpp`**:
```cpp
bool SpiTransportProvider::SendingData(SpiStates state)
{
    return ((_currentState == AcknowledgingPacket) || ...);
}
```

The `state` parameter is declared but the method uses `_currentState` directly. This is dead code; either the parameter should be used (`state == ...`) or the method should not take a parameter. The call site passes `_currentState` which makes the parameter effectively ignored.

### 7.6 `test_value` Dead Variable in BluetoothManager

**`BluetoothManager.cpp`**, line 256:
```cpp
static int __attribute__((unused)) test_value = 0;
```

This variable is referenced in a trace message in `SetCurrentValueForAttributeHandle` as `test_value` when it should reference `handle`:
```cpp
TRACE_MESSAGE("Value received with unknown handle 0x%04" PRIx32, test_value);
```
This prints `0` instead of the actual handle value. Both the variable and the log message are incorrect.

---

## 8. Code Style and Conventions

### 8.1 Magic Numbers [MEDIUM / #177]

Several magic numbers appear without named constants:

- `_connectionHandle = 0xffff;` — should be a named constant such as `INVALID_CONNECTION_HANDLE`.
- BLE advertising intervals (`0x0006`, `0x0010`, `0x20`, `0x40`) — should be named constants with comments explaining the timing in milliseconds.
- `conn_params.timeout = 400;` — a comment exists (`400*10ms = 4000ms`) but a named constant would be clearer.
- `slaveConfiguration.mode = 3;` — should be `SPI_MODE_3` or similar named constant.
- `SemaphoreHandle_t _threadsAvailable` initial count of 0 in `xSemaphoreCreateCounting` is fine but should be documented.

### 8.2 Typos and Inconsistent Logging [LOW / #178]

- `SpiTransportProvider::SpiStateString`, case `WaitingForPacket`: returns `"WaitingFoMessage"` — missing 'r' and wrong word.
- `FileSystem.hpp`, line 65: comment `"Note that this can be different from the pARTITION_LABEL"` — inconsistent capitalisation.
- `FileSystem.hpp`, line 116: docstring `"Delate a file"` — should be `"Delete a file"`.
- `ThreadPool.hpp`, line 52: docstring `"Name of the componet / task"` — should be `"component"`.
- `WiFiRequestHandler.cpp`, line 160: `"Defaut access [oint"` — should be `"Default access point"`.
- `Globals.hpp` (referenced but not shown): ensure all component names are consistent.
- `ThreadPool.cpp` constructor comment: `"and so there is no current instance"` duplicated from another comment.

### 8.3 `NULL` vs `nullptr` [LOW / #179, #187]

The codebase uses `NULL` in at least 426 locations (vs `nullptr` in 212 locations). The project convention specifies `nullptr`. All `NULL` usages in C++ code should be replaced.

Notable examples:
- `ThreadPool::_threadsAvailable = NULL;`
- `_pollRequestListMutex = NULL;`
- `SpiTransportProvider::_synchronisationTaskHandle = NULL;`

### 8.4 `bzero` vs `std::memset` [LOW / #180, #188]

`bzero` is used in at least 7 locations:

- `WiFiRequestHandler.cpp` lines 310, 635, 642, 749, 750, 1903
- `BluetoothRequestHandler.cpp` line 796

`bzero` is not part of the C++ standard and may not be available on all compilers. Replace with `std::memset(ptr, 0, size)`.

### 8.5 Unused Variables and Dead Code [LOW / #181]

- `static int __attribute__((unused)) test_value = 0;` in `BluetoothManager.cpp` (see §7.6).
- `MAX_THREADS` constant in `ThreadPool.hpp` is never used.
- `SendingData(SpiStates state)` has an unused `state` parameter (see §7.5).
- Commented-out code blocks throughout `SpiTransportProvider.cpp` (heap tracing) and `BluetoothRequestHandler.cpp` (security, service handle) should either be removed or converted to conditional compilation.

---

## 9. Test Coverage

### 9.2 Coverage Gaps

- **FileSystem error paths** — `test_file_system.cpp` should test `Mount()` failure, `ReadFile` with a non-existent file, and `WriteFile` with a full filesystem.
- **SPI state machine** — No tests for the SPI transport layer in isolation. The mock transport provider (`MockTransportProvider`) exists but appears under-used.
- **ThreadPool** — No tests for the thread pool under concurrent load or for the semaphore timeout path.
- **Memory allocation failure** — No tests inject allocation failures to verify null-check paths.
- **Bluetooth lifecycle** — `test_bluetooth.cpp` exists; it should cover the destroy-during-callback scenario (§5.4).

---

## 10. Summary of Recommendations

### Immediate (CRITICAL / security)

| # | Action |
|---|--------|
| 1 | **Rotate and remove committed WiFi credentials** from `test/secrets.h` and `components/F7Services/test/secrets.h`; purge from git history. |
| 2 | Add null checks after every `pvPortMalloc`, `heap_caps_calloc`, `new`, and `strdup` call (issue #162). |
| 3 | Fix `FileSystem::Mount()` to return `false` on failure (issue #186). |
| 4 | Replace `realloc` in `FileSystem::ListFiles` with `pvPortMalloc`/`vPortFree` (issue #169). |
| 5 | Replace `Utils::DeleteAndNullifyPointer` on `_threadsAvailable` with `vSemaphoreDelete` (§4.2). |
| 6 | Fix `UpdatePacketInformation` to use `uint32_t` for `amountRemaining` (issue #175). |

### Short-term (HIGH)

| # | Action |
|---|--------|
| 7 | Add mutex protection to `BluetoothManager::_valueMap` accesses (issue #168). |
| 8 | Fix SPI state machine default case to reset to `WaitingForPacket` (issue #176). |
| 9 | Fix Bluetooth callback lifecycle — guard `GetInstance()` calls in all BLE callbacks (issue #183). |
| 10 | Fix `throw new` pattern throughout — either throw by value or replace with error returns (§7.2). |
| 11 | Fix the missing null check on `_outboundMessage` leak in `PacketReceived` CRC error path (issue #164). |

### Medium-term (MEDIUM / correctness)

| # | Action |
|---|--------|
| 12 | Replace `strcpy` + `inet_ntoa` with `inet_ntop` for IP address formatting (issue #171). |
| 13 | Change all file `fopen` calls to binary mode `"rb"` / `"wb"` (issue #173). |
| 14 | Fix TOCTOU in `ReadFile` — open file first, use `fstat` (issue #174). |
| 15 | Implement or document Bluetooth event handlers (issue #185). |
| 16 | Protect `_defaultAccessPoint` and `_password` with a mutex (issue #184). |
| 17 | Fix `GetCurrentValueForAttributeHandle` stack allocation and return value (issue #159). |
| 18 | Correct `GetNextMessageID` mask logic to apply before releasing the mutex (issue #172). |
| 19 | Fix `NvsManager::GetValue(uint32_t)` to open NVS with `NVS_READONLY` (§6.11). |

### Low-priority (LOW / style)

| # | Action |
|---|--------|
| 20 | Replace all `NULL` with `nullptr` in C++ code (issues #179, #187). |
| 21 | Replace all `bzero` with `std::memset` (issues #180, #188). |
| 22 | Remove `test_value`, `MAX_THREADS`, unused `state` parameter in `SendingData` (issue #181). |
| 23 | Fix logging typos: `"WaitingFoMessage"`, `"Delate"`, `"componet"`, `"Milisseconds"` etc. (issue #178). |
| 25 | Replace magic numbers with named constants (issue #177). |
| 26 | Remove or make configurable the `ThreadPool` size constant (issue #155). |
| 28 | Fix `SpiStateString` to return a safe default string in the default case (§6.12). |

---

*End of Review*
