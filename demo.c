#define MINIAUDIO_IMPLEMENTATION
#include "demo/miniaudio.h"
#define MIDI_INTERFACE_IMPLEMENTATION
#include "MIDI_interface.h"
#include <stdbool.h>

typedef enum
{
    SYNTH_ACTIVE            = (1 << 0),
    SYNTH_NOTE_ON           = (1 << 1),
    SYNTH_NOTE_OFF          = (1 << 2),
    SYNTH_ATTACKING         = (1 << 3),
    SYNTH_DECAYING          = (1 << 4),
    SYNTH_WAITING_NOTE_ON   = (1 << 5)
} Synth_FLAGS;

#define SYNTH_BUFFER_BEING_READ (1 << 0)
typedef struct
{
    float* buffer;
    uint32_t cursor;
    uint32_t bufferMax;
    double phase;
    double phaseIncrement;
    float volume;
    float frequency;
    float decay_time;
    float decay_rate;
    float attack_time;
    float attack_rate;
    float adjustment_rate; //will be used when attacking or decaying
    uint8_t audio_thread_flags;
    uint8_t velocity; // used for midi input, (0 - 127) At VELOCITY_WEIGHTING_NEUTRAL will be the attack_time set, higher or lower will just accordingly
    uint32_t FLAGS;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Synth;

typedef struct
{
    Synth** synth;
    uint8_t synth_count;
    MIDI_Controller* midi_controller;
    bool midi_clock;
    uint32_t loopFrameLength; //4 beat timer for swapping samples or bring in queued samples
    uint32_t globalCursor;
    MIDI_Controller* midiController;
} Sound_Controller;


void data_callback_f32(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);

uint32_t calculate_loop_frames(float bpm, uint32_t sample_rate, uint32_t beats_per_bar, uint32_t bars)
{
    float seconds_per_beat = 60.0f / bpm;
    float frames_per_beat = seconds_per_beat * sample_rate;

    return (uint32_t)(frames_per_beat * beats_per_bar * bars);
}
#define CHANNEL_COUNT 2
#define SAMPLE_RATE 44100
#define SYNTH_ATTACK_TIME 0.3f
#define SYNTH_DECAY_TIME 0.5f
#define VELOCITY_WEIGHTING_NEUTRAL 64
#define ATTACK_OR_DECAY_FINISHED -999.9f
#define PI 3.14159265358979323846
#define TWO_PI (2.0 * PI)
void sound_controller_init(Sound_Controller* sc, const float bpm, MIDI_Controller* midiController, bool midi_clock)
{
    memset(sc, 0, sizeof(Sound_Controller));
    sc->synth_count = 5;
    sc->loopFrameLength = calculate_loop_frames(bpm, SAMPLE_RATE, 4, 4); // looping every 4 bars
    sc->midiController = midiController;
    sc->midi_clock = midi_clock;
    sc->globalCursor = 0;
    sc->synth = (Synth**)malloc(sizeof(Synth*) * sc->synth_count);
    for (uint8_t i = 0; i < sc->synth_count; ++i)
    {
        sc->synth[i] = (Synth*)malloc(sizeof(Synth));
        Synth* synth = sc->synth[i];
        memset(synth, 0, sizeof(Synth));
        synth->bufferMax = SAMPLE_RATE * 2; // for 2 seconds of audio
        synth->buffer = (float*)malloc(sizeof(float) * synth->bufferMax);
        memset(synth->buffer, 0, sizeof(float) * synth->bufferMax);
        synth->decay_time = SYNTH_DECAY_TIME;
        synth->attack_time = SYNTH_ATTACK_TIME;
        synth->velocity = VELOCITY_WEIGHTING_NEUTRAL;
        synth->adjustment_rate = ATTACK_OR_DECAY_FINISHED;
        synth->audio_thread_flags = 0;
        synth->cursor = 0;
        synth->frequency = 440.0f;
        synth->phase = 0.0f;
        synth->volume = 1.0f;
        synth->phaseIncrement = TWO_PI * synth->frequency / SAMPLE_RATE;
        synth->FLAGS |= SYNTH_ACTIVE ;
        pthread_mutex_init(&synth->mutex, NULL);
        pthread_cond_init(&synth->cond, NULL);
    }
}

void synth_generate_audio(Synth* synth)
{
    assert(synth != NULL);
    if (!(synth->FLAGS & SYNTH_ACTIVE)) // Only entering synth is currently active
        return;


    pthread_mutex_lock(&synth->mutex);  // Lock before checking

    while (synth->audio_thread_flags & SYNTH_BUFFER_BEING_READ)
        pthread_cond_wait(&synth->cond, &synth->mutex); // Wait if being currently read

    if (synth->FLAGS & SYNTH_NOTE_ON)
    {
        synth->cursor = synth->bufferMax;
        synth->FLAGS &= ~(SYNTH_NOTE_ON | SYNTH_WAITING_NOTE_ON);
        synth->FLAGS |= SYNTH_ATTACKING;
        synth->attack_rate = ((1.0f / (synth->attack_time * SAMPLE_RATE)) * ((float)synth->velocity / VELOCITY_WEIGHTING_NEUTRAL));
        synth->adjustment_rate = synth->adjustment_rate == ATTACK_OR_DECAY_FINISHED ? 0.0f : synth->adjustment_rate;
        if (synth->attack_rate <= 0.0f)
            synth->attack_rate = 0.000001f;
        // printf("attack rate: %f\n", synth->attack_rate);
        // printf("adjustment rate: %f\n", synth->adjustment_rate);
    }
    else
        memmove(synth->buffer, synth->buffer + synth->cursor, sizeof(float) * (synth->bufferMax - synth->cursor));

    if (synth->FLAGS & SYNTH_NOTE_OFF)
    {
        synth->FLAGS &= ~SYNTH_NOTE_OFF;
        synth->FLAGS |= SYNTH_DECAYING;
        synth->decay_rate = 1.0f / (synth->decay_time * SAMPLE_RATE);
        synth->adjustment_rate = synth->adjustment_rate == ATTACK_OR_DECAY_FINISHED ? 1.0f : synth->adjustment_rate;
        if (synth->decay_rate <= 0.0f)
            synth->decay_rate = 0.000001f;
        // printf("adjustment rate: %f\n", synth->adjustment_rate);
        // printf("decay_rate: %f\n", synth->decay_rate);
    }

    synth->phaseIncrement = TWO_PI * synth->frequency / SAMPLE_RATE;
    for (uint32_t i = synth->bufferMax - synth->cursor; i < synth->bufferMax; ++i)
    {
        double toGenPhase = synth->phase; //Saving the phase to be used to generate the sound, to the certin LFO's can maniplate it in different ways

        if (synth->FLAGS & SYNTH_DECAYING)
        {
            synth->buffer[i] = (sin(toGenPhase) * 0.05) * synth->adjustment_rate;
            if (i + 1 < synth->bufferMax)
                synth->buffer[++i] = (sin(toGenPhase) * 0.05) * synth->adjustment_rate;
            else
                printf("WARNING - Odd number of frames generated\n");

            synth->adjustment_rate -= synth->decay_rate;
            if (synth->adjustment_rate < 0)
            {
                synth->FLAGS &= ~SYNTH_DECAYING;
                synth->FLAGS |= SYNTH_WAITING_NOTE_ON;
                synth->adjustment_rate = ATTACK_OR_DECAY_FINISHED;
            }
        }
        else if (synth->FLAGS & SYNTH_ATTACKING)
        {
            synth->buffer[i] = (sin(toGenPhase) * 0.05) * synth->adjustment_rate; // *0.05 to get the sound down in line with other sample
            if (i + 1 < synth->bufferMax)
                synth->buffer[++i] = (sin(toGenPhase) * 0.05) * synth->adjustment_rate; // generating the same sample for both frames
            else
                printf("WARNING - Odd number of frames generated\n");

            synth->adjustment_rate += synth->attack_rate;
            if (synth->adjustment_rate > 1)
            {
                synth->FLAGS &= ~SYNTH_ATTACKING;
                synth->adjustment_rate = ATTACK_OR_DECAY_FINISHED;
            }
        }
        else if (synth->FLAGS & SYNTH_WAITING_NOTE_ON)
        {
            synth->buffer[i] = 0;
            if (i + 1 < synth->bufferMax)
                synth->buffer[++i] = 0;
            else
                printf("WARNING - Odd number of frames generated\n");
        }
        else
        {
            synth->buffer[i] = sin(toGenPhase) * 0.05; // *0.05 to get the sound down in line with other sample
            if (i + 1 < synth->bufferMax)
                synth->buffer[++i] = sin(toGenPhase) * 0.05; // generating the same sample for both frames
            else
                printf("WARNING - Odd number of frames generated\n");
        }
        synth->phase += synth->phaseIncrement;

        if (synth->phase >= TWO_PI)
            synth->phase -= TWO_PI;
    }

    synth->cursor = 0;

    pthread_mutex_unlock(&synth->mutex);
}

void controller_synth_generate_audio(Sound_Controller* sc)
{
    if (sc->synth_count == 0)
        return;

    for (uint8_t i = 0; i < sc->synth_count; ++i)
        synth_generate_audio(sc->synth[i]);
}

void note_on(Sound_Controller* sc, uint8_t channel, uint8_t key, uint8_t velocity)
{
    if (channel >= sc->synth_count)
        return;

    Synth* synth = sc->synth[channel];
    if(synth->FLAGS & SYNTH_NOTE_ON)
        printf("WARNING - Synth %u is already Deactive\n", channel +1);

    synth->frequency = midi_note_to_frequence(key);
    synth->velocity = velocity;
    synth->FLAGS |= SYNTH_NOTE_ON;
    synth->FLAGS &= ~SYNTH_DECAYING; // incase the note is still decaying
    printf("NOTE ON - Channel: %u, Key: %u, Velocity: %u, Frequency: %f\n", channel +1, key, velocity, synth->frequency);

}

void note_off(Sound_Controller* sc, uint8_t channel, uint8_t key, uint8_t velocity)
{
    if (channel >= sc->synth_count)
        return;

    Synth* synth = sc->synth[channel];
    if (!(synth->FLAGS & SYNTH_ACTIVE))
    {
        printf("WARNING - Synth %u is Deactive\n", channel +1);
        return;
    }
    synth->velocity = velocity;
    synth->frequency = midi_note_to_frequence(key);
    synth->FLAGS |= SYNTH_NOTE_OFF;
    synth->FLAGS &= ~SYNTH_ATTACKING; // incase the note is still attacking
    printf("NOTE OFF - Channel: %u, Key: %u, Velocity: %u, Frequency: %f\n", channel +1, key, velocity, synth->frequency);

}

void process_midi_commands(Sound_Controller* sc)
{
    if (sc->midi_controller->command_count == 0)
        return;

    pthread_mutex_lock(&sc->midi_controller->mutex);


    for(uint8_t i = sc->midi_controller->commands_processed; i < sc->midi_controller->command_count; ++i)
    {
        MIDI_Command command = sc->midi_controller->commands[i];

        uint8_t command_nibble = 0;
        uint8_t channel = 0;
        midi_command_byte_parse(command.command_byte, &command_nibble, &channel);
        switch(command_nibble)
        {
        case MIDI_SYSTEM_MESSAGE:
            if (command.command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_CLOCK))
                ;
            else if (command.command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_START))
                printf("MIDI START\n");
            break;
        case MIDI_NOTE_OFF:
            note_off(sc, channel, command.param1, command.param2);
            break;
        case MIDI_NOTE_ON:
            note_on(sc, channel, command.param1, command.param2);
            break;
        case MIDI_AFTERTOUCH:
        case MIDI_CONTINUOUS_CONTROLLER:
        case MIDI_PATCH_CHANGE:
        case MIDI_CHANEL_PRESSURE:
        case MIDI_PITCH_BEND:
            break;
        }
        ++sc->midi_controller->commands_processed;
    }
    pthread_mutex_unlock(&sc->midi_controller->mutex);

    return;
}


int main(int argc, char* argv[])
{
    MIDI_Controller midi_controller = {0};

    // =========================================================================
    // Welcome screen and MIDI controller initialization
    // =========================================================================
    printf("============================================\n");
    printf("       MIDI Interface Demo Application      \n");
    printf("============================================\n\n");

    char filepath_buf[256] = {0};
    char external_buf[256] = {0};
    const char* filepath = NULL;
    const char* midi_external = NULL;
    uint8_t external_midi_set_up = EXTERNAL_INPUT_INACTIVE;

    printf("Load a MIDI command file? (y/n): ");
    char choice = 0;
    scanf(" %c", &choice);
    if (choice == 'y' || choice == 'Y')
    {
        printf("Enter path to MIDI command file: ");
        scanf(" %255s", filepath_buf);
        filepath = filepath_buf;
    }

    printf("Connect to an external MIDI device? (y/n): ");
    scanf(" %c", &choice);
    if (choice == 'y' || choice == 'Y')
    {
        printf("Enter path to external MIDI device (e.g. /dev/midi1): ");
        scanf(" %255s", external_buf);
        midi_external = external_buf;

        printf("\nExternal MIDI input options:\n");
        printf("  1. No external input (output only)\n");
        printf("  2. Use external device as clock source\n");
        printf("  3. Pass-through external input\n");
        printf("  4. External clock + pass-through\n");
        printf("Select option (1-4): ");
        int ext_option = 1;
        scanf(" %d", &ext_option);

        switch (ext_option)
        {
        case 1:
            external_midi_set_up = EXTERNAL_INPUT_INACTIVE;
            break;
        case 2:
            external_midi_set_up = EXTERNAL_INPUT_CLOCK;
            break;
        case 3:
            external_midi_set_up = EXTERNAL_INPUT_THROUGH;
            break;
        case 4:
            external_midi_set_up = EXTERNAL_INPUT_CLOCK | EXTERNAL_INPUT_THROUGH;
            break;
        default:
            printf("Invalid option, defaulting to no external input.\n");
            external_midi_set_up = EXTERNAL_INPUT_INACTIVE;
            break;
        }
    }

    printf("\nInitializing MIDI controller...\n");
    if (midi_controller_set(&midi_controller, filepath, midi_external, external_midi_set_up) != 0)
    {
        printf("Failed to initialize MIDI controller.\n");
        return -1;
    }
    printf("MIDI controller initialized successfully.\n\n");

    // =========================================================================
    // Tempo and clock setup
    // =========================================================================
    float bpm = 120.0f;
    bool use_internal_clock = true;

    printf("--------------------------------------------\n");
    printf("            Clock & Tempo Setup              \n");
    printf("--------------------------------------------\n");

    if (external_midi_set_up & EXTERNAL_INPUT_CLOCK)
    {
        printf("External clock source detected - using external clock.\n");
        use_internal_clock = false;
        printf("Enter BPM for loop timing (e.g. 120.0): ");
        scanf(" %f", &bpm);
    }
    else
    {
        printf("Clock mode options:\n");
        printf("  1. Internal MIDI clock (interface manages timing)\n");
        printf("  2. Application-driven clock (you call midi_command_clock from audio callback)\n");
        printf("Select clock mode (1-2): ");
        int clock_option = 1;
        scanf(" %d", &clock_option);

        printf("Enter BPM (e.g. 120.0): ");
        scanf(" %f", &bpm);
        if (bpm <= 0.0f)
        {
            printf("Invalid BPM, defaulting to 120.\n");
            bpm = 120.0f;
        }

        if (clock_option == 1)
        {
            use_internal_clock = true;
            printf("Starting internal MIDI clock at %.1f BPM...\n", bpm);
        }
        else
        {
            use_internal_clock = false;
            printf("Application-driven clock selected at %.1f BPM.\n", bpm);
        }
    }

    Sound_Controller sc = {0};
    sound_controller_init(&sc, bpm, &midi_controller, !use_internal_clock);
    sc.midi_controller = &midi_controller;
    printf("Sound controller initialized at %.1f BPM.\n\n", bpm);

    // =========================================================================
    // Audio device setup
    // =========================================================================
    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS)
    {
        printf("Failed to initialize audio context.\n");
        midi_controller_destrory(&midi_controller);
        return -1;
    }

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    uint32_t captureCount;
    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS)
    {
        printf("Failed to enumerate audio devices.\n");
        ma_context_uninit(&context);
        midi_controller_destrory(&midi_controller);
        return -2;
    }

    printf("--------------------------------------------\n");
    printf("         Available Playback Devices          \n");
    printf("--------------------------------------------\n");
    for (ma_uint32 i = 0; i < playbackCount; ++i)
        printf("  [%u] %s\n", i, pPlaybackInfos[i].name);
    printf("\n");

    printf("Connect device to: ");
    uint32_t indexDecoder = 0;
    scanf("%u", &indexDecoder);
    if (indexDecoder >= playbackCount)
    {
        printf("Invalid device index.\n");
        ma_context_uninit(&context);
        midi_controller_destrory(&midi_controller);
        return -3;
    }

    printf("Connecting to: %s\n\n", pPlaybackInfos[indexDecoder].name);

    ma_result result;
    ma_device device;
    ma_device_config deviceConfig;

    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.pDeviceID = &pPlaybackInfos[indexDecoder].id;
    deviceConfig.playback.format   = ma_format_f32;
    deviceConfig.playback.channels = CHANNEL_COUNT;
    deviceConfig.sampleRate        = SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback_f32;
    deviceConfig.pUserData         = &sc;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
    {
        printf("Failed to initialize audio device.\n");
        ma_context_uninit(&context);
        midi_controller_destrory(&midi_controller);
        return -1;
    }
    if (ma_device_start(&device) != MA_SUCCESS)
    {
        ma_device_uninit(&device);
        ma_context_uninit(&context);
        midi_controller_destrory(&midi_controller);
        printf("Failed to start playback device.\n");
        return -4;
    }

    printf("============================================\n");
    printf("  Playback running. Press ENTER to stop...  \n");
    printf("============================================\n");

    midi_start(&midi_controller);
    if (use_internal_clock)
        midi_clock_set(&midi_controller, bpm);

    bool running = true;

    while (running)
    {
        process_midi_commands(&sc);
        controller_synth_generate_audio(&sc);

        struct timespec ts = {0, 10000000L}; // 10ms
        nanosleep(&ts, NULL);

        fd_set readfds;
        struct timeval tv = {0, 0};
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        if (select(STDIN_FILENO + 1, &readfds, NULL, NULL, &tv) > 0)
        {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0 && c == '\n')
                running = false;
        }
    }

    // =========================================================================
    // Cleanup and free all resources
    // =========================================================================
    printf("\nShutting down...\n");

    midi_stop(&midi_controller);

    ma_device_uninit(&device);
    ma_context_uninit(&context);

    for (uint8_t i = 0; i < sc.synth_count; ++i)
    {
        Synth* synth = sc.synth[i];
        pthread_mutex_destroy(&synth->mutex);
        pthread_cond_destroy(&synth->cond);
        free(synth->buffer);
        free(synth);
    }
    free(sc.synth);

    midi_controller_destrory(&midi_controller);

    printf("Goodbye!\n");
    return 0;
}

bool synth_buffer_being_read(Synth* synth)
{
    pthread_mutex_lock(&synth->mutex);
    if (synth->FLAGS & SYNTH_ACTIVE)
    {
        synth->audio_thread_flags |= SYNTH_BUFFER_BEING_READ;
        pthread_mutex_unlock(&synth->mutex);
        return true;
    }
    else
    {
        pthread_mutex_unlock(&synth->mutex);
        return false;
    }
}

// Called by audio callback after reading buffer
void synth_frames_read(Synth *synth)
{
    pthread_mutex_lock(&synth->mutex);
    synth->audio_thread_flags &= ~SYNTH_BUFFER_BEING_READ;
    pthread_cond_signal(&synth->cond);
    pthread_mutex_unlock(&synth->mutex);
}
void data_callback_f32(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    //printf("FrameCount: %u\n", frameCount);
    Sound_Controller* s = (Sound_Controller*)pDevice->pUserData;

    float* pOutputF32 = (float*)pOutput;
    uint32_t pushedFrames = 0;

    for (uint8_t i = 0; i < s->synth_count; ++i)
    {
        Synth* synth = s->synth[i];
        if(!synth_buffer_being_read(synth))
            continue;
        float volume = synth->volume;
        uint32_t synth_pushedFrames = 0;

        while(synth_pushedFrames < frameCount * 2)
        {
            pOutputF32[synth_pushedFrames++] += synth->buffer[synth->cursor++] * volume;
            if (synth->cursor > synth->bufferMax)
            {
                synth->cursor = 0;
                printf("WARNING - synth[%u] cursor wrapped around\n", i);
            }
        }
        synth_frames_read(synth);
    }

    while(pushedFrames < frameCount * 2)
    {
        ++pushedFrames;
        //for MIDI_Clock
        if (s->globalCursor % (s->loopFrameLength / (MIDI_TICKS_PER_BAR)) == 0 && s->midiController != NULL)
        {
            if (s->midi_clock)
                midi_command_clock(s->midiController);
            //printf("clock and command count: %u\n", s->midiController->command_count);
        }
        ++s->globalCursor;
        if (s->globalCursor > s->loopFrameLength)
            s->globalCursor = 0;
    }
    (void)pDevice;
    (void)pOutput;
}
