/**
 * @file UnitTestConfiguration.cpp
 * @author Mark Stevens
 * @brief Configuration for the unit tests, read from a file on the device.
 * @version 0.1
 * @date 2026-09-19
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cJSON.h"

#include "IMessageDispatcher.hpp"
#include "FileSystem.hpp"
#include "SharedEnums.hpp"
#include "Utils.hpp"

#include "UnitTestConfiguration.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the file the settings are read from.
 */
const char *UnitTestConfiguration::CONFIGURATION_FILE_NAME = "unittest.config.json";

bool UnitTestConfiguration::_loaded = false;
char *UnitTestConfiguration::_wiFiNetwork = nullptr;
char *UnitTestConfiguration::_wiFiPassword = nullptr;
char *UnitTestConfiguration::_simpleWebServerName = nullptr;
char *UnitTestConfiguration::_simpleWebPage = nullptr;
char *UnitTestConfiguration::_binaryResourceName = nullptr;
char *UnitTestConfiguration::_largeTestFile = nullptr;
char *UnitTestConfiguration::_webServerIpAddress = nullptr;

/**
 *  @brief Port used when the file does not give one.
 */
uint32_t UnitTestConfiguration::_webServerPort = 80;

/**
 *  @brief Number of repeats used when the file does not give one.
 */
uint32_t UnitTestConfiguration::_iterations = 100;

/**
 *  @brief Largest configuration file that will be read.
 *
 *  @note The file holds a handful of short settings.  Anything appreciably larger than
 *        this is not the file we are looking for.
 */
static const size_t MAXIMUM_CONFIGURATION_FILE_SIZE = 4096;

/*
 * ----------------------------------------------------------------------------
 *
 *                             Private methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Stop the run.
 *
 *  Used when the configuration file is there but cannot be understood.  Restarting would
 *  read the same file and stop again, so the task is suspended instead: the board stays
 *  up and the message stays on the console where it can be read.
 *
 *  @param reason Text saying what was wrong with the file.
 */
[[noreturn]] static void Halt(const char *reason)
{
    printf("\n");
    printf("##############################################################\n");
    printf("#  The unit test configuration file cannot be used.\n");
    printf("#\n");
    printf("#  %s\n", reason);
    printf("#\n");
    printf("#  The tests have not been started.  Correct the file, rebuild\n");
    printf("#  the image with mkfsimage.sh and write it to the board.\n");
    printf("##############################################################\n");
    printf("\n");
    fflush(stdout);

    vTaskSuspend(NULL);

    //
    //  Nothing resumes this task, but saying so lets the compiler and cppcheck see that
    //  the code after a call to this is not reached.  Without it the two fclose() calls
    //  on either side of a Halt() look like the same handle being closed twice.
    //
    while (true)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

/**
 *  @brief Take a copy of a string held in the configuration.
 *
 *  @param json Object holding the settings.
 *  @param name Name of the setting wanted.
 *  @param value Filled in with a copy of the setting, left alone if it is not there.
 */
static void ReadString(const cJSON * const json, const char *name, char **value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON *>(json), name);

    if (item == nullptr)
    {
        return;
    }
    if (!cJSON_IsString(item) || (item->valuestring == nullptr))
    {
        Halt("A setting that should be text is not.");
    }

    const size_t length = strlen(item->valuestring) + 1;
    char *copy = static_cast<char *>(pvPortMalloc(length));
    if (copy == nullptr)
    {
        Halt("There is not enough memory to hold the configuration.");
    }
    memcpy(copy, item->valuestring, length);

    if (*value != nullptr)
    {
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(value));
    }
    *value = copy;
}

/**
 *  @brief Read a number held in the configuration.
 *
 *  @param json Object holding the settings.
 *  @param name Name of the setting wanted.
 *  @param value Filled in with the setting, left alone if it is not there.
 */
static void ReadUInt32(const cJSON * const json, const char *name, uint32_t *value)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(const_cast<cJSON *>(json), name);

    if (item == nullptr)
    {
        return;
    }
    if (!cJSON_IsNumber(item) || (item->valuedouble < 0))
    {
        Halt("A setting that should be a number is not, or is negative.");
    }

    *value = static_cast<uint32_t>(item->valuedouble);
}

/**
 *  @brief Report a setting, saying so when it has not been given one.
 *
 *  @param name Name the setting goes by in the file.
 *  @param value Value the setting holds.
 */
static void ReportString(const char *name, const char *value)
{
    printf("  %-20s : %s\n", name, ((value == nullptr) || (*value == '\0')) ? "(not set)" : value);
}

/**
 *  @brief Read the whole of the configuration file.
 *
 *  @param path Full path to the file.
 *
 *  @return Contents of the file as a string the caller owns, nullptr if there is no file.
 */
static char *ReadConfigurationFile(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == nullptr)
    {
        return (nullptr);
    }

    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if ((length < 0) || (static_cast<size_t>(length) > MAXIMUM_CONFIGURATION_FILE_SIZE))
    {
        fclose(file);
        Halt("The configuration file is not a size a configuration file should be.");
    }

    char *contents = static_cast<char *>(pvPortMalloc(static_cast<size_t>(length) + 1));
    if (contents == nullptr)
    {
        fclose(file);
        Halt("There is not enough memory to read the configuration file.");
    }

    const size_t read = fread(contents, 1, static_cast<size_t>(length), file);
    contents[read] = '\0';
    fclose(file);

    return (contents);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Read the configuration file.
 */
void UnitTestConfiguration::Load()
{
    printf("\n#### Unit test configuration #####\n\n");

    //
    //  The file system is mounted only for as long as it takes to read the file.  The
    //  tests that work on the file system mount it themselves, and several of them
    //  format it, so nothing is left holding it here.
    //
    FileSystem *fileSystem = new FileSystem(nullptr);
    if ((fileSystem == nullptr) || (fileSystem->Setup() != StatusCodes::CompletedOk))
    {
        printf("  The file system could not be mounted, so the default settings will be used.\n\n");
        delete fileSystem;
        return;
    }

    std::string path = FileSystem::PARTITION_PATH_NAME;
    path += "/";
    path += CONFIGURATION_FILE_NAME;

    char *contents = ReadConfigurationFile(path.c_str());
    delete fileSystem;

    if (contents == nullptr)
    {
        printf("  WARNING: %s was not found on the device.\n", CONFIGURATION_FILE_NAME);
        printf("  The default settings will be used, and any test needing a setting that\n");
        printf("  has no default will fail.  Build an image holding the file with\n");
        printf("  mkfsimage.sh and write it to the board to change that.\n\n");
        return;
    }

    cJSON *json = cJSON_Parse(contents);

    if (json == nullptr)
    {
        //
        //  cJSON_GetErrorPtr() points into the text that was being parsed, so the text
        //  has to still be there while the message is put together.
        //
        const char *error = cJSON_GetErrorPtr();
        printf("  The file could not be read as JSON");
        if ((error != nullptr) && (error >= contents))
        {
            printf(", at character %d, at or near: %.24s", static_cast<int>(error - contents), error);
        }
        printf("\n");
        Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&contents));
        Halt("The configuration file does not hold valid JSON.");
    }

    Utils::DeleteAndNullifyPointer(reinterpret_cast<void **>(&contents));

    ReadString(json, "WiFiNetwork", &_wiFiNetwork);
    ReadString(json, "WiFiPassword", &_wiFiPassword);
    ReadString(json, "SimpleWebServerName", &_simpleWebServerName);
    ReadString(json, "SimpleWebPage", &_simpleWebPage);
    ReadString(json, "BinaryResourceName", &_binaryResourceName);
    ReadString(json, "LargeTestFile", &_largeTestFile);
    ReadString(json, "WebServerIpAddress", &_webServerIpAddress);
    ReadUInt32(json, "WebServerPort", &_webServerPort);
    ReadUInt32(json, "Iterations", &_iterations);

    cJSON_Delete(json);
    _loaded = true;

    printf("  Read from %s\n\n", CONFIGURATION_FILE_NAME);
    ReportString("WiFiNetwork", _wiFiNetwork);
    //
    //  The password is not printed.  The console output of a test run gets pasted into
    //  issues and pull requests.
    //
    printf("  %-20s : %s\n", "WiFiPassword", ((_wiFiPassword == nullptr) || (*_wiFiPassword == '\0')) ? "(not set)" : "********");
    ReportString("SimpleWebServerName", _simpleWebServerName);
    ReportString("SimpleWebPage", _simpleWebPage);
    ReportString("BinaryResourceName", _binaryResourceName);
    ReportString("LargeTestFile", _largeTestFile);
    ReportString("WebServerIpAddress", _webServerIpAddress);
    printf("  %-20s : %" PRIu32 "\n", "WebServerPort", _webServerPort);
    printf("  %-20s : %" PRIu32 "\n", "Iterations", _iterations);
    printf("\n");
}

/**
 *  @brief Was a configuration file found and read?
 */
bool UnitTestConfiguration::IsLoaded()
{
    return (_loaded);
}

/**
 *  @brief Get the name of the access point the tests should join.
 */
const char *UnitTestConfiguration::WiFiNetwork()
{
    return ((_wiFiNetwork == nullptr) ? "" : _wiFiNetwork);
}

/**
 *  @brief Get the password for that access point.
 */
const char *UnitTestConfiguration::WiFiPassword()
{
    return ((_wiFiPassword == nullptr) ? "" : _wiFiPassword);
}

/**
 *  @brief Get the name of the test web server.
 */
const char *UnitTestConfiguration::SimpleWebServerName()
{
    return ((_simpleWebServerName == nullptr) ? "" : _simpleWebServerName);
}

/**
 *  @brief Get the resource returning a simple page.
 */
const char *UnitTestConfiguration::SimpleWebPage()
{
    return ((_simpleWebPage == nullptr) ? "" : _simpleWebPage);
}

/**
 *  @brief Get the resource returning binary content.
 */
const char *UnitTestConfiguration::BinaryResourceName()
{
    return ((_binaryResourceName == nullptr) ? "" : _binaryResourceName);
}

/**
 *  @brief Get the resource returning a large file.
 */
const char *UnitTestConfiguration::LargeTestFile()
{
    return ((_largeTestFile == nullptr) ? "" : _largeTestFile);
}

/**
 *  @brief Get the address of the test web server.
 */
const char *UnitTestConfiguration::WebServerIpAddress()
{
    return ((_webServerIpAddress == nullptr) ? "" : _webServerIpAddress);
}

/**
 *  @brief Get the port the test web server answers on.
 */
uint32_t UnitTestConfiguration::WebServerPort()
{
    return (_webServerPort);
}

/**
 *  @brief Get the number of times a test that repeats itself should do so.
 */
uint32_t UnitTestConfiguration::Iterations()
{
    return (_iterations);
}

/**
 *  @brief Read the configuration file.
 */
extern "C" void UnitTestConfigurationLoad(void)
{
    UnitTestConfiguration::Load();
}
