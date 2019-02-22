/*
 *      TransportProviderBase.hpp
 *
 *
 */

#ifndef _TRANSPORT_PROVIDER_BASE_HPP_
#define _TRANSPORT_PROVIDER_BASE_HPP_

#include "SharedEnums.hpp"
#include "Messaging.h"

class TransportProviderBase
{
public:
    typedef void (*FrameReceivedFunctionPointer)(uint8_t *, uint32_t);

    /*
     *      Default frame size.
     */
    const uint32_t DEFAULT_FRAME_SIZE = 256;

    /*
     *      minimum size of a data frame.
     */
    const uint32_t MINIMUM_FRAME_SIZE = DEFAULT_FRAME_SIZE;

    /*
     *      Setup the transport provider.
     */
    virtual void Setup() = 0;

    /*
     *      Send a frame to the STM32.
     */
    virtual void SendFrame(uint8_t *, uint32_t) = 0;

    /*
     *      Get the current frame size.
     */
    uint32_t GetFrameSize();

    /*
     *      Set the the new frame size.
     */
    uint32_t SetFrameSize(uint32_t);

    /*
     *      Get the maximum number of bytes in a frame.
     */
    uint32_t GetMaximumFrameSize();

    /*
     *      Set the method to ba called when a new frame is received.
     */
    void SetHeaderReceivedCallback(FrameReceivedFunctionPointer);

private:
    /*
     *      Current frame size
     */
    uint32_t _frameSize = DEFAULT_FRAME_SIZE;

    /*
     *      Maximum frame size for this transport provider.  This must be set in the
     *      transport provider constructor.
     */
    uint32_t _maximumFrameSize;

    /*
     *      Function to call when a new frame is received.
     */
    FrameReceivedFunctionPointer _headerReceivedCallback = NULL;

    /*
     *      Default constructor.
     */
    TransportProviderBase();

    /*
     *      Default destructor.
     */
    ~TransportProviderBase();
};

#endif /* _TRANSPORT_PROVIDER_BASE_HPP_ */
