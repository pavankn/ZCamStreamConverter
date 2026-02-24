using System;
using System.Threading.Tasks;

namespace com.khelai.ZCamStreamUI
{
    public partial class Form1 : Form
    {
        private MdnsDiscoverer _discoverer;
        private Panel overlayPanel;
        private ProgressBar spinner;
        private readonly Dictionary<string, VideoSettings> _videoProfiles
            = new Dictionary<string, VideoSettings>();

        private int _nextCameraIndex = 0;
        private const int CAMERA_START_Y = 25;
        private const int CAMERA_SPACING = 8;
        private const int DISCOVER_TIMEOUT = 2000;
        private const int QUERY_INTERVAL = 1000;
        private const String MDNS_SERVICE_NAME = "_eagle._tcp.local";

        // Stores video settings per camera IP
        private readonly Dictionary<string, VideoSettings> _videoSettings
            = new Dictionary<string, VideoSettings>();

        // Currently selected / active camera IP
        private string _activeCameraIp;

        public Form1()
        {
            InitializeComponent();
        }


        private int CenterX(Control parent, Control child)
        {
            return (parent.ClientSize.Width - child.Width) / 2;
        }

        private void InitializeSpinnerOverlay()
        {
            overlayPanel = new Panel
            {
                Dock = DockStyle.Fill,
                BackColor = Color.FromArgb(120, Color.Gray), // semi-transparent
                Visible = false
            };

            spinner = new ProgressBar
            {
                Style = ProgressBarStyle.Marquee,
                MarqueeAnimationSpeed = 30,
                Size = new Size(200, 20)
            };

            overlayPanel.Controls.Add(spinner);
            this.Controls.Add(overlayPanel);
            overlayPanel.BringToFront();

            overlayPanel.Resize += (s, e) =>
            {
                spinner.Left = (overlayPanel.Width - spinner.Width) / 2;
                spinner.Top = (overlayPanel.Height - spinner.Height) / 2;
            };
        }

        private void ShowSpinner()
        {
            overlayPanel.Visible = true;
            overlayPanel.BringToFront();
        }

        private void HideSpinner()
        {
            overlayPanel.Visible = false;
        }
        private void SetUiEnabled(bool enabled)
        {
            foreach (Control c in this.Controls)
            {
                if (c != overlayPanel)
                    c.Enabled = enabled;
            }
        }

        private CheckBox CreateCameraCheckBox(int index, string ip)
        {
            return new CheckBox
            {
                Name = $"chkCamera_{index}",
                Text = $"ZCam @ {ip}",
                Tag = ip,
                AutoSize = true,
                ForeColor = Color.White,
                BackColor = Color.Transparent
            };
        }

        private void ReLayoutCameraCheckboxes()
        {
            int y = CAMERA_START_Y;

            foreach (var chk in grpBoxCameras.Controls.OfType<CheckBox>())
            {
                chk.Left = (grpBoxCameras.ClientSize.Width - chk.Width) / 2;
                chk.Top = y;
                y += chk.Height + CAMERA_SPACING;
            }
        }

        private VideoSettings GetOrCreateSettings(string ip)
        {
            if (!_videoSettings.ContainsKey(ip))
            {
                _videoSettings[ip] = CreateDefaultSettings();
            }
            return _videoSettings[ip];
        }

        private void Camera_CheckedChanged(object sender, EventArgs e)
        {
            var chk = (CheckBox)sender;
            string ip = chk.Tag.ToString();

            // Ensure settings exist when checked
            if (chk.Checked)
            {
                GetOrCreateSettings(ip);

                // If no active camera yet, make this one active
                if (_activeCameraIp == null)
                {
                    _activeCameraIp = ip;
                    LoadSettingsToUI(_videoSettings[ip]);
                    groupBoxVideo.Visible = true;
                }
            }
            else
            {
                // Unchecked camera
                if (_activeCameraIp == ip)
                {
                    _activeCameraIp = null;
                    groupBoxVideo.Visible = false;
                }
            }
        }      

        private void AddCamera(string ip)
        {
            // Prevent duplicates
            if (grpBoxCameras.Controls
                .OfType<CheckBox>()
                .Any(c => c.Tag?.ToString() == ip))
                return;

            var chk = CreateCameraCheckBox(_nextCameraIndex++, ip);
            chk.Tag = ip;
            chk.CheckedChanged += Camera_CheckedChanged;
            chk.Click += Camera_Clicked;

            grpBoxCameras.Controls.Add(chk);

            ReLayoutCameraCheckboxes();
        }

        private void Camera_Clicked(object? sender, EventArgs e)
        {
            var chk = (CheckBox)sender;
            string ip = chk.Tag.ToString();

            // Only allow editing if camera is checked
            if (!chk.Checked)
                return;

            _activeCameraIp = ip;

            LoadSettingsToUI(GetOrCreateSettings(ip));

            groupBoxVideo.Visible = true;
            groupBoxVideo.Text = $"Video Settings – {ip}";
        }

        private async Task ScanCamerasAsync()
        {
            grpBoxCameras.Controls.Clear();
            groupBoxVideo.Visible = false;
            _nextCameraIndex = 0;

            ShowSpinner();
            SetUiEnabled(false);

            _discoverer?.Dispose();
            _discoverer = new MdnsDiscoverer(MDNS_SERVICE_NAME, QUERY_INTERVAL);

            _discoverer.DeviceDiscovered += ip =>
            {
                BeginInvoke(new Action(() =>
                {
                    Console.WriteLine($"Device discovered: {ip}");
                    AddCamera(ip);
                }));
            };


            _discoverer.Start();

            // Let discovery run for some time (example: 6 seconds)
            await Task.Delay(DISCOVER_TIMEOUT);

            _discoverer.Stop();

            HideSpinner();
            SetUiEnabled(true);
        }

        private async void OnClick_ScanCamera(object sender, EventArgs e)
        {
            _discoverer?.Dispose();

            InitializeSpinnerOverlay();
            await ScanCamerasAsync();
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            comboBoxResolution.DropDownStyle = ComboBoxStyle.DropDownList;

            comboBoxResolution.Items.AddRange(new object[]
            {
                new VideoResolution { Width = 640,  Height = 480 },
                new VideoResolution { Width = 1280, Height = 720 },
                new VideoResolution { Width = 1920, Height = 1080 }
            });
        }

        private VideoSettings CreateDefaultSettings()
        {
            return new VideoSettings
            {
                Stream = "Stream0",
                Resolution = new VideoResolution { Width = 1920, Height = 1080 },
                HwDecoding = true,
                Codec = "H264"
            };
        }

        private void LoadSettingsToUI(VideoSettings s)
        {
            // Stream
            comboBoxStream.SelectedItem =
                comboBoxStream.Items.Cast<object>()
                    .FirstOrDefault(x => x.ToString() == s.Stream);

            // Codec
            comboBoxCodec.SelectedItem =
                comboBoxCodec.Items.Cast<object>()
                    .FirstOrDefault(x => x.ToString() == s.Codec);

            // HW Accel
            comboBoxHWAccel.SelectedItem = s.HwDecoding ? "True" : "False";

            // Resolution (SAFE)
            if (s.Resolution != null)
            {
                comboBoxResolution.SelectedItem =
                    comboBoxResolution.Items
                        .OfType<VideoResolution>()
                        .FirstOrDefault(r =>
                            r.Width == s.Resolution.Width &&
                            r.Height == s.Resolution.Height);
            }
            else
            {
                // Fallback to default 1920x1080
                comboBoxResolution.SelectedIndex = 2;
            }
        }
        private void SaveUIToSettings(object sender, EventArgs e)
        {
            if (_activeCameraIp == null)
                return;

            var s = _videoSettings[_activeCameraIp];

            s.Stream = comboBoxStream.SelectedItem?.ToString();
            s.Codec = comboBoxCodec.SelectedItem?.ToString();
            s.HwDecoding = comboBoxHWAccel.SelectedItem?.ToString() == "True";

            s.Resolution = comboBoxResolution.SelectedItem as VideoResolution;
        }

        private List<string> GetSelectedCameraIps()
        {
            return grpBoxCameras.Controls
                .OfType<CheckBox>()
                .Where(chk => chk.Checked && chk.Tag != null)
                .Select(chk => chk.Tag.ToString())
                .ToList();
        }

        private StreamConfigFile BuildStreamConfig()
        {
            var cfg = new StreamConfigFile();

            foreach (var ip in GetSelectedCameraIps())
            {
                var s = _videoSettings[ip];

                cfg.Streams.Add(new StreamConfig
                {
                    Ip = ip,
                    Stream = s.Stream,
                    Resolution = s.Resolution,
                    Codec = s.Codec,
                    HwDecoding = s.HwDecoding
                });
            }

            return cfg;
        }

        private string WriteStreamsJson()
        {
            var config = BuildStreamConfig();

            string path = Path.Combine(
                AppDomain.CurrentDomain.BaseDirectory,
                "streams.json");

            var json = System.Text.Json.JsonSerializer.Serialize(
                config,
                new System.Text.Json.JsonSerializerOptions
                {
                    WriteIndented = true
                });

            File.WriteAllText(path, json);

            return path;
        }

        private void OnClick_StartCamera(object sender, EventArgs e)
        {
            var selectedIps = GetSelectedCameraIps();

            if (selectedIps.Count == 0)
            {
                MessageBox.Show("Please select at least one camera.");
                return;
            }

            string jsonPath = WriteStreamsJson();

            int ret = ZCamNativeProcessor.ZCamNative_ProcessStream(jsonPath);

            if (ret != 0)
            {
                MessageBox.Show($"ProcessStream failed: {ret}");
            }
        }
    }
}
