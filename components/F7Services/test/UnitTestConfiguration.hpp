/**
 * @file UnitTestConfiguration.hpp
 * @author Mark Stevens
 * @brief Configuration for the unit tests, read from a file on the device.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */
#ifndef _UNIT_TEST_CONFIGURATION_HPP_
#define _UNIT_TEST_CONFIGURATION_HPP_

#include "sdkconfig.h"

#include <stdint.h>

/**
 * @brief Settings the tests need, read from a file written to the board.
 *
 *  The settings used to be compiled in through secrets.h, which meant changing any of
 *  them needed a rebuild and a reflash.  They are now read at startup from a JSON file
 *  in the littlefs partition, put there by mkfsimage.sh, so a test run can be pointed at
 *  a different network or server without the firmware changing at all.
 *
 *  The values are reached through static methods because a test that wants one should
 *  not have to be handed anything to get at it.
 *
 *  Not every test needs configuration and not every setting has to be in the file.  A
 *  setting the file does not mention keeps its default, and a file that is not there at
 *  all leaves every setting at its default and says so.  A file that is there but cannot
 *  be understood is a different matter and stops the run: carrying on would mean testing
 *  against settings nobody chose.
 */
class UnitTestConfiguration
{
private:
    /**
     *  @brief Whether a configuration file was found and read.
     */
    static bool _loaded;

    /**
     *  @brief Name of the access point the tests should join.
     */
    static char *_wiFiNetwork;

    /**
     *  @brief Password for that access point.
     */
    static char *_wiFiPassword;

    /**
     *  @brief Name of the test web server.
     */
    static char *_simpleWebServerName;

    /**
     *  @brief Resource on the test web server returning a simple page.
     */
    static char *_simpleWebPage;

    /**
     *  @brief Resource on the test web server returning binary content.
     */
    static char *_binaryResourceName;

    /**
     *  @brief Resource on the test web server returning a large file.
     */
    static char *_largeTestFile;

    /**
     *  @brief Address of the test web server.
     */
    static char *_webServerIpAddress;

    /**
     *  @brief Port the test web server answers on.
     */
    static uint32_t _webServerPort;

    /**
     *  @brief Number of page fetches the POSIX soak test should make.
     */
    static uint32_t _posixIterations;

    /**
     *  @brief Number of scans the repeated access point scan test should make.
     */
    static uint32_t _scanIterations;

    /**
     *  @brief Number of times the Bluetooth restart test should restart the service.
     */
    static uint32_t _bluetoothRestartIterations;

    /**
     *  @brief How far the free heap may fall across the POSIX soak test.
     */
    static uint32_t _soakHeapAllowance;

    /**
     *  @brief How often the POSIX soak test reports its progress.
     */
    static uint32_t _soakProgressInterval;

public:
    /**
     *  @brief Name of the file the settings are read from.
     */
    static const char *CONFIGURATION_FILE_NAME;

    /**
     *  @brief Read the configuration file.
     *
     *  Called once as the tests start, before any of them run.  A file that cannot be
     *  understood stops the run rather than letting the tests use settings nobody chose.
     */
    static void Load();

    /**
     *  @brief Was a configuration file found and read?
     *
     *  @return True if the settings came from a file, false if they are all defaults.
     */
    static bool IsLoaded();

    /**
     *  @brief Get the name of the access point the tests should join.
     */
    static const char *WiFiNetwork();

    /**
     *  @brief Get the password for that access point.
     */
    static const char *WiFiPassword();

    /**
     *  @brief Get the name of the test web server.
     */
    static const char *SimpleWebServerName();

    /**
     *  @brief Get the resource returning a simple page.
     */
    static const char *SimpleWebPage();

    /**
     *  @brief Get the resource returning binary content.
     */
    static const char *BinaryResourceName();

    /**
     *  @brief Get the resource returning a large file.
     */
    static const char *LargeTestFile();

    /**
     *  @brief Get the address of the test web server.
     */
    static const char *WebServerIpAddress();

    /**
     *  @brief Get the port the test web server answers on.
     */
    static uint32_t WebServerPort();

    /**
     *  @brief Get the number of page fetches the POSIX soak test should make.
     */
    static uint32_t PosixIterations();

    /**
     *  @brief Get the number of scans the repeated access point scan test should make.
     */
    static uint32_t ScanIterations();

    /**
     *  @brief Get the number of times the Bluetooth restart test should restart the service.
     */
    static uint32_t BluetoothRestartIterations();

    /**
     *  @brief Get how far the free heap may fall across the POSIX soak test.
     *
     *  Allocation patterns leave the heap slightly differently arranged from one run to
     *  the next, so a soak is not expected to give back exactly what it took.  This says
     *  how much of a fall is put down to that rather than to a leak, so it belongs with
     *  the number of fetches: a longer soak allows more, a shorter one less.
     */
    static uint32_t SoakHeapAllowance();

    /**
     *  @brief Get how often the POSIX soak test reports its progress.
     *
     *  @note Counted in fetches.  Zero turns the reports off.
     */
    static uint32_t SoakProgressInterval();
};

/**
 * @brief Read the configuration file.
 *
 * @note app_main is C, so it reaches UnitTestConfiguration::Load() through this.
 */
extern "C" void UnitTestConfigurationLoad(void);

#endif // _UNIT_TEST_CONFIGURATION_HPP_
