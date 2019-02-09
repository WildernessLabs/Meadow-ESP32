using System;
using System.IO;
using System.Collections.Generic;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace FileGenerator
{
    public class EnumDataType
    {
        public string Name;
        public string BaseDataType;
        public string Namespace;
        public string Comment;
        public string[] EnumNames;
        public IList<EnumNameValuePair> NameValuePairs;
    }

    public class EnumNameValuePair
    {
        public string Name;
        public string Value;
    }

    
}