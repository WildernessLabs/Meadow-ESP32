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
        
        public const string CHIP_NAME = "Espressif device";

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
        public const UInt32 ESP32_UART_DATE_REGISTER_VALUE = 0;    // TODO: fill in this value.

        /// <summary>
        /// Start address of the ROM instruction memory.
        /// </summary>
        public const UInt32 IROM_MAP_START = 0x40200000;
        
        /// <summary>
        /// End address of the ROM instruction memory.
        /// </summary>
        public const UInt32 IROM_MAP_END = 0x40300000;

        // The number of bytes in the UART response that signify command status
        public const UInt32 STATUS_BYTES_LENGTH = 2;
        
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
        
        #endregion
    }
}
