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
            string esp32EnumJsonFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}SharedEnums.json";
            string cEnumFileName = $"{esp32SourceDirectory}SharedEnums.hpp";
            //
            string cSharpEnumFileName = $"..{Path.DirectorySeparatorChar}UARTTransport{Path.DirectorySeparatorChar}Esp32Messaging.SharedEnums.cs";

            Console.WriteLine("File Generator");

            Console.WriteLine("    Generating enums.");
            EnumProcessor enumProcessor = new EnumProcessor(esp32EnumJsonFileName, cEnumFileName, cSharpEnumFileName);
            enumProcessor.ProcessFile();

            Console.WriteLine("    Generating message encoders and decoders.");
            MessageProcessor messageProcessor = new MessageProcessor();
            messageProcessor.InputFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}Messages.json";;
            messageProcessor.CHeaderFileName = $"{esp32SourceDirectory}Encoders.hpp";
            messageProcessor.CSourceFileName = $"{esp32SourceDirectory}Encoders.cpp";
            messageProcessor.CSharpFileName = $"..{Path.DirectorySeparatorChar}UARTTransport{Path.DirectorySeparatorChar}MessageEncoders.cs";;
            messageProcessor.CSharpStructureFileName = $"..{Path.DirectorySeparatorChar}UARTTransport{Path.DirectorySeparatorChar}Esp32Messaging.MessageStructures.cs";;
            messageProcessor.CHeaderTemplateFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}Encoders.hpp.txt";
            messageProcessor.CSourceTemplateFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}Encoders.cpp.txt";
            messageProcessor.ProcessFile();
        }
    }
}
