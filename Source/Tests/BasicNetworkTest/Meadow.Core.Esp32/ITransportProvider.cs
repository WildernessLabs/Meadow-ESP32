using System;
using Meadow.Core.Esp32;

namespace SpiTest
{
    public interface ITransportProvider
    {
        void QueueMessageForEsp32(Messages.Message message);
    }
}
