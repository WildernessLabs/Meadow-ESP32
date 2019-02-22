/*
 * TransportProviderBase.cpp
 *
 *  Created on: 15 Feb 2019
 *      Author: mark
 */

#include "TransportProviderBase.hpp"
#include "Messaging.h"

/*
 * ---------------------------------------------------------------------------
 *
 *                         Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Default constructor.
 */
TransportProviderBase::TransportProviderBase()
{
}

/*
 *      Default destructor.
 */
TransportProviderBase::~TransportProviderBase()
{
}

/*
 * ---------------------------------------------------------------------------
 *
 *                           Getters and Setters
 *
 * ---------------------------------------------------------------------------
 */

/*
 *      Get the current frame size.
 */
uint32_t TransportProviderBase::GetFrameSize()
{
    return(_frameSize);
}

/*
 *      Change the frame size.
 *
 *      @param newFrameSize
 *          New frame size.
 *
 *      @return
 *          New frame size (if the new size is within bounds) or the current
 *          frame size if the frame size is out of bounds.
 *
 *          It is important that the caller checks the return result to make
 *          sure that the new frame size has been accepted.
 */
uint32_t TransportProviderBase::SetFrameSize(uint32_t newFrameSize)
{
    if ((newFrameSize <= _maximumFrameSize) && (newFrameSize >= MINIMUM_FRAME_SIZE))
    {
        _frameSize = newFrameSize;
    }
    return(_frameSize);
}

/*
 *      Set the function to be called when a new data frame is received from the STM32.
 *
 *      @param newHeaderReceviedCallback
 *          Pointer to the function to be called when a new data frame is received.
 */
void TransportProviderBase::SetFrameReceivedCallback(FrameReceivedFunctionPointer newHeaderReceivedCallback)
{
    _headerReceivedCallback = newHeaderReceivedCallback;
}

/*
 *      Get the maximum size of a data frame for this transport provider.
 *
 *      @return
 *          Maximum size of a data frame that this provider can support.
 */
uint32_t TransportProviderBase::GetMaximumFrameSize()
{
    return(_maximumFrameSize);
}
