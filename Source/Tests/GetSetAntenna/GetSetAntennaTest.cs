using System;
using System.Net;
using System.Text;
using Meadow;
using Meadow.Devices;
using Meadow.Gateways;
using System.Threading;
using System.Threading.Tasks;

namespace GetSetAntenna
{
    public class GetSetAntennaTest : App<F7Micro, GetSetAntennaTest>
    {
        public GetSetAntennaTest()
        {
            Console.WriteLine("initialising WiFi Adapter object.");

            Device.InitWiFiAdapter().Wait();
        }

        public void RunTest()
        {
            AntennaType antenna = Device.WiFiAdapter.Antenna;
            AntennaType newAntenna = AntennaType.OnBoard;

            switch (antenna)
            {
                case AntennaType.External:
                    newAntenna = AntennaType.OnBoard;
                    Console.WriteLine("Current antenna: External.");
                    break;
                case AntennaType.OnBoard:
                    newAntenna = AntennaType.External;
                    Console.WriteLine("Current antenna: onboard.");
                    break;
            }

            Device.WiFiAdapter.SetAntenna(newAntenna, true);
            Console.WriteLine("Default antenna changed.");
        }
    }
}
