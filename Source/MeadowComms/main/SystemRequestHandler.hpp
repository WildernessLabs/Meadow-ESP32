/*
 * SystemRequestHandler.hpp
 *
 *  Created on: 3 Feb 2019
 *      Author: mark
 */

#ifndef MAIN_SYSTEMREQUESTHANDLER_HPP_
#define MAIN_SYSTEMREQUESTHANDLER_HPP_

#include "RequestHandlerBase.hpp"

class SystemRequestHandler : public RequestHandlerBase
{
public:
    SystemRequestHandler();
    ~SystemRequestHandler();

    void Setup();
};


#endif /* MAIN_SYSTEMREQUESTHANDLER_HPP_ */
