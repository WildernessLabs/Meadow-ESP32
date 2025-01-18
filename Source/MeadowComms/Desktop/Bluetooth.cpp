#include <iostream>
#include "test_bluetooth_service_json.hpp"
#include "BluetoothManager.hpp"

using namespace std;

/**
 * @brief Main program loop.
 */
extern "C" int main(int argc, char *argv[])
{
    //
    //  The following two lines are required to suppress the warnings about unused variables.
    //
    (void) argc;
    (void) argv;

    struct timespec start, end;
    clock_gettime(CLOCK_REALTIME, &start);

    BluetoothManager _manager;
    _manager.BuildGraphFromJson((char *) meadow_f7_json);
    _manager.DeleteAllServices();

    clock_gettime(CLOCK_REALTIME, &end);

    printf("\n\n\nProgram execution complete.\n");
    printf("Executed in %ld nanoseconds.\n", end.tv_nsec - start.tv_nsec);

    return(0);
}