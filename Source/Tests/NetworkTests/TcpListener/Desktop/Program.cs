using System;
using System.IO;
using System.Net.Sockets;
using System.Threading;

namespace TCPClient
{
    class MainClass
    {
        public static void Main(string[] args)
        {
            try
            {
                string server = "192.168.1.200";
                Int32 port = 11000;
                TcpClient client = new TcpClient(server, port);

                string message = "Hello from computer.";
                Byte[] data = System.Text.Encoding.ASCII.GetBytes(message);

                NetworkStream stream = client.GetStream();

                //Thread.Sleep(15000);

                stream.Write(data, 0, data.Length);
                Console.WriteLine("Sent: {0}", message);

                // Receive the TcpServer.response.

                // Buffer to store the response bytes.
                //data = new Byte[256];

                // String to store the response ASCII representation.
                String responseData = String.Empty;

                // Read the first batch of the TcpServer response bytes.
                //Int32 bytes = stream.Read(data, 0, data.Length);
                //responseData = System.Text.Encoding.ASCII.GetString(data, 0, bytes);
                //Console.WriteLine("Received: {0}", responseData);

                // Close everything.
                //stream.Close();
                client.Close();
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
