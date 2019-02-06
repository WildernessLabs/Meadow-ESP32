using System;
using System.Collections.Generic;

namespace FileGenerator
{
    public class EnumDataType
    {
        public string Name;
        public string BaseDataType;
        public string[] EnumNames;
        public IList<EnumNameValuePairs> NameValuePairs;
    }

    public class EnumNameValuePairs
    {
        public string Name;
        public string Value;
    }
}