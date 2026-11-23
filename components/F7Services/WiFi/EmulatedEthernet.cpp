/**
 * @file EmulatedEthernet.cpp
 * @author Mark Stevens
 * @brief Network connection for the socket tests when running under QEMU.
 *
 * QEMU has no WiFi radio but does emulate an OpenCores Ethernet controller.  In a test
 * build made with --qemuethernet a request to join an access point brings that controller
 * up instead, so everything above the network interface runs as it does on a board.
 *
 * Nothing here is compiled unless CONFIG_ETH_USE_OPENETH is set, and only
 * test/sdkconfig.qemuethernet sets it, so board firmware does not contain this code.
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "sdkconfig.h"

#if CONFIG_ETH_USE_OPENETH

    #include "esp_eth.h"
    #include "esp_eth_mac_openeth.h"
    #include "esp_event.h"
    #include "esp_netif.h"

    #include "WiFiRequestHandler.hpp"

/**
 *  @brief Address of the PHY that QEMU's OpenCores Ethernet model presents.
 */
static constexpr int32_t EMULATED_PHY_ADDRESS = 1;

/**
 *  @brief Milliseconds allowed for auto-negotiation, which the emulated PHY finishes at once.
 */
static constexpr uint32_t EMULATED_NEGOTIATION_TIMEOUT = 100;

/**
 *  @brief Pass the address the emulated network assigned to the station's got IP handling.
 *
 *  @param arg Instance of WiFiRequestHandler that registered this handler.
 *  @param eventBase Subsystem that created the event, not used.
 *  @param eventId Type of event, not used as only IP_EVENT_ETH_GOT_IP is registered.
 *  @param eventData An ip_event_got_ip_t describing the address.
 */
void WiFiRequestHandler::EmulatedEthernetGotIp(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    (void) eventBase;
    (void) eventId;

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
    static_cast<WiFiRequestHandler *>(arg)->StationGotIpEvent(static_cast<const ip_event_got_ip_t *>(eventData));
}

/**
 *  @brief Bring up the emulated Ethernet interface and wait for an address.
 *
 *  The interface takes the place of the station interface, so the socket and ioctl handlers
 *  use it without knowing the difference.  A failure to create the driver stops the test
 *  run, as it means the image was not built or started for this network.
 *
 *  @return CompletedOk once an address is held, Timeout if none arrives in time.
 */
StatusCodes::StatusCodes WiFiRequestHandler::ConnectToEmulatedEthernet()
{
    if (_stationNetIfHandle == nullptr)
    {
        ESP_ERROR_CHECK(esp_netif_init());
        const esp_err_t eventLoopResult = esp_event_loop_create_default();
        if (eventLoopResult != ESP_ERR_INVALID_STATE)
        {
            ESP_ERROR_CHECK(eventLoopResult);
        }

        const esp_netif_config_t networkConfiguration = ESP_NETIF_DEFAULT_ETH();
        esp_netif_t * const networkInterface = esp_netif_new(&networkConfiguration);

        const eth_mac_config_t macConfiguration = ETH_MAC_DEFAULT_CONFIG();
        eth_phy_config_t phyConfiguration = ETH_PHY_DEFAULT_CONFIG();
        phyConfiguration.phy_addr = EMULATED_PHY_ADDRESS;
        phyConfiguration.autonego_timeout_ms = EMULATED_NEGOTIATION_TIMEOUT;
        phyConfiguration.reset_gpio_num = -1;

        esp_eth_mac_t * const mac = esp_eth_mac_new_openeth(&macConfiguration);
        esp_eth_phy_t * const phy = esp_eth_phy_new_generic(&phyConfiguration);
        if ((networkInterface == nullptr) || (mac == nullptr) || (phy == nullptr))
        {
            ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
        }

        esp_eth_config_t driverConfiguration = ETH_DEFAULT_CONFIG(mac, phy);
        esp_eth_handle_t driver = nullptr;
        ESP_ERROR_CHECK(esp_eth_driver_install(&driverConfiguration, &driver));

        //
        //  The MAC address has to be set before the driver starts, as the emulated
        //  controller has none of its own.
        //
        uint8_t macAddress[6] = {};
        GetMacAddress(macAddress, WIFI_IF_STA);
        ESP_ERROR_CHECK(esp_eth_ioctl(driver, ETH_CMD_S_MAC_ADDR, macAddress));

        ESP_ERROR_CHECK(esp_netif_attach(networkInterface, esp_eth_new_netif_glue(driver)));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &WiFiRequestHandler::EmulatedEthernetGotIp, this));

        //
        //  Stored before the start so that the got IP event finds the interface.
        //
        _stationNetIfHandle = networkInterface;
        ESP_ERROR_CHECK(esp_eth_start(driver));
    }

    const EventBits_t bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_HAVE_IP_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);

    return (((bits & WIFI_HAVE_IP_BIT) != 0U) ? StatusCodes::CompletedOk : StatusCodes::Timeout);
}

#endif
