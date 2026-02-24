using com.khelai.ZCamStreamUI;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace com.khelai.ZCamStreamUI
{
    public class StreamConfigFile
    {
        public int Version { get; set; } = 1;
        public List<StreamConfig> Streams { get; set; } = new();
    }

    public class StreamConfig
    {
        public string Ip { get; set; }
        public string Stream { get; set; }
        public VideoResolution Resolution { get; set; }
        public string Codec { get; set; }
        public bool HwDecoding { get; set; }
    }
}


