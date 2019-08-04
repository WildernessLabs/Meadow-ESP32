using System;
using NUnit.Framework;
using Meadow.Core.Esp32;

namespace BasicNetworkTest.Tests
{
    [TestFixture]
    public class SlipEncoderTests
    {
        [Test]
        public void EncoderTests()
        {
            //
            //    Simple data should just get the 0xC0 added to the start and end of the packet.
            //
            byte[] source = {1, 2, 3};
            byte[] expectedResult = {0xC0, 1, 2, 3, 0xC0};
            Assert.AreEqual(expectedResult, SlipEncoder.Encode(source));

            //
            //    Next check that the 0xDB and 0xC0 bytes are escaped correctly.
            //
            byte[] source2 = {1, 2, 0xDB, 0xC0};
            byte[] expectedResult2 = {0xC0, 1, 2, 0xDB, 0xDD, 0xDB, 0xDC, 0xC0};
            Assert.AreEqual(expectedResult2, SlipEncoder.Encode(source2));
        }

        [Test]
        public void DecoderTests()
        {
            //
            //    Simple data should just get the 0xC0 removed from the start and end of the packet.
            //
            byte[] source = {0xC0, 1, 2, 3, 0xC0};
            byte[] expectedResult = {1, 2, 3};
            Assert.AreEqual(expectedResult, SlipEncoder.Decode(source));

            //
            //    Next check that the 0xDB escape sequences are interpreted correctly.
            //
            byte[] source2 = {0xC0, 1, 2, 0xDB, 0xDD, 0xDB, 0xDC, 0xC0};
            byte[] expectedResult2 = {1, 2, 0xDB, 0xC0};
            Assert.AreEqual(expectedResult2, SlipEncoder.Decode(source2));
        }
    }
}
