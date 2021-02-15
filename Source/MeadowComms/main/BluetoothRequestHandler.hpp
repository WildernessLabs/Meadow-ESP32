/*
 *  BluetoothRequestHandler.hpp
 *
 *  Bluetooth request handler will deal with incoming bluetooth requests.
 */

#ifndef _BLUETOOTH_REQUEST_HANDLER_HPP_
#define _BLUETOOTH_REQUEST_HANDLER_HPP_

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
#include "BluetoothConfig.hpp"

#include <vector>
#include <string>

#define BT_FULL_LOGGING

using namespace std;

/**
 *  @brief Bluetooth request handler object.
 *
 *  This class deals with dispatching Bluetooth requests.
 */
class BluetoothRequestHandler : public RequestHandlerBase
{
private:
    struct Uuid16
    {
        uint8_t data[16];
    };

    struct BluetoothService
    {
        std::string Name;
        Uuid16 Uuid;
    };

    struct BluetoothCharacteristic
    {
        std::string Name;

    };

    struct BluetoothDescriptor
    {
        std::string Name;
        
    };

    vector<BluetoothService> _services;

    /**
     *  Event group bits.
     * 
     *  The Event group bits are used to synchronise the requests and the event processing
     *  system.  They effectively convert asynchronous requests and responses into synchronous
     *  method calls.
     */
    
    /**
     *  BT_READY_BIT indicates if the Bluetooth processing system can process a
     *  request.  It is used to synchronise requests
     */
    const int BT_READY_BIT = BIT0;

    /**
     *  Event group containing 24-bits used to indicate the state of the WiFi connection.
     */
    EventGroupHandle_t _xBtEventGroup = nullptr;

    /**
     *  Name of the FreeRTOS task for the Bluetooth system.
     */
    const char *_taskName = "BluetoothTask";

    /**
     *  Determine if this class is instantiated already (i.e. this should be a singleton).
     */
    static bool _instantiated;

    /**
     *  This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *) override;

    /**
     *  This instance method contains the event loop code and processes the event for the
     *  EventLoop method.
     */
    void EventHandlerHelper(esp_event_base_t, int32_t, void *);

    const int BT_GATT_MTU = 500;
    const int MEADOW_PROFILE_APP_ID = 0x55;

    /*
     *  Private constructor and destructor.
     */
    BluetoothRequestHandler();
    ~BluetoothRequestHandler();

    void StartStack(Message *msg);
    void TeardownStack(Message *msg);

    // this will add a characteristic ,and if required its containing service 
    int _attribute_database_length = 0;
    esp_gatts_attr_db_t *_attribute_database;
    void AddCharacteristic(Message *msg);

public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /*
     *  Construct a new Bluetooth request handler object with reference to a
     *  Message Dispatcher object.
     */
    explicit BluetoothRequestHandler(MessageDispatcher *);

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

    static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
    static void gatts_profile_event_handler(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t *);

    static BluetoothConfig Config;

};

#endif /* _BLUETOOTH_REQUEST_HANDLER_HPP_ */
