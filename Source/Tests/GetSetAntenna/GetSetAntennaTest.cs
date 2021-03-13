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
            Console.WriteLine("Initialising WiFi Adapter object.");

            Device.InitWiFiAdapter().Wait();
        }

        public void DisplayAntenna(AntennaType antenna)
        {
            switch (antenna)
            {
                case AntennaType.External:
                    Console.WriteLine("Current antenna: External.");
                    break;
                case AntennaType.OnBoard:
                    Console.WriteLine("Current antenna: OnBoard.");
                    break;
            }
        }

        public void RunTest()
        {
            AntennaType newAntenna = AntennaType.OnBoard;
            AntennaType antenna = Device.WiFiAdapter.Antenna;
            DisplayAntenna(antenna);

            switch (antenna)
            {
                case AntennaType.External:
                    newAntenna = AntennaType.OnBoard;
                    break;
                case AntennaType.OnBoard:
                    newAntenna = AntennaType.External;
                    break;
            }

            Device.WiFiAdapter.SetAntenna(newAntenna, true);
            Console.WriteLine("Default antenna changed.");
            antenna = Device.WiFiAdapter.Antenna;
            DisplayAntenna(antenna);
        }
    }
}
