using System;

namespace FileGenerator
{
    public class DataType
    {
        public string Name;
        public int Size;
        public string CTypeName;
        public string CSharpTypeName;

        public DataType(string name, int size, string cTypeName, string cSharpTypeName)
        {
            Name = name;
            Size = size;
            CTypeName = cTypeName;
            CSharpTypeName = cSharpTypeName;
        }
    }
}