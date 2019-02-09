using System;
using System.IO;
using System.Collections.Generic;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace FileGenerator
{
    class Program
    {
        static DataType[] _dataTypes = 
        {
            new DataType("uint32", 4),
            new DataType("int", 4),
            new DataType("byte", 1),
            new DataType("array", -1)
        };

        static void Main(string[] args)
        {
            string sourceFileName = $"..{Path.DirectorySeparatorChar}MeadowComms{Path.DirectorySeparatorChar}main{Path.DirectorySeparatorChar}SharedEnums.json";
            string cFileName = $"..{Path.DirectorySeparatorChar}MeadowComms{Path.DirectorySeparatorChar}main{Path.DirectorySeparatorChar}SharedEnums.hpp";
            string cSharpFileName = $"..{Path.DirectorySeparatorChar}UARTTransport{Path.DirectorySeparatorChar}Esp32Messaging.SharedEnums.cs";
            EnumProcessor enumProcessor = new EnumProcessor(sourceFileName, cFileName, cSharpFileName);

            Console.WriteLine("File Generator");

            enumProcessor.ProcessFile();

            using (StreamReader structuresFile = new StreamReader("Messages.json"))
            {
                string fileContents = structuresFile.ReadToEnd();
                JObject o = JObject.Parse(fileContents);

                JArray a = (JArray)o["Messages"];

                IList<Message> messages = a.ToObject<IList<Message>>();
                foreach (Message m in messages)
                {
                    Console.WriteLine($"Message name: {m.Name}");
                    if (m.MessageElements != null)
                    {
                        Console.WriteLine("    Message elements: ");
                        foreach (MessageElement me in m.MessageElements)
                        {
                            Console.WriteLine($"        {me.Name} : {me.DateType}");
                        }
                    }
                }
            }
        }
    }
}
