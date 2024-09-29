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

// nogpu UDP server
#define UDP_PORT 32100

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
    void draw();
    void save() {}
    void record() {}
    void toggle_fsfx() {}
    void add_audio_to_recording(const uint16_t *buffer, int samples_this_frame);

private:
    std::unique_ptr<uint8_t[]> m_bmdata;
    size_t                      m_bmsize;

    // npgpu private members
    GroovyMister groovyMister;
    bool m_initialized = false;
    bool m_first_blit = true;
    int m_compression = 0;
    int m_frame = 0;
    int m_field = 0;
    unsigned int m_width = 0;
    unsigned int m_height = 0;
    int m_vtotal = 0;
    int m_vsync_scanline = 0;
    double m_period = 16.666667;
    double m_line_period = 0.064;
    double m_frame_delay = 0.0;
    double m_fd_margin = 1.5;
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

    bool nogpu_init();
    bool nogpu_switch_video_mode();
    void nogpu_register_frametime(uint64_t frametime);
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
    groovyMister.CmdClose();
}

//============================================================
//  renderer_nogpu::draw
//============================================================
void renderer_nogpu::draw()
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
            m_first_blit = false;
        }
    }

    // only send frame if nogpu is initialized
    if (!m_initialized)
        return;

    m_frame++;

    if (groovyMister.fpga.frame > m_frame)
        m_frame = groovyMister.fpga.frame + 1;

    // get current field for interlaced mode
    if (m_current_mode.interlace)
        m_field = !groovyMister.fpga.vgaF1 ^ ((m_frame - groovyMister.fpga.frame) % 2);
    else
        m_field = 0;

    unsigned int drawIndex = lastVideoCaptureIndex;
    int screenwidth = videoCaptures[drawIndex].width;
    int screenheight = videoCaptures[drawIndex].height;
    bool drawInt = (m_field == 0);

    int j = 0;
    
    float stepx;
    float stepy;
    int interlaceStepX = 0;
    int interlaceStepY = 0;
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

    char* fb = groovyMister.getPBufferBlit(m_field);
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

        
        fb[j] =     (char)videoCaptures[drawIndex].buffer[bmpi];
        fb[j + 1] = (char)videoCaptures[drawIndex].buffer[bmpi + 1];
        fb[j + 2] = (char)videoCaptures[drawIndex].buffer[bmpi + 2];

        j += 3;
    }

    // change video mode right before the blit
    if (shouldUpdateVideoMode)
        nogpu_switch_video_mode();

    bool valid_status = true;

    time_entry = CurrentTicks();

    if (source_config.syncrefresh && m_first_blit)
    {
        time_start = time_entry;
        time_blit = time_entry;
        time_exit = time_entry;

        m_first_blit = false;
        m_frame = 0;

        // Skip blitting first frame, so we avoid glitches while MAME loads roms
        return;
    }

    int vsync_offset = 0;

    if (source_config.framedelay == 0)
        // automatic
        m_frame_delay = std::max((double)(m_period - std::max(m_fd_margin, get_ms(time_frame_dm))) / m_period, 0.0);
    else
    {
        // user defined
        m_frame_delay = (double)(source_config.framedelay) / 10.0;
        vsync_offset = 0;// window().machine().video().vsync_offset();
    }

    // Capture and send audio
    if (source_config.audio)
    {
        TickAudioCapture();
        if (AudioWritePos > 0)
            add_audio_to_recording(audioBuffer, AudioWritePos);
    }

    // Update vsync scanline
    m_vsync_scanline = std::min<int>(int((m_current_mode.vtotal) * m_frame_delay + vsync_offset + 1), m_current_mode.vtotal);

    // Blit now
    groovyMister.CmdBlit(m_frame, m_field, 0/*m_vsync_scanline*/, 15000, 0);
    groovyMister.WaitSync();

    time_blit = CurrentTicks();
    nogpu_register_frametime(time_entry - time_exit);
    time_exit = CurrentTicks();

    return;
}

//============================================================
//  renderer_nogpu::nogpu_init
//============================================================

bool renderer_nogpu::nogpu_init()
{
    int result;

    m_compression = 0x01; // lz4 compression

    switch (audioSampleRate)
    {
    case 22050:
        LogMessage("Audio Freq 22.05KHz");
        break;
    case 44100:
        LogMessage("Audio Freq 44.1KHz");
        break;
    case 48000:
        LogMessage("Audio Freq 48KHz");
        break;
    default:
        LogMessage("Unsupported audio sample rate. Only 48kHz, 44.1kHz and 22.05kHz are supported.");
    }

    LogMessage("Sending CMD_INIT...");

    // Reset current mode
    m_current_mode = {};

    int ret = groovyMister.CmdInit(m_targetip.c_str(), UDP_PORT, m_compression, audioSampleRate, 2, 0, 1500);
    if (ret == 0)
    {
        audioBuffer = (uint16_t*)groovyMister.getPBufferAudio();
        return true;
    }
    else
    {
        LogMessage("Groovy MiSTer API failed to initialize!");
        return false;
    }
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

    m_width = mode->hactive;
    m_height = mode->vactive;
    m_vtotal = mode->vtotal;
    m_field = 0;

    shouldUpdateVideoMode = false;
    groovyMister.CmdSwitchres(
        mode->pclock,
        mode->hactive,
        mode->hbegin,
        mode->hend,
        mode->htotal,
        mode->vactive,
        mode->vbegin,
        mode->vend,
        mode->vtotal,
        mode->interlace
    );

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
//  renderer_nogpu::add_audio_to_recording
//============================================================

void renderer_nogpu::add_audio_to_recording(const uint16_t *buffer, int samples_this_frame)
{
    if (!groovyMister.fpga.audio)
        return;

    groovyMister.CmdAudio(samples_this_frame << 1);
}
