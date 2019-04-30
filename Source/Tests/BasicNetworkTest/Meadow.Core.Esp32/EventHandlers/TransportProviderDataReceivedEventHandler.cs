using System;

namespace Meadow.Core.Esp32
{
    #region Delegates

    /// <summary>
    ///     Delegate for the Uart data received event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">Messages.Message object containing the message received.</param>
    public delegate void TransportProviderDataReceivedEventHandler(object sender, MessageReceviedEventArgs e);

    #endregion
}
