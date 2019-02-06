using System;
using System.IO;
using System.Collections.Generic;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

namespace FileGenerator
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("File Generator");

            using (StreamReader enumFile = new StreamReader("enums.json"))
            {
                string fileContents = enumFile.ReadToEnd();
                JObject o = JObject.Parse(fileContents);

                JArray a = (JArray)o["Enums"];

                IList<EnumDataType> enums = a.ToObject<IList<EnumDataType>>();
                foreach (EnumDataType e in enums)
                {
                    Console.WriteLine($"Enum name: {e.Name}, data type: {e.BaseDataType}");
                    if (e.EnumNames != null)
                    {
                        Console.Write("    Enum value names: ");
                        foreach (string en in e.EnumNames)
                        {
                            Console.Write($"{en} ");
                        }
                        Console.WriteLine();
                    }
                    if (e.NameValuePairs != null)
                    {
                        Console.Write("    Name value pairs: ");
                        foreach (EnumNameValuePairs nvp in e.NameValuePairs)
                        {
                            Console.Write($"{nvp.Name} = {nvp.Value} ");
                        }
                        Console.WriteLine();
                    }
                }
            }
            using (StreamReader structuresFile = new StreamReader("Messages.json"))
            {
                string fileContents = structuresFile.ReadToEnd();
                JObject o = JObject.Parse(fileContents);

                JArray a = (JArray)o["Messages"];

                IList<Message> messages = a.ToObject<IList<Message>>();
                foreach (Message m in messages)
                {
                    Console.WriteLine($"Message name: {m.Name}");
                    if (m.MessageElements != null)
                    {
                        Console.WriteLine("    Message elements: ");
                        foreach (MessageElement me in m.MessageElements)
                        {
                            Console.WriteLine($"        {me.Name} : {me.DateType}");
                        }
                    }
                }
            }
        }
    }
}
