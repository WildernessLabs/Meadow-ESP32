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
                case 0x00:
                    flashSize = "1MB";
                    break;
                case 0x10:
                    flashSize = "2MB";
                    break;
                case 0x20:
                    flashSize = "4MB";
                    break;
                case 0x30:
                    flashSize = "8MB";
                    break;
                case 0x40:
                    flashSize = "16MB";
                    break;
            }

            return (flashSize);
        }
    }
}
