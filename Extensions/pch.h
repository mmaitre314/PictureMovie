//
// pch.h
// Header for standard system include files.
//

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <wrl.h>
#include <wrl\wrappers\corewrappers.h>
#include <ppltasks.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>  
#include <Mferror.h>
#include <d3d11_1.h>
#include <dxgi1_2.h> 

#define CHK(statement)	{HRESULT _hr = (statement); if (FAILED(_hr)) { throw ref new Platform::COMException(_hr); };}
#define CHKNULL(p)	{if ((p) == nullptr) { throw ref new Platform::NullReferenceException(); };}

// Class to start and shutdown Media Foundation
class AutoMF
{
public:
    AutoMF()
        : _bInitialized(false)
    {
        CHK(MFStartup(MF_VERSION));
        _bInitialized = true;
    }

    ~AutoMF()
    {
        if (_bInitialized)
        {
            (void) MFShutdown();
        }
    }

private:
    bool _bInitialized;
};
