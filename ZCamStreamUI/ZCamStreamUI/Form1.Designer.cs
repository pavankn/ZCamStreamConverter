namespace com.khelai.ZCamStreamUI
{
    partial class Form1
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            groupBoxVideo = new GroupBox();
            labelStream = new Label();
            labelHW = new Label();
            labelCodec = new Label();
            labelResolution = new Label();
            comboBoxStream = new ComboBox();
            comboBoxHWAccel = new ComboBox();
            comboBoxCodec = new ComboBox();
            comboBoxResolution = new ComboBox();
            groupBoxControl = new GroupBox();
            btnScanCamera = new Button();
            grpBoxCameras = new GroupBox();
            btnStartCamera = new Button();
            groupBoxVideo.SuspendLayout();
            groupBoxControl.SuspendLayout();
            SuspendLayout();
            // 
            // groupBoxVideo
            // 
            groupBoxVideo.Controls.Add(labelStream);
            groupBoxVideo.Controls.Add(labelHW);
            groupBoxVideo.Controls.Add(labelCodec);
            groupBoxVideo.Controls.Add(labelResolution);
            groupBoxVideo.Controls.Add(comboBoxStream);
            groupBoxVideo.Controls.Add(comboBoxHWAccel);
            groupBoxVideo.Controls.Add(comboBoxCodec);
            groupBoxVideo.Controls.Add(comboBoxResolution);
            groupBoxVideo.Dock = DockStyle.Right;
            groupBoxVideo.Location = new Point(550, 0);
            groupBoxVideo.Name = "groupBoxVideo";
            groupBoxVideo.Size = new Size(250, 450);
            groupBoxVideo.TabIndex = 0;
            groupBoxVideo.TabStop = false;
            groupBoxVideo.Text = "groupBox1";
            groupBoxVideo.Visible = false;
            // 
            // labelStream
            // 
            labelStream.AutoSize = true;
            labelStream.Font = new Font("Segoe UI", 12F, FontStyle.Regular, GraphicsUnit.Point, 0);
            labelStream.ForeColor = SystemColors.Control;
            labelStream.Location = new Point(6, 297);
            labelStream.Name = "labelStream";
            labelStream.Size = new Size(73, 28);
            labelStream.TabIndex = 7;
            labelStream.Text = "Stream";
            // 
            // labelHW
            // 
            labelHW.AutoSize = true;
            labelHW.Font = new Font("Segoe UI", 12F, FontStyle.Regular, GraphicsUnit.Point, 0);
            labelHW.ForeColor = SystemColors.Control;
            labelHW.Location = new Point(6, 210);
            labelHW.Name = "labelHW";
            labelHW.Size = new Size(45, 28);
            labelHW.TabIndex = 6;
            labelHW.Text = "HW";
            // 
            // labelCodec
            // 
            labelCodec.AutoSize = true;
            labelCodec.Font = new Font("Segoe UI", 12F, FontStyle.Regular, GraphicsUnit.Point, 0);
            labelCodec.ForeColor = SystemColors.Control;
            labelCodec.Location = new Point(5, 131);
            labelCodec.Name = "labelCodec";
            labelCodec.Size = new Size(67, 28);
            labelCodec.TabIndex = 5;
            labelCodec.Text = "Codec";
            // 
            // labelResolution
            // 
            labelResolution.AutoSize = true;
            labelResolution.Font = new Font("Segoe UI", 12F, FontStyle.Regular, GraphicsUnit.Point, 0);
            labelResolution.ForeColor = SystemColors.Control;
            labelResolution.Location = new Point(2, 47);
            labelResolution.Name = "labelResolution";
            labelResolution.Size = new Size(104, 28);
            labelResolution.TabIndex = 4;
            labelResolution.Text = "Resolution";
            // 
            // comboBoxStream
            // 
            comboBoxStream.DropDownStyle = ComboBoxStyle.DropDownList;
            comboBoxStream.FormattingEnabled = true;
            comboBoxStream.Items.AddRange(new object[] { "Stream0", "Stream1" });
            comboBoxStream.Location = new Point(108, 297);
            comboBoxStream.Name = "comboBoxStream";
            comboBoxStream.Size = new Size(120, 28);
            comboBoxStream.TabIndex = 3;
            comboBoxStream.SelectedIndexChanged += SaveUIToSettings;
            // 
            // comboBoxHWAccel
            // 
            comboBoxHWAccel.DropDownStyle = ComboBoxStyle.DropDownList;
            comboBoxHWAccel.FormattingEnabled = true;
            comboBoxHWAccel.Items.AddRange(new object[] { "True", "False" });
            comboBoxHWAccel.Location = new Point(108, 214);
            comboBoxHWAccel.Name = "comboBoxHWAccel";
            comboBoxHWAccel.Size = new Size(120, 28);
            comboBoxHWAccel.TabIndex = 2;
            comboBoxHWAccel.SelectedIndexChanged += SaveUIToSettings;
            // 
            // comboBoxCodec
            // 
            comboBoxCodec.DropDownStyle = ComboBoxStyle.DropDownList;
            comboBoxCodec.FormattingEnabled = true;
            comboBoxCodec.Items.AddRange(new object[] { "H264", "HEVC" });
            comboBoxCodec.Location = new Point(108, 132);
            comboBoxCodec.Name = "comboBoxCodec";
            comboBoxCodec.Size = new Size(120, 28);
            comboBoxCodec.TabIndex = 1;
            comboBoxCodec.SelectedIndexChanged += SaveUIToSettings;
            // 
            // comboBoxResolution
            // 
            comboBoxResolution.DropDownStyle = ComboBoxStyle.DropDownList;
            comboBoxResolution.FormattingEnabled = true;
            comboBoxResolution.Location = new Point(108, 46);
            comboBoxResolution.Name = "comboBoxResolution";
            comboBoxResolution.Size = new Size(120, 28);
            comboBoxResolution.TabIndex = 0;
            comboBoxResolution.SelectedIndexChanged += SaveUIToSettings;
            // 
            // groupBoxControl
            // 
            groupBoxControl.Controls.Add(btnStartCamera);
            groupBoxControl.Controls.Add(btnScanCamera);
            groupBoxControl.Dock = DockStyle.Bottom;
            groupBoxControl.Location = new Point(0, 325);
            groupBoxControl.Name = "groupBoxControl";
            groupBoxControl.Size = new Size(550, 125);
            groupBoxControl.TabIndex = 1;
            groupBoxControl.TabStop = false;
            groupBoxControl.Text = "groupBox1";
            // 
            // btnScanCamera
            // 
            btnScanCamera.Location = new Point(27, 26);
            btnScanCamera.Name = "btnScanCamera";
            btnScanCamera.Size = new Size(94, 29);
            btnScanCamera.TabIndex = 0;
            btnScanCamera.Text = "Scan";
            btnScanCamera.UseVisualStyleBackColor = true;
            btnScanCamera.Click += OnClick_ScanCamera;
            // 
            // grpBoxCameras
            // 
            grpBoxCameras.Dock = DockStyle.Fill;
            grpBoxCameras.Location = new Point(0, 0);
            grpBoxCameras.Name = "grpBoxCameras";
            grpBoxCameras.Size = new Size(550, 325);
            grpBoxCameras.TabIndex = 2;
            grpBoxCameras.TabStop = false;
            grpBoxCameras.Text = "groupBox1";
            // 
            // btnStartCamera
            // 
            btnStartCamera.Location = new Point(27, 84);
            btnStartCamera.Name = "btnStartCamera";
            btnStartCamera.Size = new Size(94, 29);
            btnStartCamera.TabIndex = 1;
            btnStartCamera.Text = "Start";
            btnStartCamera.UseVisualStyleBackColor = true;
            btnStartCamera.Click += OnClick_StartCamera;
            // 
            // Form1
            // 
            AutoScaleDimensions = new SizeF(8F, 20F);
            AutoScaleMode = AutoScaleMode.Font;
            BackColor = SystemColors.ControlText;
            ClientSize = new Size(800, 450);
            Controls.Add(grpBoxCameras);
            Controls.Add(groupBoxControl);
            Controls.Add(groupBoxVideo);
            Name = "Form1";
            Text = "Form1";
            Load += Form1_Load;
            groupBoxVideo.ResumeLayout(false);
            groupBoxVideo.PerformLayout();
            groupBoxControl.ResumeLayout(false);
            ResumeLayout(false);
        }

        #endregion

        private GroupBox groupBoxVideo;
        private GroupBox groupBoxControl;
        private GroupBox grpBoxCameras;
        private Button btnScanCamera;
        private ComboBox comboBoxResolution;
        private ComboBox comboBoxCodec;
        private ComboBox comboBoxHWAccel;
        private ComboBox comboBoxStream;
        private Label labelResolution;
        private Label labelCodec;
        private Label labelHW;
        private Label labelStream;
        private Button btnStartCamera;
    }
}
