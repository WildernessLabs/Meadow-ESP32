using System;
using System.Data.SqlClient;

namespace Meadow.Network
{
    public interface IPosixApi
    {
        int GetAddrInfo(string nodeName, string serviceName, object hints, out object res);
        
        
    }
}
