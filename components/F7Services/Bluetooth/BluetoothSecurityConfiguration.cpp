/**
 * @file BluetoothSecurityConfiguration.cpp
 * @author Mark Stevens
 * @brief Implementation of the BluetoothSecurityConfiguration class.
 * @date 2026-10-30
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include "BluetoothSecurityConfiguration.hpp"
#include "BluetoothComponentName.hpp"
#include "Logging.hpp"

/**
 * @brief Name of the component (used for logging).
 */
const char *BluetoothSecurityConfiguration::COMPONENT_NAME = BLUETOOTH_COMPONENT_NAME;

/**
 * @brief Read an optional true or false setting.
 *
 * @param security The "security" object.
 * @param key Name of the setting.
 * @param value Set to the setting when it is present and valid, otherwise left as it is.
 *
 * @return true if the setting is absent or is true or false, false if it is anything else.
 */
static bool ParseBoolean(const cJSON * const security, const char * const key, bool &value)
{
    const cJSON * const item = cJSON_GetObjectItem(security, key);
    if (item == nullptr)
    {
        return (true);
    }
    if (!cJSON_IsBool(item))
    {
        ERROR_MESSAGE_SPECIFY_COMPONENT(BLUETOOTH_COMPONENT_NAME, "Security setting %s is not true or false.", key);
        return (false);
    }
    value = cJSON_IsTrue(item);
    return (true);
}

/**
 * @brief Put every setting back to its default.
 */
void BluetoothSecurityConfiguration::SetDefaults()
{
    *this = BluetoothSecurityConfiguration();
}

/**
 * @brief Read the security settings from the configuration's "security" object.
 *
 * @param security The "security" object, or nullptr if the configuration has none.
 *
 * @return true if the settings were valid (or absent), false if any was not.
 */
bool BluetoothSecurityConfiguration::Parse(const cJSON * const security)
{
    SetDefaults();
    if (security == nullptr)
    {
        return (true);
    }
    if (!cJSON_IsObject(security))
    {
        ERROR_MESSAGE(SECURITY_KEY " is not an object.");
        return (false);
    }

    BluetoothSecurityConfiguration parsed;
    bool valid = true;

    const cJSON * const capability = cJSON_GetObjectItem(security, IO_CAPABILITY_KEY);
    if (capability != nullptr)
    {
        //
        //  A number such as 1.5 is refused rather than truncated, as is anything outside the five
        //  capabilities the Bluetooth Core Specification defines.
        //
        if (!cJSON_IsNumber(capability) || (capability->valuedouble != static_cast<double>(capability->valueint)) || (capability->valueint < static_cast<int>(IoCapability::DisplayOnly)) || (capability->valueint > static_cast<int>(IoCapability::KeyboardDisplay)))
        {
            ERROR_MESSAGE("Security setting " IO_CAPABILITY_KEY " is not a number from 0 to 4.");
            valid = false;
        }
        else
        {
            parsed.Capability = static_cast<IoCapability>(capability->valueint);
        }
    }
    valid = ParseBoolean(security, BONDING_KEY, parsed.Bonding) && valid;
    valid = ParseBoolean(security, MITM_KEY, parsed.Mitm) && valid;
    valid = ParseBoolean(security, SECURE_CONNECTIONS_ONLY_KEY, parsed.SecureConnectionsOnly) && valid;
    valid = ParseBoolean(security, ENCRYPT_ON_CONNECT_KEY, parsed.EncryptOnConnect) && valid;
    valid = ParseBoolean(security, ACCEPT_PAIRING_KEY, parsed.AcceptPairing) && valid;

    //
    //  With no input or output every pairing is Just Works, so MITM protection can never be had.
    //  Asking for it used to be quietly downgraded (issue #398); now the configuration is refused.
    //
    if (valid && parsed.Mitm && (parsed.Capability == IoCapability::NoInputNoOutput))
    {
        ERROR_MESSAGE("Security setting " MITM_KEY " needs an " IO_CAPABILITY_KEY " other than NoInputNoOutput (3).");
        valid = false;
    }

    if (valid)
    {
        *this = parsed;
    }
    return (valid);
}

/**
 * @brief Authentication requirements to give the stack, ESP_LE_AUTH_* bits.
 *
 * @return The value for ESP_BLE_SM_AUTHEN_REQ_MODE.
 */
esp_ble_auth_req_t BluetoothSecurityConfiguration::AuthenticationRequirement() const
{
    //
    //  ESP_LE_AUTH_REQ_SC_ONLY is the Secure Connections bit of the pairing request.  On its own it
    //  only offers Secure Connections; OnlyAcceptSpecifiedAuthentication() is what makes it a
    //  requirement.
    //
    esp_ble_auth_req_t requirement = ESP_LE_AUTH_REQ_SC_ONLY;
    if (Bonding)
    {
        requirement |= ESP_LE_AUTH_BOND;
    }
    if (Mitm)
    {
        requirement |= ESP_LE_AUTH_REQ_MITM;
    }
    return (requirement);
}

/**
 * @brief Whether the stack should refuse a pairing that does not meet AuthenticationRequirement().
 *
 * @return The value for ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH.
 */
uint8_t BluetoothSecurityConfiguration::OnlyAcceptSpecifiedAuthentication() const
{
    return ((Mitm || SecureConnectionsOnly) ? ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE : ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE);
}

/**
 * @brief Level of security to ask for when EncryptOnConnect is set.
 *
 * @return ESP_BLE_SEC_ENCRYPT_MITM when Mitm is set, ESP_BLE_SEC_ENCRYPT_NO_MITM otherwise.
 */
esp_ble_sec_act_t BluetoothSecurityConfiguration::EncryptionLevel() const
{
    return (Mitm ? ESP_BLE_SEC_ENCRYPT_MITM : ESP_BLE_SEC_ENCRYPT_NO_MITM);
}

/**
 * @brief Does an attribute with this permission need a pairing protected against a man in the middle?
 *
 * @param permission Permission of the attribute.
 *
 * @return true if any read or write needs MITM protection.
 */
bool BluetoothSecurityConfiguration::PermissionRequiresMitm(const CharacteristicPermission permission)
{
    const uint16_t mitm = static_cast<uint16_t>(CharacteristicPermission::ReadEncMITM) | static_cast<uint16_t>(CharacteristicPermission::WriteEncMITM) | static_cast<uint16_t>(CharacteristicPermission::WriteSignedMITM);
    return ((static_cast<uint16_t>(permission) & mitm) != 0);
}

/**
 * @brief Does an attribute with this permission need an encrypted link?
 *
 * @param permission Permission of the attribute.
 *
 * @return true if any read or write needs encryption, with or without MITM protection.
 */
bool BluetoothSecurityConfiguration::PermissionRequiresEncryption(const CharacteristicPermission permission)
{
    const uint16_t encrypted = static_cast<uint16_t>(CharacteristicPermission::ReadEncrypted) | static_cast<uint16_t>(CharacteristicPermission::WriteEncrypted);
    return (((static_cast<uint16_t>(permission) & encrypted) != 0) || PermissionRequiresMitm(permission));
}

/**
 * @brief Permission for the Client Characteristic Configuration descriptor of a characteristic.
 *
 * @param descriptorPermission Permission the descriptor has now.
 * @param valuePermission Permission of the characteristic's value.
 *
 * @return descriptorPermission with the write security the value needs added.
 */
CharacteristicPermission BluetoothSecurityConfiguration::ClientConfigurationPermission(const CharacteristicPermission descriptorPermission, const CharacteristicPermission valuePermission)
{
    CharacteristicPermission result = descriptorPermission;
    if (PermissionRequiresMitm(valuePermission))
    {
        result = result | CharacteristicPermission::WriteEncMITM;
    }
    else
    {
        if (PermissionRequiresEncryption(valuePermission))
        {
            result = result | CharacteristicPermission::WriteEncrypted;
        }
    }
    return (result);
}
