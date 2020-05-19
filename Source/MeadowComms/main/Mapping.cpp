/*
 *  Mapping.hpp
 *
 *  Define the class that will allow the mapping of values on the STM32 to
 *  those on the ESP32 and vice-versa.
 * 
 *  This file also contains the mappings.
 */
#include <sys/socket.h>
#include "errno.h"
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
 *  Map the STM32 error codes to ESP32 error codes.
 * 
 *  STM value is first, ESP value second.
 */
Mapping::int_mapping_t Mapping::_errno[] = 
{
    { 1, EPERM },
    { 2, ENOENT },
    { 3, ESRCH },
    { 4, EINTR },
    { 5, EIO },
    { 6, ENXIO },
    { 7, E2BIG },
    { 8, ENOEXEC },
    { 9, EBADF },
    { 10, ECHILD },
    { 11, EAGAIN },
    { 11, EWOULDBLOCK },
    { 12, ENOMEM },
    { 13, EACCES },
    { 14, EFAULT },
//    { 15, ENOTBLK },
    { 16, EBUSY },
    { 17, EEXIST },
    { 18, EXDEV },
    { 19, ENODEV },
    { 20, ENOTDIR },
    { 21, EISDIR },
    { 22, EINVAL },
    { 23, ENFILE },
    { 24, EMFILE },
    { 25, ENOTTY },
    { 26, ETXTBSY },
    { 27, EFBIG },
    { 28, ENOSPC },
    { 29, ESPIPE },
    { 30, EROFS },
    { 31, EMLINK },
    { 32, EPIPE },
    { 33, EDOM },
    { 34, ERANGE },
    { 35, ENOMSG },
    { 36, EIDRM },
    /*
    { 37, ECHRNG },
    { 38, EL2NSYNC },
    { 39, EL3HLT },
    { 40, EL3RST },
    { 41, ELNRNG },
    { 42, EUNATCH },
    { 43, ENOCSI },
    { 44, EL2HLT },
    */
    { 45, EDEADLK },
    { 46, ENOLCK },
    /*
    { 50, EBADE },
    { 51, EBADR },
    { 52, EXFULL },
    { 53, ENOANO },
    { 54, EBADRQC },
    { 55, EBADSLT },
    { 56, EDEADLOCK },
    { 57, EBFONT },
    */
    { 60, ENOSTR },
    { 61, ENODATA },
    { 62, ETIME },
    { 63, ENOSR },
    { 64, ENOENT },
    /*
    { 65, ENOPKG },
    { 66, EREMOTE },
    */
    { 67, ENOLINK },
    /*
    { 68, EADV },
    { 69, ESRMNT },
    { 70, ECOMM },
    */
    { 71, EPROTO },
    { 74, EMULTIHOP },
    /*
    { 75, ELBIN },
    { 76, EDOTDOT },
    */
    { 77, EBADMSG },
    { 79, EFTYPE },
    /*
    { 80, ENOTUNIQ },
    { 81, EBADFD },
    { 82, EREMCHG },
    { 83, ELIBACC },
    { 84, ELIBBAD },
    { 85, ELIBSCN },
    { 86, ELIBMAX },
    { 87, ELIBEXEC },
    { 88, ENOSYS },
    { 89, ENMFILE },
    */
    { 90, ENOTEMPTY },
    { 91, ENAMETOOLONG },
    { 92, ELOOP },
    { 95, EOPNOTSUPP },
    { 96, EPFNOSUPPORT },
    { 104, ECONNRESET },
    { 105, ENOBUFS },
    { 106, EAFNOSUPPORT },
    { 107, EPROTOTYPE },
    { 108, ENOTSOCK },
    { 109, ENOPROTOOPT },
    { 110, ESHUTDOWN },
    { 111, ECONNREFUSED },
    { 112, EADDRINUSE },
    { 113, ECONNABORTED },
    { 114, ENETUNREACH },
    { 115, ENETDOWN },
    { 116, ETIMEDOUT },
    { 117, EHOSTDOWN },
    { 118, EHOSTUNREACH },
    { 119, EINPROGRESS },
    { 120, EALREADY },
    { 121, EDESTADDRREQ },
    { 122, EMSGSIZE },
    { 123, EPROTONOSUPPORT },
    // { 124, ESOCKTNOSUPPORT },
    { 125, EADDRNOTAVAIL },
    { 126, ENETRESET },
    { 127, EISCONN },
    { 128, ENOTCONN },
    { 129, ETOOMANYREFS },
    // { 130, EPROCLIM },
    // { 131, EUSERS },
    { 132, EDQUOT },
    { 133, ESTALE },
    { 134, ENOTSUP },
    // { 135, ENOMEDIUM },
    // { 136, ENOSHARE },
    // { 137, ECASECLASH },
    { 138, EILSEQ },
    { 139, EOVERFLOW },
    { 140, ECANCELED },
    { 141, ENOTRECOVERABLE },
    { 142, EOWNERDEAD }
    // { 143, ESTRPIPE }
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
        case MappingTable::Errno:
            table = _errno;
            size = sizeof(_errno) / sizeof(int_mapping_t);
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

