using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using Windows.Storage;
using Windows.Storage.Pickers;
using Windows.Graphics.Imaging;
using Extensions;

namespace PictureMovie
{
    public sealed partial class MainPage : Page
    {
        private const uint _width = 1920;
        private const uint _height = 1080;

        public MainPage()
        {
            this.InitializeComponent();
        }

        private async void Button_Click(object sender, RoutedEventArgs e)
        {
            var picker = new FileOpenPicker();
            picker.ViewMode = PickerViewMode.Thumbnail;
            picker.SuggestedStartLocation = PickerLocationId.PicturesLibrary;
            picker.FileTypeFilter.Add(".jpg");
            picker.FileTypeFilter.Add(".png");
            picker.FileTypeFilter.Add(".bmp");
            picker.FileTypeFilter.Add(".gif");

            var imageFiles = await picker.PickMultipleFilesAsync();
            if (imageFiles.Count == 0)
            {
                TextLog.Text += "\nNo file selected";
                return;
            }

            TextLog.Text += "\nCreating video file";

            var file = await KnownFolders.VideosLibrary.CreateFileAsync("PictureMovie.mp4", CreationCollisionOption.GenerateUniqueName);
            using (var outputStream = await file.OpenAsync(FileAccessMode.ReadWrite))
            {
                TextLog.Text += String.Format("\nCreated {0}", file.Path);

                using (var writer = new PictureWriter(outputStream, _width, _height, TimeSpan.FromSeconds(1)))
                {
                    foreach (var imageFile in imageFiles)
                    {
                        TextLog.Text += String.Format("\nAdding {0}", imageFile.Path);

                        using (var inputStream = await imageFile.OpenAsync(FileAccessMode.Read))
                        {
                            var decoder = await BitmapDecoder.CreateAsync(inputStream);

                            var transform = new BitmapTransform();
                            transform.ScaledHeight = _height;
                            transform.ScaledWidth = _width;

                            var pixels = await decoder.GetPixelDataAsync(
                                BitmapPixelFormat.Bgra8,
                                BitmapAlphaMode.Ignore,
                                transform,
                                ExifOrientationMode.RespectExifOrientation,
                                ColorManagementMode.ColorManageToSRgb
                                );

                            writer.AddFrame(pixels.DetachPixelData());
                        }
                    }
                }

            }

            TextLog.Text += "\nDone";
        }
    }
}
