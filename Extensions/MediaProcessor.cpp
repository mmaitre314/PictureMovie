#include "pch.h"
#include <windows.ui.xaml.media.dxinterop.h>
#include "MediaProcessor.h"

using namespace Microsoft::WRL;
using namespace concurrency;
using namespace Extensions;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Storage::Streams;

// some parameters
const unsigned int WIDTH = 640;   
const unsigned int HEIGHT = 480;   
const unsigned int RATE_NUM = 30000;   
const unsigned int RATE_DENOM = 1000;   
const unsigned int BITRATE = 3000000;   
const unsigned int ASPECT_NUM = 1;   
const unsigned int ASPECT_DENOM = 1;   
const long long ONE_SECOND = 10000000;      // In hundred-nano-seconds
const long long DURATION = 15 * ONE_SECOND; // In hundred-nano-seconds

MediaProcessor::MediaProcessor(void)
{
    //
    // Create the DX Device
    //

    static const D3D_FEATURE_LEVEL levels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    D3D_FEATURE_LEVEL FeatureLevel;
    CHK(D3D11CreateDevice(nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &_spDevice,
        &FeatureLevel,
        nullptr
        ));
}

IAsyncActionWithProgress<double>^ MediaProcessor::ProcessAsync(IRandomAccessStream^ inputStream, IRandomAccessStream^ outputStream)
{ 
    return create_async([this, inputStream, outputStream] (progress_reporter<double> reporter, cancellation_token token) {

        //   
        // Create the media type shared between Source Reader output and Sink Writer input
        //   

        ComPtr<IMFMediaType> spTypeShared;
        CHK(MFCreateMediaType(&spTypeShared));   
        CHK(spTypeShared->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));   
        CHK(spTypeShared->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32));     
        CHK(spTypeShared->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
        CHK(MFSetAttributeSize(spTypeShared.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT) );   
        CHK(MFSetAttributeRatio(spTypeShared.Get(), MF_MT_FRAME_RATE, RATE_NUM, RATE_DENOM));   
        CHK(MFSetAttributeRatio(spTypeShared.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));   

        //   
        // Create the Sink Writer output media type   
        //   

        ComPtr<IMFMediaType> spTypeOut;  
        CHK(MFCreateMediaType(&spTypeOut));   
        CHK(spTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));     
        CHK(spTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));   
        CHK(spTypeOut->SetUINT32(MF_MT_AVG_BITRATE, BITRATE));   
        CHK(spTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
        CHK(MFSetAttributeSize(spTypeOut.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT));   
        CHK(MFSetAttributeRatio(spTypeOut.Get(), MF_MT_FRAME_RATE, RATE_NUM, RATE_DENOM));
        CHK(MFSetAttributeRatio(spTypeOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));

        ComPtr<ID3D10Multithread> spMultithread;
        CHK(_spDevice.As(&spMultithread));
        spMultithread->SetMultithreadProtected(true);

        //
        // Create MF DX Device Manager
        //

        unsigned int resetToken;
        ComPtr<IMFDXGIDeviceManager> spDeviceManager;
        CHK(MFCreateDXGIDeviceManager(&resetToken, &spDeviceManager));

        CHK(spDeviceManager->ResetDevice(_spDevice.Get(), resetToken));

        //
        // Create MF DX Sample Allocator
        //

        ComPtr<IMFVideoSampleAllocatorEx> spAllocator;
        CHK(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&spAllocator)));
        CHK(spAllocator->SetDirectXManager(spDeviceManager.Get()));

        ComPtr<IMFAttributes> spAllocatorAttributes;
        CHK(MFCreateAttributes(&spAllocatorAttributes, 3));
        CHK(spAllocatorAttributes->SetUINT32(MF_SA_D3D11_USAGE, D3D11_USAGE_DEFAULT));
        CHK(spAllocatorAttributes->SetUINT32(MF_SA_D3D11_BINDFLAGS, D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE));

        CHK(spAllocator->InitializeSampleAllocatorEx(2, 20, spAllocatorAttributes.Get(), spTypeShared.Get()));

        //
        // Create the Source Reader
        //

        ComPtr<IMFByteStream> spInputByteStream;
        CHK(MFCreateMFByteStreamOnStreamEx((IUnknown*)inputStream, &spInputByteStream));

        ComPtr<IMFAttributes> spReaderAttributes;
        CHK(MFCreateAttributes(&spReaderAttributes, 10));
        CHK(spReaderAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, spDeviceManager.Get()));
        CHK(spReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, true));
        CHK(spReaderAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_TRANSCODE_ONLY_TRANSFORMS, true));
        CHK(spReaderAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true));
        CHK(spReaderAttributes->SetUINT32(MF_READWRITE_D3D_OPTIONAL, false));
        CHK(spReaderAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, false));

        ComPtr<IMFSourceReader> spSourceReader;
        CHK(MFCreateSourceReaderFromByteStream(spInputByteStream.Get(), spReaderAttributes.Get(), &spSourceReader));

        // Only read the first video stream
        CHK(spSourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, false));
        CHK(spSourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, true));

        CHK(spSourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, spTypeShared.Get()));

        //
        // Create the Sink Writer
        //
        
        ComPtr<IMFByteStream> spOutputByteStream;
        CHK(MFCreateMFByteStreamOnStreamEx((IUnknown*)outputStream, &spOutputByteStream));

        ComPtr<IMFAttributes> spWriterAttributes;
        CHK(MFCreateAttributes(&spWriterAttributes, 10));
        CHK(spWriterAttributes->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, spDeviceManager.Get()));
        CHK(spWriterAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true));

        ComPtr<IMFSinkWriter> spSinkWriter;
        CHK(MFCreateSinkWriterFromURL(L".mp4", spOutputByteStream.Get(), spWriterAttributes.Get(), &spSinkWriter));

        DWORD streamIndex;
        CHK(spSinkWriter->AddStream(spTypeOut.Get(), &streamIndex));   
     
        CHK(spSinkWriter->SetInputMediaType(streamIndex, spTypeShared.Get(), nullptr));   
     
        //   
        // Write some data   
        //   

        CHK(spSinkWriter->BeginWriting());

        ComPtr<IMFSample> spSample;
        DWORD flags = 0;
        double progress = 0.;
        long long time = 0;
        while (!token.is_canceled() && (time < DURATION) && !(flags & MF_SOURCE_READERF_ENDOFSTREAM))
        {
            // Notify caller of progress
            double newProgress = 100. * (double)time / (double)DURATION;
            if (newProgress - progress >= 1.)
            {
                _preview.Set(spSample.Get());

                progress = newProgress;
                reporter.report(progress);
            }

            // Read a frame
            DWORD sampleStreamIndex;
            CHK(spSourceReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                &sampleStreamIndex,
                &flags,
                &time,
                &spSample
                ));

            if (flags & MF_SOURCE_READERF_ERROR)
            {
                CHK(E_FAIL);
            }

            if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
            {
                // Check that nothing we care about has changed
                ComPtr<IMFMediaType> spTypeNew;
                CHK(spSourceReader->GetCurrentMediaType(sampleStreamIndex, &spTypeNew));

                BOOL match;
                CHK(spTypeShared->Compare(spTypeNew.Get(), MF_ATTRIBUTES_MATCH_INTERSECTION, &match));
                if (!match)
                {
                    CHK(MF_E_UNSUPPORTED_FORMAT);
                }
            }

            if (spSample == nullptr)
            {
                continue;
            }

            // Write a frame
            CHK(spSinkWriter->WriteSample(streamIndex, spSample.Get()));
        }

        if (!token.is_canceled())
        {
            CHK(spSinkWriter->Finalize());   

            reporter.report(100.);
        }

        _preview.Set(nullptr);
    });
}

SurfaceImageSource^ MediaProcessor::GetPreview()
{
    ComPtr<IMFSample> spSample;
    _preview.Get(&spSample);

    if (spSample == nullptr)
    {
        return nullptr;
    }

    //
    // Get the DX Texture from the MF Sample
    //

    ComPtr<IMFMediaBuffer> spBufferSrc;
    ComPtr<IMFDXGIBuffer> spDXGIBufferSrc;
    ComPtr<ID3D11Texture2D> spTexSrc;
    CHK(spSample->ConvertToContiguousBuffer(&spBufferSrc));
    CHK(spBufferSrc.As(&spDXGIBufferSrc));
    CHK(spDXGIBufferSrc->GetResource(IID_PPV_ARGS(&spTexSrc)));

    //
    // Create the image
    //

    ComPtr<ISurfaceImageSourceNative> spNativeImage;
    SurfaceImageSource^ image = ref new SurfaceImageSource(WIDTH, HEIGHT);
    CHK(((IUnknown*)image)->QueryInterface(IID_PPV_ARGS(&spNativeImage)));

    ComPtr<IDXGIDevice> spDevice;
    CHK(_spDevice.As(&spDevice));
    CHK(spNativeImage->SetDevice(spDevice.Get()));

    //
    // Draw
    //

    RECT updateRect = {0, 0, WIDTH, HEIGHT};
    POINT offset;
    ComPtr<IDXGISurface> spSurfaceDst;
    ComPtr<ID3D11Texture2D> spTexDst;
    CHK(spNativeImage->BeginDraw(updateRect, &spSurfaceDst, &offset));
    CHK(spSurfaceDst.As(&spTexDst));

    ComPtr<ID3D11DeviceContext> spContext;
    _spDevice->GetImmediateContext(&spContext);
    spContext->CopyResource(spTexDst.Get(), spTexSrc.Get());

    CHK(spNativeImage->EndDraw());

    return image;
}