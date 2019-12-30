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

/* W25Q64JV:
         +-------+
 #CS    -|1     8|- Vcc = 3.3 V
 DO/IO1 -|2     7|- IO3
 IO2    -|3     6|- CLK
 GND    -|4     5|- DI/IO0
         +-------+

 #CS    = #CS   = GP2 output = DTR
 CLK    = CLK   = GP3 output = RTS

 DI/IO0 = MOSI  = GP4 output = RI
 DO/IO1 = MISO  = GP5 input  = DCD
 IO2    =       = GP6 input  = DSR // not used yet
 IO3    =       = GP7 input  = CTS // not used yet
 */

#define GP_CS   2
#define GP_CLK  3
#define GP_MOSI 4
#define GP_MISO 5

#define Tclk_us 100000

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

uint8_t
send_bit(uint8_t bit) {
    /*
      NOTE: The maximal allowed clk freq of the eeprom is 50 MHz even for data reads, that means 10 ns for half-period time.

      An USB control transfer by itself means transmitting 8 payload bytes (9 for reads) plus 46 frame bits, 110 bits in total,
      and we're doing two such transfers in each half-period, resulting 224 bits on average. The bit stuffing also adds about
      1%, so let's make it 226 bits in total, which (at 12 Mbit/s) takes about 18.83 usec.

      That's about 1883 times more than what the eeprom is still comfortable with, so that's why those `usleep` calls
      are commented out below.
     */

    gpio_write(GP_MOSI, bit ? 1 : 0);
    //usleep(Tclk_us);
    gpio_write(GP_CLK, 1);

    uint8_t result = gpio_read(GP_MISO);
    //usleep(Tclk_us);
    gpio_write(GP_CLK, 0);

    return result;
}

uint8_t
send_byte(uint8_t byte) {
    uint8_t response = 0;
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
        if (send_bit(byte & mask) != 0)
            response |= mask;
    }
    return response;
}

void
set_cs(uint8_t bit) {
    gpio_write(GP_CS, bit);
}


int
main(int argc, char **argv) {
    if (!init_device())
        die("Could not init device");

    // setup everything
    write_register(0, 0); // flow control = OFF
    reg_01_shadow = 0;
    reg_06_shadow = 0;
    reg_07_shadow = 0;

    set_gpio_type(GP_CS, gpio::type::OUTPUT);
    gpio_write(GP_CS, 1);
    set_gpio_type(GP_CLK, gpio::type::OUTPUT);
    gpio_write(GP_CS, 0);
    set_gpio_type(GP_MOSI, gpio::type::OUTPUT);
    gpio_write(GP_MOSI, 0);
    set_gpio_type(GP_MISO, gpio::type::INPUT);

    // Perform reset
    set_cs(0);
    send_byte(0x66);
    set_cs(1);
    set_cs(0);
    send_byte(0x99);
    set_cs(1);
    

#if 1
    // cmd = JEDEC_ID
    set_cs(0);
    send_byte(0x9f);
    for (int i = 0; i < 3; ++i)
        printf("resp[%d]=0x%02x\n", i, send_byte(i));
    set_cs(1);
#endif

#if 0
    // cmd = READ_STATUS_1
    set_cs(0);
    send_byte(0x05);
    while (true) {
        printf("resp=0x%02x\n", send_byte(0));
    }
    set_cs(1);
#endif

#if 1
    // cmd = READ_MEMORY
    int fd = open("memory.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        die("Cannot create dump file: %s", strerror(errno));
    set_cs(0);
    send_byte(0x03);
    send_byte(0); // address bits 23..16
    send_byte(0); // address bits 15..8
    send_byte(0); // address bits 7..0
    time_t t_start;
    time(&t_start);
    for (uint32_t addr = 0; addr < 0x800000; ) {
        uint8_t data[4096];
        for (int p = 0; p < 4096; ++p, ++addr) {
            data[p] = send_byte(0);
            time_t t_now;
            time(&t_now);
            printf("addr=0x%06x, spd=%lf\r", addr, 1.0 * addr / (t_now - t_start));
            fflush(stdout);
        }
        write(fd, data, 4096);
    }
    set_cs(1);
    close(fd);
#endif

    libusb_close(dh);
    libusb_exit(ctx);
    return 0;
}
