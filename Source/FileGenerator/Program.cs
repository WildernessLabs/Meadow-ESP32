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
        /// Location of the ESP32 Coprocessor code for the STM32.
        /// </summary>
        const string ESP32_COPROCESSOR_SOURCE_DIRECTORY = "EspcpSourceDirectory";

        /// <summary>
        /// Location of the ESP32 coprocessor Meadow.Core source files.
        /// </summary>
        const string MEADOW_CORE_SOURCE_DIRECTORY = "MeadowCoreSourceDirectory";

        static void Main(string[] args)
        {
            Console.WriteLine("File Generator 1.02");
            //
            string stm32DestinationDirectory = Environment.GetEnvironmentVariable(ESP32_COPROCESSOR_SOURCE_DIRECTORY);
            if (string.IsNullOrEmpty(stm32DestinationDirectory))
            {
                Console.WriteLine($"ERROR: Please set the environment variable '{ESP32_COPROCESSOR_SOURCE_DIRECTORY}' before running this application.");
                Environment.Exit(-1);
            }
            string meadowCoreSourceDirectory = Environment.GetEnvironmentVariable(MEADOW_CORE_SOURCE_DIRECTORY);
            if (string.IsNullOrEmpty(meadowCoreSourceDirectory))
            {
                Console.WriteLine($"ERROR: Please set the environment variable '{MEADOW_CORE_SOURCE_DIRECTORY}' before running this application.");
                Environment.Exit(-1);
            }
            if(!Directory.Exists(meadowCoreSourceDirectory))
            {
                Console.WriteLine($"Creating output folder for Meadow.Core files at '{MEADOW_CORE_SOURCE_DIRECTORY}'...");
                Directory.CreateDirectory(meadowCoreSourceDirectory);
            }
            //
            string outputDirectory = $".{Path.DirectorySeparatorChar}Output{Path.DirectorySeparatorChar}";
            if (!Directory.Exists(outputDirectory))
            {
                Directory.CreateDirectory(outputDirectory);
            }

            // find the source folder for the JSON files.  Start at the parent of the current, and walk up.
            var esp32SourceRoot = AppDomain.CurrentDomain.BaseDirectory;
            var metaPath = Path.Combine(esp32SourceRoot, "Metadata");
            while(!Directory.Exists(metaPath))
            {
                esp32SourceRoot = Directory.GetParent(esp32SourceRoot).FullName;
                metaPath = Path.Combine(esp32SourceRoot, "Metadata");
            }
            Console.WriteLine($"Using JSON files from {metaPath}");

            //
            //  Generate the enum files.
            //
            var enumJsonFileName = Path.Combine(metaPath,"SharedEnums.json");

            Console.WriteLine("    Generating enums.");
            EnumProcessor enumProcessor = new EnumProcessor(enumJsonFileName, outputDirectory);
            enumProcessor.ProcessFile();
            //
            //  Generate the message processor (structure, encoders & extract) methods and structure defintions.
            //
            Console.WriteLine("    Generating message encoders and decoders.");
            var messageProcessor = new MessageProcessor(metaPath, outputDirectory);
            messageProcessor.ProcessFile();
            //
            //  Now copy the files into the right locations.
            //
            Console.WriteLine("    Copying files.");
            //
            //  Copy the C# files.
            //
            File.Copy($"{outputDirectory}Messages.cs", $"{meadowCoreSourceDirectory}/Messages.cs", true);
            File.Copy($"{outputDirectory}Encoders.cs", $"{meadowCoreSourceDirectory}/Encoders.cs", true);
            File.Copy($"{outputDirectory}SharedEnums.cs", $"{meadowCoreSourceDirectory}/SharedEnums.cs", true);
            //
            //  Copy the ESP32 files.
            //
            var meadowCommsFolder = Path.Combine(esp32SourceRoot, "MeadowComms/components/Messaging");
            File.Copy($"{outputDirectory}Encoders.cpp", Path.Combine(meadowCommsFolder, "Encoders.cpp"), true);
            File.Copy($"{outputDirectory}Encoders.hpp", Path.Combine(meadowCommsFolder, "Encoders.hpp"), true);
            File.Copy($"{outputDirectory}SharedEnums.hpp", Path.Combine(meadowCommsFolder, "SharedEnums.hpp"), true);
            //
            //  Copy the STM32 files.
            //
            File.Copy($"{outputDirectory}espcp_encoders.c", $"{stm32DestinationDirectory}/espcp_encoders.c", true);
            File.Copy($"{outputDirectory}espcp_encoders.h", $"{stm32DestinationDirectory}/espcp_encoders.h", true);
            File.Copy($"{outputDirectory}espcp_shared_enums.h", $"{stm32DestinationDirectory}/espcp_shared_enums.h", true);
        }
    }
}
