using System;
using System.Collections.Generic;

namespace FileGenerator
{
    public class MessageElement
    {
        public string Name;
        public string DataType;

        public UInt32 Length = 0;
    }

    public class Message
    {
        public string Name;
        public IList<MessageElement> MessageElements;
    }
}
