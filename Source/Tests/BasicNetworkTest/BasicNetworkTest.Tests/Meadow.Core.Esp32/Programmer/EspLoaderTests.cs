using System;
using NUnit.Framework;
using Meadow.Core.Esp32;

namespace BasicNetworkTest.Tests
{
    [TestFixture]
    public class EspLoaderTests
    {
        [Test]
        public void CheckSumTest()
        {
            byte[] source = { 0x12, 0x34, 0x56 };
            EspLoader loader = new EspLoader("COM1", 115200);
            Assert.AreEqual(0x9f, loader.Checksum(source));
        }
    }
}
