using System;

namespace Meadow.Coprocessor.F7Micro
{
    #region Delegates

    /// <summary>
    ///     Delegate for the Transport Provider data received event handler.
    /// </summary>
    /// <param name="sender">Object that is sending the error response.</param>
    /// <param name="e">Messages.Message object containing the message received.</param>
    public delegate void TransportProviderDataReceivedEventHandler(object sender, MessageReceviedEventArgs e);

    #endregion
}
