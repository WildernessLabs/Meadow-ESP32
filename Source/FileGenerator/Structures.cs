using System;
using System.Collections.Generic;

namespace FileGenerator
{
    public class MessageElement
    {
        public string Name;
        public string DateType;
    }

    public class Message
    {
        public string Name;
        public IList<MessageElement> MessageElements;
    }
}