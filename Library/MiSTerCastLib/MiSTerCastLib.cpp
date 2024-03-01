#include "pch.h"
#include "MiSTerCastLib.h"
#include "AudioCapture.h"
#include "VideoCapture.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Winmm.lib")

log_function logFunction = nullptr;
void LogMessage(std::string message, bool error)
{
    if (logFunction != nullptr)
        logFunction(message.c_str(), error);
}

std::atomic_bool stopCapture = false;
std::atomic_bool stopStream = false;
std::string targetIpString;

#include "renderer_nogpu.h"

std::atomic_bool capturing_screen = false;
void capture_screen()
{
    LogMessage("Screen capture starting.");
    capturing_screen = true;
    do
    {
        TickVideoCapture();
    } while (!stopCapture);
    capturing_screen = false;
    LogMessage("Screen capture stopped.");
}

std::atomic_bool casting_screen = false;
void cast_screen()
{
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
    {
        LogMessage("Setting cast screen thread priority failed: " + std::to_string(GetLastError()), true);
    }

    if (source_config.audio)
    {
        LogMessage("Audio capture starting.");
        StartAudioCapture();
    }

    LogMessage("Casting to MiSTer starting.");
    casting_screen = true;
    {
        auto renderer = std::make_unique<renderer_nogpu>(targetIpString);
        {
            do
            {
                renderer->draw(0);
            } while (!stopStream);
        }
    }
    casting_screen = false;
    LogMessage("Casting to MiSTer stopped.");

    if (source_config.audio)
    {
        StopAudioCapture();
        LogMessage("Audio capture stopped.");
    }
}

bool initialized = false;
std::unique_ptr<std::thread> captureScreenTask;
MISTERCASTLIB_API bool Initialize(log_function fnLog, capture_image_function fnCapture)
{
    if (initialized)
    {
        LogMessage("MiSTerCast is already initialized.", true);
        return true;
    }

    logFunction = fnLog;
    LogMessage("Initializing MiSTerCast");

    source_config.syncrefresh = true;
    source_config.framedelay = 0;
    source_config.useTransmitExtension = true;

    selected_modeline.pclock = 6.700;
    selected_modeline.hactive = 320;
    selected_modeline.hbegin = 336;
    selected_modeline.hend = 367;
    selected_modeline.htotal = 426;
    selected_modeline.vactive = 240;
    selected_modeline.vbegin = 244;
    selected_modeline.vend = 247;
    selected_modeline.vtotal = 262;
    selected_modeline.interlace = 0;
    

    if (!InitializeVideoCapture(0, fnCapture))
    {
        LogMessage("Failed to initialize video capture.", true);
        return false;
    }

    if (!InitAudioCapture())
    {
        LogMessage("Failed to initialize audio capture.", true);
        return false;
    }

    // Fill the buffers to be safe
    for (int i = 0; i < BUFFER_COUNT; i++)
        TickVideoCapture();

    captureScreenTask = std::make_unique<std::thread>(capture_screen);

    LogMessage("MiSTerCast ready.");

    initialized = true;
    return true;
}

MISTERCASTLIB_API bool Shutdown()
{
    stopCapture = true;
    do {} while (capturing_screen); // wait for threads
    stopCapture = false;

    captureScreenTask->detach();

    return true;
}

std::unique_ptr<std::thread> castScreenTask;

MISTERCASTLIB_API bool StartStream(const char* targetIp)
{
    LogMessage("Starting stream.");
    targetIpString = std::string(targetIp);
    castScreenTask = std::make_unique<std::thread>(cast_screen);

    return true;
}

MISTERCASTLIB_API bool StopStream()
{
    stopStream = true;
    do {} while (casting_screen); // wait for threads
    stopStream = false;

    castScreenTask->detach();
    return true;
}

MISTERCASTLIB_API bool SetModeline(
    double pclock,
    UINT16 hactive,
    UINT16 hbegin,
    UINT16 hend,
    UINT16 htotal,
    UINT16 vactive,
    UINT16 vbegin,
    UINT16 vend,
    UINT16 vtotal,
    bool interlace)
{
    LogMessage("SetModeline called");
    selected_modeline.pclock = pclock;
    selected_modeline.hactive = hactive;
    selected_modeline.hbegin = hbegin;
    selected_modeline.hend = hend;
    selected_modeline.htotal = htotal;
    selected_modeline.vactive = vactive;
    selected_modeline.vbegin = vbegin;
    selected_modeline.vend = vend;
    selected_modeline.vtotal = vtotal;
    selected_modeline.interlace = interlace;

    shouldUpdateVideoMode = true;

    return true;
}

MISTERCASTLIB_API bool SetSource(
    UINT8 display,
    bool audio,
    bool preview,
    UINT8 alignment,
    UINT8 cropmode,
    UINT16 xcrop,
    UINT16 ycrop,
    INT16 xoffset,
    INT16 yoffset,
    UINT8 rotation)
{
    source_config.display = display;
    source_config.audio = audio;
    source_config.preview = preview;
    source_config.alignment = (Alignment)alignment;
    source_config.cropmode = (CropMode)cropmode;
    source_config.width = xcrop;
    source_config.height = ycrop;
    source_config.xoffset = xoffset;
    source_config.yoffset = yoffset;
    source_config.rotation = rotation;

    switch (cropmode)
    {
    case CropMode::X1:
        source_config.width = selected_modeline.hactive;
        source_config.height = selected_modeline.vactive;
        break;
    case CropMode::X2:
        source_config.width = selected_modeline.hactive * 2;
        source_config.height = selected_modeline.vactive * 2;
        break;
    case CropMode::X3:
        source_config.width = selected_modeline.hactive * 3;
        source_config.height = selected_modeline.vactive * 3;
        break;
    case CropMode::X4:
        source_config.width = selected_modeline.hactive * 4;
        source_config.height = selected_modeline.vactive * 4;
        break;
    case CropMode::X5:
        source_config.width = selected_modeline.hactive * 5;
        source_config.height = selected_modeline.vactive * 5;
        break;
    default:
        break;
    }

    if (displayIndex != source_config.display)
    {
        stopCapture = true;
        do {} while (capturing_screen); // wait for threads
        stopCapture = false;

        captureScreenTask->detach();
        CleanupVideoCapture();
        InitializeVideoCapture(source_config.display, captureFunction);
        captureScreenTask = std::make_unique<std::thread>(capture_screen);
    }

    SetSourceOptions(&source_config);

    return true;
}
