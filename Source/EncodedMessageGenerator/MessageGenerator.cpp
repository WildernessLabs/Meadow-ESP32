#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 *      Seed for the 32-bit CRC calculation.
 */
#define ESPCP_CRC32_SEED 0xffffffff

/*
 *      Offset of the CRC into the message header.
 */
#define ESPCP_CRC_OFFSET 18

/*
 *      Number of bytes actually used in a message header.
 */
#define ESPCP_HEADER_SIZE 23


/*
 *  Size of an encoded message header (in bytes).
 */
static const uint32_t ESPCP_MESSAGE_HEADER_SIZE = 23;

/*
 *  Offset of the CRC in an encoded message header.
 */
static const uint32_t ESPCP_MESSAGE_CRC_OFFSET = 18;

/*
 *  Message ID used to indicate an invalid (or unknown) message ID.
 */
static const uint32_t ESPCP_MESSAGE_INVALID_MESSAGE_ID = 0xffffffff;

/*
 *    Status and error codes for the various function calls.
 */
enum espcp_status_codes
{
    espcp_status_codes_completed_ok = 0,
    espcp_status_codes_crc_error = 1,
    espcp_status_codes_restart = 2,
    espcp_status_codes_failure = 3,
    espcp_status_codes_invalid_interface = 4,
    espcp_status_codes_queue_error = 5,
    espcp_status_codes_invalid_packet = 6,
    espcp_status_codes_invalid_header = 7,
    espcp_status_codes_unexpected_data = 8,
    espcp_status_codes_missing_end_of_frame_marker = 9,
    espcp_status_codes_header_body_field_mismatch = 10,
    espcp_status_codes_wi_fi_already_started = 11,
    espcp_status_codes_invalid_ssid = 12,
    espcp_status_codes_invalid_password = 13,
    espcp_status_codes_wi_fi_disconnected = 14,
    espcp_status_codes_invalid_antenna_data = 15,
    espcp_status_codes_invalid_antenna_value = 16,
    espcp_status_codes_no_messages_waiting = 17
};
typedef enum espcp_status_codes espcp_status_codes_t;

/*
 *    System interfaces available on the ESP32 for the various function calls.
 */
enum espcp_esp32_interfaces
{
    espcp_esp32_interfaces_none = 0,
    espcp_esp32_interfaces_wi_fi = 1,
    espcp_esp32_interfaces_blue_tooth = 2,
    espcp_esp32_interfaces_mesh_network = 3,
    espcp_esp32_interfaces_system = 4,
    espcp_esp32_interfaces_transport = 5
};
typedef enum espcp_esp32_interfaces espcp_esp32_interfaces_t;

/*
 *    System functions available on the ESP32
 */
enum espcp_system_function
{
    espcp_system_function_get_configuration = 0,
    espcp_system_function_set_configuration = 1,
    espcp_system_function_deep_sleep = 2,
    espcp_system_function_battery_level = 3
};
typedef enum espcp_system_function espcp_system_function_t;

/*
 *    WiFi functions available on the ESP32
 */
enum espcp_wi_fi_function
{
    espcp_wi_fi_function_start = 0,
    espcp_wi_fi_function_stop = 1,
    espcp_wi_fi_function_connect_to_ap = 2,
    espcp_wi_fi_function_disconnect = 3,
    espcp_wi_fi_function_get_access_points = 4,
    espcp_wi_fi_function_get_configuration = 5,
    espcp_wi_fi_function_set_configuration = 6,
    espcp_wi_fi_function_set_antenna = 7,
    espcp_wi_fi_function_get_antenna = 8,
    espcp_wi_fi_function_get_addr_info = 9,
    espcp_wi_fi_function_socket = 10,
    espcp_wi_fi_function_connect = 11,
    espcp_wi_fi_function_free_addr_info = 12,
    espcp_wi_fi_function_write = 13,
    espcp_wi_fi_function_set_sock_opt = 14,
    espcp_wi_fi_function_read = 15,
    espcp_wi_fi_function_close = 16
};
typedef enum espcp_wi_fi_function espcp_wi_fi_function_t;

/*
 *    Bluetooth functions available on the ESP32
 */
enum espcp_bluetooth_function
{
    espcp_bluetooth_function_get_configuration = 0,
    espcp_bluetooth_function_set_configuration = 1
};
typedef enum espcp_bluetooth_function espcp_bluetooth_function_t;

/*
 *    Transport functions available on the ESP32
 */
enum espcp_transport_function
{
    espcp_transport_function_response_ready = 0,
    espcp_transport_function_send_response = 1,
    espcp_transport_function_kill_nuttx_thread = 2
};
typedef enum espcp_transport_function espcp_transport_function_t;

/*
 *    Possible transport packet types.
 */
enum espcp_message_types
{
    espcp_message_types_ack = 0x00,
    espcp_message_types_nak = 0x01,
    espcp_message_types_reset = 0x02,
    espcp_message_types_ready = 0x04,
    espcp_message_types_response = 0x10,
    espcp_message_types_transport = 0x20,
    espcp_message_types_header = 0x40,
    espcp_message_types_data = 0x80
};
typedef enum espcp_message_types espcp_message_types_t;

/*
 *    Access point authentication method.
 */
enum espcp_wi_fi_authentication_mode
{
    espcp_wi_fi_authentication_mode_open = 0,
    espcp_wi_fi_authentication_mode_wep = 1,
    espcp_wi_fi_authentication_mode_wpa_psk = 2,
    espcp_wi_fi_authentication_mode_wpa2_psk = 3,
    espcp_wi_fi_authentication_mode_wpa_wpa2_psk = 4,
    espcp_wi_fi_authentication_mode_wpa2_enterprise = 5
};
typedef enum espcp_wi_fi_authentication_mode espcp_wi_fi_authentication_mode_t;

/*
 *    WiFi Country Policy.
 */
enum espcp_wi_fi_country_policy
{
    espcp_wi_fi_country_policy_automatic = 0,
    espcp_wi_fi_country_policy_manual = 1
};
typedef enum espcp_wi_fi_country_policy espcp_wi_fi_country_policy_t;

/*
 *    Location of the secondary channel in respect to the primary channel.
 */
enum espcp_wi_fi_second_channel
{
    espcp_wi_fi_second_channel_none = 0,
    espcp_wi_fi_second_channel_above = 1,
    espcp_wi_fi_second_channel_below = 2
};
typedef enum espcp_wi_fi_second_channel espcp_wi_fi_second_channel_t;

/*
 *    WiFiScanType
 */
enum espcp_wi_fi_scan_type
{
    espcp_wi_fi_scan_type_active = 0,
    espcp_wi_fi_scan_type_passive = 1
};
typedef enum espcp_wi_fi_scan_type espcp_wi_fi_scan_type_t;

/*
 *    Types of antennas that can be selected.
 */
enum espcp_antenna_types
{
    espcp_antenna_types_on_board = 0,
    espcp_antenna_types_external = 1,
    espcp_antenna_types_max = 1
};
typedef enum espcp_antenna_types espcp_antenna_types_t;

/*
 *    Encryption method used by the access point.
 */
enum espcp_wi_fi_cipher_type
{
    espcp_wi_fi_cipher_type_none = 0,
    espcp_wi_fi_cipher_type_wep40 = 1,
    espcp_wi_fi_cipher_type_wep104 = 2,
    espcp_wi_fi_cipher_type_tkip = 3,
    espcp_wi_fi_cipher_type_ccmp = 4,
    espcp_wi_fi_cipher_type_tkip_ccmp = 5,
    espcp_wi_fi_cipher_type_unknown = 6
};
typedef enum espcp_wi_fi_cipher_type espcp_wi_fi_cipher_type_t;

/*
 *    Address family types for the low level sockets..
 */
enum espcp_address_family_type
{
    espcp_address_family_type_af_unspec = 0,
    espcp_address_family_type_af_inet = 2,
    espcp_address_family_type_af_inet6 = 10
};
typedef enum espcp_address_family_type espcp_address_family_type_t;

/*
 *    Protocol family types for the low level sockets..
 */
enum espcp_protocol_family_type
{
    espcp_protocol_family_type_pf_unspec = 0,
    espcp_protocol_family_type_pf_inet = 2,
    espcp_protocol_family_type_pf_inet6 = 10
};
typedef enum espcp_protocol_family_type espcp_protocol_family_type_t;

/*
 *    Protocol types for the low level sockets.
 */
enum espcp_ip_protocol_type
{
    espcp_ip_protocol_type_ip_proto_i_p = 0,
    espcp_ip_protocol_type_ip_proto_icmp = 1,
    espcp_ip_protocol_type_ip_proto_tcp = 6,
    espcp_ip_protocol_type_ip_proto_udp = 17,
    espcp_ip_protocol_type_ip_proto_ip_v6 = 41,
    espcp_ip_protocol_type_ip_proto_icmp_v6 = 58,
    espcp_ip_protocol_type_ip_proto_udp_lite = 136,
    espcp_ip_protocol_type_ip_proto_new = 255
};
typedef enum espcp_ip_protocol_type espcp_ip_protocol_type_t;

/*
 *    Socket protocol types for the low level sockets.
 */
enum espcp_socket_protocol_type
{
    espcp_socket_protocol_type_sock_stream = 1,
    espcp_socket_protocol_type_sock_dgram = 2,
    espcp_socket_protocol_type_sock_raw = 3
};
typedef enum espcp_socket_protocol_type espcp_socket_protocol_type_t;

/*
 *    Socket level types for the low level sockets.
 */
enum espcp_socket_level_type
{
    espcp_socket_level_type_sol_socket = 0xfff
};
typedef enum espcp_socket_level_type espcp_socket_level_type_t;

/*
 *    Socket option types for the low level sockets.
 */
enum espcp_socket_option_type
{
    espcp_socket_option_type_so_debug = 0x0001,
    espcp_socket_option_type_so_accept_conn = 0x0002,
    espcp_socket_option_type_so_dont_route = 0x0010,
    espcp_socket_option_type_use_loopback = 0x0040,
    espcp_socket_option_type_so_linger = 0x0080,
    espcp_socket_option_type_so_dont_linger = 0xff7f,
    espcp_socket_option_type_so_oob_inline = 0x0100,
    espcp_socket_option_type_so_reuse_port = 0x0200,
    espcp_socket_option_type_so_snd_buf = 0x1001,
    espcp_socket_option_type_so_rcv_buf = 0x1002,
    espcp_socket_option_type_so_snd_lo_wat = 0x1003,
    espcp_socket_option_type_so_s_rcv_lo_wat = 0x1004,
    espcp_socket_option_type_so_snd_time_o = 0x1005,
    espcp_socket_option_type_so_rcv_time_o = 0x1006,
    espcp_socket_option_type_so_error = 0x1007,
    espcp_socket_option_type_so_type = 0x1008,
    espcp_socket_option_type_so_con_time_o = 0x1009,
    espcp_socket_option_type_so_no_check = 0x100a
};
typedef enum espcp_socket_option_type espcp_socket_option_type_t;

/*
 *    ESP32 Error codes (errno).
 */
enum espcp_esp32_error_codes
{
    espcp_esp32_error_codes_perm = 1,
    espcp_esp32_error_codes_no_ent = 2,
    espcp_esp32_error_codes_srch = 3,
    espcp_esp32_error_codes_intr = 4,
    espcp_esp32_error_codes_i_o = 5,
    espcp_esp32_error_codes_nx_i_o = 6,
    espcp_esp32_error_codes_too_big = 7,
    espcp_esp32_error_codes_no_exec = 8,
    espcp_esp32_error_codes_bad_f = 9,
    espcp_esp32_error_codes_child = 10,
    espcp_esp32_error_codes_again = 11,
    espcp_esp32_error_codes_no_mem = 12,
    espcp_esp32_error_codes_acces = 13,
    espcp_esp32_error_codes_fault = 14,
    espcp_esp32_error_codes_busy = 16,
    espcp_esp32_error_codes_exist = 17,
    espcp_esp32_error_codes_x_dev = 18,
    espcp_esp32_error_codes_no_dev = 19,
    espcp_esp32_error_codes_not_dir = 20,
    espcp_esp32_error_codes_is_dir = 21,
    espcp_esp32_error_codes_in_val = 22,
    espcp_esp32_error_codes_n_file = 23,
    espcp_esp32_error_codes_m_file = 24,
    espcp_esp32_error_codes_no_tty = 25,
    espcp_esp32_error_codes_txt_bsy = 26,
    espcp_esp32_error_codes_f_big = 27,
    espcp_esp32_error_codes_no_spc = 28,
    espcp_esp32_error_codes_s_pipe = 29,
    espcp_esp32_error_codes_ro_fs = 30,
    espcp_esp32_error_codes_m_link = 31,
    espcp_esp32_error_codes_pipe = 32,
    espcp_esp32_error_codes_dom = 33,
    espcp_esp32_error_codes_range = 34,
    espcp_esp32_error_codes_no_msg = 35,
    espcp_esp32_error_codes_id_rm = 36,
    espcp_esp32_error_codes_dead_lck = 45,
    espcp_esp32_error_codes_no_lock = 46,
    espcp_esp32_error_codes_no_str = 60,
    espcp_esp32_error_codes_no_data = 61,
    espcp_esp32_error_codes_time = 62,
    espcp_esp32_error_codes_no_sr = 63,
    espcp_esp32_error_codes_no_link = 67,
    espcp_esp32_error_codes_proto = 71,
    espcp_esp32_error_codes_multi_hop = 74,
    espcp_esp32_error_codes_bad_msg = 77,
    espcp_esp32_error_codes_f_type = 79,
    espcp_esp32_error_codes_no_sys = 88,
    espcp_esp32_error_codes_not_empty = 90,
    espcp_esp32_error_codes_name_too_long = 91,
    espcp_esp32_error_codes_loop = 92,
    espcp_esp32_error_codes_op_not_support = 95,
    espcp_esp32_error_codes_p_no_support = 96,
    espcp_esp32_error_codes_conn_reset = 104,
    espcp_esp32_error_codes_no_bufs = 105,
    espcp_esp32_error_codes_af_no_support = 106,
    espcp_esp32_error_codes_proto_type = 107,
    espcp_esp32_error_codes_not_sock = 108,
    espcp_esp32_error_codes_no_proto_opt = 109,
    espcp_esp32_error_codes_conn_refused = 111,
    espcp_esp32_error_codes_addr_in_use = 112,
    espcp_esp32_error_codes_conn_aborted = 113,
    espcp_esp32_error_codes_net_unreach = 114,
    espcp_esp32_error_codes_net_down = 115,
    espcp_esp32_error_codes_timed_out = 116,
    espcp_esp32_error_codes_host_down = 117,
    espcp_esp32_error_codes_host_unreach = 118,
    espcp_esp32_error_codes_in_progress = 119,
    espcp_esp32_error_codes_already = 120,
    espcp_esp32_error_codes_dest_addr_req = 121,
    espcp_esp32_error_codes_msg_size = 122,
    espcp_esp32_error_codes_proto_no_support = 123,
    espcp_esp32_error_codes_addr_not_avail = 125,
    espcp_esp32_error_codes_net_reset = 126,
    espcp_esp32_error_codes_not_conn = 128,
    espcp_esp32_error_codes_too_many_refs = 129,
    espcp_esp32_error_codes_d_quot = 132,
    espcp_esp32_error_codes_stale = 133,
    espcp_esp32_error_codes_not_sup = 134,
    espcp_esp32_error_codes_il_seq = 138,
    espcp_esp32_error_codes_overflow = 139,
    espcp_esp32_error_codes_cancelled = 140,
    espcp_esp32_error_codes_not_recoverable = 141,
    espcp_esp32_error_codes_owner_dead = 142,
    espcp_esp32_error_codes_would_block = 11
};
typedef enum espcp_esp32_error_codes espcp_esp32_error_codes_t;

struct espcp_message_s
{
    /*
     *  Type of message.
     */
    uint8_t message_type;

    /*
     *  Interface that this message is destined for.
     */
    uint8_t interface;

    /*
     *  Function (on the interface) to be executed.
     */
    uint32_t function;

    /*
     *  Status code (for returning messages) from the function.
     */
    uint32_t status_code;

    /*
     *  Unique ID of this message.
     */
    uint32_t message_id;

    /*
     *  Pointer to the payload data to be processed (or returned from) the function.
     */
    uint8_t *payload;

    /*
     *  Number of bytes in the payload.
     */
    uint32_t payload_length;
};
typedef struct espcp_message_s espcp_message_t;

/**
 *  Calculate the amount of memory that should be allocated for
 *  the receive buffer.
 *
 *  SPI reception on the ESP32 should be on a 32-bit boundary and
 *  also a multiple of 4 bytes long (See the article linked below).
 *
 *  https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_slave.html#restrictions-and-known-issues
 *
 *  @param requestedSize
 *      Actual amount of data requested / to be received.
 *
 *  @returns
 *      Number of bytes that should be allocated.
 */
uint32_t espcp_calculate_spi_buffer_size(uint32_t requestedSize)
{
    uint32_t result = requestedSize;

    if (result < 8)
    {
        result = 8;
    }
    //
    //  The buffer should always be 4 bytes longer than needed.  During development it
    //  was found that the last four bytes of any transmission were being discarded.
    //  Empirical tests proved this for 24, 32 and 40 byte packets.
    //
    //  The work around is to increase the packet size by 4 and have dummy data in the
    //  last four bytes and discard the bytes.
    //
    //
    //  See support post: https://esp32.com/viewtopic.php?f=13&t=10117
    //
    requestedSize += 4;
    if ((requestedSize & 3) != 0)
    {
        result = (requestedSize & 0xfffffffc) + 4;
    }
    else
    {
        result += 4;
    }
    return (result);
}

/**
 *  Encode a 16-bit integer as two bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      16-bit value to encode in the first two bytes of the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where the two bytes will be inserted
 *      from the 16-bit unsigned integer (LSB first).
 */
void espcp_encode_uint16(uint16_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
}

/**
 *  Encode a 32-bit unsigned integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      Unsigned 32-bit integer value to write into the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where 4 bytes will be replaced with the four
 *      bytes representing unsigned integer (LSB first).
 */
void espcp_encode_uint32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/**
 *  Encode a 32-bit  integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      Unsigned 32-bit integer value to write into the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where 4 bytes will be replaced with the four
 *      bytes representing unsigned integer (LSB first).
 */
void espcp_encode_int32(int32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/**
 *  Copy the string into the buffer.
 *
 *  @param source
 *      Block of memory containing the string.
 *
 *  @param buffer
 *      Pointer to a block of memory to take the string.
 */
void espcp_encode_string(char *source, uint8_t *buffer)
{
    strcpy((char *) buffer, (char *) source);
}

/**
 *  Calculate the 8-bit CRC value for the specified data buffer.
 *
 *  This algorithm is loosely based upon the Dallas 1-Wire algorithm.
 *
 *  @param data
 *      Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *  @param len
 *      Number of bytes in the data buffer.
 *
 *  @returns
 *      8-bit checksum of the bytes in the buffer.
 */
uint8_t espcp_crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        uint8_t byte = data[byteCounter];
        for (uint8_t bitCounter = 8; bitCounter; bitCounter--)
        {
            uint8_t sum = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (sum)
            {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return(crc);
}

/**
 *  Calculate the CRC32 value change for the specified byte.
 *
 *  @param currentChecksum
 *      Current value of the checksum.
 *
 *  @param byte
 *      Next byte to use in the checksum calculation.
 *
 *  @returns
 *      Next checksum value.
 */
uint32_t espcp_progressive_crc32(uint32_t currentChecksum, uint8_t byte)
{
    uint32_t crc = currentChecksum;
    crc ^= byte;
    for (uint32_t index = 0; index < 8; index++)
    {
        uint32_t mask = (uint32_t) -(crc & 0x01);
        crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
    return (crc);
}

/**
 *  Calculate the 32-bit CRC of the buffer of data.
 *
 *  @param data
 *      Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *  @param len
 *      Number of bytes in the data buffer.
 *
 *  @returns
 *      32-bit checksum of the bytes in the buffer.
 */
uint32_t espcp_crc32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = ESPCP_CRC32_SEED;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        crc = espcp_progressive_crc32(crc, data[byteCounter]);
    }
    return (crc);
}

/****************************************************************************
 * Name: espcp_encode_message
 *
 * Description:
 *  Encode a message in a byte buffer.
 *
 * Input Parameters:
 *  message - Message to be encoded.
 *  buffer_length - Pointer to a uint32_t object that the contain the size of the
 *      buffer that contains the encoded message.  This is a return
 *      value from the method.
 *  header_only - if true then the header will be encoded but not the
 *      payload.
 *
 * Returned Value:
 *  Pointer to the block of memory containing the encoded message.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint8_t *espcp_encode_message(espcp_message_t *message, uint32_t *buffer_length, bool header_only)
{
    uint32_t message_size = ESPCP_HEADER_SIZE;
    if (!header_only)
    {
        message_size += message->payload_length;
    }
    uint32_t buffer_size = espcp_calculate_spi_buffer_size(message_size);
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);
    if (buffer != NULL)
    {
        memset(buffer, 0, buffer_size);

        int offset = 0;
        *buffer = message->message_type;
        offset++;
        *(buffer + offset) = message->interface;
        offset++;
        espcp_encode_uint32(message->function, buffer + offset);
        offset += 4;
        espcp_encode_uint32(message->status_code, buffer + offset);
        offset += 4;
        espcp_encode_uint32(message->message_id, buffer + offset);
        offset += 4;
        espcp_encode_uint32(message->payload_length, buffer + offset);
        offset += 4;
        //
        //  The byte after the payload_length is the CRC.  This is filled in later
        //  and so a 0 is written into the message temporarily.
        //
        espcp_encode_uint32(0, buffer + offset);
        offset += 4;
        if (!header_only && (message->payload_length > 0))
        {
            memcpy((uint8_t *) (buffer + offset), message->payload, message->payload_length);
        }
        else
        {
            espcp_encode_uint32(0, buffer + offset);
        }
        uint32_t crc = espcp_crc32(buffer, buffer_size);
        espcp_encode_uint32(crc, buffer + ESPCP_CRC_OFFSET);
    }
    else
    {
        buffer_size = 0;
    }

    *buffer_length = buffer_size;
    return(buffer);
}

void print_message(uint8_t *buffer, uint32_t length)
{
    printf("{\n");
    for (int index = 0; index < length; index++)
    {
        if ((index %16) == 0)
        {
            printf("\n  ");
        }
        printf("0x%02x, ", buffer[index]);
    }
    printf("\n};\n\n");
}

int main(void)
{
    espcp_message_t message;
    uint8_t *encoded_message;
    uint32_t encoded_length;

    memset(&message, 0, sizeof(espcp_message_t));
    message.message_type = espcp_message_types_header;
    message.interface = espcp_esp32_interfaces_wi_fi;
    message.function = espcp_wi_fi_function_connect;
    message.message_id = 10;
    message.payload_length = 5;
    message.payload = (uint8_t *) malloc(message.payload_length);
    for (int index = 0; index < message.payload_length; index++)
    {
        message.payload[index] = index;
    }

    printf("Message header\n");
    encoded_message = espcp_encode_message(&message, &encoded_length, true);
    print_message(encoded_message, encoded_length);
    free(encoded_message);

    printf("Header acknowledgement\n");
    message.message_type = espcp_message_types_ack;
    encoded_message = espcp_encode_message(&message, &encoded_length, true);
    print_message(encoded_message, encoded_length);
    free(encoded_message);

    printf("Message\n");
    message.message_type = espcp_message_types_data;
    encoded_message = espcp_encode_message(&message, &encoded_length, false);
    print_message(encoded_message, encoded_length);
    free(encoded_message);

    printf("Message acknowledgement\n");
    message.message_type = espcp_message_types_ack;
    encoded_message = espcp_encode_message(&message, &encoded_length, true);
    print_message(encoded_message, encoded_length);
    free(encoded_message);

    return(0);
}