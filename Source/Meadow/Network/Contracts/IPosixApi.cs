using System;
using System.Data.SqlClient;

namespace Meadow.Network
{
    public interface IPosixApi
    {
        int GetAddrInfo(string nodeName, string serverName, object hints, out UInt32 res);
    }
}
