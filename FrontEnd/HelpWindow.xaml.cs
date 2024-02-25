using System;
using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Markup;

namespace MiSTerCast
{
    /// <summary>
    /// Interaction logic for HelpWindow.xaml
    /// </summary>
    public partial class HelpWindow : Window
    {
        public HelpWindow()
        {
            InitializeComponent();
            TextBlock textBlock = (TextBlock)XamlReader.Parse(
                "<TextBlock xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xml:space=\"preserve\" TextWrapping=\"Wrap\"  Margin=\"10 10 10 10\">" +
                File.ReadAllText("README.txt") + "</TextBlock>");

            foreach(Inline inline in textBlock.Inlines)
            {
                Hyperlink link = inline as Hyperlink;
                if (link != null)
                {
                    link.Click += Link_Click;
                }
            }
            InfoScrollView.Content = textBlock;
        }

        private void Link_Click(object sender, RoutedEventArgs e)
        {
            Hyperlink hl = sender as Hyperlink;
            Uri uri = new Uri(((Run)hl.Inlines.FirstInline).Text);
            Process.Start(new ProcessStartInfo(uri.AbsoluteUri));
            e.Handled = true;
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            e.Cancel = true;
            this.Hide();
        }
    }
}
