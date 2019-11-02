using System;

namespace Meadow.Core.Esp32
{
    /// <summary>
    /// Base class providing access to ESP ROM & software stub bootloaders.
    /// Subclasses provide ESP8266 & ESP32 specific functionality.
    ///
    /// Don't instantiate this base class directly, either instantiate a subclass or
    /// call ESPLoader.detect_chip() which will interrogate the chip and return the
    /// appropriate subclass instance.
    /// </summary>
    public class EspLoader
    {
        #region Constants

        public const string CHIP_NAME = "ESP32";

        public const bool IS_STUB = false;

        public string DEFAULT_PORT = "/dev/ttyUSB0";

        // Commands supported by ESP8266 ROM bootloader
        public const byte ESP_FLASH_BEGIN = 0x02;
        public const byte ESP_FLASH_DATA = 0x03;
        public const byte ESP_FLASH_END = 0x04;
        public const byte ESP_MEM_BEGIN = 0x05;
        public const byte ESP_MEM_END = 0x06;
        public const byte ESP_MEM_DATA = 0x07;
        public const byte ESP_SYNC = 0x08;
        public const byte ESP_WRITE_REG = 0x09;
        public const byte ESP_READ_REG = 0x0a;

        // Some commands supported by ESP32 ROM bootloader (or -8266 w/ stub)
        public const byte ESP_SPI_SET_PARAMS = 0x0B;
        public const byte ESP_SPI_ATTACH = 0x0D;
        public const byte ESP_CHANGE_BAUDRATE = 0x0F;
        public const byte ESP_FLASH_DEFL_BEGIN = 0x10;
        public const byte ESP_FLASH_DEFL_DATA = 0x11;
        public const byte ESP_FLASH_DEFL_END = 0x12;
        public const byte ESP_SPI_FLASH_MD5 = 0x13;

        // Some commands supported by stub only
        public const byte ESP_ERASE_FLASH = 0xD0;
        public const byte ESP_ERASE_REGION = 0xD1;
        public const byte ESP_READ_FLASH = 0xD2;
        public const byte ESP_RUN_USER_CODE = 0xD3;

        /// <summary>
        /// Maximum block sized for RAM writes. 
        /// </summary>
        public const UInt16 ESP_RAM_BLOCK = 0x1800;

        /// <summary>
        /// Maximum block sized for Flash writes. 
        /// </summary>
        public const UInt16 FLASH_WRITE_SIZE = 0x400;

        // Default baudrate. The ROM auto-bauds, so we can use more or less whatever we want.
        public const int ESP_ROM_BAUD = 115200;

        /// <summary>
        /// First byte of the application image.
        /// </summary>
        public const byte ESP_IMAGE_MAGIC = 0xe9;

        /// <summary>
        /// Initial state for the checksum routine.
        /// </summary>
        public const byte ESP_CHECKSUM_MAGIC = 0xef;

        /// <summary>
        /// Flash sector size, minimum unit of erase.
        /// </summary>
        public const UInt32 FLASH_SECTOR_SIZE = 0x1000;

        public const UInt32 UART_DATA_REG_ADDR = 0x60000078;

        /// <summary>
        /// Expected contents of the ESP32 Date register
        /// </summary>
        public const UInt32 ESP32_UART_DATE_REGISTER_VALUE = 0; // TODO: fill in this value.

        /// <summary>
        /// Start address of the ROM instruction memory.
        /// </summary>
        public const UInt32 IROM_MAP_START = 0x400d0000;

        /// <summary>
        /// End address of the ROM instruction memory.
        /// </summary>
        public const UInt32 IROM_MAP_END = 0x40400000;

        /// <summary>
        /// Start address of the ROM data memory.
        /// </summary>
        public const UInt32 DROM_MAP_START = 0x3F400000;

        /// <summary>
        /// End address of the ROM data memory.
        /// </summary>
        public const UInt32 DROM_MAP_END = 0x3F800000;

        /// <summary>
        /// Number of bytes returned by the ESP to indicate the status.
        /// </summary>
        public const UInt32 STATUS_BYTES_LENGTH = 4;

        /// <summary>
        /// Base address of the registers.
        /// </summary>
        public const UInt32 SPI_REG_BASE = 0x60002000;

        /// <summary>
        /// Base address of the fuses in memory.
        /// </summary>
        public const UInt32 EFUSE_REG_BASE = 0x6001a000;

        public const UInt32 SPI_W0_OFFS = 0x80;
        public const bool SPI_HAS_MOSI_DLEN_REG = true;

        public const UInt32 BOOTLOADER_FLASH_OFFSET = 0x1000;

        /// <summary>
        /// SDIO Vdd choices.
        /// </summary>
        public string[] OVERRIDE_VDDSDIO_CHOICES = { "1.8V", "1.9V", "OFF" };

        #endregion
        
        #region Private Variables

        /// <summary>
        /// Serial port connecting the STM32 and the ESP32.
        /// </summary>
        private readonly SerialPort _serialPort;
        
        #endregion
        
        #region Properties
        
        #endregion
        
        #region Constructor(s)

        /// <summary>
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private EspLoader()
        {
        }

        /// <summary>
        /// Create a new EspLoad instance that uses the specified serial port to connect the
        /// ESP32 and the STM32.
        /// </summary>
        /// <param name="serialPortName">Name of the serial port connecting the two chips.</param>
        /// <param name="baudRate">Speed of the connection between the two chips.</param>
        public EspLoader(string serialPortName, int baudRate)
        {
            _serialPort = new SerialPort(serialPortName, baudRate);
        }
        
        #endregion
        
        #region Methods

        /// <summary>
        /// Read the specified register and return the value.
        /// </summary>
        /// <param name="registerAddress">Address of the register in the ESP32 address space.</param>
        /// <returns>Contents of the register.</returns>
        private UInt32 ReadRegister(UInt32 registerAddress)
        {
            return (20);        // TODO: Implement read register command.
        }

        /// <summary>
        /// Read the contents of the specified fuse.
        /// </summary>
        /// <param name="fuseNumber">Fuse number to read.</param>
        /// <returns>Contents of the fuse to be read.</returns>
        private UInt32 ReadEFuse(UInt32 fuseNumber)
        {
            return (ReadRegister(EFUSE_REG_BASE + fuseNumber));
        }

        /// <summary>
        /// Detect the chip connected to the serial port.
        /// </summary>
        /// <exception cref="Exception">Thrown if an unknown chip is detected.</exception>
        public void DetectChip()
        {
            _serialPort.Open();
            var dateRegister = ReadRegister(UART_DATA_REG_ADDR);
            if (dateRegister != ESP32_UART_DATE_REGISTER_VALUE)
            {
                throw new Exception("ESP32 chip expected.");
            }
            Console.WriteLine("ESP32 chip detected.");
        }

        /// <summary>
        /// Generate a checksum for the data.
        /// </summary>
        /// <param name="data">Array of bytes needing a checksum generating.</param>
        /// <returns>Simple XOR checksum of the data.</returns>
        public byte Checksum(byte[] data)
        {
            byte result = ESP_CHECKSUM_MAGIC;
            foreach (byte b in data)
            {
                result ^= b;
            }
                
            return (result);
        }

        /// <summary>
        /// Try to read the encryption key from BLOCK1 and check if it valid.
        /// </summary>
        /// <returns>True if the encryption key is valid, false otherwise.</returns>
        public bool IsFlashEncryptionKeyValid()
        {
            bool result = true;    // Assume a valid key is present.
            
            //
            //  Bit 0 of efuse_rd_disable[3:0] is mapped to BLOCK1 this bit is at
            // position 16 in EFUSE_BLK0_RDATA0_REG
            //
            UInt32 word0 = ReadEFuse(0);
            bool rdDisable = ((word0 >> 16) & 0x01) != 0x01;
            if (rdDisable)
            {
                result = true;
            }
            else
            {
                //
                //  Reading of BLOCK1 is ALLOWED so we will read and verify for non-zero.
                //  When ESP32 has not generated AES/encryption key in BLOCK1, the contents will be readable and 0.
                //  If the flash encryption is enable it is expected to have a valid non-zero key. We break out on
                //  first occurence of non-zero value. 
                //
                for (uint fuse = 0; fuse < 7; fuse++)
                {
                    if (ReadEFuse(14 + fuse) != 0)
                    {
                        result = false;
                        break;
                    }
                }
            }

            return (result);
        }
        
        #endregion
    }
}
