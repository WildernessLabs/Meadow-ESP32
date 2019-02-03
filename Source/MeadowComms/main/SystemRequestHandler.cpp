/*
 * SystemRequestHandler.cpp
 *
 *  Created on: 3 Feb 2019
 *      Author: mark
 */

#include "SystemRequestHandler.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                      Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Default constructor for the system request handler class.
 */
SystemRequestHandler::SystemRequestHandler()
{

}

/*
 *      Default destructor for the system request handler class.
 */
SystemRequestHandler::~SystemRequestHandler()
{

}

/*
 * ----------------------------------------------------------------------------
 *
 *                          Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/*
 *      Get the name of this task.
 */
const char *GetTaskName()
{
    return("SystemTask");
}

/*
 *      Setup the system request handler.
 */
void SystemRequestHandler::Setup()
{

}
