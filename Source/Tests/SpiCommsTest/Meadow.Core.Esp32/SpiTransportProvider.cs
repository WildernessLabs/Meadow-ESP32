using System;
using Meadow.Core.Esp32;

namespace SpiTest
{
    public class SpiTransportProvider : ITransportProvider
    {
        #region Constriuctor(s)

        /// <summary>
        /// Default constructor for the SPI Transport Provider implementation.
        /// </summary>
        public SpiTransportProvider()
        {
        }

        #endregion

        #region Methods

        public void QueueMessageForEsp32(Messages.Message message)
        {

        }

        #endregion
    }
}
