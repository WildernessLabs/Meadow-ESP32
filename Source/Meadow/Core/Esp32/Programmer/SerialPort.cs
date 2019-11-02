//
//    Place holder for the serial port that will eventually be connect the ESP32 and the STM32.
//
namespace Meadow.Core.Esp32
{
    public class SerialPort
    {
        #region Constructor(s)
        
        /// <summary>
        /// Default constructor is private to prevent it being used.
        /// </summary>
        private SerialPort()
        {
            
        }

        /// <summary>
        /// Create a new serial port connected to the specified hardware port using the requested baud rate.
        /// </summary>
        /// <param name="portName">Name of the port to use.</param>
        /// <param name="baudRate">Communication speed.</param>
        public SerialPort(string portName, int baudRate)
        {
        }
        
        #endregion
        
        #region Methods

        /// <summary>
        /// Open the serial port and establish the connection.
        /// </summary>
        public void Open()
        {
            
        }

        /// <summary>
        /// Write the array of bytes to the serial port.
        /// </summary>
        /// <param name="data">Array of bytes to be written to the serial port.</param>
        public void Write(byte[] data)
        {
            
        }

        /// <summary>
        /// Read data from the serial port and return the array of bytes containing the data.
        /// </summary>
        /// <returns>Array of bytes read from the serial port.</returns>
        public byte[] Read()
        {
            return (new byte[20]);
        }
        
        #endregion
    }
}
