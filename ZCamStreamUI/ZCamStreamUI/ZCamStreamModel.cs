using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ZCamStreamUI
{
    public class StreamEntry
    {
        public string Ip { get; set; }
        public string Stream { get; set; }
        public Resolution Resolution { get; set; }
        public string Codec { get; set; }
        public bool HwDecoding { get; set; }
        public int Bitrate { get; set; }
        public int Fps { get; set; }
        public int VFR { get; set; }
    }

    public class Resolution
    {
        public int Width { get; set; }
        public int Height { get; set; }

        // This allows you to just call resolution.ToString()
        public override string ToString()
        {
            return $"{Width}x{Height}";
        }
    }

    public class ZCamConfig
    {
        public int Version { get; set; }
        public List<StreamEntry> Streams { get; set; }
    }
}
