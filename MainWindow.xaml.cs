using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Runtime.InteropServices;
using System.Windows.Interop;
using Forms = System.Windows.Forms;
using Application = System.Windows.Application;
using Clipboard = System.Windows.Clipboard;
using MessageBox = System.Windows.MessageBox;
using TextBox = System.Windows.Controls.TextBox;
using Color = System.Windows.Media.Color;
using ColorConverter = System.Windows.Media.ColorConverter;
using Brush = System.Windows.Media.Brush;

namespace HexDecTC
{
    public partial class MainWindow : Window
    {
        [DllImport("dwmapi.dll")]
        public static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int attrValue, int attrSize);

        private const int DWMWA_USE_IMMERSIVE_DARK_MODE = 20;

        private Dictionary<string, Dictionary<string, string>> _opcodesData = new();
        private Forms.NotifyIcon? _notifyIcon;
        private bool _isUpdating = false;
        private bool _isExiting = false;

        public MainWindow()
        {
            InitializeComponent();
            LoadOpcodes();
            UpdateOpcodeList(); // Ensure list is populated
            SetupTray();
            this.Closing += MainWindow_Closing;
            this.SourceInitialized += MainWindow_SourceInitialized;
        }

        private void MainWindow_SourceInitialized(object? sender, EventArgs e)
        {
            var hwnd = new WindowInteropHelper(this).Handle;
            int darkMode = 1;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, ref darkMode, sizeof(int));
        }

        private void LoadOpcodes()
        {
            try
            {
                // Load from Embedded Resource
                var assembly = System.Reflection.Assembly.GetExecutingAssembly();
                using (Stream? stream = assembly.GetManifestResourceStream("HexDecTC.opcodes.json"))
                {
                    if (stream != null)
                    {
                        using (StreamReader reader = new StreamReader(stream))
                        {
                            string jsonContent = reader.ReadToEnd();
                            _opcodesData = JsonSerializer.Deserialize<Dictionary<string, Dictionary<string, string>>>(jsonContent) ?? new();
                            UpdateOpcodeList();
                        }
                    }
                    else
                    {
                        MessageBox.Show("Nem található az opcodes.json erőforrás.");
                    }
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Hiba az opcode-ok betöltésekor: {ex.Message}");
            }
        }

        private void UpdateOpcodeList()
        {
            if (OpcodeList == null) return;

            string version = (VersionCombo?.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "12.0";
            string searchText = OpcodeSearch?.Text?.Trim().ToLower() ?? "";

            OpcodeList.Items.Clear();

            if (_opcodesData.TryGetValue(version, out var opcodes))
            {
                var uniqueNames = new SortedSet<string>();
                foreach (var kvp in opcodes)
                {
                    string name = kvp.Value;
                    if (string.IsNullOrEmpty(searchText) || name.ToLower().Contains(searchText))
                    {
                        uniqueNames.Add(name);
                    }
                }

                foreach (var name in uniqueNames)
                {
                    OpcodeList.Items.Add(name);
                }
            }
        }

        private void OpcodeSearch_TextChanged(object sender, TextChangedEventArgs e)
        {
            UpdateOpcodeList();
        }

        private void OpcodeList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (_isUpdating) return;
            if (OpcodeList.SelectedItem is string selectedName)
            {
                string version = (VersionCombo.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "12.0";
                if (_opcodesData.TryGetValue(version, out var opcodes))
                {
                    // Find the hex value for this name
                    foreach (var kvp in opcodes)
                    {
                        if (kvp.Value == selectedName && kvp.Key.StartsWith("0x"))
                        {
                            _isUpdating = true;
                            HexInput.Text = kvp.Key;
                            
                            // Trigger hex to dec conversion
                            if (long.TryParse(kvp.Key.Substring(2), System.Globalization.NumberStyles.HexNumber, null, out long val))
                            {
                                DecInput.Text = val.ToString();
                                ShowOpcode(selectedName);
                            }
                            _isUpdating = false;
                            break;
                        }
                    }
                }
            }
        }

        private void SetupTray()
        {
            _notifyIcon = new Forms.NotifyIcon();
            
            try
            {
                // Use Process.MainModule.FileName instead of Assembly.Location for single-file apps
                string exePath = System.Diagnostics.Process.GetCurrentProcess().MainModule?.FileName ?? "";
                if (!string.IsNullOrEmpty(exePath))
                {
                    _notifyIcon.Icon = System.Drawing.Icon.ExtractAssociatedIcon(exePath);
                }
            }
            catch
            {
                try
                {
                    // Fallback to embedded resource stream
                    var iconStream = Application.GetResourceStream(new Uri("pack://application:,,,/app.ico"))?.Stream;
                    if (iconStream != null)
                    {
                        _notifyIcon.Icon = new System.Drawing.Icon(iconStream);
                    }
                }
                catch
                {
                    _notifyIcon.Icon = System.Drawing.SystemIcons.Application;
                }
            }

            _notifyIcon.Visible = true;
            _notifyIcon.Text = "HexDec TC";
            _notifyIcon.Click += (s, e) => ShowWindow();
            
            var contextMenu = new Forms.ContextMenuStrip();
            contextMenu.Items.Add("Show", null, (s, e) => ShowWindow());
            contextMenu.Items.Add("Exit", null, (s, e) => ExitApp());
            _notifyIcon.ContextMenuStrip = contextMenu;
        }

        private void ShowWindow()
        {
            this.Show();
            this.WindowState = WindowState.Normal;
            this.Activate();
        }

        private void ExitApp()
        {
            _isExiting = true;
            _notifyIcon?.Dispose();
            Application.Current.Shutdown();
        }

        private void MainWindow_Closing(object? sender, System.ComponentModel.CancelEventArgs e)
        {
            if (!_isExiting && TrayOnCloseCheck.IsChecked == true)
            {
                e.Cancel = true;
                this.Hide();
                
                // Show notification
                _notifyIcon?.ShowBalloonTip(2000, "HexDec TC", "Az alkalmazás a tálcán továbbra is fut.", Forms.ToolTipIcon.Info);
            }
            else
            {
                _notifyIcon?.Dispose();
            }
        }

        private void HexInput_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (_isUpdating) return;
            _isUpdating = true;

            string hexText = HexInput.Text.Trim();
            if (string.IsNullOrEmpty(hexText))
            {
                DecInput.Text = "";
                HideOpcode();
            }
            else
            {
                try
                {
                    if (hexText.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
                        hexText = hexText.Substring(2);

                    if (long.TryParse(hexText, System.Globalization.NumberStyles.HexNumber, null, out long val))
                    {
                        DecInput.Text = val.ToString();
                        CheckOpcode(val);
                    }
                    else
                    {
                        HideOpcode();
                    }
                }
                catch
                {
                    HideOpcode();
                }
            }

            _isUpdating = false;
        }

        private void DecInput_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (_isUpdating) return;
            _isUpdating = true;

            string decText = DecInput.Text.Trim();
            if (string.IsNullOrEmpty(decText))
            {
                HexInput.Text = "";
                HideOpcode();
            }
            else
            {
                if (long.TryParse(decText, out long val))
                {
                    HexInput.Text = "0x" + val.ToString("X").ToUpper();
                    CheckOpcode(val);
                }
                else
                {
                    HideOpcode();
                }
            }

            _isUpdating = false;
        }

        private void CheckOpcode(long val)
        {
            string version = (VersionCombo.SelectedItem as ComboBoxItem)?.Content?.ToString() ?? "12.0";
            string hexStr = "0x" + val.ToString("X").ToUpper();
            
            if (_opcodesData.TryGetValue(version, out var opcodes) && opcodes.TryGetValue(hexStr, out var name))
            {
                // Find and select in list
                if (OpcodeList != null)
                {
                    foreach (var item in OpcodeList.Items)
                    {
                        if (item is string itemName && itemName == name)
                        {
                            OpcodeList.SelectedItem = item;
                            OpcodeList.ScrollIntoView(item);
                            FlashOpcodeList();
                            break;
                        }
                    }
                }
            }
            else
            {
                if (OpcodeList != null) OpcodeList.SelectedItem = null;
            }
        }

        private void FlashOpcodeList()
        {
            var animation = new ColorAnimation
            {
                From = Colors.White,
                To = (Color)ColorConverter.ConvertFromString("#3a7ebf"),
                Duration = TimeSpan.FromMilliseconds(200),
                AutoReverse = true,
                RepeatBehavior = new RepeatBehavior(2)
            };
            OpcodeList.BeginAnimation(System.Windows.Controls.ListBox.BorderBrushProperty, new SolidColorBrushAnimation(animation));
        }

        private void ShowOpcode(string name)
        {
            // Just ensure it's selected and scrolled into view
            if (OpcodeList != null)
            {
                foreach (var item in OpcodeList.Items)
                {
                    if (item is string itemName && itemName == name)
                    {
                        OpcodeList.SelectedItem = item;
                        OpcodeList.ScrollIntoView(item);
                        FlashOpcodeList();
                        break;
                    }
                }
            }
        }

        private void HideOpcode()
        {
            if (OpcodeList != null) OpcodeList.SelectedItem = null;
        }

        private void AlwaysOnTopCheck_Changed(object sender, RoutedEventArgs e)
        {
            this.Topmost = AlwaysOnTopCheck.IsChecked ?? false;
        }

        private void VersionCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            UpdateOpcodeList();
            if (DecInput != null && long.TryParse(DecInput.Text, out long val))
            {
                CheckOpcode(val);
            }
        }

        private void ClearAll_Click(object sender, RoutedEventArgs e)
        {
            HexInput.Text = "";
            DecInput.Text = "";
            HideOpcode();
        }

        private void CopyHex_Click(object sender, RoutedEventArgs e) => Clipboard.SetText(HexInput.Text);
        private void CopyDec_Click(object sender, RoutedEventArgs e) => Clipboard.SetText(DecInput.Text);
        
        private void CopyOpcode_Click(object sender, RoutedEventArgs e) 
        {
            if (OpcodeList.SelectedItem is string selectedName)
            {
                Clipboard.SetText(selectedName);
            }
        }

        private void PasteHex_Click(object sender, RoutedEventArgs e)
        {
            HexInput.Text = Clipboard.GetText();
        }

        private void PasteDec_Click(object sender, RoutedEventArgs e)
        {
            DecInput.Text = Clipboard.GetText();
        }
    }

    // Helper class for animating BorderBrush which is a Brush, not a Color
    public class SolidColorBrushAnimation : AnimationTimeline
    {
        private ColorAnimation _colorAnimation;
        public override Type TargetPropertyType => typeof(Brush);

        public SolidColorBrushAnimation(ColorAnimation colorAnimation)
        {
            _colorAnimation = colorAnimation;
        }

        public override object GetCurrentValue(object defaultOriginValue, object defaultDestinationValue, AnimationClock animationClock)
        {
            return new SolidColorBrush((Color)_colorAnimation.GetCurrentValue(Colors.Transparent, Colors.Transparent, animationClock));
        }

        protected override Freezable CreateInstanceCore() => new SolidColorBrushAnimation(_colorAnimation);
    }
}
