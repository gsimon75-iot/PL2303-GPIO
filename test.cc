#include <errno.h>
#include <fcntl.h>
#include <libusb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

libusb_context          *ctx;
libusb_device           **devices;
libusb_device_handle    *dh;

#define VENDOR  0x067b
#define PRODUCT 0x2303
#define DEVICE 0x0300
#define MANUFACTURER  "Prolific Technology Inc."
#define PRODUCT_NAME  "USB-Serial Controller"

#define UCDC_SET_CONTROL_LINE_STATE 0x22
#define UCDC_LINE_DTR           0x0001
#define UCDC_LINE_RTS           0x0002

#define UPLCOM_SET_REQUEST          0x01


uint16_t hw_control_lines;
uint8_t reg_01_shadow, reg_06_shadow, reg_07_shadow;

struct gpio {
    enum type {
        INPUT = 0,
        OUTPUT_OC = 2,
        OUTPUT = 3
    };
};

bool
die(char const * fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

bool
check_device_string(int id, const char *shouldbe) {
    char text[1024];

    if (id == 0)
        return false;

    int res = libusb_get_string_descriptor_ascii(dh, id, (uint8_t*)&text, sizeof(text));
    if (res < 0)
        return false;

    text[res] = '\0';
    return !strcmp(text, shouldbe);
}

bool
init_device(void) {
    int res;

    res = libusb_init(&ctx);
    if (res != LIBUSB_SUCCESS)
        return false; // die("libusb_init failed", res);

    int len = libusb_get_device_list(ctx, &devices);
    for (int i = 0; i < len; ++i) {
        libusb_device_descriptor descr_dev;

        res = libusb_get_device_descriptor(devices[i], &descr_dev);
        if (res != LIBUSB_SUCCESS)
            return false; // die("libusb_get_device_descriptor failed", res);

        if ((descr_dev.idVendor == VENDOR) && (descr_dev.idProduct == PRODUCT) && (descr_dev.bcdDevice == DEVICE)) {
            res = libusb_open(devices[i], &dh);
            if (res != LIBUSB_SUCCESS)
                return false; // die("libusb_open failed", res);

            if (check_device_string(descr_dev.iManufacturer, MANUFACTURER) &&
                check_device_string(descr_dev.iProduct, PRODUCT_NAME)) {
                // gotcha!
                break;
            }
            libusb_close(dh);
            dh = NULL;
        }
    }
    libusb_free_device_list(devices, 1);
    if (!dh)
        return false; // die("Could not find device", LIBUSB_ERROR_OTHER);
    // now we have the device

    return true;
}

void
write_register(uint16_t idx, uint16_t value) {
    //fprintf(stderr, "write_register(0x%04x, 0x%04x)\n", idx, value);
    int res = libusb_control_transfer(dh,
            LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, // bmRequestType
            UPLCOM_SET_REQUEST, // bRequest
            idx, // wValue
            value, // wIndex
            nullptr, // data
            0, // wLength
            1000); // timeout
    if (res != LIBUSB_SUCCESS)
        die("Could not send WriteRegister request: %s (%d=%s)\n", libusb_strerror(res), res, libusb_error_name(res));
}

uint8_t
read_register(uint16_t idx) {
    uint8_t response[1];

    //fprintf(stderr, "read_register(0x%04x)\n", idx);
    int res = libusb_control_transfer(dh,
            LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, // bmRequestType
            UPLCOM_SET_REQUEST, // bRequest
            idx, // wValue
            0, // wIndex
            response, // data
            1, // wLength
            1000); // timeout
    //fprintf(stderr, "res=%d, data=0x%02x\n", res, response[0]);
    return response[0];
}

void
set_gpio_type(uint8_t idx, gpio::type type) {
    if (idx <= 1) {
        switch (type) {
            case gpio::type::INPUT:
            reg_01_shadow &= ~(0x10 << idx);
            break;

            case gpio::type::OUTPUT_OC:
                die("GPIO 0 and 1 does not support open-collector output mode");

            case gpio::type::OUTPUT:
            reg_01_shadow |= (0x10 << idx);
            break;
        }
        write_register(0x01, reg_01_shadow);
    }
    else if (idx <= 3) {
        if (type != gpio::type::OUTPUT) {
            die("GPIO 2 and 3 are output only on PL2303HX");
        }
    }
    else if (idx <= 7) {
        uint8_t shift = 2 * (idx - 4);
        reg_06_shadow &= ~(3 << shift);
        reg_06_shadow |= type << shift;
        write_register(0x06, reg_06_shadow);
    }
    else {
        die("GPIO 8 and above are not supported on PL2303HX");
    }
}

uint8_t
gpio_read(uint8_t idx) {
    if (idx <= 1) {
        return ((read_register(0x81) & (0x40 << idx)) == 0) ? 0 : 1;
    }
    else if (idx <= 3) {
        die("GPIO 2 and 3 are output-only on PL2303HX");
    }
    else if (idx <= 7) {
        return ((read_register(0x87) & (0x01 << (idx - 4))) == 0) ? 0 : 1;
    }
    else {
        die("GPIO 8 and above are not supported on PL2303HX");
    }
}


void
gpio_write(uint8_t idx, uint8_t bit) {
    if (idx <= 1) {
        if (bit == 0) {
            reg_01_shadow &= ~(0x40 << idx);
        }
        else {
            reg_01_shadow |= (0x40 << idx);
        }
        write_register(0x01, reg_01_shadow);
    }
    else if (idx <= 3) {
        uint8_t mask;

        switch (idx) {
            case 2: mask = UCDC_LINE_DTR; break;
            case 3: mask = UCDC_LINE_RTS; break;
        }
        if (bit != 0) { // DTR and RTS are inverted
            hw_control_lines &= ~mask;
        }
        else {
            hw_control_lines |= mask;
        }
        int res = libusb_control_transfer(dh,
                LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, // bmRequestType
                UCDC_SET_CONTROL_LINE_STATE, // bRequest
                hw_control_lines, // wValue
                0, // wIndex
                nullptr, // data
                0, // wLength
                1000); // timeout
        if (res != LIBUSB_SUCCESS)
            die("Could not set control line state: %s (%d=%s)\n", libusb_strerror(res), res, libusb_error_name(res));
    }
    else if (idx <= 7) {
        if (bit == 0) {
            reg_07_shadow = reg_07_shadow & ~(0x01 << (idx - 4));
        }
        else {
            reg_07_shadow = reg_07_shadow | 0x01 << (idx - 4);
        }
        write_register(0x07, reg_07_shadow);
    }
    else {
        die("GPIO 8 and above are not supported on PL2303HX");
    }
}

void
write_test() {
    for (int i = 0; i < 8; ++i) {
        set_gpio_type(i, gpio::type::OUTPUT);
        gpio_write(i, 0);
    }

    for (uint8_t i = 0; true; i = (i + 1) & 0x07) {
        gpio_write(i, 1);
        sleep(1);
        gpio_write(i, 0);
    }
}

void
read_test() {
    set_gpio_type(0, gpio::type::INPUT);
    set_gpio_type(1, gpio::type::INPUT);
    set_gpio_type(4, gpio::type::INPUT);
    set_gpio_type(5, gpio::type::INPUT);
    set_gpio_type(6, gpio::type::INPUT);
    set_gpio_type(7, gpio::type::INPUT);

    while (true) {
        printf("%d", gpio_read(0));
        printf("%d", gpio_read(1));
        printf("%d", gpio_read(4));
        printf("%d", gpio_read(5));
        printf("%d", gpio_read(6));
        printf("%d", gpio_read(7));
        printf("\n");
        sleep(1);
    }
}

int
main(int argc, char **argv) {
    if (!init_device())
        die("Could not init device");

    //write_register(2, 0); // disable tx/rx

    write_register(0, 0); // flow control = OFF

    reg_01_shadow = 0;
    reg_06_shadow = 0;
    reg_07_shadow = 0;

    write_test();
    //read_test();

    //write_register(2, 0x44); // re-enable rx/tx, 0x44 for PL2303HX, 0x24 for PL2303

    libusb_close(dh);
    libusb_exit(ctx);
    return 0;
}
