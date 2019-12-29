public class Something {
    public static final int PL2303HXD_CTS_ON = 8;
    public static final int PL2303HXD_DCD_ON = 2;
    public static final int PL2303HXD_DSR_ON = 4;
    public static final int PL2303HXD_RI_ON = 1;
   
    private UsbEndpoint epInterrupt;
    private UsbEndpoint epInput;
    private UsbEndpoint epOutput;

    private int revision;
    private boolean isSomeVariant1;
    private boolean isSomeVariant2;
    private int reg06Shadow;
    private int reg07Shadow;
    private int reg0eShadow;
    private int reg0cShadow;

    public enum BaudRate {
        private int value;
        private BaudRate(int value) { this.value = value; }
        public value() { return this.value; }

        B0(0),
        B75(75),
        B150(150),
        B300(300),
        B600(600),
        B1200(1200),
        B1800(1800),
        B2400(2400),
        B4800(4800),
        B9600(9600),
        B14400(14400),
        B19200(19200),
        B38400(38400),
        B57600(57600),
        B115200(115200),
        B230400(230400),
        B460800(460800),
        B614400(614400),
        B921600(921600),
        B1228800(1228800),
        B2457600(2457600),
        B3000000(3000000),
        B6000000(6000000),
        B12000000(12000000)
    }

    public enum DataBits {
        private int value;
        private BaudRate(int value) { this.value = value; }
        public value() { return this.value; }

        D5(5),
        D6(6),
        D7(7),
        D8(8)
    }

    public enum FlowControl {
        private int value;
        private FlowControl(int value) { this.value = value; }
        public value() { return this.value; }

        OFF(1),
        RTSCTS(2),
        RFRCTS(3),
        DTRDSR(4),
        RTSCTSDTRDSR(5),
        XONXOFF(6)
    }

    public enum Parity {
        private int value;
        private Parity(int value) { this.value = value; }
        public value() { return this.value; }

        NONE(0),
        ODD(1),
        EVEN(2)
    }

    public enum StopBits {
        private int value;
        private StopBits(int value) { this.value = value; }
        public value() { return this.value; }

        S1(0),
        S2(2)
    }

    public int setBitMask(int bitfield, int mask, boolean value) {
        return value ? (bitfield | mask) : (bitfield & ~mask);
    }
    public int setBit(int bitfield, int idx, boolean value) {
        return setBitMask(bitfield, mask = 1 << idx, value);
    }
    public int getBit(int bitfield, int idx) {
        return ((bitfield & (1 << idx)) != 0) ? 1 : 0;
    }

    private void writeRegister(int i, int i2) {
        this.usbDeviceConnection.controlTransfer(0x40, 1, i, i2, null, 0, this.controlTimeout);
    }

    private int readRegister(int i) {
        byte[] bArr = new byte[]{0};
        this.usbDeviceConnection.controlTransfer(0xc0, 1, i, 0, bArr, 1, this.controlTimeout);
        return bArr[0];
    }

    private int readExtRegister(int i) {
        readRegister(0x84);
        writeRegister(0x04, i);
        readRegister(0x84);
        return readRegister(0x83);
    }


    private void init(...) {
        this.reg0cShadow = 15;
        this.reg06Shadow = 0;
        this.reg07Shadow = 0;
        this.reg0eShadow = 0;

        int q0 = readExtRegister(0);
        int q1 = readExtRegister(1);
        if (q0 == 0x7b && q1 == 0x06) {
            this.isSomeVariant2 = true;
        }
        if (!this.isSomeVariant2) {
            writeRegister(0x00, 0x01);
            writeRegister(0x01, 0x00);
            writeRegister(0x02, 0x44);
            readRegister(0x80);
            readRegister(0x81);
            readRegister(0x82);
        }
        if (usbDeviceConnection.getRawDescriptors()[13] == (byte) 4) {
            this.revision = 4;
        }
        else { // detectRevision
            int i = readRegister(0x81);
            writeRegister(1, 0xff);
            int iArr = readRegister(0x81);
            this.revision = 2;
            if ((iArr & 0x0f) == 0x0f) {
                int iArr0 = readExtRegister(0xfa);
                int iArr1 = readExtRegister(0xfb);
                if ((iArr0 == 1 && iArr1 == 4) || (iArr0 == 2 && iArr1 == 4) || (iArr0 == 3 && iArr1 == 4) || (iArr0 == 1 && iArr1 == 3)) {
                    this.revision = 4;
                }
            }
            writeRegister(1, i);
        }

        if (this.isSomeVariant2) {
            int c = readExtRegister(9);
            if ((c & 8) == 8) {
                writeRegister(0x00, 0x31);
                writeRegister(0x01, 0x08);
                this.isSomeVariant1 = true;
            }
        }
        if (usbDeviceConnection.getRawDescriptors()[13] == (byte) 5) {
            int iArr = readRegister(0x94);
            this.revision = ((iArr & 0x94) == 0x94) ? 6 : 2;
        }
        readRegister(0x80);
        readRegister(0x81);
        readRegister(0x82);
        setup(BaudRate.B9600, DataBits.D8, StopBits.S1, Parity.NONE, FlowControl.OFF);
        return 0;
    }

    // Misc:
    // this.usbDeviceConnection.bulkTransfer(epInput, buf, buf.length, this.timeoutInput);
    // this.usbDeviceConnection.bulkTransfer(epOutput, buf, length, this.timeoutOutput);
    // this.usbDeviceConnection.controlTransfer(0x21, 0x22, dtr_rts_state, 0, null, 0, this.controlTimeout); // 0x22 = UCDC_SET_CONTROL_LINE_STATE

    public int setup(BaudRate baudRate, DataBits dataBits, StopBits stopBits, Parity parity, FlowControl flowControl) throws IOException {
        byte[] j = new byte[7];
        this.usbDeviceConnection.controlTransfer(0xa1, 0x21, 0, 0, j, 7, this.controlTimeout);

        int baudValue = baudRate.value();
        j[0] = (byte) (baudValue & 255);
        j[1] = (byte) ((baudValue >> 8) & 255);
        j[2] = (byte) ((baudValue >> 16) & 255);
        j[3] = (byte) ((baudValue >> 24) & 255);
        j[4] = stopBits.value();
        j[5] = parity.value();
        j[6] = dataBits.value();

        this.usbDeviceConnection.controlTransfer(0x21, 0x20, 0, 0, j, 7, this.controlTimeout); // 0x20 = UCDC_SET_LINE_CODING
        this.usbDeviceConnection.controlTransfer(0x21, 0x23, 0, 0, null, 0, this.controlTimeout); // 0x23 = UCDC_SEND_BREAK

        this.flowControl = flowControl;
        switch (flowControl) {
            case FlowControl.OFF:
                this.usbDeviceConnection.controlTransfer(0x40, 1, 0, 0x00, null, 0, this.controlTimeout);
                this.usbDeviceConnection.controlTransfer(0x40, 1, 1, 0x00, null, 0, this.controlTimeout);
                this.usbDeviceConnection.controlTransfer(0x40, 1, 2, 0x44, null, 0, this.controlTimeout);
                break;
        
            case FlowControl.RTSCTS:
                this.usbDeviceConnection.controlTransfer(0x40, 1, 0, 0x61, null, 0, this.controlTimeout);
                this.usbDeviceConnection.controlTransfer(0x40, 1, 1, 0x00, null, 0, this.controlTimeout);
                this.usbDeviceConnection.controlTransfer(0x40, 1, 2, 0x44, null, 0, this.controlTimeout);
                break;
            
            case FlowControl.RFRCTS:
                break;
            
            case FlowControl.DTRDSR:
                if (this.revision == 4) {
                    this.usbDeviceConnection.controlTransfer(0x40, 1, 0, 0x49, null, 0, this.controlTimeout);
                    this.usbDeviceConnection.controlTransfer(0x40, 1, 1, 0x05, null, 0, this.controlTimeout);
                    this.usbDeviceConnection.controlTransfer(0x40, 1, 2, 0x44, null, 0, this.controlTimeout);
                }
                break;

            case FlowControl.RTSCTSDTRDSR:
                if (this.revision == 4) {
                    this.usbDeviceConnection.controlTransfer(0x40, 1, 0, 0x69, null, 0, this.controlTimeout);
                    this.usbDeviceConnection.controlTransfer(0x40, 1, 1, 0x07, null, 0, this.controlTimeout);
                    this.usbDeviceConnection.controlTransfer(0x40, 1, 2, 0x44, null, 0, this.controlTimeout);
                }
                break;

            case FlowControl.XONXOFF:
                this.usbDeviceConnection.controlTransfer(0x40, 1, 0, 0xc1, null, 0, this.controlTimeout);
                this.usbDeviceConnection.controlTransfer(0x40, 1, 1, 0x00, null, 0, this.controlTimeout);
                this.usbDeviceConnection.controlTransfer(0x40, 1, 2, 0x44, null, 0, this.controlTimeout);
                break;
        }
        if (this.isSomeVariant1) {
            writeRegister(0x00, 0x31);
            writeRegister(0x01, 0x08);
        }
        return 0;
    }

    public void PL2303TB_Set_PWM(int PWM_IO_Num, byte Frequency_value, byte Duty_value) {
        int i2 = (Duty_value << 8) + Frequency_value;

        if (PWM_IO_Num == 0) {
            writeRegister(0x02, 0x00);
        }
        writeRegister(0x10 + PWM_IO_Num, i2);
    }

    public void PL2303TB_Enable_GPIO(int GPIO_Num, boolean Enable) {
        if (GPIO_Num == 6 || GPIO_Num == 7 || GPIO_Num == 9) {
            writeRegister(0x02, 0x00);
        }
        this.reg0eShadow = setBit(this.reg0eShadow, GPIO_Num, Enable);
        writeRegister(0x0e, this.reg0eShadow);
    }

    public void PL2303HXD_Enable_GPIO(int GPIO_Num, boolean Enable) {
        switch (GPIO_Num) {
            case 0:
                writeRegister(0x01, setBit(readRegister(0x81), 4, Enable));
                break;
            case 1:
                writeRegister(0x01, setBit(readRegister(0x81), 5, Enable));
                break;
            case 2:
                this.reg0cShadow = setBitMask(this.reg0cShadow, 0x03, Enable);
                writeRegister(0x0c, this.reg0cShadow);
                break;
            case 3:
                this.reg0cShadow = setBitMask(this.reg0cShadow, 0x0c, Enable);
                writeRegister(0x0c, this.reg0cShadow);
                break;
            case 4:
                this.reg06Shadow = setBitMask(this.reg06Shadow, 0x03, Enable);
                writeRegister(0x06, this.reg06Shadow);
                break;
            case 5:
                this.reg06Shadow = setBitMask(this.reg06Shadow, 0x0c, Enable);
                writeRegister(0x06, this.reg06Shadow);
                break;
            case 6:
                this.reg06Shadow = setBitMask(this.reg06Shadow, 0x30, Enable);
                writeRegister(0x06, this.reg06Shadow);
                break;
            case 7:
                this.reg06Shadow = setBitMask(this.reg06Shadow, 0xc0, Enable);
                writeRegister(0x06, this.reg06Shadow);
                break;
        }
    }

    public void PL2303TB_Set_GPIO_Value(int GPIO_Num, boolean val) {
        writeRegister(0x0f, set_bit(readRegister(0x8f), GPIO_Num, val));
    }

    public void PL2303HXD_Set_GPIO_Value(int GPIO_Num, boolean val) {
        switch (GPIO_Num) {
            case 0:
                writeRegister(0x01, setBit(readRegister(0x81), 6, val));
                break;
            case 1:
                writeRegister(0x01, setBit(readRegister(0x81), 7, val));
                break;
            case 2:
                writeRegister(0x0d, setBit(readRegister(0x8d), 0, val));
                break;
            case 3:
                writeRegister(0x0d, setBit(readRegister(0x8d), 1, val));
                break;
            case 4:
                this.reg07Shadow = setBit(this.reg07Shadow, 0, val);
                writeRegister(0x07, this.reg07Shadow);
                break;
            case 5:
                this.reg07Shadow = setBit(this.reg07Shadow, 1, val);
                writeRegister(0x07, this.reg07Shadow);
                break;
            case 6:
                this.reg07Shadow = setBit(this.reg07Shadow, 2, val);
                writeRegister(0x07, this.reg07Shadow);
                break;
            case 7:
                this.reg07Shadow = setBit(this.reg07Shadow, 3, val);
                writeRegister(0x07, this.reg07Shadow);
                break;
        }
    }

    public int[] PL2303TB_Get_GPIO_Value(int GPIO_Num) {
        return get_bit(readRegister(0x8f), GPIO_Num);
    }

    public int[] PL2303HXD_Get_GPIO_Value(int GPIO_Num) {
        switch (GPIO_Num) {
            case 0:
                return get_bit(readRegister(0x81), 6);
            case 1:
                return get_bit(readRegister(0x81), 7);
            case 2:
                return get_bit(readRegister(0x8d), 0);
            case 3:
                return get_bit(readRegister(0x8d), 1);
            case 4:
                return get_bit(readRegister(0x87), 0);
            case 5:
                return get_bit(readRegister(0x87), 1);
            case 6:
                return get_bit(readRegister(0x87), 2);
            case 7:
                return get_bit(readRegister(0x87), 3);
        }
        return -1;
    }

    private int d(int i) {
        byte[] bArr = new byte[2];
        bArr[0] = (byte) (i & 255);
        bArr[1] = (byte) ((i >> 8) & 255);
        this.usbDeviceConnection.controlTransfer(0xa1, 0x20, 0, 0, bArr, 2, this.timeout_out);
        Thread.sleep(100);
        this.usbDeviceConnection.bulkTransfer(this.epInterrupt, bArr, bArr.length, this.timeout_in);
        return (bArr[1] << 8) | bArr[0];
    }

    public int PL2303HXD_GetCommModemStatus() {
        return readRegister(0x87);
        // bit0: RI
        // bit1: DCD
        // bit2: DSR
        // bit3: CTS
    }
}
