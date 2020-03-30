using System;
using System.IO;
using System.Collections.Generic;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace FileGenerator
{
    class Program
    {
        /// <summary>
        /// Location of the F7Micro source directory.
        /// </summary>
        const string MEADOW_F7MICRO_SOURCE_DIRECTORY = "MeadowF7MicroCoprocessorSourceDirectory";

        static void Main(string[] args)
        {
            Console.WriteLine("File Generator 1.01");
            string csharpDestinationDirectory = Environment.GetEnvironmentVariable(MEADOW_F7MICRO_SOURCE_DIRECTORY);
            if (string.IsNullOrEmpty(csharpDestinationDirectory))
            {
                Console.WriteLine($"Please set the environment variable '{MEADOW_F7MICRO_SOURCE_DIRECTORY}' before running this application.");
                Environment.Exit(-1);
            }
            Console.WriteLine($"    C# source directory: {csharpDestinationDirectory}");
            string nuttxOutputDirectory = $".{Path.DirectorySeparatorChar}Output{Path.DirectorySeparatorChar}";

            //
            //  Generate the enum files.
            //
            string esp32SourceDirectory = $"..{Path.DirectorySeparatorChar}MeadowComms{Path.DirectorySeparatorChar}main{Path.DirectorySeparatorChar}";
            Console.WriteLine($"    ESP32 source directory: {esp32SourceDirectory}");
            string enumJsonFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}SharedEnums.json";
            string cEnumFileName = $"{esp32SourceDirectory}SharedEnums.hpp";
            //
            string cSharpEnumFileName = $"{csharpDestinationDirectory}{Path.DirectorySeparatorChar}Enums.cs";

            Console.WriteLine("    Generating enums.");
            EnumProcessor enumProcessor = new EnumProcessor(enumJsonFileName, nuttxOutputDirectory);
            enumProcessor.ProcessFile();

            //
            //  Generate the message processor (structure, encoders & extract) methods and structure defintions.
            //
            // Console.WriteLine("    Generating message encoders and decoders.");
            // MessageProcessor messageProcessor = new MessageProcessor();
            // messageProcessor.NuttxOutputDirectory = nuttxOutputDirectory;
            // messageProcessor.MessageDefinitionInputFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}Messages.json";
            // messageProcessor.CHeaderFileName = $"{esp32SourceDirectory}Encoders.hpp";
            // messageProcessor.CSourceFileName = $"{esp32SourceDirectory}Encoders.cpp";
            // messageProcessor.CSharpFileName = $"{csharpDestinationDirectory}{Path.DirectorySeparatorChar}Encoders.cs";;
            // messageProcessor.CSharpStructureFileName = $"{csharpDestinationDirectory}{Path.DirectorySeparatorChar}Messages.cs";;
            // messageProcessor.CHeaderTemplateFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}Encoders.hpp.txt";
            // messageProcessor.CSourceTemplateFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}Encoders.cpp.txt";
            // messageProcessor.CSharpSourceTemplateFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}Encoders.cs.txt";
            // messageProcessor.ProcessFile();
        }
    }
}
