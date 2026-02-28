using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace com.khelai.ZCamStreamUI
{
    public class FastbootResult
    {
        public int ExitCode { get; set; }
        public string Output { get; set; }
        public bool ToolNotFound { get; set; }

        public bool Success
        {
            get { return ExitCode == 0 && !ToolNotFound; }
        }

        public FastbootResult()
        {
            Output = string.Empty;
        }
    }

    public class ZCamNativeProcess : IDisposable
    {
        public event Action<string> OutputReceived;

        private readonly string _fastbootPath;
        private bool _disposed;

        public ZCamNativeProcess(string fastbootPath = null)
        {
            _fastbootPath = fastbootPath
                ?? @"C:\Program Files\KhelAI\ZCamStreamConverter\ZCamNative.exe";

            if (!File.Exists(_fastbootPath))
            {
                Logger.Error("Fastboot executable not found at: " + _fastbootPath);
                throw new FileNotFoundException("fastboot.exe not found", _fastbootPath);
            }

            Logger.Info("═══════════════════════════════════════════════");
            Logger.Info("FastbootProcess initialized at " + DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            Logger.Info("Fastboot path: " + _fastbootPath);
            Logger.Info("═══════════════════════════════════════════════");
        }


        private async Task<FastbootResult> RunInternal(string arguments, bool isAsync, CancellationToken ct)
        {
            ct.ThrowIfCancellationRequested();

            Logger.Debug("Preparing fastboot command: " + arguments);

            var psi = new ProcessStartInfo
            {
                FileName = _fastbootPath,
                Arguments = arguments,
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
                using (var process = new Process { StartInfo = psi })
                {
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

                    Logger.Debug("Starting: fastboot " + arguments);
                    process.Start();

                    process.BeginOutputReadLine();
                    process.BeginErrorReadLine();

                    Task waitTask = Task.Run(() => process.WaitForExit(), ct);

                    if (isAsync)
                        await waitTask;
                    else
                        waitTask.GetAwaiter().GetResult();

                    ct.ThrowIfCancellationRequested();

                    Logger.Debug("fastboot finished → exit code: " + process.ExitCode);

                    return new FastbootResult
                    {
                        ExitCode = process.ExitCode,
                        Output = outputBuilder.ToString().TrimEnd(),
                        ToolNotFound = false
                    };
                }
            }
            catch (OperationCanceledException)
            {
                Logger.Warning("Command cancelled: fastboot " + arguments);
                throw;
            }
            catch (Exception ex)
            {
                bool toolNotFound = ex is Win32Exception &&
                    (ex.Message.IndexOf("not found", StringComparison.OrdinalIgnoreCase) >= 0 ||
                     ex.Message.IndexOf("cannot find", StringComparison.OrdinalIgnoreCase) >= 0);

                if (toolNotFound)
                {
                    Logger.Error("fastboot not found → " + ex.Message);
                    return new FastbootResult
                    {
                        ExitCode = -1,
                        Output = ex.Message,
                        ToolNotFound = true
                    };
                }

                Logger.Error("Failed to run fastboot " + arguments + "\n" + ex);
                return new FastbootResult
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
            OutputReceived = null;
        }

        public Task<FastbootResult> RunAsync(string arguments, CancellationToken ct = default(CancellationToken))
        {
            return RunInternal(arguments, true, ct);
        }
    }
}