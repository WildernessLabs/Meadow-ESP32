/*
 *  TestProfile.cpp
 *
 *  Tests that recording the Meadow events is safe whether or not a SystemView capture is running.
 *  Without one, which is how a SystemView build spends most of its time, the calls do nothing; with
 *  one, the events go into the trace (this is how the events were checked on the board).  A build
 *  without SystemView compiles the calls away.
 */

#include "sdkconfig.h"

#include <stdint.h>
#include "unity.h"

#include "Profile.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Profile][Emulated]"

/**
 * @brief Number of times each event is recorded.
 */
#define RECORD_ITERATIONS 1000

TEST_CASE("Meadow events can be recorded whether or not a SystemView capture is running", CLASS_OF_TESTS)
{
    for (uint32_t index = 0; index < RECORD_ITERATIONS; index++)
    {
        Profile::Record(ProfileEvent::SpiTransaction, 1, index);
        Profile::Record(ProfileEvent::RequestReceived, index, 1, 2);
        Profile::Record(ProfileEvent::MessageAcknowledged, index);
        Profile::MessageStart(index);
        Profile::MessageStop(index);
        {
            ProfileSpan span(ProfileEvent::HandleRequest, ProfileEvent::HandleRequestDone, index, 1, 2);
        }
        {
            ProfileSpan span(ProfileEvent::SpiProcess, ProfileEvent::SpiProcessDone, 1);
        }
    }
    TEST_PASS();
}
