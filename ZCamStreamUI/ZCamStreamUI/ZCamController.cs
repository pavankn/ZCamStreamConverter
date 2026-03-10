using com.khelai.ZCamStreamUI;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace ZCamStreamUI
{
    // To Set Params using HTTP
    public class ZCamController
    {
        private static readonly HttpClient _httpClient = new HttpClient { Timeout = TimeSpan.FromSeconds(5) };
        private readonly string _ip;

        public ZCamController(string ip)
        {
            _ip = ip;
        }

        public async Task<bool> SetResolutionAsync(string resolution)
        {
            try
            {
                // C# handles the URL string building safely
                string url = $"http://{_ip}/ctrl/set?resolution={resolution}";

                // Equivalent to curl_easy_perform
                string response = await _httpClient.GetStringAsync(url);

                // Equivalent to checking response.find("\"code\":0")
                return response.Contains("\"code\":0");
            }
            catch (Exception ex)
            {
                // Handles timeouts (CURLE_OPERATION_TIMEDOUT) or connection issues
                Logger.Error($"Request failed: {ex.Message}");
                return false;
            }
        }
        public async Task<bool> SetVideoEncoderAsync(string videoencoder)
        {
            try
            {
                // C# handles the URL string building safely
                string url = $"http://{_ip}/ctrl/set?video_encoder={videoencoder}";

                // Equivalent to curl_easy_perform
                string response = await _httpClient.GetStringAsync(url);

                // Equivalent to checking response.find("\"code\":0")
                return response.Contains("\"code\":0");
            }
            catch (Exception ex)
            {
                // Handles timeouts (CURLE_OPERATION_TIMEDOUT) or connection issues
                Logger.Error($"Request failed: {ex.Message}");
                return false;
            }
        }
        public async Task<bool> SetStreamAsync(string streamindex)
        {
            try
            {
                // C# handles the URL string building safely
                string url = $"http://{_ip}/ctrl/set?send_stream={streamindex}";

                // Equivalent to curl_easy_perform
                string response = await _httpClient.GetStringAsync(url);

                // Equivalent to checking response.find("\"code\":0")
                return response.Contains("\"code\":0");
            }
            catch (Exception ex)
            {
                // Handles timeouts (CURLE_OPERATION_TIMEDOUT) or connection issues
                Logger.Error($"Request failed: {ex.Message}");
                return false;
            }
        }
        public async Task<bool> SetVFRAsync(string vfr)
        {
            try
            {
                // C# handles the URL string building safely
                string url = $"http://{_ip}/ctrl/set?movvfr={vfr}";

                // Equivalent to curl_easy_perform
                string response = await _httpClient.GetStringAsync(url);

                // Equivalent to checking response.find("\"code\":0")
                return response.Contains("\"code\":0");
            }
            catch (Exception ex)
            {
                // Handles timeouts (CURLE_OPERATION_TIMEDOUT) or connection issues
                Logger.Error($"Request failed: {ex.Message}");
                return false;
            }
        }
        public async Task<bool> ApplySettingsAsync(StreamEntry s)
        {
            try
            {
                await SetResolutionAsync(s.Resolution.ToString());
                await SetVFRAsync(s.VFR.ToString());
                await SetStreamAsync(s.Stream.ToString());

                int bitrate = s.Bitrate * 1024 * 1024;
                // Map JSON properties to ZCam API parameter names
                var queryParams = new Dictionary<string, string>
                {
                    { "index", s.Stream },
                    { "width", s.Resolution.Width.ToString() },
                    { "height", s.Resolution.Height.ToString() },
                    { "venc", s.Codec.ToLower() == "hevc" ? "h265" : "h264" }, // ZCam usually expects h265 for HEVC
                    { "bitrate", bitrate.ToString() },
                    { "fps", s.Fps.ToString() }
                };

                var content = new FormUrlEncodedContent(queryParams);
                string query = await content.ReadAsStringAsync();

                string url = $"http://{s.Ip}/ctrl/stream_setting?{query}";

                var response = await _httpClient.GetStringAsync(url);
                return response.Contains("\"code\":0");
            }
            catch (Exception ex)
            {
                Logger.Error($"Failed to connect to {s.Ip}: {ex.Message}");
                return false;
            }
        }
    }
}
