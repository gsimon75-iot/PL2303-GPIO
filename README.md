# PL2303-GPIO

Using the GPIO pins on PL2303 USB-to-serial controllers

If you're looking for just the registers and bits, scroll to [GPIO registers](#gpio-registers).


## History

Back in "those good ol' days" when we wanted to communicate with some digital circuitry, we just hooked it up to the
[printer port](https://en.wikipedia.org/wiki/Parallel_port) which had 8 outputs, 5 inputs and 4 bi-directional lines, all
free and available for us to control. We even had IRQ interrupts (now you would name them callbacks or event hooks) that
triggered when certain input lines made certain state transitions.

Later on, in those even better but still ol' days, the output lines became bi-directional as well, and we also had some
niceties like DMA-assisted super fast (that is, compared to the previous ones) transfer modes, and it was all good and
well.

But that DB25 connector was clumsy. So clumsy that anyone with a soldering iron could use it for his circuits, and its
software interface was also too simple, you just flipped a bit in some port, and some wire changed state.
Heck, you could program it even from TurboPascal!

So it had to be retired, replaced by things like USB which has a so complicated _bus protocol_ that requires dedicated
circuitry and a complex software stack atop of it.

The other good ol' communication port, the [serial port](https://en.wikipedia.org/wiki/RS-232) fared a bit better.
Its DE-9M connector was a bit less clumsy, so for some time you found it even on laptops, and by the time they were
finally banished as well (also too simple, just write a value to a port and bits start marching out onto the line),
there were a lot of hardware (UPSes, house alarms, garden watering systems, etc.) that relied on it.

This means business demand, and where there is demand, there will soon be supply as well.

The then-newfounded USB standard included the protocol specification for such serial communication devices, so
one could just take a well-built circuitry that talked USB on one end and RS232 on the other, and proceed as before.

As always, first there were several manufacturers who each designed and produced their own implementation, but
soon it all boiled down to two major brands: FTDI with their FT2232 family and Prolific with their PL2303 variants.

They both provide the standard RS232 signals (TXD/RXD, RTS/CTS, DTR/DSR, DCD and RI), and that's OK for serial
communication (that was the point, after all), but for arbitrary binary bitmongery it's way too poor:

We can control only 2 outputs (RTS and DTR) and we can read 4 inputs (CTS, DSR, DCD and RI). (Tx and Rx are out of
play because we can't just set them to any state for anytime.)

Fortunately, both of the mentioned two major manufacturers added some _general purpose input/output_ (*GPIO*) pins
to their chips, just in case.

The only problem is that their controlling methods are non-standard and sometimes not even documented to the public.

Which is exactly the case with the PL2303 family...


## About the PL2303 variants

The source of the info below is the [official Prolific website](http://www.prolific.com.tw/US/CustomerLogin.aspx),
just login with guest/guest, click to [PL2303 USB to Serial Drivers](http://www.prolific.com.tw/US/supportDownload.aspx?FileType=56&FileID=133&pcid=85&Page=0)
and browse around.

So, there are a lot of PL2303 variants, with slightly different capabilities:

- PL2303HX (Chip Rev D): On-chip clock generator, OTPROM, 4 GPIO, supports RS232/RS422/RS485
- PL2303EA: High ESD protection
- PL2303RA: Built-in RS232 transceiver
- PL2303SA: TX/RX only SOP8 package
- PL2303TB: Supports 12 GPIO, multi-clock output generator, PWM output, TX/RX access LED
- PL2303TA: PL-2303HX Rev A and PL-2303X pin-compatible EOL replacement solution

| Part number | Maximum baud rate | Crystal  | Memory | RS232    | RS232 transciever | RS422 / RS485 | GPIO  | Special features                   | Package       |
| ----------- | ----------------- | -------- | ------ | -------- | ----------------- | ------------- | ----- | ---------------------------------- | ------------- |
| PL2303TA/HX | 6 Mbps            | External | 24C02  | DB9 Pins | External          | No            | 2     |                                    | SSOP28        |
| PL2303HXD   | 12 Mbps           | Internal | OTPROM | DB9 Pins | External          | Yes           | 8(+)  |                                    | SSOP28, QFN32 |
| PL2303SA    | 115kbps           | Internal | N/A    | TX / RX  | External          | No            | 0     |                                    | SOP8          |
| PL2303EA    | 12 Mbps           | Internal | OTPROM | DB9 Pins | External          | Yes           | 8(+)  | ESD protection +-15kV              | SSOP28        |
| PL2303RA    | 1 Mbps            | Internal | OTPROM | DB9 Pins | Internal          | No            | 4     |                                    | SSOP28        |
| PL2303TB    | 12 Mbps           | External | 24C02  | DB9 Pins | External          | Yes           | 12(+) | PWM, multi-clock, TX/RX access LED | SSOP28        |

* 4 modem pins of PL2303HXD/PL2303EA can work as GPIO
* 4 modem pins and TX/RX pins of PL2303TB can work as GPIO

Identifying which chip you have can be difficult, because Prolific chose not to use a different `bcdDevice` value for
each chip version (in the USB device descriptor), but we must check a lot of other parameters and perform a complex
rain dance. Don't ask me why is this better than a sequentially increasing `bcdDevice` field...

Now, an important note: I have a PL2303HX, so I could actually test things only with this, and unless explicitely
stated, all information relates to this variant.


## Sources of information

- The [Linux kernel module](https://github.com/torvalds/linux/blob/master/drivers/usb/serial/pl2303.c)
- The [FreeBSD kernel module](https://github.com/freebsd/freebsd/blob/master/sys/dev/usb/serial/uplcom.c)
- A [document from Tommie](https://gist.github.com/tommie/89011c5ac06553d5cdb8)
- Some java code snippet I found on the Internet somewhere (`found_somewhere.java`)


## The communication protocol

First of all, if you're not yet familiar with [libusb](https://libusb.info/), please read some about it and get some
sample codes compile and run, otherwise the rest won't make too much sense.

So, the communication with the PL2303s employs four kinds of USB transactions:

- The regular serial setup (baud rate, parity, stop bits) is accessible via USB CDC class-specific calls
- The serial data transfer and reception are done via normal bulk transfers through the bulk/in and bulk/out endpoints
- Incoming data notification uses the interrupt endpoint
- And finally, GPIO manipulation is done via control transfers (yes, endpoint zero), using vendor-level request codes


## The GPIO-related communication

When using the controller as a normal RS232 device, I was quite content with the way the kernel presented it to me,
and used it via `/dev/ttyU0`, using just `ioctl`, `read`, `write`, and `poll`. No USB was needed here :).

When it comes to GPIO, however, we'll do everything manually. And if we want to play nicely, then whatever registers
we change, we set them back at the end, so the kernel can continue using the controller as before.

I said 'changing registers'. PL2303 has a bunch of (byte) registers that we can access via control transfer calls.

Writing a register means setting up a regular 8-byte control packet like this:

- `bmRequestType`: dir=out, type=vendor, recipient=device
- `bRequest`: 0x01 (`UPLCOM_SET_REQUEST`)
- `wValue`: the register *index*
- `wIndex`: the new register *value*
- `wLength`: 0 (no data follows)
- so the data pointer is null as well

Yes, that's right, the register index travels in `wValue` and the value travels in `wIndex`. Very funny, isn't it...

Reading a register is very similar:

- `bmRequestType`: dir=in, type=vendor, recipient=device
- `bRequest`: 0x01 (`UPLCOM_SET_REQUEST`)
- `wValue`: the register *index*
- `wIndex`: 0 (or don't care, I haven't checked)
- `wLength`: 1 (one byte will follow)
- so the data pointer points to an array of 1 byte

*IMPORTANT*: When reading a register, set its 7th bit to 1, that is, when reading register 0x02, it's 0x82 instead.

Some registers can be read back, some are write-only, some may have different functions for write and read access.
(At least in that good old era it was common if a port was 'command' for write and 'status' for read...)

Now that we can read and write registers, the question is just what do they do?


## The registers in general

And here comes the undocumented part. I think most information comes from

- Experimenting: "write this value here, that other value there, and check with a multimeter what happened")
- ~Reverse engineering~ Mysterious sources found somewhere on the net
- Analysing: Use `wireshark` to record traffic on the `usbus0`, `usbus1`, etc. interfaces and start some (proprietary)
    program that does something interesting - even a Windows-based one in `qemu`

As a small tool for such analysing, in Wireshark you can export the packets in .json format, which is very verbose.
To narrow it down to what we actually need, I added a small `wsdump_filter.jq` script to process it.


## GPIO registers

On PL2303HX I found the following lines operational:
- GP 0 and 1: they can be configured either as input or as (the usual) push-pull output
- GP 4 .. 7: they can be configured as:
    - Input: 00
    - Pull-down (or open-collector) output: 10
    - Push-pull (or totem-pole) output: 11

I failed to control RX, TX, RTS and DTR as GPIO, but RTS and DTR can be controlled via the usual UCDC requests,
though only as push-pull outputs.

The pin assignment:
- GP0: GP0
- GP1: GP1
- GP2: DTR (unofficial, just my choice)
- GP3: RTS (unofficial, just my choice)
- GP4: RI
- GP5: DCD
- GP6: DSR
- GP7: CTS

The registers assignment:

### Register 0x01: GPIO 0, 1 type and value

- bits 0..3: serial handshake, we don't need them now
- bit 4: GP0 type: 0=input, 1=push-pull output
- bit 5: GP1 type: 0=input, 1=push-pull output
- bit 6: GP0 value
- bit 7: GP1 value

Remember: it reads as 0x81...

To avoid the read-modify-write ritual, just maintain a shadow of it so you'll need only a write operation.


### Register 0x06: GPIO 4 .. 7 type

- bits 0, 1: GP4/RI
- bits 2, 3: GP5/DCD
- bits 4, 5: GP6/DSR
- bits 6, 7: GP7/CTS

The values:

- 0x: input
- 10: pull-down output (0=pull-down, 1=high-Z)
- 11: push-pull output


### Register 0x07: GPIO 4 .. 7 value

- bit 0: GP4 (RI)
- bit 1: GP5 (DCD)
- bit 2: GP6 (DSR)
- bit 3: GP7 (CTS)

Remember: it reads as 0x87, and also useful to maintain a shadow of it.


## Conclusion

We have
- 2 in/out ports: GP0,1
- 2 out-only ports: GP2,3
- 4 in/out/pulldown ports: GP4..7

Still fewer than the printer port, but definitely enough to drive an SPI EEPROM (for which I originally needed
all this :) ).

[//]: # ( vim: set sw=4 ts=4 et: )
