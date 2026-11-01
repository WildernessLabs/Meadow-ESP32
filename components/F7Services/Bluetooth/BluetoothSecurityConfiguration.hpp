/**
 * @file BluetoothSecurityConfiguration.hpp
 * @author Mark Stevens
 * @brief How this device pairs and bonds with Bluetooth clients, as the STM32 configured it.
 * @date 2026-10-30
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#pragma once

#include <stdint.h>

#include "cJSON.h"
#include "esp_gap_ble_api.h"

#include "BluetoothAttribute.hpp"

/**
 * @brief Key name for the security settings in the JSON configuration.
 */
#define SECURITY_KEY "security"

/**
 * @brief Key name for the input and output capability in the security settings.
 */
#define IO_CAPABILITY_KEY "ioCapability"

/**
 * @brief Key name for whether pairing keys are stored (bonding) in the security settings.
 */
#define BONDING_KEY "bonding"

/**
 * @brief Key name for whether pairing must protect against a man in the middle in the security settings.
 */
#define MITM_KEY "mitm"

/**
 * @brief Key name for whether legacy pairing is refused in the security settings.
 */
#define SECURE_CONNECTIONS_ONLY_KEY "secureConnectionsOnly"

/**
 * @brief Key name for whether this device asks for encryption when a client connects.
 */
#define ENCRYPT_ON_CONNECT_KEY "encryptOnConnect"

/**
 * @brief Key name for whether a client that is not bonded may pair.
 */
#define ACCEPT_PAIRING_KEY "acceptPairing"

/**
 * @brief How this device pairs and bonds with Bluetooth clients.
 *
 *  The STM32 sends these settings as the optional "security" object of the configuration that
 *  starts the Bluetooth service.  A configuration without one gets the defaults, which match how
 *  the service behaved before security could be configured: a client may pair using Just Works and
 *  bond, and the link is only encrypted if the client asks (issue #398).
 *
 *  Every default is safe to use with an STM32 application that knows nothing of pairing: with no
 *  input or output, pairing never asks the STM32 for a passkey or a confirmation.
 */
class BluetoothSecurityConfiguration
{
public:
    /**
     *  @brief Name of the component / task for the Bluetooth system.
     */
    static const char *COMPONENT_NAME;

    /**
     * @brief Input and output this device can use to authenticate a pairing.
     *
     *  The values are the Bluetooth Core Specification's, which are also ESP-IDF's ESP_IO_CAP_*.
     */
    enum class IoCapability : uint8_t {
        /**
         * @brief The STM32 can show a passkey (PasskeyDisplayEvent) but not take one.
         */
        DisplayOnly = ESP_IO_CAP_OUT,
        /**
         * @brief The STM32 can show a number and ask the user whether it matches (NumericComparisonRequestEvent).
         */
        DisplayYesNo = ESP_IO_CAP_IO,
        /**
         * @brief The STM32 can take a passkey from the user (PasskeyRequestEvent) but not show one.
         */
        KeyboardOnly = ESP_IO_CAP_IN,
        /**
         * @brief No input or output: every pairing is Just Works, which cannot protect against a man in the middle.
         */
        NoInputNoOutput = ESP_IO_CAP_NONE,
        /**
         * @brief The STM32 can show a passkey and take one.
         */
        KeyboardDisplay = ESP_IO_CAP_KBDISP,
    };

    /**
     * @brief Size of the encryption key, in bytes, that pairing must agree.  The largest allowed.
     */
    static constexpr uint8_t KEY_SIZE = 16;

    /**
     * @brief Largest passkey the user can be asked to enter: six decimal digits.
     */
    static constexpr uint32_t MAXIMUM_PASSKEY = 999999;

    /**
     * @brief Input and output this device can use to authenticate a pairing.
     */
    IoCapability Capability = IoCapability::NoInputNoOutput;

    /**
     * @brief Store the keys from pairing so that the client can reconnect without pairing again.
     */
    bool Bonding = true;

    /**
     * @brief Refuse a pairing that does not protect against a man in the middle.
     *
     *  Needs a Capability other than NoInputNoOutput.  Implies Secure Connections, as legacy
     *  passkey pairing can be broken by anyone who overhears it.
     */
    bool Mitm = false;

    /**
     * @brief Refuse legacy pairing; only LE Secure Connections is accepted.
     */
    bool SecureConnectionsOnly = false;

    /**
     * @brief Ask the client to encrypt the link as soon as it connects.
     *
     *  A bonded client encrypts with its stored keys; any other client is asked to pair.  Without
     *  this the link is encrypted only when the client asks, which a client does when it is
     *  refused access to an attribute that needs encryption.
     */
    bool EncryptOnConnect = false;

    /**
     * @brief Let a client that is not bonded pair.
     *
     *  When false, only clients already bonded can encrypt the link.
     */
    bool AcceptPairing = true;

    /**
     * @brief Read the security settings from the configuration's "security" object.
     *
     *  Every setting is optional and takes its default when absent.  The settings are kept only if
     *  all of them are valid, otherwise this object is left with the defaults.
     *
     * @param security The "security" object, or nullptr if the configuration has none.
     *
     * @return true if the settings were valid (or absent), false if any was not.
     */
    bool Parse(const cJSON * const security);

    /**
     * @brief Put every setting back to its default.
     */
    void SetDefaults();

    /**
     * @brief Authentication requirements to give the stack, ESP_LE_AUTH_* bits.
     *
     *  Secure Connections is always offered.  Bonding and MITM protection are asked for as configured.
     *
     * @return The value for ESP_BLE_SM_AUTHEN_REQ_MODE.
     */
    esp_ble_auth_req_t AuthenticationRequirement() const;

    /**
     * @brief Whether the stack should refuse a pairing that does not meet AuthenticationRequirement().
     *
     *  Without this the stack accepts whatever the client negotiates, so a request for MITM
     *  protection is quietly downgraded to Just Works (issue #398).  With it, a pairing that does not
     *  bond is refused too when Bonding is set.
     *
     * @return The value for ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH.
     */
    uint8_t OnlyAcceptSpecifiedAuthentication() const;

    /**
     * @brief Level of security to ask for when EncryptOnConnect is set.
     *
     * @return ESP_BLE_SEC_ENCRYPT_MITM when Mitm is set, ESP_BLE_SEC_ENCRYPT_NO_MITM otherwise.
     */
    esp_ble_sec_act_t EncryptionLevel() const;

    /**
     * @brief Does an attribute with this permission need a pairing protected against a man in the middle?
     *
     * @param permission Permission of the attribute.
     *
     * @return true if any read or write needs MITM protection.
     */
    static bool PermissionRequiresMitm(const CharacteristicPermission permission);

    /**
     * @brief Does an attribute with this permission need an encrypted link?
     *
     * @param permission Permission of the attribute.
     *
     * @return true if any read or write needs encryption, with or without MITM protection.
     */
    static bool PermissionRequiresEncryption(const CharacteristicPermission permission);

    /**
     * @brief Permission for the Client Characteristic Configuration descriptor of a characteristic.
     *
     *  A client that subscribes is sent every new value, so subscribing has to need the same
     *  security as the value does.  Otherwise a client that has not paired could subscribe and be
     *  sent a value it is not allowed to read.
     *
     * @param descriptorPermission Permission the descriptor has now.
     * @param valuePermission Permission of the characteristic's value.
     *
     * @return descriptorPermission with the write security the value needs added.
     */
    static CharacteristicPermission ClientConfigurationPermission(const CharacteristicPermission descriptorPermission, const CharacteristicPermission valuePermission);

    /**
     * @brief Is this a passkey the user could have entered?
     *
     * @param passkey The passkey.
     *
     * @return true if it is at most six decimal digits.
     */
    static bool IsValidPasskey(const uint32_t passkey)
    {
        return (passkey <= MAXIMUM_PASSKEY);
    }
};
