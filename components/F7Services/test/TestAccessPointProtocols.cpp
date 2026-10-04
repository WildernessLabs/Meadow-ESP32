/*
 *  TestAccessPointProtocols.cpp
 *
 *  Tests that the protocol flags of a scanned access point each keep their own bit (issue #249).
 */

#include "sdkconfig.h"

#include <stdint.h>
#include <string.h>
#include "esp_wifi_types.h"
#include "unity.h"

#include "SharedEnums.hpp"
#include "WiFiRequestHandler.hpp"

/**
 * @brief Class of tests in this file.
 */
#define CLASS_OF_TESTS "[WiFi][Emulated]"

/**
 * @brief Test access to WiFiRequestHandler::ConvertProtocols.
 */
class TestAccessPointProtocols
{
public:
    /**
     * @brief Convert the protocol flags of a record.
     *
     * @param phy11b Value of phy_11b.
     * @param phy11g Value of phy_11g.
     * @param phy11n Value of phy_11n.
     * @param phyLowRate Value of phy_lr.
     * @param wps Value of wps.
     *
     * @return The protocols.
     */
    static uint32_t Convert(const uint8_t phy11b, const uint8_t phy11g, const uint8_t phy11n, const uint8_t phyLowRate, const uint8_t wps)
    {
        wifi_ap_record_t record;
        memset(&record, 0, sizeof(record));
        record.phy_11b = phy11b;
        record.phy_11g = phy11g;
        record.phy_11n = phy11n;
        record.phy_lr = phyLowRate;
        record.wps = wps;
        return (WiFiRequestHandler::ConvertProtocols(record));
    }
};

TEST_CASE("Access point protocol flags each keep their own bit", CLASS_OF_TESTS)
{
    TEST_ASSERT_EQUAL_UINT32(WiFiAccessPointProtocols::None, TestAccessPointProtocols::Convert(0, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(WiFiAccessPointProtocols::Phy11b, TestAccessPointProtocols::Convert(1, 0, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(WiFiAccessPointProtocols::Phy11g, TestAccessPointProtocols::Convert(0, 1, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(WiFiAccessPointProtocols::Phy11n, TestAccessPointProtocols::Convert(0, 0, 1, 0, 0));
    TEST_ASSERT_EQUAL_UINT32(WiFiAccessPointProtocols::PhyLowRate, TestAccessPointProtocols::Convert(0, 0, 0, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(WiFiAccessPointProtocols::Wps, TestAccessPointProtocols::Convert(0, 0, 0, 0, 1));
    //
    //  A typical access point, 11b/g/n with WPS, and every flag set.
    //
    TEST_ASSERT_EQUAL_UINT32(0x17, TestAccessPointProtocols::Convert(1, 1, 1, 0, 1));
    TEST_ASSERT_EQUAL_UINT32(0x1f, TestAccessPointProtocols::Convert(1, 1, 1, 1, 1));
}
