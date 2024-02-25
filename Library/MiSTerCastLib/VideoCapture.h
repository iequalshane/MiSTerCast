#pragma once

#define BUFFER_COUNT 3


struct Bitmap {
    int                  width = 0;
    int                  height = 0;
    std::vector<uint8_t> buffer;
};

SourceOptions source_config = {};
std::atomic_uint lastVideoCaptureIndex = 0;
Bitmap* videoCaptures = nullptr;
int    displayIndex = 0;
ID3D11Device*           d3dDevice = nullptr;
ID3D11DeviceContext*    d3dDeviceContext = nullptr;
IDXGIOutputDuplication* desktopDuplication = nullptr;
bool                    haveFrameLock = false;
capture_image_function  captureFunction;
std::atomic_bool        hasNewSourceOptions;
SourceOptions           currentSourceOptions;
SourceOptions           newSourceOptions;

bool InitializeVideoCapture(int outputNumber, capture_image_function fnCapture)
{
    displayIndex = outputNumber;
    captureFunction = fnCapture;
    currentSourceOptions = {};
    currentSourceOptions.display = 0;
    currentSourceOptions.framedelay = 0;
    currentSourceOptions.alignment = Alignment::Center;
    currentSourceOptions.audio = 1;
    currentSourceOptions.syncrefresh = true;
    currentSourceOptions.cropmode = CropMode::Full43;

    if (videoCaptures == nullptr)
    {
        videoCaptures = new Bitmap[BUFFER_COUNT];
        for (int i = 0; i < BUFFER_COUNT; i++)
            videoCaptures[i] = Bitmap();
    }

    HDESK hDesk = OpenInputDesktop(0, FALSE, GENERIC_ALL);
    if (!hDesk)
    {
        LogMessage("Failed to open desktop", true);
        return false;
    }

    // Attach desktop to this thread
    // Is this required? Should we do this on the capture thread?
    SetThreadDesktop(hDesk);
    CloseDesktop(hDesk);
    hDesk = nullptr;

    HRESULT hr = S_OK;

    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    auto numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_1 };
    auto numFeatureLevels = ARRAYSIZE(featureLevels);

    D3D_FEATURE_LEVEL featureLevel;
    for (size_t i = 0; i < numDriverTypes; i++) {
        hr = D3D11CreateDevice(nullptr, driverTypes[i], nullptr, 0, featureLevels, (UINT)numFeatureLevels,
            D3D11_SDK_VERSION, &d3dDevice, &featureLevel, &d3dDeviceContext);
        if (SUCCEEDED(hr))
            break;
    }

    EXIT_ON_ERROR(hr, "D3D11CreateDevice failed");

    IDXGIDevice* dxgiDevice = nullptr;
    hr = d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    EXIT_ON_ERROR(hr, "D3DDevice->QueryInterface failed");

    IDXGIAdapter* dxgiAdapter = nullptr;
    hr = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
    dxgiDevice->Release();
    dxgiDevice = nullptr;
    EXIT_ON_ERROR(hr, "DxgiDevice->GetParent failed");

    IDXGIOutput* dxgiOutput = nullptr;
    hr = dxgiAdapter->EnumOutputs(displayIndex, &dxgiOutput);
    dxgiAdapter->Release();
    dxgiAdapter = nullptr;
    EXIT_ON_ERROR(hr, "DxgiAdapter->EnumOutputs faile");

    // DXGI_OUTPUT_DESC        outputDesc;
    // hr = dxgiOutput->GetDesc(&outputDesc);
    // EXIT_ON_ERROR(hr, "DxgiOutput->GetDesc faile");

    IDXGIOutput1* dxgiOutput1 = nullptr;
    hr = dxgiOutput->QueryInterface(__uuidof(dxgiOutput1), (void**)&dxgiOutput1);
    dxgiOutput->Release();
    dxgiOutput = nullptr;
    EXIT_ON_ERROR(hr, "DxgiOutput->QueryInterface faile");

    hr = dxgiOutput1->DuplicateOutput(d3dDevice, &desktopDuplication);
    dxgiOutput1->Release();
    dxgiOutput1 = nullptr;
    EXIT_ON_ERROR(hr, "DxgiOutput1->DuplicateOutput failed");

    return true;
}

void CleanupVideoCapture()
{
    SAFE_RELEASE(desktopDuplication);
    SAFE_RELEASE(d3dDeviceContext);
    SAFE_RELEASE(d3dDevice);
    haveFrameLock = false;
}

bool TickVideoCapture()
{
    if (!desktopDuplication)
        return false;

    HRESULT hr;

    if (hasNewSourceOptions)
    {
        currentSourceOptions = newSourceOptions;
        hasNewSourceOptions = false;
    }

    // Release right before acquiring next frame
    if (haveFrameLock) {
        haveFrameLock = false;
        hr = desktopDuplication->ReleaseFrame();
    }

    IDXGIResource*          deskRes = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    hr = desktopDuplication->AcquireNextFrame(32, &frameInfo, &deskRes);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        return false;

    if (FAILED(hr)) {
        // Try to reinitialize and capture next frame
        LogMessage("Acquire failed: " + std::to_string(hr), true);
        CleanupVideoCapture();
        InitializeVideoCapture(displayIndex, captureFunction);
        return false;
    }

    haveFrameLock = true;

    ID3D11Texture2D* gpuTex = nullptr;
    hr = deskRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&gpuTex);
    deskRes->Release();
    deskRes = nullptr;
    EXIT_ON_ERROR(hr, "Query Interface for ID3D11Texture2D failed");

    bool ok = true;

    unsigned int width;
    unsigned int height;
    switch (source_config.rotation)
    {
    case Rotation::CW90:
    case Rotation::CCW90:
        width = currentSourceOptions.height;
        height = currentSourceOptions.width;
        break;
    default:
        width = currentSourceOptions.width;
        height = currentSourceOptions.height;
        break;
    }

    D3D11_TEXTURE2D_DESC desc;
    gpuTex->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    switch (currentSourceOptions.cropmode)
    {
    case CropMode::Custom:
    case CropMode::X1:
    case CropMode::X2:
    case CropMode::X3:
    case CropMode::X4:
    case CropMode::X5:
        break;
    case CropMode::Full43:
        switch (source_config.rotation)
        {
        case Rotation::CW90:
        case Rotation::CCW90:
            width = desc.Height * 3 / 4;
            break;
        default:
            width = desc.Height * 4 / 3;
            break;
        }
        height = desc.Height;
        break;
    case CropMode::Full54:
        switch (source_config.rotation)
        {
        case Rotation::CW90:
        case Rotation::CCW90:
            width = desc.Height * 4 / 5;
            break;
        default:
            width = desc.Height * 5 / 4;
            break;
        }
        height = desc.Height;
        break;
    default:
        break;
    }

    if (width > desc.Width)
        width = desc.Width;
    if (height > desc.Height)
        height = desc.Height;

    int xoffset = currentSourceOptions.xoffset;
    int yoffset = currentSourceOptions.yoffset;
    switch (currentSourceOptions.alignment)
    {
    case Alignment::Center:
        xoffset += desc.Width / 2 - width / 2;
        yoffset += desc.Height / 2 - height / 2;
        break;
    case Alignment::TopLeft:
        break;
    case Alignment::Top:
        xoffset += desc.Width / 2 - width / 2;
        break;
    case Alignment::TopRight:
        xoffset += desc.Width - width;
        break;
    case Alignment::Right:
        xoffset += desc.Width - width;
        yoffset += desc.Height / 2 - height / 2;
        break;
    case Alignment::BottomRight:
        xoffset += desc.Width - width;
        yoffset += desc.Height - height;
        break;
    case Alignment::Bottom:
        xoffset += desc.Width / 2 - width / 2;
        yoffset += desc.Height - height;
        break;
    case Alignment::BottomLeft:
        yoffset += desc.Height - height;
        break;
    case Alignment::Left:
        yoffset += desc.Height / 2 - height / 2;
    default:
        break;
    }

    if (xoffset < 0)
        xoffset = 0;
    else if (xoffset + width > desc.Width)
        xoffset = desc.Width - width;

    if (yoffset < 0)
        yoffset = 0;
    else if (yoffset + height > desc.Height)
        yoffset = desc.Height - height;

    desc.Width = width;
    desc.Height = height;

    ID3D11Texture2D* cpuTex = nullptr;
    hr = d3dDevice->CreateTexture2D(&desc, nullptr, &cpuTex);
    EXIT_ON_ERROR(hr, "D3DDevice->CreateTexture2D failed");

    D3D11_BOX sourceRegion;
    sourceRegion.left = xoffset;
    sourceRegion.right = xoffset + width;
    sourceRegion.top = yoffset;
    sourceRegion.bottom = yoffset + height;
    sourceRegion.front = 0;
    sourceRegion.back = 1;

    d3dDeviceContext->CopySubresourceRegion(
        cpuTex,
        0, // sub resource
        0, //x
        0, //y
        0, //z
        gpuTex,
        0, // sub resource
        &sourceRegion);

    unsigned int nextIndex = (lastVideoCaptureIndex + 1) % BUFFER_COUNT;
    D3D11_MAPPED_SUBRESOURCE sr;
    hr = d3dDeviceContext->Map(cpuTex, 0, D3D11_MAP_READ, 0, &sr);
    EXIT_ON_ERROR(hr, "D3DDeviceContext->Map failed");

    if (videoCaptures[nextIndex].width != width || videoCaptures[nextIndex].height != height)
    {
        videoCaptures[nextIndex].width = width;
        videoCaptures[nextIndex].height = height;
        videoCaptures[nextIndex].buffer.resize(width * height * 4);
    }

    for (int y = 0; y < (int)height; y++) // TODO: Can this be improved?
        memcpy(videoCaptures[nextIndex].buffer.data() + y * width * 4, (uint8_t*)sr.pData + sr.RowPitch * y, width * 4);
    d3dDeviceContext->Unmap(cpuTex, 0);

    if (currentSourceOptions.preview)
        captureFunction(width, height, videoCaptures[nextIndex].buffer.data());

    lastVideoCaptureIndex = nextIndex;

    cpuTex->Release();
    gpuTex->Release();

    return ok;
}

void SetSourceOptions(const SourceOptions* sourceOptions)
{
    newSourceOptions = *sourceOptions;
    hasNewSourceOptions = true;
}