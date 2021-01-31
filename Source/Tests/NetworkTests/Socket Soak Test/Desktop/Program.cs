using System;  
using System.Net;  
using System.Net.Sockets;  
using System.Text;  
  
public class SynchronousSocketListener
{  
  
    // Incoming data from the client.  
    public static string data = null;  
  
    public static void StartListening()
    {  
        // Data buffer for incoming data.  
        byte[] bytes = new Byte[1024];  
  
        IPAddress ipAddress = IPAddress.Parse("192.168.1.197");
        IPEndPoint localEndPoint = new IPEndPoint(ipAddress, 11000);  
  
        // Create a TCP/IP socket.  
        Socket listener = new Socket(ipAddress.AddressFamily, SocketType.Stream, ProtocolType.Tcp );  
  
        // Bind the socket to the local endpoint and
        // listen for incoming connections.  
        try
        {  
            listener.Bind(localEndPoint);  
            listener.Listen(10);
            Console.WriteLine("Waiting for a connection...");
            Socket handler = listener.Accept();  
            data = null;  

            // An incoming connection needs to be processed.  
            int messageNumber = 0;
            while (true)
            {
                int bytesRec = handler.Receive(bytes);  
                string message = Encoding.ASCII.GetString(bytes,0,bytesRec);  
                messageNumber++;
                Console.Write($"Received {messageNumber}: {message}");
                string response = $"Received message {messageNumber}";
                handler.Send(Encoding.ASCII.GetBytes(response));
            }
        }
        catch (Exception e)
        {  
            Console.WriteLine(e.ToString());  
        }  
    }  
  
    public static int Main(String[] args) 
    {  
        StartListening();  
        return 0;  
    }  
}