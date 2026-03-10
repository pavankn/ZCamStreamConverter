using System;
using System.Collections.Concurrent;
using System.Diagnostics;
using System.Security.Authentication.ExtendedProtection;
using System.Text.Json;
using System.Threading.Tasks;
using ZCamStreamUI;

namespace com.khelai.ZCamStreamUI
{
    public partial class Form1 : Form
    {
        private MdnsDiscoverer? _discoverer;
        private Panel overlayPanel = null!;
        private ProgressBar spinner = null!;

        private int _nextCameraIndex = 0;
        private const int CAMERA_START_Y = 25;
        private const int CAMERA_SPACING = 8;
        private const int DISCOVER_TIMEOUT = 2000;
        private const int QUERY_INTERVAL = 1000;
        private const String MDNS_SERVICE_NAME = "_eagle._tcp.local";
        private bool _isLoadingSettings;
        private bool _isAsyncClosing = false;

        private String _zcamWorkerPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ZCamWorker.exe");

        private ZCamNativeProcess _zcamProcess = new ZCamNativeProcess(
            Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ZCamWorker.exe"));

        private readonly List<ZCamNativeProcess> _activeProcesses = new List<ZCamNativeProcess>();
        private readonly object _listLock = new object();


        // Stores video settings per camera IP
        private readonly Dictionary<string, VideoSettings> _videoSettings
            = new Dictionary<string, VideoSettings>();

        // Currently selected / active camera IP
        private string? _activeCameraIp;

        public Form1()
        {
            InitializeComponent();
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

        private static CheckBox CreateCameraCheckBox(int index, string ip)
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
                    //groupBoxVideo.Visible = true;
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

            //LoadSettingsToUI(GetOrCreateSettings(ip));

            //groupBoxVideo.Visible = true;
            //groupBoxVideo.Text = $"Video Settings – {ip}";
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
                Stream = "stream0",
                Resolution = new VideoResolution { Width = 1920, Height = 1080 },
                HwDecoding = true,
                Codec = "HEVC",
                Bitrate = 240,
                Fps = 30,
                VFR = 120
            };
        }

        private void LoadSettingsToUI(VideoSettings s)
        {
            _isLoadingSettings = true;

            try
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

                // Resolution
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
                    comboBoxResolution.SelectedIndex = 2;
                }
            }
            finally
            {
                _isLoadingSettings = false;
            }
        }

        private void SaveUIToSettings(object sender, EventArgs e)
        {
            if (_isLoadingSettings)
                return;

            if (_activeCameraIp == null)
                return;

            var s = _videoSettings[_activeCameraIp];

            if (comboBoxStream.SelectedItem != null)
                s.Stream = comboBoxStream.SelectedItem?.ToString() ?? s.Stream;

            if (comboBoxCodec.SelectedItem != null)
                s.Codec = comboBoxCodec.SelectedItem?.ToString() ?? s.Codec;

            if (comboBoxHWAccel.SelectedItem != null)
                s.HwDecoding = comboBoxHWAccel.SelectedItem.ToString() == "True";

            if (comboBoxResolution.SelectedItem is VideoResolution res)
                s.Resolution = res;
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
                    HwDecoding = s.HwDecoding,
                    Bitrate = s.Bitrate,
                    Fps = s.Fps,
                    VFR = s.VFR
                });
            }

            return cfg;
        }


        public async Task ProcessConfigAndApply(string jsonContent)
        {
            // 1. Parse JSON into C# Objects
            var options = new JsonSerializerOptions { PropertyNameCaseInsensitive = true };
            var config = JsonSerializer.Deserialize<ZCamConfig>(jsonContent, options);
            if (config?.Streams == null) return;

            // 2. Iterate and Apply to each camera
            foreach (var cam in config.Streams)
            {
                ZCamController _zcamController = new ZCamController(cam.Ip);

                bool success = await _zcamController.ApplySettingsAsync(cam);

                Logger.Info($"Camera {cam.Ip} Setup: {(success ? "SUCCESS" : "FAILED")}");
            }
        }


        private string WriteStreamsJson()
        {
            var config = BuildStreamConfig();

            string appDataFolder = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "KhelAI", "ZCamStreamConverter");

            // Ensure the folder exists before trying to use the file
            if (!Directory.Exists(appDataFolder))
            {
                Directory.CreateDirectory(appDataFolder);
            }

            string path = Path.Combine(appDataFolder, "streams.json");
            

            var json = System.Text.Json.JsonSerializer.Serialize(
                config,
                new System.Text.Json.JsonSerializerOptions
                {
                    WriteIndented = true
                });

            File.WriteAllText(path, json);

            return path;
        }

        private async void OnClick_StartCamera(object sender, EventArgs e)
        {
            var selectedIps = GetSelectedCameraIps();

            if (selectedIps.Count == 0)
            {
                MessageBox.Show("Please select at least one camera.");
                return;
            }

            if (!File.Exists(_zcamWorkerPath))
            {
                MessageBox.Show($"Worker not found: {_zcamWorkerPath}");
                throw new FileNotFoundException("ZCamWorker.exe not found", _zcamWorkerPath);
            }
                     

            string jsonPath = WriteStreamsJson();

            if (!File.Exists(jsonPath))
            {
                throw new FileNotFoundException("jsonPath not found", jsonPath);
            }

            if (!File.Exists(jsonPath))
                return;

            string jsonContent = File.ReadAllText(jsonPath);

            await ProcessConfigAndApply(jsonContent);

            try
            {
                foreach (var ip in selectedIps)
                {
                    // Extract last 3 digits of IP (e.g., 192.168.1.188 -> 188)
                    string lastThree = ip.Split('.').Last();
                    string ndiName = $"Khel_NDI_{lastThree}";

                    // 1. Create a NEW instance for this specific camera
                    var workerPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "ZCamWorker.exe");
                    var processHandler = new ZCamNativeProcess(workerPath);

                    // 2. Track it in our list
                    lock (_listLock)
                    {
                        _activeProcesses.Add(processHandler);
                    }

                    // 3. Run it (assuming your RunAsync takes IP and NDI name)
                    // We don't 'await' here so all cameras start simultaneously
                    _ = _zcamProcess.RunAsync(ip, ndiName, CancellationToken.None);

                    Logger.Info($"Started worker for {ip} with NDI: {ndiName}");                  
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to start workers: " + ex.Message);
                await KillZcamProcess();
            }
        }

        public static void ForceKillAllZCamWorkers()
        {
            // Find all running processes with this name (no .exe extension needed)
            var runningWorkers = System.Diagnostics.Process.GetProcessesByName("ZCamWorker");

            foreach (var worker in runningWorkers)
            {
                try
                {
                    // Forceful termination (equivalent to Task Manager 'End Task')
                    worker.Kill();

                    // Wait up to 1 second for the OS to reclaim the memory
                    worker.WaitForExit(1000);

                    Logger.Info($"Successfully killed process ID: {worker.Id}");
                }
                catch (Exception ex)
                {
                    // This happens if the process is already closing or access is denied
                    Logger.Info($"Could not kill process {worker.Id}: {ex.Message}");
                }
                finally
                {
                    worker.Dispose();
                }
            }
        }

        private async Task KillZcamProcess()
        {
            List<ZCamNativeProcess> processesToStop;

            lock (_listLock)
            {
                processesToStop = _activeProcesses.ToList();
                _activeProcesses.Clear(); // Clear the list so UI doesn't try to stop them twice
            }

            foreach (var proc in processesToStop)
            {
                try
                {
                    // This calls your internal Stop() which likely signals the native exe
                    proc.Stop();
                    await Task.Delay(1000);
                }
                catch (Exception ex)
                {
                    Logger.Error($"Error killing process: {ex.Message}");
                }
                finally
                {
                    proc.Dispose();
                }
            }           
        }

        private async void OnClick_Stop(object sender, EventArgs e)
        {
            btnStop.Enabled = false;
            await KillZcamProcess();
            btnStop.Enabled = true;
        }

        private async void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            // If we've already done the cleanup, let the form close.
            if (_isAsyncClosing) return;
            if (_zcamProcess == null) return; // No process, no need to confirm or cleanup

            var result = MessageBox.Show(
                "A camera stream is active. Stop stream and exit?",
                "Confirm Exit",
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Question);

            if (result == DialogResult.No)
            {
                e.Cancel = true;
                return; // Exit here, form stays open
            }

            // 2. If we reach here, either process was null OR user clicked Yes
            e.Cancel = true; // Stop the immediate close to allow async work

            try
            {
                this.Enabled = false; // Prevent user interaction during cleanup
                await KillZcamProcess(); // Your async cleanup
            }
            catch (Exception ex)
            {
                // Log error but proceed with closing so the app doesn't get stuck
                Logger.Error("Cleanup failed: " + ex.Message);
            }
            finally
            {
                _isAsyncClosing = true;
                this.Close(); // Trigger the second pass of FormClosing
                ForceKillAllZCamWorkers();
            }
        }
    }
}
