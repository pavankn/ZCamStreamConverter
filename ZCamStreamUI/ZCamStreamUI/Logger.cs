using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using System;
using System.IO;

namespace com.khelai.ZCamStreamUI
{
    public static class Logger
    {
        private static readonly object _lock = new object();

        //private static readonly string LogDirectory =
        //    Path.Combine(AppContext.BaseDirectory, "Logs", "Fastboot");
        private static string appDataLocal = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);

        private static readonly string LogDirectory = Path.Combine(appDataLocal, "KhelAI", "Logs");

        private static string _logFilePath;

        // Controls whether Debug messages are written
        public static bool DebugEnabled { get; set; } = true;

        // Controls whether console output is produced (useful in GUI apps)
        public static bool WriteToConsole { get; set; } = true;

        static Logger()
        {
            // One-time initialization
            try
            {
                Directory.CreateDirectory(LogDirectory);

                // Daily log file (most common & convenient pattern)
                string dateStamp = DateTime.Now.ToString("yyyy-MM-dd");
                _logFilePath = Path.Combine(LogDirectory, $"fastboot_{dateStamp}.log");

                if (_logFilePath != null)
                {
                    File.WriteAllText(_logFilePath, dateStamp);
                }

                // Optional: write startup marker
                Info("───────────────────────────────────────────────");
                Info($"Logger initialized | App: {AppContext.BaseDirectory}");
                Info($"Log file: {_logFilePath}");
                Info($"Debug mode: {(DebugEnabled ? "ENABLED" : "disabled")}");
                Info("───────────────────────────────────────────────");
            }
            catch
            {
                // If directory creation fails → silent failure (fallback to no logging)
                _logFilePath = null;
            }
        }

        public static void Info(string message)
        {
            Write("INFO ", message, ConsoleColor.White);
        }

        public static void Debug(string message)
        {
            if (!DebugEnabled) return;
            Write("DEBUG", message, ConsoleColor.Gray);
        }

        public static void Warning(string message)
        {
            Write("WARN ", message, ConsoleColor.Yellow);
        }

        public static void Error(string message)
        {
            Write("ERROR", message, ConsoleColor.Red);
        }

        public static void Exception(Exception ex, string context = null)
        {
            string msg = context != null
                ? $"{context} → {ex.Message}\n{ex.StackTrace}"
                : ex.ToString();

            Write("ERROR", msg, ConsoleColor.Red);
        }

        private static void Write(string level, string message, ConsoleColor color)
        {
            string timestamp = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss.fff");
            string line = $"{timestamp} | {level,-5} | {message}";

            lock (_lock)
            {
                // File logging
                if (_logFilePath != null)
                {
                    try
                    {
                        File.AppendAllText(_logFilePath, line + Environment.NewLine);
                    }
                    catch
                    {
                        // silent fail – never crash app because of logging
                    }
                }

                // Console logging (useful during development)
                if (WriteToConsole)
                {
                    try
                    {
                        Console.ForegroundColor = color;
                        Console.WriteLine(line);
                        Console.ResetColor();
                    }
                    catch
                    {
                        // nop – console may not be available in some contexts
                    }
                }
            }
        }

        // Optional: one-off message without level (e.g. command output)
        public static void Raw(string message)
        {
            lock (_lock)
            {
                if (_logFilePath != null)
                {
                    try
                    {
                        File.AppendAllText(_logFilePath, message + Environment.NewLine);
                    }
                    catch { }
                }

                if (WriteToConsole)
                {
                    Console.WriteLine(message);
                }
            }
        }

        // Helper: force flush / change file mid-run (rarely needed)
        public static void RotateLog()
        {
            string dateStamp = DateTime.Now.ToString("yyyy-MM-dd");
            _logFilePath = Path.Combine(LogDirectory, $"fastboot_{dateStamp}.log");
            Info("Log file rotated to new day");
        }
    }
}