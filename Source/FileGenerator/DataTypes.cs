using System;

namespace FileGenerator
{
    public class DataType
    {
        public string Name;
        public int Size;

        public DataType(string name, int size)
        {
            Name = name;
            Size = size;
        }
    }
}