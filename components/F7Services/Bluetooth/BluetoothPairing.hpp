/**
 * @file BluetoothPairing.hpp
 * @author Mark Stevens
 * @brief State of pairing with, and bonds to, Bluetooth clients.
 * @date 2026-10-30
 *
 * @copyright Copyright (c) 2026 Wilderness Labs
 */

#pragma once

#include <stdint.h>
#include <array>
#include <map>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_bt_defs.h"

/**
 * @brief State of pairing with, and bonds to, Bluetooth clients.
 *
 *  Two things are kept here, both shared between the Bluedroid task, which reports what a client
 *  does, and the Bluetooth task, which carries out requests from the STM32:
 *
 *  - The reply the STM32 owes for a pairing in progress.  When pairing needs the user, the
 *    STM32 is sent an event and answers later with a request.  Only the answer to the question
 *    asked, for the client it was asked about, is passed to the stack.
 *
 *  - The Client Characteristic Configuration values each bonded client had when it disconnected.
 *    A bonded client's subscriptions last from one connection to the next (Bluetooth Core
 *    Specification, Vol 3, Part G, 3.3.3.3), so they are put back when it reconnects and
 *    encrypts the link.  They are kept in memory only and forgotten when the service stops,
 *    as the attribute handles they refer to may change.
 */
class BluetoothPairing
{
public:
    /**
     * @brief Reply the STM32 owes for a pairing in progress.
     */
    enum class PendingReply : uint8_t {
        /**
         * @brief Nothing is waiting for the STM32.
         */
        None = 0,
        /**
         * @brief A PasskeyRequestEvent was sent; a PasskeyReply is due.
         */
        Passkey = 1,
        /**
         * @brief A NumericComparisonRequestEvent was sent; a NumericComparisonReply is due.
         */
        NumericComparison = 2,
    };

    /**
     * @brief Value of one Client Characteristic Configuration descriptor.
     */
    struct ClientConfiguration
    {
        /**
         * @brief Handle of the descriptor.
         */
        uint16_t Handle;

        /**
         * @brief Value of the descriptor, notifications in bit 0 and indications in bit 1.
         */
        uint16_t Value;
    };

    /**
     * @brief Construct a new BluetoothPairing object.
     */
    BluetoothPairing();

    /**
     * @brief Destroy the BluetoothPairing object.
     */
    ~BluetoothPairing();

    /**
     * @brief Copying would share the mutex, so it is not allowed.
     */
    BluetoothPairing(const BluetoothPairing &) = delete;

    /**
     * @brief Copying would share the mutex, so it is not allowed.
     */
    BluetoothPairing &operator=(const BluetoothPairing &) = delete;

    /**
     * @brief Record that the STM32 has been asked a question about pairing with a client.
     *
     *  Replaces any question still unanswered: the stack pairs with one client at a time.
     *
     * @param reply Reply that is now due.
     * @param address Client the question is about.
     */
    void Expect(const PendingReply reply, const esp_bd_addr_t address);

    /**
     * @brief Accept a reply from the STM32 if it answers the question asked.
     *
     * @param reply Kind of reply the STM32 sent.
     * @param address Client the reply is about.
     *
     * @return true if the reply was due, in which case it is no longer due; false if it was not,
     *         in which case whatever is due stays due.
     */
    bool Take(const PendingReply reply, const esp_bd_addr_t address);

    /**
     * @brief Forget any reply still due, as when pairing has finished or the client has gone.
     */
    void ClearPendingReply();

    /**
     * @brief Get the reply that is due.
     *
     * @return The reply, PendingReply::None if nothing is waiting for the STM32.
     */
    PendingReply GetPendingReply();

    /**
     * @brief Record that the connected client is bonded, under the address it is bonded with.
     *
     * @param address Address the client is bonded with.
     */
    void SetBondedClient(const esp_bd_addr_t address);

    /**
     * @brief Get the address of the connected client, if it is bonded.
     *
     * @param address Set to the address when there is a bonded client.
     *
     * @return true if the connected client is bonded.
     */
    bool GetBondedClient(esp_bd_addr_t address);

    /**
     * @brief Forget the connected client, as when it disconnects.
     */
    void ClearBondedClient();

    /**
     * @brief Keep a bonded client's subscriptions for when it reconnects.
     *
     *  Only subscriptions that are switched on are kept: one that is off is what a reconnecting
     *  client starts with anyway.
     *
     * @param address Address the client is bonded with.
     * @param configurations Value of every Client Characteristic Configuration descriptor.
     */
    void SaveClientConfigurations(const esp_bd_addr_t address, const std::vector<ClientConfiguration> &configurations);

    /**
     * @brief Get the subscriptions a bonded client had when it last disconnected.
     *
     * @param address Address the client is bonded with.
     *
     * @return The subscriptions, empty if there are none.
     */
    std::vector<ClientConfiguration> GetClientConfigurations(const esp_bd_addr_t address);

    /**
     * @brief Forget the subscriptions of a client whose bond has been removed.
     *
     * @param address Address the client was bonded with.
     */
    void ForgetClient(const esp_bd_addr_t address);

    /**
     * @brief Forget everything: no reply is due, no client is connected and no subscriptions are kept.
     */
    void Reset();

private:
    /**
     * @brief Address as a key for the map of subscriptions.
     */
    using AddressKey = std::array<uint8_t, ESP_BD_ADDR_LEN>;

    /**
     * @brief Make a map key from an address.
     *
     * @param address The address.
     *
     * @return The key.
     */
    static AddressKey KeyFor(const esp_bd_addr_t address);

    /**
     * @brief Guards everything below; used by the Bluedroid task and the Bluetooth task.
     */
    SemaphoreHandle_t _mutex;

    /**
     * @brief Reply the STM32 owes.
     */
    PendingReply _pendingReply = PendingReply::None;

    /**
     * @brief Client the pending reply is about.
     */
    esp_bd_addr_t _pendingAddress = {};

    /**
     * @brief Is the connected client bonded?
     */
    bool _hasBondedClient = false;

    /**
     * @brief Address the connected client is bonded with, when _hasBondedClient is set.
     */
    esp_bd_addr_t _bondedClient = {};

    /**
     * @brief Subscriptions each bonded client had when it last disconnected.
     */
    std::map<AddressKey, std::vector<ClientConfiguration>> _clientConfigurations;
};
