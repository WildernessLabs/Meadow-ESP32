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
                string server = "http://192.168.1.200";
                Int32 port = 80;
                //TcpClient client = new TcpClient(server, port);

                //string request = "GET / HTTP/1.1\n" +
                //                 "Host: 192.168.1.200\n" +
                //                 "Connection: keep - alive\n" +
                //                 "Cache - Control: max - age = 0\n" +
                //                 "DNT: 1\n" +
                //                 "Upgrade - Insecure - Requests: 1\n" +
                //                 "User - Agent: Mozilla / 5.0(Macintosh; Intel Mac OS X 11_0_1) AppleWebKit / 537.36(KHTML, like Gecko) Chrome / 87.0.4280.88 Safari / 537.36\n" +
                //                 "Accept: text / html,application / xhtml + xml,application / xml; q = 0.9,image / avif,image / webp,image / apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\n" +
                //                 "Accept-Encoding: gzip, deflate\n" +
                //                 "Accept-Language: en-GB,en-US;q=0.9,en;q=0.8\n\n";
                //Byte[] data = System.Text.Encoding.ASCII.GetBytes(request);

                //Console.WriteLine("Getting network stream.");
                //NetworkStream stream = client.GetStream();

                ////Thread.Sleep(15000);

                //Console.WriteLine("Sending request.");
                //stream.Write(data, 0, data.Length);
                //Console.WriteLine("Request sent.");

                var data = new Byte[100000];
                String responseData = String.Empty;

                //Console.WriteLine("Reading data.");
                //Int32 bytes = stream.Read(data, 0, data.Length);
                //responseData = System.Text.Encoding.ASCII.GetString(data, 0, bytes);
                //Console.WriteLine("Received: {0}", responseData);

                //stream.Close();
                //client.Close();

                HttpWebRequest webRequest = (HttpWebRequest) WebRequest.Create(server);
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
