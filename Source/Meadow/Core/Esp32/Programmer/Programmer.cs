namespace Meadow.Core.Esp32
{
    public class Programmer
    {
        /// <summary>
        /// Version number from the Python script used as a base for this port to C#.
        /// </summary>
        public const string VERSION = "2.6-beta1";

        /// <summary>
        /// Timeout for most flash operations.
        /// </summary>
        public const int DEFAULT_TIMEOUT = 3;

        /// <summary>
        /// Timeout for starting flash (may perform erase).
        /// </summary>
        public const int START_FLASH_TIMEOUT = 20;
        
        /// <summary>
        /// Timeout for full chip erase.
        /// </summary>
        public const int CHIP_ERASE_TIMEOUT = 120;
        
        /// <summary>
        /// Longest any command can run.
        /// </summary>
        public const int MAX_TIMEOUT = CHIP_ERASE_TIMEOUT * 2;
        
        /// <summary>
        /// Timeout for syncing with bootloader.
        /// </summary>
        public const float SYNC_TIMEOUT = 0.1f;
        
        /// <summary>
        /// Timeout (per megabyte) for calculating md5sum.
        /// </summary>
        public const int MD5_TIMEOUT_PER_MB = 8;
        
        /// <summary>
        /// Timeout (per megabyte) for erasing a region.
        /// </summary>
        public const int ERASE_REGION_TIMEOUT_PER_MB = 30;
        
        /// <summary>
        /// Special short timeout for ESP_MEM_END, as it may never respond.
        /// </summary>
        public const float MEM_END_ROM_TIMEOUT = 0.05f;
        
        /// <summary>
        /// Timeout for serial port write.
        /// </summary>
        public const int DEFAULT_SERIAL_WRITE_TIMEOUT = 10;       
    }
}
