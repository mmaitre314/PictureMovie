#pragma once

namespace Extensions
{
    public ref class PictureWriter sealed
    {
    public:

        PictureWriter(
            Windows::Storage::Streams::IRandomAccessStream^ outputStream, 
            unsigned int width, 
            unsigned int height, 
            Windows::Foundation::TimeSpan duration
            );

        // IClosable
        virtual ~PictureWriter();

        void AddFrame(const Platform::Array<uint8>^ buffer);

    private:

        unsigned int _width;
        unsigned int  _height;
        LONGLONG _duration;
        LONGLONG _hnsSampleTime;
        DWORD _streamIndex;
        Microsoft::WRL::ComPtr<IMFSinkWriter> _spSinkWriter;

        AutoMF _autoMF;
    };
}