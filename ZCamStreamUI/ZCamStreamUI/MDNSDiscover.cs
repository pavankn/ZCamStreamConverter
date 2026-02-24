using System;
using System.Collections.Concurrent;
using System.Linq;
using System.Timers;
using Makaretu.Dns;
using Timer = System.Timers.Timer;

namespace com.khelai.ZCamStreamUI
{
    public class MdnsDiscoverer : IDisposable
    {
        public event Action<string> DeviceDiscovered;
        public event Action<string> DeviceLost;

        private MulticastService _mdns;
        private System.Timers.Timer _queryTimer;
        private Timer _printTimer;

        private ConcurrentDictionary<string, bool> _deviceList;
        private ConcurrentQueue<string> _printQueue;

        private string _service;
        private bool _linkLocalOnly;
        private bool _displayHostname;
        private bool _displayIPv4;
        private bool _displayIPv6;
        private bool _displayPort;

        private int _queryInterval;
        private int _recheckInterval;
        private int _timeSinceRecheck;

        public MdnsDiscoverer(
            string service = "_eagle._tcp.local",
            int queryInterval = 1000,
            bool linkLocalOnly = false,
            bool displayHostname = true,
            bool displayIPv4 = true,
            bool displayIPv6 = false,
            bool displayPort = false)
        {
            _service = service;
            _queryInterval = queryInterval;
            _recheckInterval = Math.Max(2000, queryInterval * 2);

            _linkLocalOnly = linkLocalOnly;
            _displayHostname = displayHostname;
            _displayIPv4 = displayIPv4;
            _displayIPv6 = displayIPv6;
            _displayPort = displayPort;

            _deviceList = new ConcurrentDictionary<string, bool>();
            _printQueue = new ConcurrentQueue<string>();
        }

        // ---------------- START / STOP ----------------

        public void Start()
        {
            _mdns = new MulticastService();
            _mdns.AnswerReceived += Mdns_AnswerReceived;
            _mdns.Start();
            _mdns.SendQuery(_service);

            _queryTimer = new Timer(_queryInterval);
            _queryTimer.Elapsed += FindDeviceTimer_Elapsed;
            _queryTimer.Start();

            _printTimer = new Timer(200);
            _printTimer.Elapsed += PrintTimer_Elapsed;
            _printTimer.Start();
        }

        public void Stop()
        {
            _queryTimer?.Stop();
            _printTimer?.Stop();
            _mdns?.Stop();

            _queryTimer?.Dispose();
            _printTimer?.Dispose();
            _mdns?.Dispose();
        }

        // ---------------- mDNS HANDLERS ----------------

        private void Mdns_AnswerReceived(object sender, MessageEventArgs e)
        {
            var srvRecords = e.Message.AdditionalRecords
                .Union(e.Message.Answers)
                .OfType<SRVRecord>();

            var ipv4s = e.Message.AdditionalRecords
                .Union(e.Message.Answers)
                .OfType<ARecord>();

            if (!srvRecords.Any(r => r.CanonicalName.EndsWith(_service)))
                return;

            if (_displayIPv4 && !ipv4s.Any())
                return;

            foreach (var ip in ipv4s)
            {
                string output = ip.Address.ToString();

                if (_deviceList.TryAdd(output, true))
                {
                    DeviceDiscovered?.Invoke(output);
                }
                else
                {
                    _deviceList.TryUpdate(output, true, false);
                }
            }
        }

        private void FindDeviceTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            _timeSinceRecheck += _queryInterval;

            if (_timeSinceRecheck >= _recheckInterval)
            {
                _timeSinceRecheck = 0;

                foreach (var kv in _deviceList.ToArray())
                {
                    if (!_deviceList.TryUpdate(kv.Key, false, true))
                    {
                        if (_deviceList.TryRemove(kv.Key, out _))
                        {
                            DeviceLost?.Invoke(kv.Key);
                        }
                    }
                }
            }

            _mdns.SendQuery(_service);
        }

        private void PrintTimer_Elapsed(object sender, ElapsedEventArgs e)
        {
            while (_printQueue.TryDequeue(out var msg))
            {
                // optional logging
            }
        }

        public void Dispose()
        {
            Stop();
        }
    }
}