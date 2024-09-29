#pragma once

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

// Buffers
#define AUDIO_BUFFER_SIZE 10000000
unsigned int AudioWritePos = 0;
std::atomic_int audioSampleRate;
uint16_t* audioBuffer = nullptr;

// Audio Capture
REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
UINT32 bufferFrameCount;
UINT32 numFramesAvailable;
IMMDeviceEnumerator *pEnumerator = NULL;
IMMDevice *pDevice = NULL;
IAudioClient *pAudioClient = NULL;
IAudioCaptureClient *pCaptureClient = NULL;
WAVEFORMATEX *pwfx = NULL;

bool InitAudioCapture()
{
    HRESULT hr;

    hr = CoInitialize(nullptr);
    EXIT_ON_ERROR(hr, "CoInitialize failed");

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);
    EXIT_ON_ERROR(hr, "CoCreateInstance of MMDeviceEnumerator failed");

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    EXIT_ON_ERROR(hr, "IMMDeviceEnumerator GetDefaultAudioEndpoint failed");

    hr = pDevice->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL,
        NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr, "IMMDevice Activate failed");

    hr = pAudioClient->GetMixFormat(&pwfx);
    audioSampleRate = pwfx->nSamplesPerSec;
    EXIT_ON_ERROR(hr, "IAudioClient GetMixFormat failed");

    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        hnsRequestedDuration,
        0,
        pwfx,
        NULL);
    EXIT_ON_ERROR(hr, "IAudioClient Initialize failed");

    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr, "IAudioClient  GetBufferSize failed");

    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
    EXIT_ON_ERROR(hr, "IAudioClient GetService failed");

    return true;
}

void CleanupAudioCatpure()
{
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(pAudioClient)
    SAFE_RELEASE(pCaptureClient)
}

bool StartAudioCapture()
{
    HRESULT hr = pAudioClient->Start();
    EXIT_ON_ERROR(hr, "IAudioClient Start failed");

    return true;
}

bool StopAudioCapture()
{

    HRESULT hr = pAudioClient->Stop();
    EXIT_ON_ERROR(hr, "IAudioCaptureClient Stop failed");

     return true;
}

bool TickAudioCapture()
{
    AudioWritePos = 0;
    UINT32 packetLength = 0;
    HRESULT hr = pCaptureClient->GetNextPacketSize(&packetLength);
    EXIT_ON_ERROR(hr, "IAudioCaptureClient GetNextPacketSize failed");

    while (packetLength != 0)
    {
        // Get the available data in the shared buffer.
        BYTE *pData;
        DWORD flags;
        hr = pCaptureClient->GetBuffer(
            &pData,
            &numFramesAvailable,
            &flags, NULL, NULL);
        EXIT_ON_ERROR(hr, "IAudioCaptureClient GetBuffer failed");

        if (!audioBuffer)
            return true;

        bool silence = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

        float* pDataFloat = (float*)pData;
        unsigned int lFloatsToWrite = numFramesAvailable * pwfx->nBlockAlign / sizeof(float);
        unsigned int dataPos = 0;
        while (dataPos < lFloatsToWrite)
        {
            LONG writeLength = std::min(AUDIO_BUFFER_SIZE - AudioWritePos, lFloatsToWrite - dataPos);
            if (silence)
            {
                ZeroMemory(&audioBuffer[AudioWritePos], writeLength * sizeof(audioBuffer[0]));
            }
            else
            {
                for (int i = 0; i < writeLength; i++)
                {
                    audioBuffer[AudioWritePos + i] = (uint16_t)(pDataFloat[dataPos + i] * 32767);
                }
            }

            dataPos += writeLength;
            AudioWritePos = (AudioWritePos + writeLength) % AUDIO_BUFFER_SIZE;
        }

        hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
        EXIT_ON_ERROR(hr, "IAudioCaptureClient ReleaseBuffer failed");

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        EXIT_ON_ERROR(hr, "IAudioCaptureClient GetNextPacketSize failed");
    }

    return true;
}