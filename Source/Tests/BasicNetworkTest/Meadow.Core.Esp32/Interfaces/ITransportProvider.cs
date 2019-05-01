using System;

namespace Meadow.Core.Esp32
{
    public interface ITransportProvider
    {
        event TransportProviderErrorEventHandler TransportError;

        event TransportProviderDataReceivedEventHandler MessageReceived;
        
        void QueueMessageForEsp32(Messages.Message message);
    }
}
