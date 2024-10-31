/*
 *  WiFiRequestHandlerTestFriend.hpp
 *
 *  Definition of the WiFiRequestHandler class.
 */

#include "sdkconfig.h"

#include "WiFiRequestHandler.hpp"

/**
 * @brief Friend class for the WiFiRequestHandler.
 * 
 * This class is a friend of the WiFiRequestHandler class.  It is necessary to have this
 * class present un the unit testing system to allow access to the private and protected
 * methods and properties in the WiFiRequestHandler class.
 * 
 * The friend relationship is controlled in the WiFiRequestHandler by the UNIT_TESTS
 * definition that is only created in this test application.
 */
class WiFiRequestHandlerTestFriend
{
private:
    /**
     * @brief Construct a new WiFiRequestHandlerTestFriend object.
     * 
     * @param wifiRequestHandler Instance of the WiFiRequestHandler class under test.
     */
    WiFiRequestHandler *wifiRequestHandler;

public:
    WiFiRequestHandlerTestFriend(WiFiRequestHandler *wifiRequestHandler);

    /**
     * @brief Setup the WiFiRequestHandler.
     */
    void Setup();

    /**
     * @brief Use the WiFiRequestHandler to scan for access points.
     * 
     * @param message Message containing the request for the scan for access points.
     */
    void ScanForAccessPoints(Message *message);

    /**
     * @brief Use the WiFiRequestHandler to connect to an access point.
     * 
     * @param message Message containing the request to connect to an access point.
     */
    void ConnectToAccessPoint(Message *message);

    /**
     * @brief Set the automatic reconnect flag indicating if the system should automatically
     * reconnect to the access point.
     * 
     * @param value True if the system should automatically reconnect, false otherwise.
     * @return StatusCodes::StatusCodes StatusCodes::CompletedOk if the value was set, an error code otherwise.
     */
    StatusCodes::StatusCodes SetAutomaticReconnect(bool value);
};