using System;
using System.IO;
using System.Collections.Generic;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace FileGenerator
{
    class Program
    {
        static void Main(string[] args)
        {
            string esp32SourceDirectory = $"..{Path.DirectorySeparatorChar}MeadowComms{Path.DirectorySeparatorChar}main{Path.DirectorySeparatorChar}";
            string esp32EnumJsonFileName = $"{esp32SourceDirectory}SharedEnums.json";
            string cEnumFileName = $"{esp32SourceDirectory}SharedEnums.hpp";
            string messageEncodersJsonFileName = $"{esp32SourceDirectory}Messages.json";
            string cMessageEncodersHeaderFileName = $"{esp32SourceDirectory}MessageEncoders.hpp";
            string cMessageEncodersSourceFileName = $"{esp32SourceDirectory}MessageEncoders.cpp";
            //
            string cSharpEnumFileName = $"..{Path.DirectorySeparatorChar}UARTTransport{Path.DirectorySeparatorChar}Esp32Messaging.SharedEnums.cs";
            string cSharpMessageProcessorFileName = $"..{Path.DirectorySeparatorChar}UARTTransport{Path.DirectorySeparatorChar}MessageEncoders.cs";
            string cSharpStructuresFileName = $"..{Path.DirectorySeparatorChar}UARTTransport{Path.DirectorySeparatorChar}Esp32Messaging.MessageStructures.cs";

            Console.WriteLine("File Generator");

            EnumProcessor enumProcessor = new EnumProcessor(esp32EnumJsonFileName, cEnumFileName, cSharpEnumFileName);
            enumProcessor.ProcessFile();
            MessageProcessor messageProcessor = new MessageProcessor(messageEncodersJsonFileName, cMessageEncodersHeaderFileName, cMessageEncodersSourceFileName, cSharpMessageProcessorFileName, cSharpStructuresFileName);
            messageProcessor.ProcessFile();
        }
    }
}
