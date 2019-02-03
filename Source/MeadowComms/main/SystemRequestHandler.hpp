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
    /*
     *      Constructor(s) and destructor.
     */
    SystemRequestHandler();
    ~SystemRequestHandler();

    /*
     *      Public methods.
     */
    void Setup();

    /*
     *      Getters and setters.
     */
    const char *GetTaskName();
};


#endif /* MAIN_SYSTEMREQUESTHANDLER_HPP_ */
