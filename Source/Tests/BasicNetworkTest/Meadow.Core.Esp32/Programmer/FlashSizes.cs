using System.Runtime.Remoting.Messaging;

namespace Meadow.Core.Esp32
{
    /// <summary>
    /// Possible flash sizes for the ESP8266 and ESP32 chips.
    /// </summary>
    public class FlashSizes
    {
        /// <summary>
        /// Byte code for the flash sizes.
        /// </summary>
        public byte FlashCode;

        /// <summary>
        /// Convert the flash size to a printable string.
        /// </summary>
        /// <returns>String representing the flash size.</returns>
        public override string ToString()
        {
            string flashSize = "";

            switch (FlashCode)
            {
                case 0x12:
                    flashSize = "256KB";
                    break;
                case 0x13:
                    flashSize = "512KB";
                    break;
                case 0x14:
                    flashSize = "1MB";
                    break;
                case 0x15:
                    flashSize = "2MB";
                    break;
                case 0x16:
                    flashSize = "4MB";
                    break;
                case 0x17:
                    flashSize = "8MB";
                    break;
                case 0x18:
                    flashSize = "16MB";
                    break;
            }

            return (flashSize);
        }
    }
}
