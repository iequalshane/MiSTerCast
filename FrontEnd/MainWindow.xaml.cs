using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.Win32;
using System.Globalization;
using System.Net;

namespace MiSTerCast
{
    struct Modeline
    {
        public string name;
        public Double pclock;
        public UInt16 hactive;
        public UInt16 hbegin;
        public UInt16 hend;
        public UInt16 htotal;
        public UInt16 vactive;
        public UInt16 vbegin;
        public UInt16 vend;
        public UInt16 vtotal;
        public bool interlace;
    }

    struct SourceOptions
    {
        public byte display;
        public bool audio;
        public bool preview;
        public byte alignment;
        public byte cropmode;
        public UInt16 width;
        public UInt16 height;
        public Int16 xoffset;
        public Int16 yoffset;
        public byte rotation;
    }

    public partial class MainWindow : Window
    {
        private bool isInitialized = false;
        private bool isStreaming = false;
        HelpWindow helpWindow = null;
        const string lastSaveFilename = "lastsave.dat";
        string currentSaveFilename = null;

        private void InitializeMiSTerCast()
        {
            if (LogDelegate == null)
                LogDelegate = new MiSTerCastInterop.LogDelegate(LogCallback);
            if (CaptureImageDelegate == null)
                CaptureImageDelegate = new MiSTerCastInterop.CaptureImageDelegate(CaptureImage);
            isInitialized = MiSTerCastInterop.Initialize(LogDelegate, CaptureImageDelegate);
            if (isInitialized)
            {
                OnModelineChanged();
            }
        }

        public MainWindow()
        {
            InitializeComponent();
            ReadModelinesFile();
            PopulateModelineDropdown();
            InitializeMiSTerCast();
        }

        void MainWindow_Closing(object sender, CancelEventArgs e)
        {
            if (isStreaming)
                MiSTerCastInterop.StopStream();
            MiSTerCastInterop.Shutdown();
            helpWindow.Close();
        }

        private void Window_Closed(object sender, EventArgs e)
        {
            Application.Current.Shutdown();
        }

        private void HelpButton_Click(object sender, RoutedEventArgs e)
        {
            helpWindow.Show();
        }

        private void Window_Activated(object sender, EventArgs e)
        {
            if (helpWindow == null)
            {
                helpWindow = new HelpWindow();
                if (!File.Exists(lastSaveFilename))
                {
                    // Show help the first time MiSTerCast is opened
                    helpWindow.Show();
                    try
                    {
                        File.Create(lastSaveFilename);
                    }
                    catch (Exception exception)
                    {
                        Log("Creating last save file failed: " + exception.Message, true);
                    }
                }
                else
                {
                    try
                    {
                        currentSaveFilename = null;
                        using (StreamReader sr = File.OpenText(lastSaveFilename))
                        {
                            if (!sr.EndOfStream)
                            {
                                currentSaveFilename = sr.ReadLine();
                                if (!File.Exists(currentSaveFilename))
                                {
                                    currentSaveFilename = null;
                                    Log("Last save is missing.", true);
                                }
                            }
                        }

                        if (currentSaveFilename != null)
                        {
                            Log("Auto loading settings: " + currentSaveFilename);
                            using (StreamReader sr = File.OpenText(currentSaveFilename))
                            {
                                LoadSaveFileFromStream(sr);
                            }
                        }
                    }
                    catch (Exception exception)
                    {
                        Log("Reading last save file failed: " + exception.Message, true);
                    }
                }
            }
        }

        private void ToggleStreamButton_Click(object sender, RoutedEventArgs e)
        {
            if (isStreaming)
            {
                if (MiSTerCastInterop.StopStream())
                {
                    isStreaming = false;
                    ToggleStreamButton.Content = "Start Stream";
                    CaptureSourceBox.IsEnabled = true;
                    EnableAudioCheckBox.IsEnabled = true;
                    ApplyModelineButton.IsEnabled = false;
                }
            }
            else
            {
                if (!isInitialized)
                    InitializeMiSTerCast();

                if (isInitialized)
                {
                    EnablePreviewCheckBox.IsChecked = false;
                    IPAddress ipAddress = null;
                    if (!IPAddress.TryParse(TargetIpAddresTextBox.Text, out ipAddress))
                    {
                        try
                        {
                            ipAddress = Dns.GetHostEntry(TargetIpAddresTextBox.Text).AddressList[0];
                        }
                        catch (Exception exception)
                        {
                            Log("Resolving target IP address failed: " + exception.Message, true);
                            return;
                        }
                    }

                    if (MiSTerCastInterop.StartStream(ipAddress.ToString()))
                    {
                        isStreaming = true;
                        ToggleStreamButton.Content = "Stop Stream";
                        CaptureSourceBox.IsEnabled = false;
                        EnableAudioCheckBox.IsEnabled = false;
                    }
                }
            }
        }

        #region Settings

        const int SettingsVersion = 1;

        private void SaveSettingsButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                SaveFileDialog saveFileDialog = new SaveFileDialog();
                saveFileDialog.Filter = "Settings File|*.sav";
                saveFileDialog.Title = "Save MiSTerCast settings";
                if (currentSaveFilename != null)
                {
                    saveFileDialog.InitialDirectory = Path.GetDirectoryName(currentSaveFilename);
                    saveFileDialog.FileName = Path.GetFileName(currentSaveFilename);
                }
                
                if (saveFileDialog.ShowDialog().Value)
                {
                    if (!String.IsNullOrWhiteSpace(saveFileDialog.FileName))
                    {
                        using (System.IO.FileStream fs = (System.IO.FileStream)saveFileDialog.OpenFile())
                        {
                            using (var sw = new StreamWriter(fs))
                            {
                                sw.WriteLine(SettingsVersion);

                                sw.WriteLine(TargetIpAddresTextBox.Text);

                                sw.WriteLine(ModelinePresetsBox.SelectedIndex);
                                sw.WriteLine(pclockTextBox.Text);
                                sw.WriteLine(hactiveTextBox.Text);
                                sw.WriteLine(hbeginTextBox.Text);
                                sw.WriteLine(hendTextBox.Text);
                                sw.WriteLine(htotalTextBox.Text);
                                sw.WriteLine(vactiveTextBox.Text);
                                sw.WriteLine(vbeginTextBox.Text);
                                sw.WriteLine(vendTextBox.Text);
                                sw.WriteLine(vtotalTextBox.Text);
                                sw.WriteLine(interlacedCheckBox.IsChecked.Value ? 1 : 0);

                                sw.WriteLine(CaptureSourceBox.SelectedIndex);
                                sw.WriteLine(RotateComboBox.SelectedIndex);
                                sw.WriteLine(EnableAudioCheckBox.IsChecked.Value ? 1 : 0);
                                sw.WriteLine(CropComboBox.SelectedIndex);
                                sw.WriteLine(CaptureWidth.Text);
                                sw.WriteLine(CaptureHeight.Text);
                                sw.WriteLine(CaptureXOffset.Text);
                                sw.WriteLine(CaptureYOffset.Text);

                                Log("Settings saved.");
                            }
                        }

                        UpdateLastSaveFile(saveFileDialog.FileName);
                    }
                }
            }
            catch (Exception exception)
            {
                Log("Save settings failed: " + exception.Message, true);
            }
        }

        private void LoadSettingsButton_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                OpenFileDialog openFileDialog = new OpenFileDialog();
                openFileDialog.Filter = "Settings File|*.sav";
                openFileDialog.Title = "Load MiSTerCast settings";
                if (currentSaveFilename != null)
                {
                    openFileDialog.InitialDirectory = Path.GetDirectoryName(currentSaveFilename);
                    openFileDialog.FileName = Path.GetFileName(currentSaveFilename);
                }

                if (openFileDialog.ShowDialog().Value)
                {
                    if (!String.IsNullOrWhiteSpace(openFileDialog.FileName))
                    {
                        using (System.IO.FileStream fs = (System.IO.FileStream)openFileDialog.OpenFile())
                        {
                            using (var sr = new StreamReader(fs))
                            {
                                LoadSaveFileFromStream(sr);
                                UpdateLastSaveFile(openFileDialog.FileName);
                            }
                        }
                    }
                }
            }
            catch (Exception exception)
            {
                Log("Load settings failed: " + exception.Message, true);
            }
        }

        private void LoadSaveFileFromStream(StreamReader sr)
        {
            int settingsVersion = int.Parse(sr.ReadLine());
            if (settingsVersion > SettingsVersion)
            {
                Log("Unsupported save file version: " + settingsVersion, true);
                return;
            }

            TargetIpAddresTextBox.Text = sr.ReadLine();

            ModelinePresetsBox.SelectedIndex = Math.Min(int.Parse(sr.ReadLine()), ModelinePresetsBox.Items.Count - 1);
            pclockTextBox.Text = sr.ReadLine();
            hactiveTextBox.Text = sr.ReadLine();
            hbeginTextBox.Text = sr.ReadLine();
            hendTextBox.Text = sr.ReadLine();
            htotalTextBox.Text = sr.ReadLine();
            vactiveTextBox.Text = sr.ReadLine();
            vbeginTextBox.Text = sr.ReadLine();
            vendTextBox.Text = sr.ReadLine();
            vtotalTextBox.Text = sr.ReadLine();
            interlacedCheckBox.IsChecked = sr.ReadLine() == "1" ? true : false;

            CaptureSourceBox.SelectedIndex = Math.Min(int.Parse(sr.ReadLine()), CaptureSourceBox.Items.Count - 1);
            RotateComboBox.SelectedIndex = Math.Min(int.Parse(sr.ReadLine()), RotateComboBox.Items.Count - 1);
            EnableAudioCheckBox.IsChecked = sr.ReadLine() == "1" ? true : false;
            CropComboBox.SelectedIndex = Math.Min(int.Parse(sr.ReadLine()), CropComboBox.Items.Count - 1);
            CaptureWidth.Text = sr.ReadLine();
            CaptureHeight.Text = sr.ReadLine();
            CaptureXOffset.Text = sr.ReadLine();
            CaptureYOffset.Text = sr.ReadLine();

            Log("Settings loaded.");
        }

        private void UpdateLastSaveFile(string fileName)
        {
            currentSaveFilename = fileName;
            try
            {
                using (FileStream fs = File.OpenWrite(lastSaveFilename))
                {
                    using (StreamWriter sw = new StreamWriter(fs))
                    {
                        sw.WriteLine(fileName);
                    }
                }
            }
            catch (Exception exception)
            {
                Log("Save last save file for autoload failed: " + exception.Message, true);
            }
        }

        #endregion Settings

        #region Number Entry Validation

        private void PositiveIntValidation(object sender, TextCompositionEventArgs e)
        {
            int result;
            e.Handled =
                !(int.TryParse(((TextBox)sender).Text + e.Text, out result) &&
                result >= 0);
        }

        private void IntValidation(object sender, TextCompositionEventArgs e)
        {
            int result;
            string fullText = ((TextBox)sender).Text.Insert(((TextBox)sender).CaretIndex, e.Text);
            e.Handled = !int.TryParse(fullText, out result) && fullText != "-";
        }

        private void PositiveDoubleValidation(object sender, TextCompositionEventArgs e)
        {
            double result;
            e.Handled =
                !(double.TryParse((((TextBox)sender).Text + e.Text).Replace(',','.'), NumberStyles.Any, CultureInfo.InvariantCulture, out result) &&
                result >= 0);
        }

        #endregion Number Entry Validation

        #region Logs

        private MiSTerCastInterop.LogDelegate LogDelegate;

        private void Log(string message, bool error = false)
        {
            this.Dispatcher.InvokeAsync(() =>
            {
                TextBlock logText = new TextBlock() { Text = message };
                if (error)
                    logText.Background = Brushes.Pink;
                LogPanel.Children.Add(logText);//.Insert(0, (logText));
            });
        }

        private void LogCallback(string message, bool error)
        {
            Log(message, error);
        }

        private bool autoScrollLogs = true;
        private void LogScrollView_ScrollChanged(object sender, ScrollChangedEventArgs e)
        {
            if (e.ExtentHeightChange == 0)
            {
                if (LogScrollView.VerticalOffset == LogScrollView.ScrollableHeight)
                    autoScrollLogs = true;
                else
                    autoScrollLogs = false;
            }

            if (autoScrollLogs && e.ExtentHeightChange != 0)
                LogScrollView.ScrollToVerticalOffset(LogScrollView.ExtentHeight);
        }

        #endregion Logs

        #region Capture Source

        private SourceOptions currentSourceOptions;

        private void OnCaptureSourceChanged()
        {
            currentSourceOptions.display = (byte)CaptureSourceBox.SelectedIndex;
            currentSourceOptions.alignment = (byte)AlignmentBox.SelectedIndex;
            currentSourceOptions.rotation = (byte)RotateComboBox.SelectedIndex;
            currentSourceOptions.cropmode = (byte)CropComboBox.SelectedIndex;
            CaptureWidth.IsEnabled = CropComboBox.SelectedIndex == 0;
            CaptureHeight.IsEnabled = CropComboBox.SelectedIndex == 0;
            currentSourceOptions.audio = EnableAudioCheckBox.IsChecked.Value;
            currentSourceOptions.preview = EnablePreviewCheckBox.IsChecked.Value;
            ushort.TryParse(CaptureWidth.Text, out currentSourceOptions.width);
            ushort.TryParse(CaptureHeight.Text, out currentSourceOptions.height);
            short.TryParse(CaptureXOffset.Text, out currentSourceOptions.xoffset);
            short.TryParse(CaptureYOffset.Text, out currentSourceOptions.yoffset);

            PreviewImage.Visibility = currentSourceOptions.preview ? Visibility.Visible : Visibility.Hidden;
            PreviewDisabledLabel.Visibility = currentSourceOptions.preview ? Visibility.Hidden : Visibility.Visible;

            if (currentSourceOptions.width > 0 && currentSourceOptions.height > 0)
            {
                MiSTerCastInterop.SetSource(
                    currentSourceOptions.display,
                    currentSourceOptions.audio,
                    currentSourceOptions.preview,
                    currentSourceOptions.alignment,
                    currentSourceOptions.cropmode,
                    currentSourceOptions.width,
                    currentSourceOptions.height,
                    currentSourceOptions.xoffset,
                    currentSourceOptions.yoffset,
                    currentSourceOptions.rotation);
            }
        }

        private void CaptureSource_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (isInitialized)
                OnCaptureSourceChanged();
        }

        private void CaptureSource_Checked(object sender, RoutedEventArgs e)
        {
            if (isInitialized)
                OnCaptureSourceChanged();
        }

        private void CaptureSource_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (isInitialized)
            {
                OnCaptureSourceChanged();
            }
        }

        private void UpdateCropSize()
        {
            CaptureWidth.TextChanged -= CaptureSource_TextChanged;
            CaptureHeight.TextChanged -= CaptureSource_TextChanged;
            switch (currentSourceOptions.cropmode)
            {
                case 1:
                    CaptureWidth.Text = (currentModeLine.hactive).ToString();
                    CaptureHeight.Text = (currentModeLine.vactive).ToString();
                    break;
                case 2:
                    CaptureWidth.Text = (currentModeLine.hactive * 2).ToString();
                    CaptureHeight.Text = (currentModeLine.vactive * 2).ToString();
                    break;
                case 3:
                    CaptureWidth.Text = (currentModeLine.hactive * 3).ToString();
                    CaptureHeight.Text = (currentModeLine.vactive * 3).ToString();
                    break;
                case 4:
                    CaptureWidth.Text = (currentModeLine.hactive * 4).ToString();
                    CaptureHeight.Text = (currentModeLine.vactive * 4).ToString();
                    break;
                case 5:
                    CaptureWidth.Text = (currentModeLine.hactive * 5).ToString();
                    CaptureHeight.Text = (currentModeLine.vactive * 5).ToString();
                    break;
                default:
                    break;
            }
            CaptureWidth.TextChanged += CaptureSource_TextChanged;
            CaptureHeight.TextChanged += CaptureSource_TextChanged;
        }

        #endregion Capture Source

        #region Modelines

        private Modeline currentModeLine;
        private List<Modeline> modelines;

        private void OnModelineChanged()
        {
            double.TryParse(pclockTextBox.Text.Replace(',', '.'), NumberStyles.Any, CultureInfo.InvariantCulture, out currentModeLine.pclock);
            ushort.TryParse(hactiveTextBox.Text, out currentModeLine.hactive);
            ushort.TryParse(hbeginTextBox.Text, out currentModeLine.hbegin);
            ushort.TryParse(hendTextBox.Text, out currentModeLine.hend);
            ushort.TryParse(htotalTextBox.Text, out currentModeLine.htotal);
            ushort.TryParse(vactiveTextBox.Text,  out currentModeLine.vactive);
            ushort.TryParse(vbeginTextBox.Text, out currentModeLine.vbegin);
            ushort.TryParse(vendTextBox.Text, out currentModeLine.vend);
            ushort.TryParse(vtotalTextBox.Text, out currentModeLine.vtotal);
            currentModeLine.interlace = interlacedCheckBox.IsChecked.Value;

            if (isInitialized && currentModeLine.pclock > 0 && currentModeLine.hactive > 0 && currentModeLine.vactive > 0)
            {
                MiSTerCastInterop.SetModeline(
                    currentModeLine.pclock,
                    currentModeLine.hactive,
                    currentModeLine.hbegin,
                    currentModeLine.hend,
                    currentModeLine.htotal,
                    currentModeLine.vactive,
                    currentModeLine.vbegin,
                    currentModeLine.vend,
                    currentModeLine.vtotal,
                    currentModeLine.interlace);

                UpdateCropSize();
                OnCaptureSourceChanged();
            }
        }

        private void ApplyModelineButton_Click(object sender, RoutedEventArgs e)
        {
            OnModelineChanged();
            ApplyModelineButton.IsEnabled = false;
        }

        private void SetModelineUI(Modeline modeline)
        {
            ignoreModelineChange = true;
            pclockTextBox.Text = modeline.pclock.ToString();
            hactiveTextBox.Text = modeline.hactive.ToString();
            hbeginTextBox.Text = modeline.hbegin.ToString();
            hendTextBox.Text = modeline.hend.ToString();
            htotalTextBox.Text = modeline.htotal.ToString();
            vactiveTextBox.Text = modeline.vactive.ToString();
            vbeginTextBox.Text = modeline.vbegin.ToString();
            vendTextBox.Text = modeline.vend.ToString();
            vtotalTextBox.Text = modeline.vtotal.ToString();
            interlacedCheckBox.IsChecked = modeline.interlace;
            ignoreModelineChange = false;
        }

        private void ModelinePresetsBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {

            if (ModelinePresetsBox.SelectedIndex > 0 && modelines != null && ModelinePresetsBox.SelectedIndex <= modelines.Count)
            {
                SetModelineUI(modelines[ModelinePresetsBox.SelectedIndex - 1]);
                OnModelineChanged();
            }
        }

        void ReadModelinesFile()
        {
            List<Modeline> newModeLines = new List<Modeline>();
            try
            {
                List<string> lines = new List<string>(File.ReadAllLines("modelines.dat"));

                for (int i = 0; i < lines.Count; i++)
                {
                    Modeline modeline = new Modeline();
                    bool badLine = false;
                    string line = lines[i].Trim();
                    if (line.Length == 0 || line[0] == ';')
                    {
                        badLine = true;
                    }
                    else
                    {
                        int nameStart = line.IndexOf('[');
                        int nameEnd = line.IndexOf(']');
                        if (nameStart == -1 || nameEnd == -1 || nameEnd <= nameStart + 1)
                        {
                            Log("Invalid modeline name format: " + lines[i], true);
                            badLine = true;
                        }
                        else
                        {
                            modeline.name = line.Substring(nameStart + 1, nameEnd - nameStart - 1);
                            if (String.IsNullOrEmpty(modeline.name))
                            {
                                Log("Invalid modeline name format: " + lines[i], true);
                                badLine = true;
                            }
                            else
                            {
                                string[] values = line.Remove(nameStart, nameEnd - nameStart + 1)
                                    .Split()
                                    .Select(p => p.Trim())
                                    .Where(p => !string.IsNullOrWhiteSpace(p))
                                    .ToArray();
                                if (values.Length != 10)
                                {
                                    Log("Invalid modeline values count: " + lines[i], true);
                                    badLine = true;
                                }
                                else
                                {
                                    UInt16 interlace;
                                    if (!Double.TryParse(values[0].Replace(',', '.'), NumberStyles.Any, CultureInfo.InvariantCulture,out modeline.pclock) ||
                                        !UInt16.TryParse(values[1], out modeline.hactive) ||
                                        !UInt16.TryParse(values[2], out modeline.hbegin) ||
                                        !UInt16.TryParse(values[3], out modeline.hend) ||
                                        !UInt16.TryParse(values[4], out modeline.htotal) ||
                                        !UInt16.TryParse(values[5], out modeline.vactive) ||
                                        !UInt16.TryParse(values[6], out modeline.vbegin) ||
                                        !UInt16.TryParse(values[7], out modeline.vend) ||
                                        !UInt16.TryParse(values[8], out modeline.vtotal) ||
                                        !UInt16.TryParse(values[9], out interlace))
                
                                    {
                                        Log("Invalid modeline values format: " + lines[i], true);
                                        badLine = true;
                                    }
                                    else
                                    {
                                        modeline.interlace = interlace != 0;
                                        newModeLines.Add(modeline);
                                    }
                                }
                            }
                        }
                    }

                    if (badLine)
                    {
                        lines.RemoveAt(i);
                        i--;
                    }
                }

                if (newModeLines.Count == 0)
                    throw new Exception("No valid modelines.");

                modelines = newModeLines;
            }
            catch (Exception e)
            {
                Log("Failed to read modelines.dat. " + e.Message, true);
            }
        }

        void PopulateModelineDropdown()
        {
            ModelinePresetsBox.Items.Clear();
            ModelinePresetsBox.Items.Add("Custom");
            foreach (Modeline modeline in modelines)
            {
                ModelinePresetsBox.Items.Add(modeline.name);
            }

            ModelinePresetsBox.SelectedIndex = modelines.Count > 0 ? 1 : 0;
        }

        bool ignoreModelineChange = false;
        private void ModeLineTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            OnManualModelineChange();
        }

        private void InterlacedCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            OnManualModelineChange();
        }

        private void OnManualModelineChange()
        {
            if (!ignoreModelineChange)
            {
                ModelinePresetsBox.SelectedIndex = 0;
                if (isStreaming)
                    ApplyModelineButton.IsEnabled = true;
            }
        }

        #endregion Modelines

        #region Preview

        private MiSTerCastInterop.CaptureImageDelegate CaptureImageDelegate;
        private bool isPreviewEnabled = true;
        
        public void CaptureImage(int width, int height, IntPtr buffer)
        {
            if (isPreviewEnabled)
            {
                BitmapSource source = CreateBitmapSource(width, height, buffer);
                source.Freeze();
                this.Dispatcher.InvokeAsync(() =>
                {
                    PreviewImage.Source = source;
                });
            }
        }

        [DllImport("kernel32.dll", EntryPoint = "CopyMemory", SetLastError = false)]
        public static extern void CopyMemory(IntPtr dest, IntPtr src, uint count);

        public BitmapSource CreateBitmapSource(int width, int height, IntPtr buffer)
        {
            WriteableBitmap writableImg = new WriteableBitmap(width, height, 96, 96, PixelFormats.Bgra32, null);

            writableImg.Lock();
            CopyMemory(writableImg.BackBuffer, buffer, (uint)(4 * width * height));
            writableImg.AddDirtyRect(new Int32Rect(0, 0, width, height));
            writableImg.Unlock();

            return writableImg;
        }

        #endregion Preview
    }
}
