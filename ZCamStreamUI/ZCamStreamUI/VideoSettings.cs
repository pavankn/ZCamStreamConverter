using System;
using System.Collections.Generic;
using System.Linq;
using System.Security.Policy;
using System.Text;
using System.Threading.Tasks;

namespace com.khelai.ZCamStreamUI
{ 
    public class VideoResolution
    {
        public int Width { get; set; }
        public int Height { get; set; }
        public override string ToString()
        {
            return $"{Width}x{Height}";
        }
    }
    public class VideoSettings
    {
        public string Stream { get; set; }          // Stream0 / Stream1
        public VideoResolution Resolution { get; set; }      // 1920x1080
        public bool HwDecoding { get; set; }         // true/false
        public string Codec { get; set; }            // H264 / H265
        public int Bitrate { get; set; }              // 4000 (kbps)
        public int Fps { get; set; }             // 30 (fps)
        public int VFR { get; set; }                   // 0 (off) / 1 (on)
    }
}
