/*
 * Bluetooth.hpp
 *
 *  Created on: 24 Jan 2019
 *      Author: mark
 */

#ifndef MAIN_BLUETOOTHREQUESTHANDLER_HPP_
#define MAIN_BLUETOOTHREQUESTHANDLER_HPP_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "stdlib.h"

#include "RequestHandlerBase.hpp"

class Bluetooth : public RequestHandlerBase
{
public:
    Bluetooth();
    ~Bluetooth();

    void Setup();

    char *GetTaskName();
};

extern Bluetooth *bluetooth;


#endif /* MAIN_BLUETOOTHREQUESTHANDLER_HPP_ */
