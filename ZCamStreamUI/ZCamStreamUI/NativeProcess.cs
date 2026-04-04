using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace com.khelai.ZCamStreamUI
{
    public class ZCamNativeResult
    {
        public int ExitCode { get; set; }
        public string Output { get; set; }
        public bool ToolNotFound { get; set; }

        public bool Success
        {
            get { return ExitCode == 0 && !ToolNotFound; }
        }

        public ZCamNativeResult()
        {
            Output = string.Empty;
        }
    }

    public class ZCamNativeProcess : IDisposable
    {
        public event Action<string> OutputReceived;

        private readonly string _zcamNativePath;
        private readonly string _zcamWorkerPath;
        private bool _disposed;
        private Process _runningProcess;
        private CancellationTokenSource _linkedCts;
        private readonly object _processLock = new object();

        public ZCamNativeProcess(string zCamNativePath)
        {
            _zcamNativePath = zCamNativePath
                ?? @"C:\Program Files\KhelAI\ZCamStreamConverter\ZCamNativeMain.exe";

            if (!File.Exists(_zcamNativePath))
            {
                Logger.Error("ZCamNative executable not found at: " + _zcamNativePath);
                throw new FileNotFoundException("ZCamNative.exe not found", _zcamNativePath);
            }

            Logger.Info("═══════════════════════════════════════════════");
            Logger.Info("ZCamNativeProcess initialized at " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            Logger.Info("ZCamNative path: " + _zcamNativePath);
            Logger.Info("═══════════════════════════════════════════════");
        }

        private async Task<ZCamNativeResult> RunInternalAsync(string ip, string ndiName, bool isAsync, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();


            var psi = new ProcessStartInfo
            {
                FileName = _zcamNativePath,
                ArgumentList = { ip, ndiName },
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
                StandardOutputEncoding = Encoding.UTF8,
                StandardErrorEncoding = Encoding.UTF8
            };

            var outputBuilder = new StringBuilder(4096);

            try
            {
                // 1. Create the process instance
                var process = new Process { StartInfo = psi };

                // 2. ASSIGN IT TO THE FIELD so Stop() can find it
                lock (_processLock)
                {
                    _runningProcess = process;
                }

                process.OutputDataReceived += (sender, e) =>
                {
                    if (e.Data != null)
                    {
                        outputBuilder.AppendLine(e.Data);
                        if (OutputReceived != null) OutputReceived(e.Data);
                    }
                };

                process.ErrorDataReceived += (sender, e) =>
                {
                    if (e.Data != null)
                    {
                        outputBuilder.AppendLine(e.Data);
                        if (OutputReceived != null) OutputReceived(e.Data);
                        Logger.Warning(e.Data);
                    }
                };

                Logger.Debug("Starting: ZCamNative with args: " + ip + " " + ndiName);
                process.Start();

                process.BeginOutputReadLine();
                process.BeginErrorReadLine();

                _linkedCts = CancellationTokenSource.CreateLinkedTokenSource(ct);

                _linkedCts.Token.Register(() =>
                {
                    TryKillProcess("Cancellation requested");
                });

                await process.WaitForExitAsync(_linkedCts.Token);

                Logger.Debug("ZCamNative finished exit code: " + process.ExitCode);

                return new ZCamNativeResult
                {
                    ExitCode = process.ExitCode,
                    Output = outputBuilder.ToString().TrimEnd(),
                    ToolNotFound = false
                };

            }
            catch (OperationCanceledException)
            {
                Logger.Warning("Operation cancelled: ZCamWorker.exe");
                throw;
            }
            catch (Exception ex)
            {
                bool toolNotFound = ex is Win32Exception &&
                    (ex.Message.IndexOf("not found", StringComparison.OrdinalIgnoreCase) >= 0 ||
                     ex.Message.IndexOf("cannot find", StringComparison.OrdinalIgnoreCase) >= 0);

                if (toolNotFound)
                {
                    Logger.Error("ZCamWorker not found → " + ex.Message);
                    return new ZCamNativeResult
                    {
                        ExitCode = -1,
                        Output = ex.Message,
                        ToolNotFound = true
                    };
                }

                Logger.Error("Failed to run ZCamWorker:");
                return new ZCamNativeResult
                {
                    ExitCode = -1,
                    Output = ex.ToString(),
                    ToolNotFound = false
                };
            }
        }

        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;

            Logger.Info("Disposing ZCamNativeProcess");

            try
            {
                _linkedCts?.Cancel();
                TryKillProcess("Dispose()");
            }
            finally
            {
                _runningProcess?.Dispose();
                _runningProcess = null;

                _linkedCts?.Dispose();
                _linkedCts = null;

                OutputReceived = null;
            }
        }

        public Task<ZCamNativeResult> RunAsync(string ip, string ndi, CancellationToken ct = default(CancellationToken))
        {
            return RunInternalAsync(ip, ndi, true, ct);
        }
      
        public void Stop()
        {
            Logger.Info("Stopping ZCamNative...");
            if (_runningProcess != null && !_runningProcess.HasExited)
            {
                _runningProcess.StandardInput.Close(); // Signal the process to exit gracefully

                // 1. Try to close gracefully
                _runningProcess.CloseMainWindow();

                // 2. If it's still alive after 1 second, KILL it
                if (!_runningProcess.WaitForExit(1000))
                {
                    _runningProcess.Kill(); // Forceful termination
                }
            }
        }

        private void TryKillProcess(string reason)
        {
            lock (_processLock)
            {
                try
                {
                    if (_runningProcess != null &&
                        !_runningProcess.HasExited)
                    {
                        Logger.Warning($"Killing ZCamNative ({reason})");

                        _runningProcess.Kill(true); // kill child tree
                        _runningProcess.WaitForExit(3000);
                    }
                }
                catch (Exception ex)
                {
                    Logger.Warning("Kill failed: " + ex.Message);
                }
            }
        }
    }
}