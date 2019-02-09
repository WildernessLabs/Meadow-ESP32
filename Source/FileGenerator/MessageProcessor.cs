using System;
using System.IO;
using System.Collections.Generic;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace FileGenerator
{
    public class MessageProcessor
    {
        private string InputFileName { get; set; }

        private string CEnumsFileName { get; set; }

        private string CSharpFileName { get; set; }

        public MessageProcessor(string fileName, string cFileName, string csharpFileName)
        {
            InputFileName = fileName;
            CEnumsFileName = cFileName;
            CSharpFileName = csharpFileName;
        }
    }
}