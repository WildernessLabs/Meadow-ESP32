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

#include <vector>
#include <string>

using namespace std;

/**
 *  @brief Bluetooth request handler object.
 *
 *  This class deals with dispatching Bluetooth requests.
 */
class BluetoothRequestHandler : public RequestHandlerBase
{
private:
    /**
     *  Event group bits.
     *
     *  The Event group bits are used to synchronise the requests and the event processing
     *  system.  They effectively convert asynchronous requests and responses into synchronous
     *  method calls.
     */

    /**
     *  @brief BT_READY_BIT indicates if the Bluetooth processing system can process a
     *  request.  It is used to synchronise requests from the STM32.
     */
    static const int BT_READY_BIT = BIT0;

    /**
     * @brief Instance ID for the Bluetooth service.
     */
    static const int INSTANCE_ID = 0;

    /**
     * @brief Size of the advertising service UUID.
     */
    static const int ADVERTISING_SERVICE_UUID_LENGTH = 32;

    /**
     *  @brief Stack size for the Bluetooth request handler task.
     */
    static constexpr uint32_t TASK_STACK_SIZE = 8192;

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
     */
    static uint16_t _connectionHandle;

    /**
     * @brief Hold the manufacturer data for these service.
     */
    static uint8_t _manufacturerData[2];

    static bool _advertisingDataConfigured;

    static uint8_t _dummyAdvertisingServiceUuid[ADVERTISING_SERVICE_UUID_LENGTH];

    /**
     * @brief Has the response data been configured?
     */
    static bool _responseDataConfigured;

    /**
     * @brief Table of attributes in the GATT database.
     */
    esp_gatts_attr_db_t *_attribute_table;

    /**
     * @brief Number of elements in the attribute table.
     */
    uint16_t _attributeTableLength;

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
     *  This instance method contains the event loop code and processes the event for the
     *  EventLoop method.
     */
    void EventHandlerHelper(esp_event_base_t, int32_t, void *);

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
     * @brief Get the Handles and send them to the STM32.
     */
    void GetHandles(Message * const request);

    /**
     * @brief Stop the Bluetooth stack.
     */
    void Stop(Message * const request);

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
    virtual ~BluetoothRequestHandler();

    static BluetoothManager _manager;

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
    void Setup();

    /**
     *  This is the event handler called by FreeRTOS.  It merely passes the event on to
     *  an internal private event handler within the class.
     *
     *  This method is static to allow it to be accessed from the C APIs.  This method will
     *  pass the processing on to the instance method in order to allow the event loop to
     *  have access to the class level variables, not just the static variable.
     */
    static void EventHandler(void *, esp_event_base_t, int32_t, void *);

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
