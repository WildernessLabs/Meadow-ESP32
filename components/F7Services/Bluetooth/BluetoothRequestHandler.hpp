/**
 * @file BluetoothRequestHandler.hpp
 * @author Mark Stevens
 * @brief Bluetooth request handler will deal with incoming bluetooth requests.
 * @date 2025-01-27
 *
 * @copyright Copyright (c) 2025 Wilderness Labs
 */

#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "stdlib.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "RequestHandlerBase.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "MessageDispatcher.hpp"
#include "SharedEnums.hpp"

#include "BluetoothManager.hpp"
#include "BluetoothPairing.hpp"

#include <atomic>
#include <vector>
#include <string>

/**
 *  @brief Bluetooth request handler object.
 *
 *  This class deals with dispatching Bluetooth requests.
 *
 *  Bluetooth events do not come through the ESP-IDF event loop, as WiFi events do, so this class
 *  registers no handler with esp_event_handler_register.  The Bluedroid stack reports everything
 *  through two callbacks, GAPEventHandler and GATTSProfileEventHandler, registered when the
 *  service starts, and every Bluetooth event is handled in those (issues #185 and #392).
 */
class BluetoothRequestHandler : public RequestHandlerBase
{
    /**
     * @brief Test access to the generated advertising data (issue #345).
     */
    friend class TestBluetoothAdvertising;

    /**
     * @brief Test access to the prepared write handling (issue #299).
     */
    friend class TestBluetoothWrites;

    /**
     * @brief Test access to starting and stopping the stack (issue #296).
     */
    friend class TestBluetoothUnwind;

    /**
     * @brief Test access to pairing and bonding (issue #398).
     */
    friend class TestBluetoothPairing;

    /**
     * @brief Test access to the attribute table and the handles reported from it (issue #636).
     */
    friend class TestBluetoothHandles;

private:
    /**
     *  Event group bits.
     *
     *  The Event group bits are used to synchronise the requests and the event processing
     *  system.  They effectively convert asynchronous requests and responses into synchronous
     *  method calls.
     */

    /**
     *  @brief Set by the GATT event handler once the service has actually started.
     *
     *  The attribute table is created and the service started asynchronously, after
     *  StartBluetoothService() has returned, so Start() waits for this bit or for
     *  BT_SERVICE_START_FAILED_BIT before it answers the STM32.
     */
    static const int BT_SERVICE_STARTED_BIT = BIT1;

    /**
     *  @brief Set by the GATT event handler when registering the application, creating the
     *  attribute table or starting the service fails.
     */
    static const int BT_SERVICE_START_FAILED_BIT = BIT2;

    /**
     *  @brief Milliseconds Start() waits for the service to start or fail before it gives up.
     */
    static constexpr uint32_t SERVICE_START_TIMEOUT = 5000;

    /**
     *  @brief Set by the GAP event handler when the stack has removed a bond.
     *
     *  Removing a bond finishes asynchronously, so RemoveBond and RemoveAllBonds wait for this bit
     *  or BT_BOND_REMOVE_FAILED_BIT before they answer the STM32.
     */
    static const int BT_BOND_REMOVED_BIT = BIT3;

    /**
     *  @brief Set by the GAP event handler when the stack could not remove a bond.
     */
    static const int BT_BOND_REMOVE_FAILED_BIT = BIT4;

    /**
     *  @brief Milliseconds to wait for the stack to remove one bond.
     */
    static constexpr uint32_t BOND_REMOVE_TIMEOUT = 2000;

    /**
     * @brief Instance ID for the Bluetooth service.
     */
    static const int INSTANCE_ID = 0;

    /**
     * @brief Largest legacy advertising packet, and largest scan response, in bytes.
     */
    static constexpr uint32_t MAXIMUM_ADVERTISING_DATA_LENGTH = 31;

    /**
     *  @brief Stack size for the Bluetooth request handler task.
     */
    static constexpr uint32_t TASK_STACK_SIZE = 8192;

    /**
     * @brief Invalid connection handle value.
     */
    static constexpr uint16_t INVALID_CONNECTION_HANDLE = 0xffff;

    /**
     *  @brief Event group containing 24-bits used to indicate the state of the WiFi connection.
     */
    EventGroupHandle_t _xBtEventGroup = nullptr;

    /**
     @brief *  Name of the FreeRTOS task for the Bluetooth system.
     */
    const char *_taskName = "BluetoothTask";

    /**
     *  Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /**
     * @brief Service handle for the Bluetooth service.
     */
    uint16_t _serviceHandle = -1;

    /**
     * @brief Handle for the GATTS interface.
     */
    static esp_gatt_if_t _gattsInterfaceHandle;

    /**
     * @brief The handle that is assigned a connection when a device connects over Bluetooth.
     *
     *  Set by the Bluedroid callbacks on connect and disconnect, and read on the Bluetooth
     *  task when a value changes, so it is atomic and read once per use (issue #168).
     */
    static std::atomic<uint16_t> _connectionHandle;

    /**
     * @brief Hold the manufacturer data for these service.
     */
    static uint8_t _manufacturerData[2];

    static bool _advertisingDataConfigured;

    /**
     * @brief Notifications sent to a client, counted for NotificationsSent.
     */
    static std::atomic<uint32_t> _notificationsSent;

    /**
     * @brief UUID of the configured service, as advertised.
     *
     *  Bluedroid takes each advertised UUID as a full 128-bit value and shortens it to 16
     *  bits itself when it is built on the Bluetooth base UUID, as every service here is.
     *  This used to hold two placeholder UUIDs from the ESP-IDF examples, which with the
     *  name and TX power did not fit in the 31-byte advertising packet (issue #345).
     */
    uint8_t _advertisingServiceUuid[ESP_UUID_LEN_128];

    /**
     * @brief Has the response data been configured?
     */
    static bool _responseDataConfigured;

    /**
     * @brief Table of attributes in the GATT database.
     *
     *  Initialised to nullptr so that the first call to GenerateAttributeTable() can
     *  unconditionally delete[] whatever this currently holds before replacing it -
     *  delete[] on a null pointer is a no-op, but on an uninitialised one it is undefined
     *  behaviour.
     */
    esp_gatts_attr_db_t *_attribute_table = nullptr;

    /**
     * @brief Number of elements in the attribute table.
     */
    uint16_t _attributeTableLength = 0;

    /**
     * @brief Advertising data content, according to "Supplement to the Bluetooth Core Specification".
     */
    esp_ble_adv_data_t _advertisingData;

    /**
     * @brief Advertising parameters.
     */
    esp_ble_adv_params_t _advertisingParameters;

    /**
     * @brief Advertising response data.
     */
    esp_ble_adv_data_t _scanResponseData;

    /**
     *  This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *) override;

    /**
     * @brief Maximum size of the transmission unit.  BLE supports up to 512 bytes.
     */
    const int BT_GATT_MTU = 500;

    /**
     * @brief Application ID for the Meadow Profile.
     */
    const int MEADOW_PROFILE_APP_ID = 0x55;

    /*
     *  Private constructor.
     */
    BluetoothRequestHandler();

    /**
     * @brief Start the bluetooth service from the attribute graph sent be the managed code.
     */
    void Start(Message * const request);

    /**
     * @brief Start the Bluetooth stack.
     */
    bool StartBluetoothService();

    /**
     * @brief Tell Start() whether the asynchronous part of starting the service succeeded.
     *
     * @param started true once the service has started, false if a step on the way failed.
     */
    void SignalServiceStart(const bool started);

    /**
     * @brief Give the stack the pairing and bonding settings.
     *
     *  Every setting is checked: a stack that refused one would pair with weaker security than
     *  the STM32 asked for, so the service does not start (issue #398).
     *
     * @param security Settings to give the stack.
     *
     * @return true if the stack took every setting.
     */
    static bool ApplySecurityParameters(const BluetoothSecurityConfiguration &security);

    /**
     * @brief Tell RemoveBond whether the stack removed the bond.
     *
     * @param removed true if the bond was removed.
     */
    void SignalBondRemoved(const bool removed);

    /**
     * @brief Answer a PasskeyRequestEvent with the passkey the user entered (PasskeyReply).
     *
     * @param request Request from the STM32, a BTPairingReply.
     */
    void PasskeyReply(Message * const request);

    /**
     * @brief Answer a NumericComparisonRequestEvent (NumericComparisonReply).
     *
     * @param request Request from the STM32, a BTPairingReply.
     */
    void NumericComparisonReply(Message * const request);

    /**
     * @brief Pass the STM32's answer to a question about pairing to the stack.
     *
     * @param request Request from the STM32, a BTPairingReply.
     * @param reply Kind of reply the request is.
     */
    void ReplyToPairing(Message * const request, const BluetoothPairing::PendingReply reply);

    /**
     * @brief Send the STM32 the addresses of the bonded devices (GetBondedDevices).
     *
     * @param request Request from the STM32.
     */
    void GetBondedDevices(Message * const request);

    /**
     * @brief Remove the bond with one device (RemoveBond).
     *
     * @param request Request from the STM32, a BTDeviceAddress.
     */
    void RemoveBond(Message * const request);

    /**
     * @brief Remove the bond with every device (RemoveAllBonds).
     *
     * @param request Request from the STM32.
     */
    void RemoveAllBonds(Message * const request);

    /**
     * @brief Ask the stack to remove one bond and wait for it to finish.
     *
     * @param address Device whose bond is to be removed.
     *
     * @return CompletedOk, BluetoothDeviceNotBonded, Timeout or Failure.
     */
    StatusCodes::StatusCodes RemoveOneBond(const esp_bd_addr_t address);

    /**
     * @brief Get the devices that are bonded with this one.
     *
     * @param devices Set to the bonded devices.
     *
     * @return true if the stack listed them, false if it could not.
     */
    static bool ListBondedDevices(std::vector<esp_ble_bond_dev_t> &devices);

    /**
     * @brief Is this device bonded?
     *
     * @param address Address to look for in the bond list.
     *
     * @return true if it is in the bond list.
     */
    static bool IsBonded(const esp_bd_addr_t address);

    /**
     * @brief Answer a client's request to pair, as the security settings allow.
     *
     * @param address Client asking to pair.
     */
    static void OnSecurityRequest(esp_bd_addr_t address);

    /**
     * @brief Tell the STM32 about a step in pairing that needs the user.
     *
     * @param event PasskeyDisplayEvent, PasskeyRequestEvent or NumericComparisonRequestEvent.
     * @param address Client being paired with.
     * @param passkey Number to show the user, 0 when one is being asked for.
     */
    static void RaisePairingEvent(const BluetoothFunction::BluetoothFunction event, const esp_bd_addr_t address, const uint32_t passkey);

    /**
     * @brief Act on the end of pairing, or of encryption with a bonded client, and tell the STM32.
     *
     * @param complete What the stack reported.
     */
    static void OnAuthenticationComplete(const esp_ble_auth_cmpl_t &complete);

    /**
     * @brief Act on a client connecting.
     *
     * @param address Client that has connected.
     */
    static void OnClientConnected(esp_bd_addr_t address);

    /**
     * @brief Act on the client disconnecting: keep its subscriptions if it is bonded, then clear them.
     */
    static void OnClientDisconnected();

    /**
     * @brief Read the value of every Client Characteristic Configuration descriptor from the stack.
     *
     * @return The values, in the order of the descriptors in the service graph.
     */
    static std::vector<BluetoothPairing::ClientConfiguration> ReadClientConfigurations();

    /**
     * @brief Give a bonded client back the subscriptions it had when it last disconnected.
     *
     * @param address Address the client is bonded with.
     */
    static void RestoreClientConfigurations(const esp_bd_addr_t address);

    /**
     * @brief Get the Handles and send them to the STM32.
     */
    void GetHandles(Message * const request);

    /**
     * @brief Release the attribute table and set its length back to 0.
     *
     *  Called wherever the service graph the table was built from is deleted, and only while
     *  the Bluetooth stack is down, as the stack reads the table while it is running.
     */
    void ReleaseAttributeTable();

    /**
     * @brief Stop the Bluetooth stack.
     */
    void Stop(Message * const request);

    /**
     * @brief Take down whatever part of the Bluetooth stack is up.
     *
     * @return true if everything that was up has been taken down.
     */
    bool TearDownStack();

    /**
     * @brief Shutdown the Bluetooth stack.
     */
    bool StopBluetoothService();

    /**
     * @brief Set the current value for the specified attribute handle.
     */
    void SetCurrentValueForAttributeHandle(Message * const request);

    /**
     * @brief Write request event handler (this has to be static as it is called from C APIs).
     */
    static void OnDataWriteRequest(const uint16_t handle, uint8_t * const data, const uint16_t dataLength);

    /**
     * @brief Add one piece of a prepared (long) write to the value being collected.
     *
     *  A client writing a value longer than one packet sends it as prepared writes, each
     *  giving its offset, and then asks for them to be executed.  The pieces are collected
     *  here and delivered to the STM32 as one write by ExecuteWrite (issue #299).  One
     *  attribute is collected at a time.
     *
     * @param handle Attribute being written.
     * @param offset Offset of this piece in the value.
     * @param value Bytes of this piece.
     * @param length Number of bytes in this piece.
     *
     * @return ESP_GATT_OK if the piece was kept, otherwise the status to send the client.
     */
    static esp_gatt_status_t PrepareWrite(const uint16_t handle, const uint16_t offset, const uint8_t * const value, const uint16_t length);

    /**
     * @brief Finish a prepared write: deliver the value collected to the STM32, or discard it.
     *
     * @param execute true to deliver the value, false if the client cancelled the write.
     */
    static void ExecuteWrite(const bool execute);

    /**
     * @brief Discard any prepared write that has not been executed.
     */
    static void DiscardPreparedWrite();

    /**
     * @brief Set every client configuration back to 0, no notifications or indications.
     */
    static void ResetClientConfigurations();

    /**
     * @brief Value being collected from prepared writes, or nullptr if none is in progress.
     */
    static uint8_t *_preparedWriteValue;

    /**
     * @brief Attribute the prepared write is for.
     */
    static uint16_t _preparedWriteHandle;

    /**
     * @brief Length of the value collected so far: the end of the furthest piece.
     */
    static uint16_t _preparedWriteLength;

    /*
     *  Construct a new Bluetooth request handler object with reference to a
     *  Message Dispatcher object.
     */
    explicit BluetoothRequestHandler(IMessageDispatcher *);

    /**
     * @brief Get a pointer to this instance of the BluetoothRequestHandler.
     *
     * @return BluetoothRequestHandler* Pointer to this instance of the BluetoothRequestHandler.
     */
    static BluetoothRequestHandler *GetInstance();

    esp_gatts_attr_db_t *GetAttributeTable();
    esp_ble_adv_data_t *GetAdvertisingData();
    esp_ble_adv_data_t *GetScanResponseData();
    esp_ble_adv_params_t *GetAdvertisingParameters();
    uint16_t GetAttributeTableLength();
    void GenerateAdvertisingData();
    void GenerateAttributeTable();
    int GetHandleCount();

public:
    virtual ~BluetoothRequestHandler() override;

    static BluetoothManager _manager;

    /**
     * @brief Replies the STM32 owes for a pairing in progress, and bonded clients' subscriptions.
     */
    static BluetoothPairing _pairing;

    /**
     * @brief Has the connected client enabled notifications for the characteristic with this value handle?
     *
     *  Read back from the stack, which holds the value the client wrote to the
     *  characteristic's Client Characteristic Configuration descriptor (issue #572).
     *
     * @param valueHandle Handle of the characteristic's value.
     *
     * @return true if the client set the notification bit.
     */
    static bool NotificationsEnabled(const uint16_t valueHandle);

    /**
     * @brief Number of notifications sent to a client since the board started.
     *
     * @return The count.
     */
    static uint32_t NotificationsSent()
    {
        return (_notificationsSent.load());
    }

    /**
     * @brief This instance (used to ensure only one instance is created).
     */
    static BluetoothRequestHandler *_instance;

    /**
     * @brief Get a pointer to this instance of the BluetoothRequestHandler.
     *
     * @param d Message dispatcher that can be used to send messages to the STM32.
     *
     * @return BluetoothRequestHandler* Pointer to this instance of the BluetoothRequestHandler.
     */
    static BluetoothRequestHandler *GetInstance(IMessageDispatcher *d);

    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /*
     *  Provide and additional setup operations not covered by the constructors.
     */
    void Setup() override;

    /**
     * @brief Prepare the Bluetooth interface for deep sleep.
     */
    static StatusCodes::StatusCodes PrepareForDeepSleep();

    /**
     * @brief Event handler for the GAP system (registered through esp_ble_gap_register_callback).
     */
    static void GAPEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

    /**
     * @brief Event handler for the GATTS system (registered through esp_ble_gatts_register_callback).
     */
    static void GATTSProfileEventHandler(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t *);

    /**
     * @brief Get the MAC address for the Bluetooth interface.
     */
    static void GetMacAddress(uint8_t * const macAddress);
};
