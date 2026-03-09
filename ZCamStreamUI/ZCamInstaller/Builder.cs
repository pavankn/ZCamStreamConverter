using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using WixSharp;
using WixSharp.CommonTasks;

namespace ZCamInstaller
{
    public class ProjectBuilder
    {
        private readonly string company;
        private readonly string product;
        private readonly string mainUIPath;
        private readonly string wixInstallerPath;
        private readonly string driverInstallerPath;
        private readonly string solutionDir;
        private readonly string winUSBDriverPath;
        private readonly string platformToolsPath;
        private readonly string iconFile;

        // Inside your initialization:
        private readonly string configType; // Declare it here


        public ProjectBuilder(string company, string product, string solutionDir)
        {
            this.company = company;
            this.product = product;
            this.solutionDir = solutionDir;
#if DEBUG
            configType = "Debug";
#else
            configType = "Release";
#endif

            wixInstallerPath = Path.Combine(solutionDir, "ZCamInstaller");
            mainUIPath = Path.Combine(solutionDir, "ZCamStreamUI", "bin", configType, "net8.0-windows");
            iconFile = $@"{wixInstallerPath}\res\KhelAI.ico";
        }

        public void SetProjectMetadata(ManagedProject project)
        {
            project.GUID = new Guid("6F9619FF-8B86-D011-B42D-00C04FC964FF");
            project.ControlPanelInfo.Manufacturer = company;
            project.Platform = Platform.x64;
            project.ControlPanelInfo.ProductIcon = iconFile;

            // Optional: also sets the small icon in installer dialogs (top-right corner)
            // project.SetProductIcon(iconFile);               // often redundant with ProductIcon

            project.ControlPanelInfo.Manufacturer = company;


        }

        private Dir UninistallShortcut()
        {
            // ---- Add uninstall shortcut ----
            return new Dir("%ProgramFiles%",
                new Dir(company,
                    new Dir(product,
                        new ExeFileShortcut(
                            $"Uninstall",
                            "[System64Folder]msiexec.exe",
                            "/x [ProductCode]"
                        )
                    )
                )
            );
        }

        private Dir StartMenuShorrcut()
        {
            Dir startMenu = new Dir("ProgramMenuFolder",
                new Dir(company,
                    new Dir(product,
                        new ExeFileShortcut(product, "[INSTALLDIR]ZCamStreamUI.exe", "")
                        {
                            IconFile = this.iconFile
                        }
                    )
                )
            );
            return startMenu;
        }

        private Dir DesktopShortuct()
        {
            return new Dir("DesktopFolder",
                new ExeFileShortcut(product, "[INSTALLDIR]ZCamStreamUI.exe", "")
                {
                    IconFile = this.iconFile,
                }
            );
        }

       
        public ManagedProject Build()
        {
            // --- Proper WiX directory structure ---
            Dir root = new Dir(@"%ProgramFiles%");
            Dir companyDir = new Dir(company);
            Dir appDir = new Dir(product);


            root.AddDir(companyDir);
            companyDir.AddDir(appDir);

            // --- Add files ---
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "ZCamStreamUI.exe")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "ZCamNativeMain.exe")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "ZCamWorker.exe")));

            // Add all DLL files
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "libssp.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "libcurl.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "fmt.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "Common.Logging.Core.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "Common.Logging.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "avcodec-62.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "avdevice-62.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "avformat-62.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "avfilter-11.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "swresample-6.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "swscale-9.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "zlib1.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "Processing.NDI.Lib.Advanced.x64.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "Processing.NDI.Lib.x64.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "Makaretu.Dns.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "Makaretu.Dns.Multicast.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "System.Net.IPNetwork.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "ZCamStreamUI.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "SimpleBase.dll")));
            appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "ZCamStreamUI.runtimeconfig.json")));
            //appDir.AddFile(new WixSharp.File(Path.Combine(mainUIPath, "*.dll")));


            var project = new ManagedProject(product, root, UninistallShortcut(), StartMenuShorrcut(), DesktopShortuct());

            SetProjectMetadata(project);

            return project;
        }
    }
}