/**
 *  Thread.cpp
 *
 *  Implementation of the methods for the Thread class as used by the ThreadPool class.
 */
#include "sdkconfig.h"

#include "Globals.hpp"
#include "Logging.hpp"
#include "Thread.hpp"
#include "Exceptions/ThreadPoolException.hpp"

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
    _method = nullptr;
    _parameter = nullptr;
    if (_mutex == NULL)
    {
        throw new ThreadPoolException("Cannot create mutex for thread.");
    }
    //
    //  We explicitly create the _executeMethod semaphore here rather than in the member
    //  initialiser list as using the list has generated an exception in the past.
    //
    //  We need to give the semaphore after it has been created as per the FreeRTOS documentation.
    //  See: https://www.freertos.org/Documentation/02-Kernel/04-API-references/10-Semaphore-and-Mutexes/01-xSemaphoreCreateBinary
    //
    _executeMethod = xSemaphoreCreateBinary();
    if (_executeMethod == NULL)
    {
        throw new ThreadPoolException("Cannot create execute semaphore.");
    }
    xSemaphoreGive(_executeMethod);
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
    //
    //  We take the execution semaphore here to prevent the Task from trying to execute
    //  a method before one has been assigned to it.
    //
    TakeExecutionSemaphore();
    if (xTaskCreate(Thread::Task, _name, 2048 * 2, this, PRIORITY_THREAD_POOL_TASK, &_taskHandle) != pdPASS)
    {
        throw new ThreadPoolException("Cannot create task.");
    }
    TRACE_MESSAGE("Thread: Exit creating thread %s", name);
}

/**
 *  @brief Destructor for the class.
 */
Thread::~Thread()
{
    vPortFree(_name);
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
 *  @brief Execute the specified method in this thread.
 *
 *  @param method
 *      Pointer to the method that should be executed.
 *
 *  @param parameter
 *      Parameter to be passed to the method.
 */
void Thread::Execute(method_t method, void *parameter)
{
    TRACE_MESSAGE("Execute: Thread %s Enter", _name);
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
    {
        if (_isBusy)
        {
            throw new ThreadPoolException("Attempt to execute method on a busy thread.");
        }
        _isBusy = true;
        _method = method;
        _parameter = parameter;
        xSemaphoreGive(_mutex);
        xSemaphoreGive(_executeMethod);
    }
    TRACE_MESSAGE("Execute: Thread %s Exit", _name);
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
        throw new ThreadPoolException("Task cannot take the execution mutex.");
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
