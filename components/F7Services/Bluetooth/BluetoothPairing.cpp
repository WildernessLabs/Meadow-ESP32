/**
 * @file BluetoothPairing.cpp
 * @author Mark Stevens
 * @brief Implementation of the BluetoothPairing class.
 * @date 2026-10-30
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#include <string.h>

#include "BluetoothPairing.hpp"
#include "MutexLock.hpp"
#include "Utils.hpp"

/**
 * @brief Construct a new BluetoothPairing object.
 */
BluetoothPairing::BluetoothPairing()
{
    _mutex = Utils::RequireCreated(xSemaphoreCreateMutex(), "the Bluetooth pairing mutex");
}

/**
 * @brief Destroy the BluetoothPairing object.
 */
BluetoothPairing::~BluetoothPairing()
{
    vSemaphoreDelete(_mutex);
}

/**
 * @brief Make a map key from an address.
 *
 * @param address The address.
 *
 * @return The key.
 */
BluetoothPairing::AddressKey BluetoothPairing::KeyFor(const esp_bd_addr_t address)
{
    AddressKey key;
    memcpy(key.data(), address, key.size());
    return (key);
}

/**
 * @brief Record that the STM32 has been asked a question about pairing with a client.
 *
 * @param reply Reply that is now due.
 * @param address Client the question is about.
 */
void BluetoothPairing::Expect(const PendingReply reply, const esp_bd_addr_t address)
{
    MutexLock lock(_mutex);
    _pendingReply = reply;
    memcpy(_pendingAddress, address, sizeof(_pendingAddress));
}

/**
 * @brief Accept a reply from the STM32 if it answers the question asked.
 *
 * @param reply Kind of reply the STM32 sent.
 * @param address Client the reply is about.
 *
 * @return true if the reply was due, in which case it is no longer due.
 */
bool BluetoothPairing::Take(const PendingReply reply, const esp_bd_addr_t address)
{
    MutexLock lock(_mutex);
    if ((reply == PendingReply::None) || (reply != _pendingReply) || (memcmp(address, _pendingAddress, sizeof(_pendingAddress)) != 0))
    {
        return (false);
    }
    _pendingReply = PendingReply::None;
    memset(_pendingAddress, 0, sizeof(_pendingAddress));
    return (true);
}

/**
 * @brief Forget any reply still due.
 */
void BluetoothPairing::ClearPendingReply()
{
    MutexLock lock(_mutex);
    _pendingReply = PendingReply::None;
    memset(_pendingAddress, 0, sizeof(_pendingAddress));
}

/**
 * @brief Get the reply that is due.
 *
 * @return The reply, PendingReply::None if nothing is waiting for the STM32.
 */
BluetoothPairing::PendingReply BluetoothPairing::GetPendingReply()
{
    MutexLock lock(_mutex);
    return (_pendingReply);
}

/**
 * @brief Record that the connected client is bonded, under the address it is bonded with.
 *
 * @param address Address the client is bonded with.
 */
void BluetoothPairing::SetBondedClient(const esp_bd_addr_t address)
{
    MutexLock lock(_mutex);
    memcpy(_bondedClient, address, sizeof(_bondedClient));
    _hasBondedClient = true;
}

/**
 * @brief Get the address of the connected client, if it is bonded.
 *
 * @param address Set to the address when there is a bonded client.
 *
 * @return true if the connected client is bonded.
 */
bool BluetoothPairing::GetBondedClient(esp_bd_addr_t address)
{
    MutexLock lock(_mutex);
    if (_hasBondedClient)
    {
        memcpy(address, _bondedClient, sizeof(_bondedClient));
    }
    return (_hasBondedClient);
}

/**
 * @brief Forget the connected client.
 */
void BluetoothPairing::ClearBondedClient()
{
    MutexLock lock(_mutex);
    _hasBondedClient = false;
    memset(_bondedClient, 0, sizeof(_bondedClient));
}

/**
 * @brief Keep a bonded client's subscriptions for when it reconnects.
 *
 * @param address Address the client is bonded with.
 * @param configurations Value of every Client Characteristic Configuration descriptor.
 */
void BluetoothPairing::SaveClientConfigurations(const esp_bd_addr_t address, const std::vector<ClientConfiguration> &configurations)
{
    std::vector<ClientConfiguration> subscribed;
    for (const ClientConfiguration &configuration: configurations)
    {
        if (configuration.Value != 0)
        {
            subscribed.push_back(configuration);
        }
    }

    MutexLock lock(_mutex);
    if (subscribed.empty())
    {
        _clientConfigurations.erase(KeyFor(address));
    }
    else
    {
        _clientConfigurations[KeyFor(address)] = subscribed;
    }
}

/**
 * @brief Get the subscriptions a bonded client had when it last disconnected.
 *
 * @param address Address the client is bonded with.
 *
 * @return The subscriptions, empty if there are none.
 */
std::vector<BluetoothPairing::ClientConfiguration> BluetoothPairing::GetClientConfigurations(const esp_bd_addr_t address)
{
    MutexLock lock(_mutex);
    const auto found = _clientConfigurations.find(KeyFor(address));
    if (found == _clientConfigurations.end())
    {
        return (std::vector<ClientConfiguration>());
    }
    return (found->second);
}

/**
 * @brief Forget the subscriptions of a client whose bond has been removed.
 *
 * @param address Address the client was bonded with.
 */
void BluetoothPairing::ForgetClient(const esp_bd_addr_t address)
{
    MutexLock lock(_mutex);
    _clientConfigurations.erase(KeyFor(address));
    if (_hasBondedClient && (memcmp(address, _bondedClient, sizeof(_bondedClient)) == 0))
    {
        //
        //  Still connected but no longer bonded, so its subscriptions are not kept when it goes.
        //
        _hasBondedClient = false;
        memset(_bondedClient, 0, sizeof(_bondedClient));
    }
}

/**
 * @brief Forget everything.
 */
void BluetoothPairing::Reset()
{
    MutexLock lock(_mutex);
    _pendingReply = PendingReply::None;
    memset(_pendingAddress, 0, sizeof(_pendingAddress));
    _hasBondedClient = false;
    memset(_bondedClient, 0, sizeof(_bondedClient));
    _clientConfigurations.clear();
}
