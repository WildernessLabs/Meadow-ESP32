/*
 *  WiFiRequestHandlerTestFriend.cpp
 *
 *  Implementation of the test methods for the WiFiRequestHandler class.
 */

#include "sdkconfig.h"

#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <string.h>
#include "unity.h"

#include "MockMessageDispatcher.hpp"
#include "MockTransportProvider.hpp"
#include "Esp32Messaging.hpp"
#include "Encoders.hpp"
#include "SharedEnums.hpp"
#include "WiFiRequestHandler.hpp"

class WiFiRequestHandlerTestFriend
{
    private:
        WiFiRequestHandler *wifiRequestHandler;

    public:
        WiFiRequestHandlerTestFriend(WiFiRequestHandler *wifiRequestHandler)
        {
            wifiRequestHandler = wifiRequestHandler;
        }

        void Setup()
        {
            wifiRequestHandler->Setup();
        }

        void ScanForAccessPoints(Message *message)
        {
            wifiRequestHandler->ScanForAccessPoints(message);
        }
};

