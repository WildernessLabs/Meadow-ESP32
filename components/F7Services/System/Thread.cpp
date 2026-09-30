/**
 * @file Thread.cpp
 * @author Mark Stevens
 * @brief Implementation of the Thread class used by the ThreadPool to execute methods.
 * @date 2021-01-14
 *
 * @copyright Copyright (c) 2021 Wilderness Labs
 */

#include "sdkconfig.h"

#include "Globals.hpp"
#include "Logging.hpp"
#include "Thread.hpp"
#include "Exceptions/ThreadPoolException.hpp"
#include "Utils.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the component / task for the Thread class.
 */
const char *Thread::COMPONENT_NAME = "Thread";

/**
 *  @brief Default constructor.
 *
 *  This is not intended to be called by the user.
 */
Thread::Thread() : _mutex(xSemaphoreCreateMutex())
{
    if (_mutex == NULL)
    {
        throw ThreadPoolException("Cannot create mutex for thread.");
    }
    //
    //  We explicitly create the _executeMethod semaphore here rather than in the member
    //  initialiser list as using the list has generated an exception in the past.
    //
    _executeMethod = xSemaphoreCreateBinary();
    if (_executeMethod == NULL)
    {
        throw ThreadPoolException("Cannot create execute semaphore.");
    }
}

/**
 *  @brief Constructor for the Thread class.
 *
 *  @param name
 *      Name of the thread.  This allows the thread to be easily identified.
 *
 *  @param taskCompletedMethod
 *      Pointer to the method that should be called when the task has completed and the
 *      thread is ready to run a new task.
 */
Thread::Thread(char *name, task_completed_method_t taskCompletedMethod) : Thread()
{
    TRACE_MESSAGE("Thread: Enter creating thread %s", name);
    _name = name;
    _taskCompletedMethod = taskCompletedMethod;
    if (xTaskCreate(Thread::Task, _name, TASK_STACK_SIZE, this, PRIORITY_THREAD_POOL_TASK, &_taskHandle) != pdPASS)
    {
        throw ThreadPoolException("Cannot create task.");
    }
    TRACE_MESSAGE("Thread: Exit creating thread %s", name);
}

/**
 *  @brief Destructor for the class.
 */
Thread::~Thread()
{
    vTaskDelete(_taskHandle);
    vSemaphoreDelete(_mutex);
    vSemaphoreDelete(_executeMethod);
    Utils::DeleteAndNullifyPointer(_name);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Get the name of this task.
 *
 *  Note that a mutex is not required to guard access to the backing variable as
 *  this is only set up in the constructor.
 */
char *Thread::GetTaskName()
{
    return (_name);
}

/**
 *  @brief Get the method that should be executed in this thread.
 */
method_t Thread::GetMethod()
{
    method_t method = nullptr;

    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        method = _method;
        xSemaphoreGive(_mutex);
    }

    return (method);
}

/**
 *  @brief Get the parameter to be passed to the method.
 */
void *Thread::GetParameter()
{
    void *parameter = nullptr;

    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        parameter = _parameter;
        xSemaphoreGive(_mutex);
    }

    return (parameter);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Is this thread busy executing a method?
 */
bool Thread::IsBusy()
{
    bool busy = true;

    if (xSemaphoreTake(_mutex, 10 / portTICK_PERIOD_MS) == pdTRUE)
    {
        busy = _isBusy;
        xSemaphoreGive(_mutex);
    }
    return (busy);
}

/**
 *  @brief Execute the specified method in this thread if the thread is idle.
 *
 *  Checking that the thread is idle and claiming it happen under the one hold of the
 *  mutex, so two callers can never both claim the same thread.  Checking IsBusy() and
 *  then calling Execute() leaves a window between the two in which another caller can
 *  claim the thread.
 *
 *  @param method
 *      Pointer to the method that should be executed.
 *
 *  @param parameter
 *      Parameter to be passed to the method.
 *
 *  @return
 *      true if the thread was idle and will now run the method, false if it was busy,
 *      in which case nothing about the thread has changed.
 */
bool Thread::TryExecute(method_t method, void *parameter)
{
    TRACE_MESSAGE("TryExecute: Thread %s Enter", _name);
    bool claimed = false;
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        if (!_isBusy)
        {
            _isBusy = true;
            _method = method;
            _parameter = parameter;
            claimed = true;
        }
        xSemaphoreGive(_mutex);
        if (claimed)
        {
            xSemaphoreGive(_executeMethod);
        }
    }
    TRACE_MESSAGE("TryExecute: Thread %s Exit", _name);
    return (claimed);
}

/**
 *  @brief Execute the specified method in this thread.
 *
 *  @param method
 *      Pointer to the method that should be executed.
 *
 *  @param parameter
 *      Parameter to be passed to the method.
 *
 *  @throws ThreadPoolException if the thread is already busy.
 */
void Thread::Execute(method_t method, void *parameter)
{
    if (!TryExecute(method, parameter))
    {
        throw ThreadPoolException("Attempt to execute method on a busy thread.");
    }
}

/**
 *  @brief Take the execution semaphore.
 *
 *  Taking the semaphore is used to put the FreeRTOS task in a waiting state.
 */
void Thread::TakeExecutionSemaphore()
{
    if (xSemaphoreTake(_executeMethod, portMAX_DELAY) == pdTRUE)
    {
        return;
    }
    else
    {
        throw ThreadPoolException("Task cannot take the execution mutex.");
    }
}

/**
 *  @brief Put the thread into a completed state making it ready for a new task to be assigned.
 */
void Thread::TaskCompleted()
{
    TRACE_MESSAGE("TaskCompleted: Thread %s Enter", _name);
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        _isBusy = false;
        _method = nullptr;
        _parameter = nullptr;
        xSemaphoreGive(_mutex);
    }
    if (_taskCompletedMethod != NULL)
    {
        (_taskCompletedMethod());
    }
    TRACE_MESSAGE("TaskCompleted: Thread %s Exit", _name);
}

/**
 *  @brief FreeRTOS task that will execute the methods passed to it.
 *
 *  @param taskParameters
 *      Parameters passed to the task when it is created by xCreateTask.
 *      This should be set to be a pointer to the instantiated class that
 *      created the FreeRTOS task.
 */
void Thread::Task(void *taskParameters)
{
    Thread *sender = static_cast<Thread *>(taskParameters);
    while (true)
    {
        sender->TakeExecutionSemaphore();
        method_t method = sender->GetMethod();
        if (method)
        {
            method(sender->GetParameter());
            sender->TaskCompleted();
        }
    }
}
