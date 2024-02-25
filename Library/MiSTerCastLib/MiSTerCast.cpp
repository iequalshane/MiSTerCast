#pragma once

#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <stdint.h>

#include <string>
#include <functional>
#include <vector>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <atomic>

#include "MiSTerCastLib.h"

#define BUFFER_COUNT 3

typedef std::string Error;


// BGRA U8 Bitmap
struct Bitmap {
    int                  Width = 0;
    int                  Height = 0;
    std::vector<uint8_t> Buf;
};

enum Alignment : int
{
    Center,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

enum CropMode : int
{
    Custom,
    X1,
    X2,
    X3,
    X4,
    X5,
    Full43,
    Full54,
};

struct SourceOptions {
    bool syncrefresh;
    int framedelay;
    int display;
    bool audio;
    bool preview;
    Alignment alignment;
    CropMode cropmode;
    int width;
    int height;
    int xoffset;
    int yoffset;
    int rotation;
};

// WinDesktopDup hides the gory details of capturing the screen using the
// Windows Desktop Duplication API
class WinDesktopDup {
public:
    std::atomic_uint LastCaptureIndex = 0;
    Bitmap Captures[BUFFER_COUNT];
    int    OutputNumber = 0;

    WinDesktopDup(int outputNumber, log_function fnLog, capture_image_function fnCapture);
    ~WinDesktopDup();

    Error Initialize();
    void  Close();
    bool  CaptureNext();
    void SetSourceOptions(const SourceOptions* sourceOptions);

private:
    void LogMessage(std::string message, bool error);

    ID3D11Device*           D3DDevice = nullptr;
    ID3D11DeviceContext*    D3DDeviceContext = nullptr;
    IDXGIOutputDuplication* DeskDupl = nullptr;
    DXGI_OUTPUT_DESC        OutputDesc;
    bool                    HaveFrameLock = false;
    log_function            logFunction;
    capture_image_function  captureFunction;
    std::atomic_bool        hasNewSourceOptions;
    SourceOptions           currentSourceOptions;
    SourceOptions           newSourceOptions;
};