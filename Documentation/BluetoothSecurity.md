# Bluetooth Pairing and Bonding

This describes how a Bluetooth client pairs and bonds with the ESP32, what the STM32 configures,
and the messages the two exchange while it happens (issue #398).

The ESP32 is always the peripheral and the client (a phone or a computer) the central. Pairing
uses ESP-IDF's Bluedroid security manager. Bonds, the keys kept after pairing so that a client can
reconnect without pairing again, are stored by Bluedroid in the `nvs` partition and survive a
restart. Up to 15 are kept (`CONFIG_BT_SMP_MAX_BONDS`).

## Security settings

The STM32 configures security in an optional `security` object in the JSON configuration it sends
with `Start`, alongside `deviceName` and `services`:

```json
{
    "deviceName": "MeadowSensor",
    "security": {
        "ioCapability": 1,
        "bonding": true,
        "mitm": true,
        "secureConnectionsOnly": true,
        "encryptOnConnect": true,
        "acceptPairing": true
    },
    "services": [ ... ]
}
```

Every setting is optional. A configuration without a `security` object gets the defaults, which
match how the service behaved before security could be configured, so an STM32 application that
knows nothing of pairing keeps working.

| Setting | Type | Default | Meaning |
|---|---|---|---|
| `ioCapability` | 0 to 4 | 3 | What the STM32 can show and take from the user (below) |
| `bonding` | bool | `true` | Keep the keys from pairing so the client can reconnect without pairing again |
| `mitm` | bool | `false` | Refuse a pairing that does not protect against a man in the middle |
| `secureConnectionsOnly` | bool | `false` | Refuse legacy pairing; only LE Secure Connections is accepted |
| `encryptOnConnect` | bool | `false` | Ask the client to encrypt the link as soon as it connects |
| `acceptPairing` | bool | `true` | Let a client that is not bonded pair; when false only bonded clients can encrypt |

`ioCapability` uses the Bluetooth Core Specification's values:

| Value | Capability | What the STM32 does during pairing |
|---|---|---|
| 0 | DisplayOnly | Shows a passkey (`PasskeyDisplayEvent`) |
| 1 | DisplayYesNo | Shows a number and asks whether it matches (`NumericComparisonRequestEvent`) |
| 2 | KeyboardOnly | Asks the user for a passkey (`PasskeyRequestEvent`) |
| 3 | NoInputNoOutput | Nothing; every pairing is Just Works |
| 4 | KeyboardDisplay | Either of the passkey events, or numeric comparison, as the client's capability decides |

The configuration is refused, and `Start` answered `InvalidConfigurationFile`, when:

- a setting has the wrong type, or `ioCapability` is not a whole number from 0 to 4;
- `mitm` is true with `ioCapability` 3, as Just Works cannot protect against a man in the middle;
- a characteristic or descriptor has a permission that needs MITM protection (`ReadEncMITM` 4,
  `WriteEncMITM` 64 or `WriteSignedMITM` 256) and `mitm` is not true, as no pairing could then reach it.

Some consequences of the settings:

- **Secure Connections is always offered.** `mitm` implies it: the stack then refuses legacy
  pairing, because legacy passkey pairing can be broken by anyone who overhears it.
- **`mitm` or `secureConnectionsOnly` make the stack refuse a pairing that does not meet every
  requirement asked for.** With `bonding` true as well, a client that will not bond is refused too.
  Phones and computers bond.
- **The encryption key is always 16 bytes.**
- **There is no static passkey and no out of band pairing.** Every passkey is random and chosen
  by the stack for each pairing.

### Attribute permissions

The permissions of characteristics and descriptors are the GATT permission bits, as before:

| Bit | Permission |
|---|---|
| 1 | Read |
| 2 | ReadEncrypted |
| 4 | ReadEncMITM |
| 16 | Write |
| 32 | WriteEncrypted |
| 64 | WriteEncMITM |
| 128 | WriteSigned |
| 256 | WriteSignedMITM |

The stack refuses a read or write of an attribute until the link has the security its permission
needs. The client is told its access was refused for lack of authentication, which makes phones
and computers pair and try again. So a characteristic with permission `ReadEncrypted | WriteEncrypted`
(34) is enough to make a client pair, without `encryptOnConnect`.

Two things are done to the permissions so that this works:

- A characteristic's declaration, the attribute that tells a client the characteristic exists, is
  always readable without security. It used to take the characteristic's permission, so a client
  that had not paired could not find a protected characteristic.
- Subscribing needs the security the value does. A notification is sent to whoever subscribed,
  so the Client Characteristic Configuration descriptor of a characteristic whose value needs
  encryption gets `WriteEncrypted`, and one whose value needs MITM protection gets `WriteEncMITM`.
  This applies to a descriptor the configuration declares as well as one the ESP32 adds.

## Messages

All are on the Bluetooth interface (2). Addresses are six bytes, most significant first, as an
address is usually written. Numbers are little endian.

### Requests from the STM32

| Function | Value | Payload | Response |
|---|---|---|---|
| `PasskeyReply` | 11 | `BTPairingReply` | Status only |
| `NumericComparisonReply` | 12 | `BTPairingReply` | Status only |
| `GetBondedDevices` | 13 | None | `BTBondedDevicesResponse` |
| `RemoveBond` | 14 | `BTDeviceAddress` | Status only |
| `RemoveAllBonds` | 15 | None | Status only |

`BTPairingReply` (11 bytes): the client's address (6), accept (1, non-zero to go on with the
pairing), passkey (4). `NumericComparisonReply` ignores the passkey.

`BTDeviceAddress` (6 bytes): the address.

`BTBondedDevicesResponse`: the number of devices (2), the length of the list in bytes (4), then
for each device its address (6) and address type (1: 0 public, 1 random). This is laid out like
the `GetHandles` response.

| Status | Value | When |
|---|---|---|
| `CompletedOk` | 0 | Done |
| `InvalidRequest` | 61 | The payload is missing or too short; a reply that answers no question the ESP32 asked, or asks about another client; a passkey of more than six digits |
| `BluetoothNotStarted` | 63 | The Bluetooth service is not running. The bonds are kept by the stack, so it has to be running to list or remove them |
| `BluetoothDeviceNotBonded` | 64 | `RemoveBond` named a device that is not bonded |
| `Timeout` | 6 | The stack did not say within 2 seconds whether it removed a bond |
| `Failure` | 3 | The stack refused |

A reply that is refused as `InvalidRequest` because of its passkey leaves the question open, so a
corrected reply can follow. Removing the bond of a client that is connected disconnects it.
`RemoveAllBonds` tries every bond even after one fails, and reports the first failure.

### Events to the STM32

| Event | Value | Payload | Status |
|---|---|---|---|
| `PasskeyDisplayEvent` | 16 | `BTPairingEventData` | `CompletedOk` |
| `PasskeyRequestEvent` | 17 | `BTPairingEventData`, passkey 0 | `CompletedOk` |
| `NumericComparisonRequestEvent` | 18 | `BTPairingEventData` | `CompletedOk` |
| `AuthenticationCompleteEvent` | 19 | `BTAuthenticationCompleteEventData` | `CompletedOk` or `Failure` |

`BTPairingEventData` (10 bytes): the client's address (6), the passkey or number to show (4).

`BTAuthenticationCompleteEventData` (10 bytes): the client's address (6), address type (1),
authentication mode (1, the `ESP_LE_AUTH_*` bits achieved: 0x01 bonded, 0x04 MITM protected, 0x08
Secure Connections), failure reason (1, the SMP reason when the status is `Failure`, otherwise 0),
bonded (1, non-zero when the client is now in the bond list).

### A pairing step by step

1. The client asks to pair, or `encryptOnConnect` asks it to. A client that is not bonded is
   refused at once when `acceptPairing` is false.
2. Depending on both sides' capabilities, the stack chooses Just Works, numeric comparison or
   passkey entry.
   - Just Works needs nothing from the STM32.
   - Numeric comparison: the ESP32 sends `NumericComparisonRequestEvent`. The STM32 shows the
     number, asks the user whether the client shows the same one, and sends
     `NumericComparisonReply`.
   - The client shows a passkey: the ESP32 sends `PasskeyRequestEvent`. The STM32 asks the user
     for it and sends `PasskeyReply`.
   - The STM32 shows a passkey: the ESP32 sends `PasskeyDisplayEvent`. The user types it into the
     client; no reply is needed.
3. The ESP32 sends `AuthenticationCompleteEvent`.

The client must finish within the stack's 30 second pairing timeout, so the STM32 has that long to
reply. Only the reply to the question asked, for the client it was asked about, is passed to the
stack. A question is closed when it is answered, when pairing ends and when a client connects or
disconnects.

A bonded client that reconnects encrypts with its stored keys without pairing, and the ESP32 sends
`AuthenticationCompleteEvent` for that too, with bonded set.

### Subscriptions of bonded clients

A bonded client's subscriptions last from one connection to the next (Bluetooth Core Specification,
Vol 3, Part G, 3.3.3.3). When a bonded client disconnects the ESP32 keeps the values of its Client
Characteristic Configuration descriptors, and puts them back when it reconnects and has encrypted
the link, proving it is that client. Any other client starts each connection unsubscribed.

These are kept in memory, not in flash, and forgotten when the service stops, because the attribute
handles they refer to can differ in the next configuration. A client that has been forgotten in this
way subscribes again, as phones and computers do when they reconnect.

## Limitations

- **The bond keys are stored in the `nvs` partition unencrypted.** NVS encryption is planned work.
- **One client at a time.** The service accepts one connection, so one pairing is in progress at a time.
- **With `acceptPairing` false, a bonded client that has lost its keys cannot pair again** until
  the service is started with pairing accepted.
- **A client whose bond the ESP32 has removed** still has its own copy of the keys and will not
  pair again until the user removes the device from the client's Bluetooth settings. macOS refuses
  to connect with CoreBluetooth error 14, "Peer removed pairing information". The other way round
  works: a client that has forgotten the device simply pairs again, and the new bond replaces the
  ESP32's old one.
- **macOS asks the user to confirm even a Just Works pairing**, so a product with no input or
  output still needs someone at the client to accept it.
