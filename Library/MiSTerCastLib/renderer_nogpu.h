// Groovy_MiSTer communication adapted from the Groovy_Mame source.
// See https://github.com/antonioginer/GroovyMAME for original source

// Original license:
// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Antonio Giner, Sergi Clara

// Modification by Shane Lynch

#pragma once

uint64_t CurrentTicks()
{
    // use the standard library clock function
    LARGE_INTEGER ticks;
    QueryPerformanceCounter(&ticks);
    return ticks.QuadPart;
}

uint64_t TicksPerSecond() noexcept
{
    LARGE_INTEGER val;
    QueryPerformanceFrequency(&val);
    return val.QuadPart;
}

void SleepTicks(uint64_t duration) noexcept
{
    std::this_thread::sleep_for(std::chrono::high_resolution_clock::duration(duration));
}

inline double get_ms(uint64_t ticks) { return (double)ticks / TicksPerSecond() * 1000; };

#define MAX_BUFFER_WIDTH 768
#define MAX_BUFFER_HEIGHT 576
#define VRAM_BUFFER_SIZE 65536
#define SEND_BIFFER_SIZE 2097152 //2 * 1024 * 1024
#define MAX_LZ4_BLOCK   61440
#define MAX_SAMPLE_RATE 48000
#define STREAMS_UPDATE_FREQUENCY 50 // sound.h

// nogpu UDP server
#define UDP_PORT 32100

// Server commands
#define CMD_CLOSE 1
#define CMD_INIT 2
#define CMD_SWITCHRES 3
#define CMD_AUDIO 4
#define CMD_GET_STATUS 5
#define CMD_BLIT_VSYNC 6

// Status bits
#define VRAM_READY       1 << 0
#define VRAM_END_FRAME   1 << 1
#define VRAM_SYNCED      1 << 2
#define VRAM_FRAMESKIP   1 << 3
#define VGA_VBLANK       1 << 4
#define VGA_FIELD        1 << 5
#define FPGA_AUDIO       1 << 6
#define VGA_QUEUE        1 << 7

#pragma pack(1)

typedef struct nogpu_modeline
{
    double    pclock;
    uint16_t  hactive;
    uint16_t  hbegin;
    uint16_t  hend;
    uint16_t  htotal;
    uint16_t  vactive;
    uint16_t  vbegin;
    uint16_t  vend;
    uint16_t  vtotal;
    bool  interlace;
} nogpu_modeline;

std::atomic_bool shouldUpdateVideoMode = false;
nogpu_modeline selected_modeline = {};

typedef struct nogpu_status
{
    uint16_t vcount;
    uint32_t frame_num;
} nogpu_status;

typedef struct nogpu_blit_status
{
    uint32_t frame_req;
    uint16_t vcount_req;
    uint32_t frame_gpu;
    uint16_t vcount_gpu;
    uint8_t bits;
} nogpu_blit_status;

typedef struct cmd_init
{
    const uint8_t cmd = CMD_INIT;
    uint8_t	compression;
    uint8_t sound_rate;
    uint8_t sound_channels;
} cmd_init;

typedef struct cmd_close
{
    const uint8_t cmd = CMD_CLOSE;
} cmd_close;

typedef struct cmd_switchres
{
    const uint8_t cmd = CMD_SWITCHRES;
    nogpu_modeline mode;
} cmd_switchres;

typedef struct cmd_audio
{
    const uint8_t cmd = CMD_AUDIO;
    uint16_t sample_bytes;
} cmd_blit;

typedef struct cmd_blit_vsync
{
    const uint8_t cmd = CMD_BLIT_VSYNC;
    uint32_t frame;
    uint16_t vsync;
    uint16_t block_size;
} cmd_blit_vsync;


typedef struct cmd_get_status
{
    const uint8_t cmd = CMD_GET_STATUS;
} cmd_get_status;

#pragma pack()

// renderer_nogpu is the information for the current screen
class renderer_nogpu
{
public:
    renderer_nogpu(std::string targetip)
        : m_bmdata(nullptr)
        , m_bmsize(0)
        , m_targetip(targetip)
    {
    }

    ~renderer_nogpu();
    int create();
    int draw(const int update);
    void save() {}
    void record() {}
    void toggle_fsfx() {}
    void add_audio_to_recording(const uint16_t *buffer, int samples_this_frame);

private:
    std::unique_ptr<uint8_t[]> m_bmdata;
    size_t                      m_bmsize;

    // npgpu private members
    bool m_initialized = false;
    bool m_first_blit = true;
    int m_compression = 0;
    bool m_show_window = false;
    bool m_is_internal_fe = false;
    bool m_autofilter = false;
    bool m_bilinear = false;
    int m_frame = 0;
    int m_field = 0;
    unsigned int m_width = 0;
    unsigned int m_height = 0;
    int m_vtotal = 0;
    int m_vsync_scanline = 0;
    bool m_sleep_allowed = false;
    double m_period = 16.666667;
    double m_line_period = 0.064;
    double m_frame_delay = 0.0;
    double m_fd_margin = 1.5;
    float m_aspect = 4.0f / 3.0f;
    float m_pixel_aspect = 1.0f;
    int m_sample_rate = MAX_SAMPLE_RATE;
    nogpu_status m_status;
    nogpu_blit_status m_blit_status;
    nogpu_modeline m_current_mode;

    uint64_t time_start = 0;
    uint64_t time_entry = 0;
    uint64_t time_blit = 0;
    uint64_t time_exit = 0;
    uint64_t time_frame[16];
    uint64_t time_frame_avg = 0;
    uint64_t time_frame_dm = 0;
    uint64_t time_sleep = uint64_t(TicksPerSecond() / 1000.0); // 1 ms

    int m_sockfd = -1; //INVALID_SOCKET;
    sockaddr_in m_server_addr;
    std::string m_targetip;

    char m_fb[MAX_BUFFER_HEIGHT * MAX_BUFFER_WIDTH * 3];
    char m_fb_compressed[MAX_BUFFER_HEIGHT * MAX_BUFFER_WIDTH * 3];
    char inp_buf[2][MAX_LZ4_BLOCK + 1];
    char m_ab[MAX_SAMPLE_RATE / STREAMS_UPDATE_FREQUENCY * 2 * 2];

    bool nogpu_init();
    bool nogpu_send_command(void *command, int command_size);
    bool nogpu_switch_video_mode();
    void nogpu_blit(uint32_t frame, uint16_t vsync, uint16_t line_width);
    void nogpu_send_mtu(char *buffer, int bytes_to_send, int chunk_max_size);
    void nogpu_send_lz4(char *buffer, int bytes_to_send, int block_size);
    int nogpu_compress(int id_compress, char *buffer_comp, const char *buffer_rgb, uint32_t buffer_size);
    bool nogpu_wait_ack(double timeout);
    bool nogpu_wait_status(nogpu_blit_status *status, double timeout);
    void nogpu_register_frametime(uint64_t frametime);

    // Transmit packets extension
    WSAOVERLAPPED overlapped;
    LPFN_TRANSMITPACKETS        transmitPackets;
    LPTRANSMIT_PACKETS_ELEMENT  transmitEl = new TRANSMIT_PACKETS_ELEMENT[256];
};

//============================================================
//  renderer_nogpu::create
//============================================================

int renderer_nogpu::create()
{
    return 0;
}

//============================================================
//  renderer_nogpu::~renderer_nogpu
//============================================================

renderer_nogpu::~renderer_nogpu()
{
    // Wait for fpga to flush last blit
    SleepTicks(uint64_t(m_period * time_sleep));

    LogMessage("Sending CMD_CLOSE...");
    cmd_close command;

    nogpu_send_command(&command, sizeof(command));
    LogMessage("Done.");
    closesocket(m_sockfd);
    WSACleanup();
}

//============================================================
//  renderer_nogpu::draw
//============================================================
int renderer_nogpu::draw(const int update)
{
    // Hack because these aren't intiailized...
    m_width = selected_modeline.hactive;
    m_height = selected_modeline.interlace ? selected_modeline.vactive / 2 : selected_modeline.vactive;

    // resize window if required
    static int old_width = 0;
    static int old_height = 0;
    if (old_width != m_width || old_height != m_height)
    {
        old_width = m_width;
        old_height = m_height;
    }

    // compute pitch of target
    unsigned int const pitch = (m_width + 3) & ~3;

    // make sure our temporary bitmap is big enough
    if ((pitch * m_height * 4) > m_bmsize)
    {
        m_bmsize = pitch * m_height * 4 * 2;
        m_bmdata.reset();
        m_bmdata = std::make_unique<uint8_t[]>(m_bmsize);
    }

    // initialize nogpu right before first blit
    if (m_first_blit && !m_initialized)
    {
        m_initialized = nogpu_init();
        if (m_initialized)
        {
            LogMessage("Done.");
            nogpu_switch_video_mode();
        }
        else
        {
            LogMessage("Failed.", true);
            m_first_blit = false;
        }
    }

    // only send frame if nogpu is initialized
    if (!m_initialized)
        return 0;

    // get current field for interlaced mode
    if (m_current_mode.interlace)
        m_field = (m_blit_status.bits & VGA_FIELD ? 1 : 0) ^ ((m_frame - m_blit_status.frame_gpu) % 2);

    unsigned int drawIndex = lastVideoCaptureIndex;
    int screenwidth = videoCaptures[drawIndex].width;
    int screenheight = videoCaptures[drawIndex].height;
    bool drawInt = (m_field != 0);

    int j = 0;
    
    float stepx;
    float stepy;
    float interlaceStepX = 0;
    float interlaceStepY = 0;
    switch (source_config.rotation)
    {
    case Rotation::CW90:
        stepx = ((float)(screenwidth) / (float)m_height);
        stepy = ((float)(screenheight) / (float)m_width);
        interlaceStepX = int(stepx / 2.0f);
    case Rotation::CCW90:
        stepx = ((float)(screenwidth) / (float)m_height);
        stepy = ((float)(screenheight) / (float)m_width);
        interlaceStepX = int(stepx / 2.0f);
        drawInt = !drawInt;
        break;
    case Rotation::Flip180:
        stepx = ((float)(screenwidth) / (float)m_width);
        stepy = ((float)(screenheight) / (float)m_height);
        interlaceStepY = int(stepy / 2.0f);
        drawInt = !drawInt;
    default:
        stepx = ((float)(screenwidth) / (float)m_width);
        stepy = ((float)(screenheight) / (float)m_height);
        interlaceStepY = int(stepy / 2.0f);
        break;
    }

    for (unsigned int i = 0; i < (pitch * m_height * 4); i += 4)
    {
        int x = (i / 4) % m_width;
        int y = (i / 4) / m_width;
        int tx = x;
        switch (source_config.rotation)
        {
        case Rotation::CW90:
            x = m_height - y - 1;
            y = tx;
            break;
        case Rotation::CCW90:
            x = y;
            y = m_width - tx - 1;
            break;
        case Rotation::Flip180:
            x = m_width - x - 1;
            y = m_height - y - 1;
            break;
        }

        int bmpx = int(x * stepx);
        int bmpy = int(y * stepy);
        if (m_current_mode.interlace && drawInt)
        {
            bmpx += interlaceStepX;
            bmpy += interlaceStepY;
        }
        int bmpi = (bmpy * screenwidth + bmpx) * 4;
        if (bmpi + 4 >= (screenheight * screenwidth * 4))
            continue;

        m_fb[j] =     (char)videoCaptures[drawIndex].buffer[bmpi];
        m_fb[j + 1] = (char)videoCaptures[drawIndex].buffer[bmpi + 1];
        m_fb[j + 2] = (char)videoCaptures[drawIndex].buffer[bmpi + 2];
        j += 3;
    }

    // change video mode right before the blit
    if (shouldUpdateVideoMode)
        nogpu_switch_video_mode();

    bool valid_status = false;

    time_entry = CurrentTicks();

    if (source_config.syncrefresh && m_first_blit)
    {
        time_start = time_entry;
        time_blit = time_entry;
        time_exit = time_entry;

        m_first_blit = false;
        m_frame = 1;

        // Skip blitting first frame, so we avoid glitches while MAME loads roms
        return 0;
    }

    // Blit now
    nogpu_blit(m_frame, m_width, m_height);

    // Capture and send audio
    if (source_config.audio)
    {
        TickAudioCapture();
        if (AudioWritePos > 0)
            add_audio_to_recording(audioBuffer, AudioWritePos);
    }

    time_blit = CurrentTicks();

    nogpu_register_frametime(time_entry - time_exit);

    // Wait raster position
    if (source_config.syncrefresh)
        valid_status = nogpu_wait_status(&m_blit_status, std::max(0.0, m_period - get_ms(time_blit - time_exit)));

    if (source_config.syncrefresh && valid_status)
        m_frame = m_blit_status.frame_req + 1;
    else
        m_frame++;

    time_exit = CurrentTicks();

    return 0;
}

//============================================================
//  renderer_nogpu::nogpu_init
//============================================================

bool renderer_nogpu::nogpu_init()
{
    int result;

    LogMessage("Initializing Winsock...");
    WSADATA wsa;
    result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (result != NO_ERROR)
    {
        LogMessage("Failed. Error code : " + std::to_string(WSAGetLastError()));
        return false;
    }
    LogMessage("Done.");
            
    LogMessage("Initializing socket... ");
    m_sockfd = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (m_sockfd < 0)
    {
        LogMessage("Could not create socket!", true);
        return false;
    }
    else
        LogMessage("Done.");

    m_server_addr = {};
    m_server_addr.sin_family = AF_INET;
    m_server_addr.sin_port = htons(UDP_PORT);
    m_server_addr.sin_addr.s_addr = inet_addr(m_targetip.c_str());

    LogMessage("Setting socket async...");

    u_long opt = 1;
    if (ioctlsocket(m_sockfd, FIONBIO, &opt) < 0)
        LogMessage("Could not set nonblocking.");

    LogMessage("Setting send buffer to " + std::to_string(SEND_BIFFER_SIZE));
    int opt_val = SEND_BIFFER_SIZE;
    result = setsockopt(m_sockfd, SOL_SOCKET, SO_SNDBUF, (char*)&opt_val, sizeof(opt_val));
    if (result < 0)
    {
        LogMessage("Unable to set send buffer: " + std::to_string(result), true);
        return false;
    }

    connect(m_sockfd, (sockaddr*)&m_server_addr, sizeof(m_server_addr));

    // Query the function pointer for the TransmitPacket function
    GUID transmitPacketsGuid = WSAID_TRANSMITPACKETS;
    DWORD bytesReturned;
    result = WSAIoctl(m_sockfd,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &transmitPacketsGuid,
        sizeof(GUID),
        &transmitPackets,
        sizeof(PVOID),
        &bytesReturned,
        NULL,
        NULL);

    if (result != 0) {
        printf("Unable to load TransmitPackets extension: %d\n", WSAGetLastError());
        return false;
    }

    for (int i = 0; i < 256; i++)
    {
        // Memory buffer and don't combine
        transmitEl[i] = {};
        transmitEl[i].dwElFlags = TP_ELEMENT_MEMORY | TP_ELEMENT_EOP;
    }

    m_compression = 0x01; // lz4 compression


    switch (audioSampleRate)
    {
    case 22050:
        m_sample_rate = 1;
        break;
    case 44100:
        m_sample_rate = 2;
        break;
    case 48000:
        m_sample_rate = 3;
        break;
    default:
        LogMessage("Unsupported audio sample rate. Only 48kHz, 44.1kHz and 22.05kHz are supported.");
        m_sample_rate = 0;
    }

    LogMessage("Sending CMD_INIT...");
    cmd_init command;
    command.compression = m_compression;
    command.sound_rate = m_sample_rate;
    command.sound_channels = 2;

    // Reset current mode
    m_current_mode = {};

    if (nogpu_send_command(&command, sizeof(command)))
        return nogpu_wait_ack(1000);

    return false;
}

//============================================================
//  renderer_nogpu::nogpu_switch_video_mode()
//============================================================

bool renderer_nogpu::nogpu_switch_video_mode()
{
    nogpu_modeline *mode = &selected_modeline;
    if (mode == nullptr)
        return false;

    m_current_mode = *mode;

    // Send new modeline to nogpu
    LogMessage("Sending CMD_SWITCHRES...");

    cmd_switchres command;
    nogpu_modeline *m = &command.mode;

    m->pclock = mode->pclock;
    m->hactive = mode->hactive;
    m->hbegin = mode->hbegin;
    m->hend = mode->hend;
    m->htotal = mode->htotal;
    m->vactive = mode->vactive;
    m->vbegin = mode->vbegin;
    m->vend = mode->vend;
    m->vtotal = mode->vtotal;
    m->interlace = mode->interlace;

    m_width = mode->hactive;
    m_height = mode->vactive;
    m_vtotal = mode->vtotal;
    m_field = 0;

    shouldUpdateVideoMode = false;
    return nogpu_send_command(&command, sizeof(command));
}

//============================================================
//  renderer_nogpu::nogpu_wait_ack
//============================================================

bool renderer_nogpu::nogpu_wait_ack(double timeout)
{
    uint64_t time_1 = CurrentTicks();
    socklen_t server_addr_size = sizeof(m_server_addr);

    // Poll server for ack
    do
    {
        int bytes_recv = recvfrom(m_sockfd, (char *)&m_status, sizeof(nogpu_blit_status), 0, (sockaddr*)&m_server_addr, &server_addr_size);

        if (bytes_recv == sizeof(nogpu_blit_status))
            break;

        uint64_t time_2 = CurrentTicks();
        if (get_ms(time_2 - time_1) > timeout)
        {
            LogMessage("Server ack timeout.", true);
            return false;
        }

        SleepTicks(time_sleep);

    } while (true);

    return true;
}

//============================================================
//  renderer_nogpu::nogpu_wait_status
//============================================================

bool renderer_nogpu::nogpu_wait_status(nogpu_blit_status *status, double timeout)
{
    int retries = 0;
    uint64_t time_1 = 0;
    uint64_t time_2 = 0;
    socklen_t server_addr_size = sizeof(m_server_addr);
    int bytes_recv = 0;

    time_1 = CurrentTicks();

    // Poll server for blit line timestamp
    do
    {
        retries++;
        bytes_recv = recvfrom(m_sockfd, (char *)status, sizeof(nogpu_blit_status), 0, (sockaddr*)&m_server_addr, &server_addr_size);

        if (bytes_recv > 0 && m_frame == status->frame_req)
            break;

        time_2 = CurrentTicks();
        if (get_ms(time_2 - time_1) > timeout)
        {
            return false;
        }

        if (m_sleep_allowed) SleepTicks(time_sleep);

    } while (true);


    // Compute line target for next blit, relative to last blit line timestamp
    int lines_to_wait = (status->frame_req - status->frame_gpu) * m_current_mode.vtotal + status->vcount_req - status->vcount_gpu;
    if (m_current_mode.interlace)
        lines_to_wait /= 2;

    // Compute time target for emulation of next frame, so that blit after it happens at desired line target
    uint64_t time_target = time_entry + (uint64_t)((double)lines_to_wait * m_line_period * time_sleep) - time_frame_avg;

    // Wait for target time
    if ((int)(time_target - CurrentTicks()) > 0)
    {
        do
        {
            time_2 = CurrentTicks();
            if (time_2 >= time_target)
                break;

            if (m_sleep_allowed && get_ms(time_target - time_2) > 2.0)
                SleepTicks(time_sleep);

        } while (true);
    }

    // Make sure our frame counter hasn't fallen behind gpu's
    if (status->frame_gpu > status->frame_req)
        status->frame_req = status->frame_gpu + 1;

    return true;
}

//============================================================
//  renderer_nogpu::nogpu_register_frametime
//============================================================

void renderer_nogpu::nogpu_register_frametime(uint64_t frametime)
{
    static int i = 0;
    static int regs = 0;
    const int max_regs = sizeof(time_frame) / sizeof(time_frame[0]);
    uint64_t acum = 0;
    uint64_t diff = 0;

    // Discard invalid values
    if (frametime <= 0 || get_ms(frametime) > m_period)
        return;

    // Register value and compute current average
    time_frame[i] = frametime;
    i++;

    if (i > max_regs)
        i = 0;

    if (regs < max_regs)
        regs++;

    for (int k = 0; k < regs; k++)
        acum += time_frame[k];

    time_frame_avg = acum / regs;

    // Compute current max deviation
    uint64_t max_diff = 0;

    for (int k = 1; k <= regs; k++)
    {
        diff = time_frame[k] - time_frame[k - 1];

        if (diff > 0 && diff > max_diff)
            max_diff = diff;
    }

    time_frame_dm = max_diff;
}

//============================================================
//  renderer_nogpu::nogpu_send_mtu
//============================================================

void renderer_nogpu::nogpu_send_mtu(char *buffer, int bytes_to_send, int chunk_max_size)
{
    int bytes_this_chunk = 0;
    int chunk_size = 0;
    uint32_t offset = 0;

    if (source_config.useTransmitExtension)
    {
        int packetCount = 0;
        do
        {
            chunk_size = bytes_to_send > chunk_max_size ? chunk_max_size : bytes_to_send;
            bytes_to_send -= chunk_size;
            bytes_this_chunk = chunk_size;
            transmitEl[packetCount].pBuffer = buffer + offset;
            transmitEl[packetCount].cLength = chunk_size;
            offset += chunk_size;
            packetCount++;
        } while (bytes_to_send > 0);

        SecureZeroMemory((PVOID)&overlapped, sizeof(WSAOVERLAPPED));
        overlapped.hEvent = WSACreateEvent();

        DWORD rc = (transmitPackets)(m_sockfd,
            transmitEl,
            packetCount,
            0xFFFFFFFF, // Use transmit elem size for sends
            &overlapped,
            TF_USE_SYSTEM_THREAD);

        if (rc == FALSE) {
            DWORD lastError;
            lastError = WSAGetLastError();
            if (lastError != ERROR_IO_PENDING)
            {
                LogMessage("Transmit packets failed: " + std::to_string(lastError));
            }
            else
            {
                rc = WSAWaitForMultipleEvents(1, &overlapped.hEvent, TRUE, INFINITE, TRUE);
            }
        }
    }
    else
    {
        do
        {
            chunk_size = bytes_to_send > chunk_max_size ? chunk_max_size : bytes_to_send;
            bytes_to_send -= chunk_size;
            bytes_this_chunk = chunk_size;

            nogpu_send_command(buffer + offset, bytes_this_chunk);
            offset += chunk_size;

        } while (bytes_to_send > 0);
    }
}

//============================================================
//  renderer_nogpu::nogpu_send_lz4
//============================================================

void renderer_nogpu::nogpu_send_lz4(char *buffer, int bytes_to_send, int block_size)
{
    LZ4_stream_t lz4_stream_body;
    LZ4_stream_t* lz4_stream = &lz4_stream_body;
    LZ4_initStream(lz4_stream, sizeof(*lz4_stream));

    int inp_buf_index = 0;
    int bytes_this_chunk = 0;
    int chunk_size = 0;
    uint32_t offset = 0;

    do
    {
        chunk_size = bytes_to_send > block_size ? block_size : bytes_to_send;
        bytes_to_send -= chunk_size;
        bytes_this_chunk = chunk_size;

        char* const inp_ptr = inp_buf[inp_buf_index];
        memcpy((char *)&inp_ptr[0], buffer + offset, chunk_size);

        const uint16_t c_size = LZ4_compress_fast_continue(lz4_stream, inp_ptr, (char *)&m_fb_compressed[2], bytes_this_chunk, MAX_LZ4_BLOCK, 1);
        uint16_t *c_size_ptr = (uint16_t *)&m_fb_compressed[0];
        *c_size_ptr = c_size;

        nogpu_send_mtu((char *)&m_fb_compressed[0], c_size + 2, 1472);
        offset += chunk_size;
        inp_buf_index ^= 1;

    } while (bytes_to_send > 0);
}

//============================================================
//  renderer_nogpu::nogpu_blit
//============================================================

void renderer_nogpu::nogpu_blit(uint32_t frame, uint16_t width, uint16_t height)
{
    // Compressed blocks are 16 lines long
    int block_size = m_compression ? (width << 4) * 3 : 0;

    int vsync_offset = 0;

    // Calculate frame delay factor
    if (m_is_internal_fe)
        // Internal frontend needs fd > 0
        m_frame_delay = .5;

    else if (source_config.framedelay == 0)
        // automatic
        m_frame_delay = std::max((double)(m_period - std::max(m_fd_margin, get_ms(time_frame_dm))) / m_period, 0.0);
    else
    {
        // user defined
        m_frame_delay = (double)(source_config.framedelay) / 10.0;
        vsync_offset = 0;// window().machine().video().vsync_offset();
    }

    // Update vsync scanline
    m_vsync_scanline = std::min<int>(int((m_current_mode.vtotal) * m_frame_delay + vsync_offset + 1), m_current_mode.vtotal);

    // Send CMD_BLIT
    cmd_blit_vsync command;
    command.frame = frame;
    command.vsync = source_config.syncrefresh ? m_vsync_scanline : 0;
    command.block_size = block_size;
    nogpu_send_command(&command, sizeof(command));

    if (m_compression == 0)
        nogpu_send_mtu(&m_fb[0], width * height * 3, 1470);

    else
        nogpu_send_lz4(&m_fb[0], width * height * 3, block_size);
}

//============================================================
//  renderer_nogpu::add_audio_to_recording
//============================================================

void renderer_nogpu::add_audio_to_recording(const uint16_t *buffer, int samples_this_frame)
{
    if (m_blit_status.bits & FPGA_AUDIO && m_sample_rate)
    {
        // Send CMD_AUDIO
        cmd_audio command;
        command.sample_bytes = samples_this_frame << 1;
        nogpu_send_command(&command, sizeof(command));
        nogpu_send_mtu((char*)buffer, command.sample_bytes, 1472);
    }
}

//============================================================
//  renderer_nogpu::nogpu_send_command
//============================================================

bool renderer_nogpu::nogpu_send_command(void *command, int command_size)
{
    int rc = sendto(m_sockfd, (char *)command, command_size, 0, (sockaddr*)&m_server_addr, sizeof(m_server_addr));

    if (rc < 0)
    {
        LogMessage("Send command failed: " + std::to_string(WSAGetLastError()), true);
        return false;
    }

    return true;
}
