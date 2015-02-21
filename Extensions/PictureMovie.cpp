#include "pch.h"
#include "PictureMovie.h"

using namespace Microsoft::WRL;
using namespace Extensions;
using namespace Platform;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;

const unsigned int RATE_NUM = 1;   
const unsigned int RATE_DENOM = 1;   
const unsigned int BITRATE = 3000000;   
const unsigned int ASPECT_NUM = 1;   
const unsigned int ASPECT_DENOM = 1;   

PictureWriter::PictureWriter(IRandomAccessStream^ outputStream, unsigned int width, unsigned int height, TimeSpan duration)
    : _width(width)
    , _height(height)
    , _duration(duration.Duration)
    , _hnsSampleTime(0)
    , _streamIndex(0)
{
    ComPtr<IMFByteStream> spByteStream;
    CHK(MFCreateMFByteStreamOnStreamEx((IUnknown*)outputStream, &spByteStream));

    // Create the Sink Writer

    ComPtr<IMFAttributes> spAttr;
    CHK(MFCreateAttributes(&spAttr, 10));
    CHK(spAttr->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, true));

    CHK(MFCreateSinkWriterFromURL(L".mp4", spByteStream.Get(), spAttr.Get(), &_spSinkWriter));

    // Setup the output media type   

    ComPtr<IMFMediaType> spTypeOut;  
    CHK(MFCreateMediaType(&spTypeOut));   
    CHK(spTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));     
    CHK(spTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264));   
    CHK(spTypeOut->SetUINT32(MF_MT_AVG_BITRATE, BITRATE));   
    CHK(spTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
    CHK(MFSetAttributeSize(spTypeOut.Get(), MF_MT_FRAME_SIZE, _width, _height));   
    CHK(MFSetAttributeRatio(spTypeOut.Get(), MF_MT_FRAME_RATE, MFCLOCK_FREQUENCY_HNS, (UINT32)_duration));
    CHK(MFSetAttributeRatio(spTypeOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));

    CHK(_spSinkWriter->AddStream(spTypeOut.Get(), &_streamIndex));   
     
    // Setup the input media type   

    ComPtr<IMFMediaType> spTypeIn;
    CHK(MFCreateMediaType(&spTypeIn));   
    CHK(spTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));   
    CHK(spTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32));     
    CHK(spTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));   
    CHK(MFSetAttributeSize(spTypeIn.Get(), MF_MT_FRAME_SIZE, _width, _height));   
    CHK(MFSetAttributeRatio(spTypeIn.Get(), MF_MT_FRAME_RATE, RATE_NUM, RATE_DENOM));   
    CHK(MFSetAttributeRatio(spTypeIn.Get(), MF_MT_PIXEL_ASPECT_RATIO, ASPECT_NUM, ASPECT_DENOM));   

    CHK(_spSinkWriter->SetInputMediaType(_streamIndex, spTypeIn.Get(), nullptr));   

    CHK(_spSinkWriter->BeginWriting());   
}

PictureWriter::~PictureWriter()
{
    if (_spSinkWriter != nullptr)
    {
        (void)_spSinkWriter->Finalize();
        _spSinkWriter = nullptr; // Release before shutting down MF
    }
}

void PictureWriter::AddFrame(const Platform::Array<uint8>^ buffer)
{
    // Create a media sample   
    ComPtr<IMFSample> spSample;   
    CHK(MFCreateSample(&spSample));   
    CHK(spSample->SetSampleDuration(_duration));
    CHK(spSample->SetSampleTime(_hnsSampleTime));
    _hnsSampleTime += _duration;

    // Add a media buffer
    ComPtr<IMFMediaBuffer> spBuffer;
    ComPtr<IMF2DBuffer> sp2DBuffer;
    CHK(MFCreate2DMediaBuffer(_width, _height, MFVideoFormat_ARGB32.Data1, /*fBottomUp*/ false, &spBuffer));
    CHK(spBuffer.As(&sp2DBuffer));
    CHK(sp2DBuffer->ContiguousCopyFrom(buffer->begin(), buffer->Length));
    CHK(spBuffer->SetCurrentLength(buffer->Length));
    CHK(spSample->AddBuffer(spBuffer.Get()));

    // Write the media sample
    CHK(_spSinkWriter->WriteSample(_streamIndex, spSample.Get()));
}
