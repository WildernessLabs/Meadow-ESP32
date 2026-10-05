/*
 *  TestBluetoothUnwind.cpp
 *
 *  Tests that a Bluetooth stack that is only partly up can be recovered from (issue #296).
 *
 *  The stack is put into the state a failed start leaves it in by bringing up the first stages
 *  directly.  Starting the service then fails at its first stage, as a stage failing later would,
 *  and the stack has to be left down.  These need the Bluetooth controller, so they need the board.
 */

#include "sdkconfig.h"

#include <stdint.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "unity.h"

#include "BluetoothRequestHandler.hpp"
#include "MockMessageDispatcher.hpp"
#include "TestAssertions.hpp"
#include "TestHarness.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[Bluetooth][HardwareOnly]"

/**
 * @brief How far the stack has been brought up before the service is started.
 */
enum class StackState {
    /// Controller initialised.
    ControllerInitialised,

    /// Controller enabled.
    ControllerEnabled,

    /// Bluedroid initialised.
    BluedroidInitialised,

    /// Bluedroid enabled.
    BluedroidEnabled
};

/**
 * @brief Gives the tests access to starting and stopping the Bluetooth stack.
 */
class TestBluetoothUnwind
{
public:
    /**
     * @brief Get the handler, taking down any stack a previous test left up.
     *
     * @return The handler.
     */
    static BluetoothRequestHandler *Handler()
    {
        MockMessageDispatcher *dispatcher = SetupMessageDispatcher();
        TEST_ASSERT_NOT_NULL(dispatcher);
        BluetoothRequestHandler *handler = BluetoothRequestHandler::GetInstance(dispatcher);
        TEST_ASSERT_NOT_NULL(handler);
        handler->TearDownStack();
        CheckStackIsDown();
        return (handler);
    }

    /**
     * @brief Bring the stack up as far as the state says.
     *
     * @param state How far to go.
     */
    static void BringUp(const StackState state)
    {
        esp_bt_controller_config_t configuration = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        TEST_ASSERT_EQUAL(ESP_OK, esp_bt_controller_init(&configuration));
        if (state != StackState::ControllerInitialised)
        {
            TEST_ASSERT_EQUAL(ESP_OK, esp_bt_controller_enable(ESP_BT_MODE_BLE));
            if (state != StackState::ControllerEnabled)
            {
                TEST_ASSERT_EQUAL(ESP_OK, esp_bluedroid_init());
                if (state == StackState::BluedroidEnabled)
                {
                    TEST_ASSERT_EQUAL(ESP_OK, esp_bluedroid_enable());
                }
            }
        }
    }

    /**
     * @brief Check that no part of the stack is up.
     */
    static void CheckStackIsDown()
    {
        TEST_ASSERT_EQUAL_MESSAGE(ESP_BLUEDROID_STATUS_UNINITIALIZED, esp_bluedroid_get_status(), "Bluedroid was left up.");
        TEST_ASSERT_EQUAL_MESSAGE(ESP_BT_CONTROLLER_STATUS_IDLE, esp_bt_controller_get_status(), "The controller was left up.");
    }

    /**
     * @brief Check that starting the service fails when the stack is in a state and leaves it down.
     *
     * @param state State to put the stack in.
     */
    static void CheckFailedStartLeavesStackDown(const StackState state)
    {
        BluetoothRequestHandler *handler = Handler();
        BringUp(state);

        TEST_ASSERT_FALSE_MESSAGE(handler->StartBluetoothService(), "The service started on a stack that was already up.");
        CheckStackIsDown();

        //
        //  The controller memory was released, so it can be initialised again, which is what a
        //  retry does first.
        //
        esp_bt_controller_config_t configuration = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, esp_bt_controller_init(&configuration), "The controller cannot be initialised again.");
        TEST_ASSERT_EQUAL(ESP_OK, esp_bt_controller_deinit());
    }

    /**
     * @brief Check that the service can be stopped when the stack is in a state.
     *
     * @param state State to put the stack in.
     */
    static void CheckStopRecoversStack(const StackState state)
    {
        BluetoothRequestHandler *handler = Handler();
        BringUp(state);

        TEST_ASSERT_TRUE_MESSAGE(handler->StopBluetoothService(), "Stop could not take down a partly started stack.");
        CheckStackIsDown();
    }

    /**
     * @brief Make a handler of the test's own, bring the stack up as far as a state, destroy the handler, and put the shared one back.
     *
     *  The handler is created without Setup(), so it starts no task of its own.
     *
     * @param state How far to bring the stack up before the handler is destroyed.
     */
    static void DestroyHandlerWithStackUp(const StackState state)
    {
        Handler();
        BluetoothRequestHandler * const savedInstance = BluetoothRequestHandler::_instance;
        const bool savedInstantiated = BluetoothRequestHandler::_instantiated;
        IMessageDispatcher * const savedDispatcher = BluetoothRequestHandler::_messageDispatcher;
        BluetoothRequestHandler::_instance = nullptr;
        BluetoothRequestHandler::_instantiated = false;
        BluetoothRequestHandler * const handler = new BluetoothRequestHandler(savedDispatcher);
        TEST_ASSERT_NOT_NULL(handler);

        BringUp(state);
        delete handler;

        BluetoothRequestHandler::_instance = savedInstance;
        BluetoothRequestHandler::_instantiated = savedInstantiated;
        BluetoothRequestHandler::_messageDispatcher = savedDispatcher;
        CheckStackIsDown();
    }

    /**
     * @brief Stop the service when nothing is running.
     *
     * @return The result of the stop.
     */
    static bool StopWhenNothingIsRunning()
    {
        return (Handler()->StopBluetoothService());
    }
};

TEST_CASE("A failed Start leaves the controller down (controller initialised)", CLASS_OF_TESTS)
{
    TestBluetoothUnwind::CheckFailedStartLeavesStackDown(StackState::ControllerInitialised);
}

TEST_CASE("A failed Start leaves the controller down (controller enabled)", CLASS_OF_TESTS)
{
    TestBluetoothUnwind::CheckFailedStartLeavesStackDown(StackState::ControllerEnabled);
}

TEST_CASE("A failed Start leaves the stack down (Bluedroid initialised)", CLASS_OF_TESTS)
{
    TestBluetoothUnwind::CheckFailedStartLeavesStackDown(StackState::BluedroidInitialised);
}

TEST_CASE("A failed Start leaves the stack down (Bluedroid enabled)", CLASS_OF_TESTS)
{
    TestBluetoothUnwind::CheckFailedStartLeavesStackDown(StackState::BluedroidEnabled);
}

TEST_CASE("Stop takes down a stack that is only partly up", CLASS_OF_TESTS)
{
    TestBluetoothUnwind::CheckStopRecoversStack(StackState::ControllerInitialised);
    TestBluetoothUnwind::CheckStopRecoversStack(StackState::ControllerEnabled);
    TestBluetoothUnwind::CheckStopRecoversStack(StackState::BluedroidInitialised);
    TestBluetoothUnwind::CheckStopRecoversStack(StackState::BluedroidEnabled);
}

TEST_CASE("Stop still reports failure when nothing is running", CLASS_OF_TESTS)
{
    TEST_ASSERT_FALSE(TestBluetoothUnwind::StopWhenNothingIsRunning());
}

TEST_CASE("Destroying the handler takes the stack down", CLASS_OF_TESTS)
{
    //
    //  The stack keeps calling the handler's callbacks and reading its attribute table until it is down, and the
    //  callbacks cannot be unregistered, so the handler's destructor has to take the stack down before it goes
    //  (issue #183).  Each state the stack can be left in is tried.
    //
    TestBluetoothUnwind::DestroyHandlerWithStackUp(StackState::ControllerInitialised);
    TestBluetoothUnwind::DestroyHandlerWithStackUp(StackState::ControllerEnabled);
    TestBluetoothUnwind::DestroyHandlerWithStackUp(StackState::BluedroidInitialised);
    TestBluetoothUnwind::DestroyHandlerWithStackUp(StackState::BluedroidEnabled);
}
