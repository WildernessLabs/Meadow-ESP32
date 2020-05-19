/*
 *  Mapping.hpp
 *
 *  Define the class that will allow the mapping of values on the STM32 to
 *  those on the ESP32 and vice-versa.
 * 
 *  This file also contains the mappings.
 */
#include <sys/socket.h>
#include "Mapping.hpp"

/*
 *  Map the STM32 address family codes to ESP32 address family code.
 * 
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_addressFamilies[] =
{
    { 1, AF_UNSPEC },
    { 2, AF_INET },
    { 10, AF_INET6 },
#if LWIP_IPV6
    { 10, AF_INET6 }    /* AF_INET6 */
#else /* LWIP_IPV6 */
    { 1, AF_INET6 }     /* AF_UNSPEC */
#endif
};

/*
 *  Map the STM32 socket options to ESP32 socket options.
 * 
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketOptions[] = 
{
    { 0, SO_ACCEPTCONN },
    { 1, SO_BROADCAST },
    { 2, SO_DEBUG },
    { 3, SO_DONTROUTE },
    { 4, SO_ERROR },
    { 5, SO_KEEPALIVE },
    { 6, SO_LINGER },
    { 7, SO_OOBINLINE },
    { 8, SO_RCVBUF },
    { 9, SO_RCVLOWAT },
    { 10, SO_RCVTIMEO },
    { 11, SO_REUSEADDR },
    { 12, SO_SNDBUF },
    { 13, SO_SNDLOWAT },
    { 14, SO_SNDTIMEO },
    { 15, SO_TYPE }
};

/*
 *  Map the STM32 protocol family codes to ESP32 protocol family code.
 * 
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_protocolFamilies[] = 
{
    { 2, PF_INET },
    { 10, PF_INET6 },
    { 1, PF_UNSPEC }
};

/*
 *  Map the STM32 socket types to ESP32 socket types.
 * 
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketTypes[] = 
{
    { 1, SOCK_STREAM },
    { 2, SOCK_DGRAM },
    { 3, SOCK_RAW }
};

/*
 *  Map the STM32 socket types to ESP32 socket types.
 * 
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_socketLevels[] = 
{
    { 0, SOL_SOCKET }
};

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  Default constructor for the mapping system.
 */
Mapping::Mapping()
{
}

/**
 *  Default destructor for the mapping system.
 */
Mapping::~Mapping()
{
}

/*
 * ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Convert the specified value for the specified platform.
 * 
 *  This method will take a STM constant and convert it into the correspnding ESP
 *  constant (or the reverse depending upon the requested direction).
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param type
 *      Direction of the mapping.
 *
 *  @param value
 *      Value to look for.
 *
 *  @returns
 *      Mapped value (if found) or -1 if there is a problem.
 */
int32_t Mapping::GetValue(Mapping::MappingTable mapping, MappingDirection type, int32_t value)
{
    int_mapping_t *table = NULL;
    size_t size = 0;
    switch (mapping)
    {
        case MappingTable::AddressFamily:
            table = _addressFamilies;
            size = sizeof(_addressFamilies) / sizeof(int_mapping_t);
            break;
        case MappingTable::SocketOptions:
            table = _socketOptions;
            size = sizeof(_socketOptions) / sizeof(int_mapping_t);
            break;
        case MappingTable::ProtocolFamilies:
            table = _protocolFamilies;
            size = sizeof(_protocolFamilies) / sizeof(int_mapping_t);
            break;
        case MappingTable::SocketTypes:
            table = _socketTypes;
            size = sizeof(_socketTypes) / sizeof(int_mapping_t);
            break;
        case MappingTable::SocketLevel:
            table = _socketLevels;
            size = sizeof(_socketLevels) / sizeof(int_mapping_t);
            break;
    }

    if (table != NULL)
    {
        for (int index = 0; index < size; index++)
        {
            if (type == Mapping::StmToEsp)
            {
                if (table[index].stm == value)
                {
                    return(table[index].esp);
                }
            }
            else
            {
                if (table[index].esp == value)
                {
                    return(table[index].stm);
                }
            }            
        }
    }
    return(-1);
}

/**
 *  Get the ESP constant given the value of the STM constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @returns
 *      Mapped value (if found) or -1 if there is a problem.
 */
int32_t Mapping::GetEspValue(Mapping::MappingTable mapping, int32_t value)
{
    return(GetValue(mapping, Mapping::StmToEsp, value));
}

/**
 *  Get the ESP32 constant given the specified STM constant.
 *
 *  @param mapping
 *      Which type of mapping should be used (i.e. which mapping table).
 *
 *  @param value
 *      Value to look for.
 *
 *  @returns
 *      Mapped value (if found) or -1 if there is a problem.
 */
int32_t Mapping::GetStmValue(Mapping::MappingTable mapping, int32_t value)
{
    return(GetValue(mapping, Mapping::EspToStm, value));
}

