
#include <string.h>

#include "dynamixel_protocol.h"

/**
 * Dynamixel CRC16 lookup table (from the Protocol 2.0 reference)
 */
static const u16 dxl_crc_table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011, 0x8033, 0x0036, 0x003C, 0x8039, 0x0028, 0x802D,
    0x8027, 0x0022, 0x8063, 0x0066, 0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072, 0x0050, 0x8055, 0x805F, 0x005A,
    0x804B, 0x004E, 0x0044, 0x8041, 0x80C3, 0x00C6, 0x00CC, 0x80C9, 0x00D8, 0x80DD, 0x80D7, 0x00D2, 0x00F0, 0x80F5,
    0x80FF, 0x00FA, 0x80EB, 0x00EE, 0x00E4, 0x80E1, 0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE, 0x00B4, 0x80B1,
    0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087, 0x0082, 0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D,
    0x8197, 0x0192, 0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1, 0x01E0, 0x81E5, 0x81EF, 0x01EA,
    0x81FB, 0x01FE, 0x01F4, 0x81F1, 0x81D3, 0x01D6, 0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2, 0x0140, 0x8145,
    0x814F, 0x014A, 0x815B, 0x015E, 0x0154, 0x8151, 0x8173, 0x0176, 0x017C, 0x8179, 0x0168, 0x816D, 0x8167, 0x0162,
    0x8123, 0x0126, 0x012C, 0x8129, 0x0138, 0x813D, 0x8137, 0x0132, 0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E,
    0x0104, 0x8101, 0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317, 0x0312, 0x0330, 0x8335, 0x833F, 0x033A,
    0x832B, 0x032E, 0x0324, 0x8321, 0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371, 0x8353, 0x0356,
    0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342, 0x03C0, 0x83C5, 0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1,
    0x83F3, 0x03F6, 0x03FC, 0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2, 0x83A3, 0x03A6, 0x03AC, 0x83A9, 0x03B8, 0x83BD,
    0x83B7, 0x03B2, 0x0390, 0x8395, 0x839F, 0x039A, 0x838B, 0x038E, 0x0384, 0x8381, 0x0280, 0x8285, 0x828F, 0x028A,
    0x829B, 0x029E, 0x0294, 0x8291, 0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7, 0x02A2, 0x82E3, 0x02E6,
    0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2, 0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1,
    0x8243, 0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252, 0x0270, 0x8275, 0x827F, 0x027A, 0x826B, 0x026E,
    0x0264, 0x8261, 0x0220, 0x8225, 0x822F, 0x022A, 0x823B, 0x023E, 0x0234, 0x8231, 0x8213, 0x0216, 0x021C, 0x8219,
    0x0208, 0x820D, 0x8207, 0x0202,
};

u16 dxl_crc16(const u8 *data, size_t length) {
    u16 crc = 0;
    for (size_t i = 0; i < length; i++) {
        u16 idx = ((crc >> 8) ^ data[i]) & 0xFF;
        crc = (crc << 8) ^ dxl_crc_table[idx];
    }
    return crc;
}

dxl_result_t dxl_build_packet(const dxl_packet_t *pkt, u8 *out_buf, size_t out_buf_size, size_t *out_length) {
    // Build the packet into a temporary buffer so we can calculate length
    // after byte stuffing.
    //
    // Wire format:
    //   [0xFF][0xFF][0xFD][0x00][ID][LEN_L][LEN_H][INST][PARAMS...][CRC_L][CRC_H]
    //
    // Length field = number of bytes after the length field itself:
    //   instruction(1) + params_with_stuffing + CRC(2)

    // First pass: figure out stuffed parameter size
    size_t stuffed_param_size = 0;
    for (u16 i = 0; i < pkt->param_count; i++) {
        stuffed_param_size++;
        // Check for byte stuffing: if we see FF FF FD in params, stuff a 0xFD
        if (i >= 2 && pkt->params[i - 2] == 0xFF && pkt->params[i - 1] == 0xFF && pkt->params[i] == 0xFD) {
            stuffed_param_size++;
        }
    }

    u16 wire_length = 1 + (u16)stuffed_param_size + 2; // instruction + stuffed params + CRC
    size_t total_size = 4 + 1 + 2 + wire_length;       // header(3) + reserved(1) + id(1) + length(2) + rest

    // Correct: header is 4 bytes (FF FF FD 00), then ID (1), then length (2), then payload
    total_size = 7 + wire_length;

    if (total_size > out_buf_size) {
        return DXL_BUFFER_OVERFLOW;
    }

    size_t pos = 0;

    // Header
    out_buf[pos++] = DXL_HEADER_0;
    out_buf[pos++] = DXL_HEADER_1;
    out_buf[pos++] = DXL_HEADER_2;
    out_buf[pos++] = DXL_RESERVED;

    // ID
    out_buf[pos++] = pkt->id;

    // Length (little-endian)
    out_buf[pos++] = (u8)(wire_length & 0xFF);
    out_buf[pos++] = (u8)((wire_length >> 8) & 0xFF);

    // Instruction
    out_buf[pos++] = pkt->instruction;

    // Parameters with byte stuffing
    for (u16 i = 0; i < pkt->param_count; i++) {
        out_buf[pos++] = pkt->params[i];

        // Byte stuffing: after writing a byte that completes FF FF FD, insert 0xFD
        if (i >= 2 && pkt->params[i - 2] == 0xFF && pkt->params[i - 1] == 0xFF && pkt->params[i] == 0xFD) {
            out_buf[pos++] = 0xFD;
        }
    }

    // CRC16 over everything except the CRC itself
    u16 crc = dxl_crc16(out_buf, pos);
    out_buf[pos++] = (u8)(crc & 0xFF);
    out_buf[pos++] = (u8)((crc >> 8) & 0xFF);

    *out_length = pos;
    return DXL_OK;
}

dxl_result_t dxl_parse_packet(const u8 *data, size_t length, dxl_packet_t *pkt) {
    // Minimum packet size: header(4) + id(1) + length(2) + instruction(1) + CRC(2) = 10
    if (length < 10) {
        return DXL_INVALID_PACKET;
    }

    // Validate header
    if (data[0] != DXL_HEADER_0 || data[1] != DXL_HEADER_1 || data[2] != DXL_HEADER_2 || data[3] != DXL_RESERVED) {
        return DXL_INVALID_PACKET;
    }

    pkt->id = data[4];

    u16 wire_length = (u16)data[5] | ((u16)data[6] << 8);

    // Total packet size = 7 (header through length) + wire_length
    size_t expected_total = 7 + (size_t)wire_length;
    if (expected_total > length) {
        return DXL_INVALID_PACKET;
    }

    // Verify CRC: covers everything up to but not including the 2 CRC bytes
    size_t crc_offset = expected_total - 2;
    u16 received_crc = (u16)data[crc_offset] | ((u16)data[crc_offset + 1] << 8);
    u16 computed_crc = dxl_crc16(data, crc_offset);

    if (received_crc != computed_crc) {
        return DXL_CRC_MISMATCH;
    }

    pkt->instruction = data[7];

    // For status packets, byte 8 is the error field
    if (pkt->instruction == DXL_INST_STATUS) {
        pkt->error = data[8];

        // Unstuff parameters: remove the extra 0xFD after FF FF FD sequences
        u16 param_idx = 0;
        size_t raw_start = 9;
        size_t raw_end = crc_offset;

        for (size_t i = raw_start; i < raw_end; i++) {
            if (param_idx >= DXL_MAX_PARAMS) {
                return DXL_BUFFER_OVERFLOW;
            }
            pkt->params[param_idx++] = data[i];

            // If we just wrote a byte that completed FF FF FD, skip the stuffed byte
            if (param_idx >= 3 && pkt->params[param_idx - 3] == 0xFF && pkt->params[param_idx - 2] == 0xFF &&
                pkt->params[param_idx - 1] == 0xFD && (i + 1) < raw_end && data[i + 1] == 0xFD) {
                i++; // skip the stuffed byte
            }
        }
        pkt->param_count = param_idx;
    } else {
        pkt->error = 0;

        // Non-status packets: parameters start at byte 8
        u16 param_idx = 0;
        size_t raw_start = 8;
        size_t raw_end = crc_offset;

        for (size_t i = raw_start; i < raw_end; i++) {
            if (param_idx >= DXL_MAX_PARAMS) {
                return DXL_BUFFER_OVERFLOW;
            }
            pkt->params[param_idx++] = data[i];

            if (param_idx >= 3 && pkt->params[param_idx - 3] == 0xFF && pkt->params[param_idx - 2] == 0xFF &&
                pkt->params[param_idx - 1] == 0xFD && (i + 1) < raw_end && data[i + 1] == 0xFD) {
                i++;
            }
        }
        pkt->param_count = param_idx;
    }

    // Check for servo-reported errors
    if (pkt->error != 0) {
        return DXL_SERVO_ERROR;
    }

    return DXL_OK;
}

const char *dxl_result_to_string(dxl_result_t result) {
    switch (result) {
    case DXL_OK:
        return "OK";
    case DXL_TIMEOUT:
        return "Timeout";
    case DXL_INVALID_PACKET:
        return "Invalid packet";
    case DXL_CRC_MISMATCH:
        return "CRC mismatch";
    case DXL_SERVO_ERROR:
        return "Servo error";
    case DXL_TX_FAIL:
        return "TX failure";
    case DXL_BUFFER_OVERFLOW:
        return "Buffer overflow";
    default:
        return "Unknown error";
    }
}

const char *dxl_error_to_string(u8 error) {
    switch (error) {
    case 0:
        return "None";
    case DXL_ERR_RESULT_FAIL:
        return "Result fail";
    case DXL_ERR_INST_ERROR:
        return "Instruction error";
    case DXL_ERR_CRC_ERROR:
        return "CRC error";
    case DXL_ERR_DATA_RANGE:
        return "Data range error";
    case DXL_ERR_DATA_LENGTH:
        return "Data length error";
    case DXL_ERR_DATA_LIMIT:
        return "Data limit error";
    case DXL_ERR_ACCESS:
        return "Access error";
    default:
        return "Unknown error";
    }
}
