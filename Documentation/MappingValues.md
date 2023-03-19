# Mapping Constants Between NuttX and ESP-IDF

The `Mapping` class runs on the ESP32 microcontroller and allows the conversion of NuttX defined constants and ESP-IDF defined constants.  The class provides the ability to map the values in both directions.

The mapping functionality has been provided on the ESP32 in order to spread the load on the system.

Typical uses for this class is the translation of the socket options from the NuttX values to the ESP-IDF values.  Similarly, any error codes that may be generated on the ESP-IDF will need to be translated into error codes for NuttX.

The `Mapping class also provides the ability to map flag values  (values comprised of an or-ed set of bits values) between the two systems.