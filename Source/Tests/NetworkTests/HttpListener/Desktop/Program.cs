using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace GetFromHttpListener
{
    class MainClass
    {
        public static void Main(string[] args)
        {
            try
            {
                string server = "http://192.168.1.211";
                Int32 port = 80;
                var data = new Byte[100000];
                String responseData = String.Empty;

                HttpWebRequest webRequest = (HttpWebRequest) WebRequest.Create(server);
                webRequest.Timeout = Timeout.Infinite;
                WebResponse response = webRequest.GetResponse();
                Stream stream = webRequest.GetRequestStream();

                Console.WriteLine("Reading data.");
                Int32 bytes = stream.Read(data, 0, data.Length);
                responseData = System.Text.Encoding.ASCII.GetString(data, 0, bytes);
                Console.WriteLine("Received: {0}", responseData);

                response.Close();
            }
            catch (ArgumentNullException e)
            {
                Console.WriteLine("ArgumentNullException: {0}", e);
            }
            catch (SocketException e)
            {
                Console.WriteLine("SocketException: {0}", e);
            }

            Console.WriteLine("\n Press Enter to continue...");
            Console.Read();
        }
    }
}
