#include <stdio.h>
#include <stdlib.h>
#include <bcm2835.h>

#include <unistd.h>
#include <errno.h>


int main(int argc, char **argv) {

    if (geteuid() != 0) {
        printf("This must be run as root!\n");
        return EXIT_FAILURE;
    }

    if (!bcm2835_init())
        return EXIT_FAILURE;

    char buf[] = {0xE7};

    printf("%d\n", errno);

    bcm2835_i2c_begin();

    bcm2835_i2c_setClockDivider(BCM2835_I2C_CLOCK_DIVIDER_148);
    bcm2835_i2c_setSlaveAddress(0x40);

    bcm2835_i2c_write(buf, 1);
    bcm2835_i2c_read(buf, 1);

    printf("User Register = %X \r\n", buf[0]);

    bcm2835_i2c_end();

    return EXIT_SUCCESS;
}
