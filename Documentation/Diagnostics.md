# ESP32 Serial Diagnostic Output

## Serial Output

The majority of the debugging output follows the following principles:

* Use `#define` to determine which diagnostics should be built into the system
* All diagnostics produced using the `ESP_LOGI` macro.

### `DebugSettings.hpp`

This file contains a number of macro definitions that are used to determine which diagnostic statements are compiled into the application.  The definition is paired with a `#undef` statement for the same macro.  This allows diagnostics to be switched in and out by commenting out the `#undef` statements.

```C
#define WIFI_FULL_LOGGING 1
#undef WIFI_FULL_LOGGING

#define WIFI_DEBUG_CONNECT 1
#undef WIFI_DEBUG_CONNECT
```

The source code for the `connect` method contains diagnostic statements such as:

```C
#if defined(WIFI_FULL_LOGGING) || defined(WIFI_DEBUG_CONNECT)
    ESP_LOGI(__func__, "Enter");
#endif
```

The `ESP-PROG` macro act like `printf`.  The first entry `__func__` above is an identifier called a tag in ESP parlance and is used to set logging levels globally across the application.  This is not really used in this application but it does appear in debug output on the console.

The second parameter is a format string following the same rules as `printf`.  So the following are more examples of `ESP_LOGI` using format strings:

```C
#if defined(WIFI_FULL_LOGGING) || defined(WIFI_DEBUG_CONNECT)
    ESP_LOGI(__func__, "Connecting socket: %d", request->SocketHandle);
    ESP_LOGI(__func__, "Family: %d", sa.sin_family);
    ESP_LOGI(__func__, "IP address: %s", inet_ntoa(sa.sin_addr));
    ESP_LOGI(__func__, "Port: %d", ntohs(sa.sin_port));
#endif
```

By default, each method contains a trace on the entry and exit for a method.  This has proven useful in following the flow through the application.

ESP-IDF contains an extensive logging library.  Full documentation is available through the [Logging Library](https://docs.espressif.com/projects/esp-idf/en/release-v4.1/api-reference/system/log.html) section on the ESP-IDF documentation web site.

## Application Trace

To Be Completed.

## Heap Tracing

To Be Completed.
