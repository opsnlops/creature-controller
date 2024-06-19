

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "hardware/i2c.h"
#include "util/string_utils.h"


#include "logging/logging.h"

#include "controller-config.h"

#include "device/pac1954.h"


// From i2c.c
extern volatile bool i2c_setup_completed;



void init_pac1954(i2c_inst_t *i2c, u8 address) {

    debug("init'ing the PAC1954 at address 0x%02X (%s)", address, to_binary_string(address));

    configASSERT(i2c_setup_completed);

    // Gather some device information
    u8 device_id = pac1954_read_register_8bit(i2c, address, PAC1954_PRODUCT_ID_REGISTER);
    u8 manufacturer_id = pac1954_read_register_8bit(i2c, address, PAC1954_MANUFACTURER_ID_REGISTER);
    u8 revision_id = pac1954_read_register_8bit(i2c, address, PAC1954_REVISION_ID_REGISTER);

    info("pac1954 device id: 0x%02X, manufacturer id: 0x%02X, revision id: 0x%02X", device_id, manufacturer_id, revision_id);

}





u8 pac1954_read_register_8bit(i2c_inst_t *i2c, u8 address, u8 register_address) {
    configASSERT(i2c_setup_completed);

    u8 data;
    i2c_write_blocking(i2c, address, &register_address, 1, false);
    i2c_read_blocking(i2c, address, &data, 1, false);
    return data;
}


u16 pac1954_read_register_16bit(i2c_inst_t *i2c, u8 address, u8 register_address) {
    configASSERT(i2c_setup_completed);

    uint8_t data[2];
    data[0] = register_address;
    i2c_write_blocking(i2c, address, data, 1, false);
    i2c_read_blocking(i2c, address, data, 2, false);
    return (data[0] << 8) | data[1];
}