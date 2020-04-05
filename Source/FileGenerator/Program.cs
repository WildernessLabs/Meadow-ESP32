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

        /// <summary>
        /// Location of the ESP32 Coprocessor code for the STM32.
        /// </summary>
        const string ESP32_COPROCESSOR_SOURCE_DIRECTORY = "EspcpSourceDirectory";

        static void Main(string[] args)
        {
            Console.WriteLine("File Generator 1.01");
            string csharpDestinationDirectory = Environment.GetEnvironmentVariable(MEADOW_F7MICRO_SOURCE_DIRECTORY);
            if (string.IsNullOrEmpty(csharpDestinationDirectory))
            {
                Console.WriteLine($"ERROR: Please set the environment variable '{MEADOW_F7MICRO_SOURCE_DIRECTORY}' before running this application.");
                Environment.Exit(-1);
            }
            //
            string stm32DestinationDirectory = Environment.GetEnvironmentVariable(ESP32_COPROCESSOR_SOURCE_DIRECTORY);
            if (string.IsNullOrEmpty(stm32DestinationDirectory))
            {
                Console.WriteLine($"ERROR: Please set the environment variable '{ESP32_COPROCESSOR_SOURCE_DIRECTORY}' before running this application.");
                Environment.Exit(-1);
            }
            //
            string outputDirectory = $".{Path.DirectorySeparatorChar}Output{Path.DirectorySeparatorChar}";
            //
            //  Generate the enum files.
            //
            string enumJsonFileName = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}SharedEnums.json";
            Console.WriteLine("    Generating enums.");
            EnumProcessor enumProcessor = new EnumProcessor(enumJsonFileName, outputDirectory);
            enumProcessor.ProcessFile();
            //
            //  Generate the message processor (structure, encoders & extract) methods and structure defintions.
            //
            Console.WriteLine("    Generating message encoders and decoders.");
            string metadataDirectory = $"..{Path.DirectorySeparatorChar}Metadata{Path.DirectorySeparatorChar}";
            MessageProcessor messageProcessor = new MessageProcessor(metadataDirectory, outputDirectory);
            messageProcessor.ProcessFile();
            //
            //  Now copy the files into the right locations.
            //
            Console.WriteLine("    Copying files.");
            //
            //  Copy the C# files.
            //
            // string sourceFile = $"{outputDirectory}Messages.cs";
            // string destinationFile = $"{csharpDestinationDirectory}/Messages.cs";
            // Console.WriteLine($"Source: {sourceFile}");
            // Console.WriteLine($"Source: {destinationFile}");
            // Environment.Exit(-1);
            File.Copy($"{outputDirectory}Messages.cs", $"{csharpDestinationDirectory}/Messages.cs", true);
            File.Copy($"{outputDirectory}Encoders.cs", $"{csharpDestinationDirectory}/Encoders.cs", true);
            File.Copy($"{outputDirectory}SharedEnums.cs", $"{csharpDestinationDirectory}/SharedEnums.cs", true);
            //
            //  Copy the ESP32 files.
            //
            File.Copy($"{outputDirectory}Encoders.cpp", "../MeadowComms/Main/Encoders.cpp", true);
            File.Copy($"{outputDirectory}Encoders.hpp", "../MeadowComms/Main/Encoders.hpp", true);
            File.Copy($"{outputDirectory}SharedEnums.hpp", "../MeadowComms/Main/SharedEnums.hpp", true);
            //
            //  Copy the STM32 files.
            //
            File.Copy($"{outputDirectory}espcp_encoders.c", $"{stm32DestinationDirectory}/espcp_encoders.c", true);
            File.Copy($"{outputDirectory}espcp_encoders.h", $"{stm32DestinationDirectory}/espcp_encoders.h", true);
            File.Copy($"{outputDirectory}espcp_shared_enums.h", $"{stm32DestinationDirectory}/espcp_shared_enums.h", true);
        }
    }
}
