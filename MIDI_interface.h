// SPDX-FileCopyrightText: 2026 Jack.B - jack.goldsbrough@outlook.com
// SPDX-License-Identifier: MIT

#ifndef MIDI_INTERFACE_H
#define MIDI_INTERFACE_H

#ifdef __GNUC__
#define MIDI_INLINE static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define MIDI_INLINE static __forceinline
#elif __STDC_VERSION__ >= 199901L
#define MIDI_INLINE static inline
#else
#define MIDI_INLINE static
#endif

// define DEBUG to have asserts on and debug printouts to stderr
#ifndef DEBUG
#define NDEBUG
#endif
#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] %s:%d:%s(): " fmt, \
        __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) // Do nothing in release builds
#endif

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <immintrin.h>


#define MIDI_COMMAND_TYPE_BYTE_MASK 0xF0
typedef enum
{
    /***************************************** command byte                 data byte   date byte
    ****************************************** first 4bits channel(4bits)   (7bits)     (7bits)
    ****************************************** Hex   Binay  0-E             param1      param2 */
    MIDI_NOTE_OFF               = 0x80,     // 0x8 - 0b1000 0-E             key         velocity
    MIDI_NOTE_ON                = 0x90,     // 0x9 - 0b1001 0-E             key         velocity
    MIDI_AFTERTOUCH             = 0xA0,     // 0xA - 0b1010 0-E             key         touch
    MIDI_CONTINUOUS_CONTROLLER  = 0xB0,     // 0xB - 0b1011 0-E             controller# controller Value
    MIDI_PATCH_CHANGE           = 0xC0,     // 0xC - 0b1100 0-E             instrument# instrument#
    MIDI_CHANEL_PRESSURE        = 0xD0,     // 0xD - 0b1101 0-E             pressure
    MIDI_PITCH_BEND             = 0xE0,     // 0xE - 0b1110 0-E             lsb(7bits)  msb(7bits)
    MIDI_SYSTEM_MESSAGE         = 0xF0,     // 0xF - 0b1111 *uses channel for options
    MIDI_COMMAND_INVALID        = 0x00
} MIDI_Command_type;

#define MIDI_CHANNEL_BYTE_MASK 0x0F
typedef enum
{
    MIDI_CHANNEL_1  = 0x0,
    MIDI_CHANNEL_2  = 0x1,
    MIDI_CHANNEL_3  = 0x2,
    MIDI_CHANNEL_4  = 0x3,
    MIDI_CHANNEL_5  = 0x4,
    MIDI_CHANNEL_6  = 0x5,
    MIDI_CHANNEL_7  = 0x6,
    MIDI_CHANNEL_8  = 0x7,
    MIDI_CHANNEL_9  = 0x8,
    MIDI_CHANNEL_10 = 0x9,
    MIDI_CHANNEL_11 = 0xA,
    MIDI_CHANNEL_12 = 0xB,
    MIDI_CHANNEL_13 = 0xC,
    MIDI_CHANNEL_14 = 0xD,
    MIDI_CHANNEL_15 = 0xE,
    MIDI_CHANNEL_16 = 0xF,
    MIDI_CHANNEL_UNDEFINED
} MIDI_Channels;

typedef enum
{
    MIDI_CLOCK = 0x8 // midi clock message sent out 24 times per quater note
} MIDI_System_Message_Option;

typedef struct
{
    uint8_t command_byte;
    uint8_t param1;
    uint8_t param2;
} MIDI_Command;


typedef struct Channel_Node Channel_Node;
typedef struct Channel_Node
{
    const MIDI_Command command;
    /* 1-byte hole */
    const uint16_t on_tick;
    /* 2-byte hole */
    Channel_Node* next;
} Channel_Node;


#define MIDI_TICKS_PER_QUATER_NOTE 24
#define MIDI_TICKS_PER_BAR MIDI_TICKS_PER_QUATER_NOTE * 4 //as one quater note translates to "one beat" in 4x4 music
#define MIDI_MAX_CHANNELS 16

typedef struct
{
    uint16_t node_count[MIDI_MAX_CHANNELS];
    uint16_t loop_steps[MIDI_MAX_CHANNELS];
    uint16_t current_step[MIDI_MAX_CHANNELS];
    uint16_t next_command[MIDI_MAX_CHANNELS];
    Channel_Node* channel[MIDI_MAX_CHANNELS];
} Input_Controller;

#define MIDI_COMMAND_MAX_COUNT 50
#define MIDI_CLOCK_COMMAND_SENT (1<<0)
#define MIDI_INTERFACE_DESTORY (1<<7)
typedef struct MIDI_Controller
{
    MIDI_Command commands[MIDI_COMMAND_MAX_COUNT];
    uint8_t commands_processed;
    uint8_t command_count;
    uint8_t flags;
    /* 1-byte hole */
    uint16_t active_channels;
    /* 4-byte hole */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    Input_Controller midi_commands;
} MIDI_Controller;

/* initalise the midi_controller on the stack and pass the address to the setup function */
MIDI_INLINE void midi_controller_set(MIDI_Controller* controller, const char* filepath);
/* Call when exiting to program to clean up the midi_thread and parsed command nodes */
MIDI_INLINE void midi_controller_destrory(MIDI_Controller* controller);

/* COMMANDS TO CALL */
//call this clock 24 times every quater note to keep the interface in sync, and increments the auto-inputs feeder
MIDI_INLINE void midi_command_clock(MIDI_Controller* controller);
MIDI_INLINE void midi_note_on(MIDI_Controller* controller, MIDI_Channels channel, float frequency, uint8_t velocity);
MIDI_INLINE void midi_note_off(MIDI_Controller* controller, MIDI_Channels channel);

/* Helper functions */
MIDI_INLINE void midi_command_byte_parse(uint8_t commmand_byte, uint8_t* out_type, uint8_t* out_channel);
MIDI_INLINE float midi_note_to_frequence(uint8_t midi_note);
MIDI_INLINE uint8_t midi_frequency_to_midi_note(float frequency);
MIDI_INLINE void print_binary_32(uint32_t var_32);
MIDI_INLINE void print_binary_16(uint16_t var_16);
MIDI_INLINE void print_binary_8(uint8_t var_8);

#endif // MIDI_INTERFACE_H
#ifdef MIDI_INTERFACE_IMPLEMENTATION

MIDI_INLINE void print_binary_32(uint32_t var_32)
{
    for (int i = 31; i >= 0; --i)
    {
        if (var_32 & (1<<i))
            printf("1");
        else printf("0");
    }
    printf("\n");
}

MIDI_INLINE void print_binary_16(uint16_t var_16)
{
    for (int i = 15; i >= 0; --i)
    {
        if (var_16 & (1<<i))
            printf("1");
        else printf("0");
    }
    printf("\n");
}

MIDI_INLINE void print_binary_8(uint8_t var_8)
{

    for (int i = 7; i >= 0; --i)
    {
        if (var_8 & (1<<i))
            printf("1");
        else printf("0");
    }
    printf("\n");
}


MIDI_INLINE void midi_command_launch(MIDI_Controller* controller, uint8_t channel)
{
    assert(controller->command_count < MIDI_COMMAND_MAX_COUNT && "ERROR - comomand limit reached");

    Input_Controller* input_controller = &controller->midi_commands;

    //pop command into queue, move node to the next, and assign the command step
    controller->commands[controller->command_count++] = input_controller->channel[channel]->command;
    input_controller->channel[channel] = input_controller->channel[channel]->next;
    input_controller->next_command[channel] = input_controller->channel[channel]->on_tick;
}

MIDI_INLINE uint16_t midi_merge_mask(uint32_t comparision_mask, uint16_t active_channels)
{
    uint16_t result = 0;
    uint8_t channel = 0;
    for (uint8_t i = 0; i < 32; i += 2)
    {
        if (comparision_mask & (1<<i) && active_channels & (1<<channel))
            result |= (1<<channel);
        ++channel;
    }
    return result;
}

#define __AVX2__
MIDI_INLINE void midi_increment_step_count_simd(MIDI_Controller* controller)
{
#ifdef __AVX2__

    //load in current steps
    __m256i current_steps = _mm256_loadu_si256((__m256i*)controller->midi_commands.current_step);

    // checking for commands to launch
    __m256i next_command = _mm256_loadu_si256((__m256i*)controller->midi_commands.next_command);
    __m256i comparision = _mm256_cmpeq_epi16(next_command, current_steps);
    uint32_t comparision_mask = _mm256_movemask_epi8(comparision);
    uint16_t active_mask = midi_merge_mask(comparision_mask, controller->active_channels);
    //print_binary_32(comparision_mask);
    //print_binary_16(active_mask);
    if(active_mask != 0)
    {
        for(uint8_t i = 0; i < 16; ++i)
        {
            if (active_mask & (1<<i))
                midi_command_launch(controller, i);
        }
    }

    // AVX2: 16 elements in one operation
    //printf("AVX2 increment\n");
    //increment
    __m256i ones = _mm256_set1_epi16(1);
    __m256i after_increment = _mm256_add_epi16(current_steps, ones);

    //check loop_steps and reset if over
    __m256i loop_steps = _mm256_loadu_si256((__m256i*)controller->midi_commands.loop_steps);

    /* need to check with flipping the bit as no uint16 gt comparision on AVX2 */
    __m256i sign_flip = _mm256_set1_epi16((int16_t)0x8000);  // 0x8000 = -32768
    __m256i a_signed = _mm256_xor_si256(loop_steps, sign_flip);
    __m256i b_signed = _mm256_xor_si256(after_increment, sign_flip);
    __m256i gt_mask = _mm256_cmpgt_epi16(b_signed, a_signed);

    __m256i new_step = _mm256_blendv_epi8(after_increment, _mm256_setzero_si256(), gt_mask);
    _mm256_storeu_si256((__m256i*)controller->midi_commands.current_step, new_step);

#else
    // Scalar fallback
    for(int i = 0; i < 16; ++i)
    {
        if (controller->midi_commands.current_step[i] == controller->midi_commands.next_command[i] && controller->active_channels & (1<<i))
            midi_command_launch(controller, i);
    }
    for (int i = 0; i < 16; ++i)
    {
        ++controller->midi_commands.current_step[i];
        if (controller->midi_commands.current_step[i] > controller->midi_commands.loop_steps[i])
            controller->midi_commands.current_step[i] = 0;
    }
#endif
}


MIDI_INLINE void* midi_thread_loop(void* arg)
{
    MIDI_Controller* controller = (MIDI_Controller*)arg;

    while (1)
    {
        pthread_mutex_lock(&controller->mutex);
        while(!(controller->flags & MIDI_CLOCK_COMMAND_SENT))
            pthread_cond_wait(&controller->cond, &controller->mutex);
        controller->flags &= ~MIDI_CLOCK_COMMAND_SENT;

        //destory thread if flag is set
        if(controller->flags & MIDI_INTERFACE_DESTORY)
        {
            pthread_mutex_unlock(&controller->mutex);
            break;
        }

        midi_increment_step_count_simd(controller);

        //push processed commands
        if (controller->commands_processed > 0)
        {
            uint8_t difference = controller->command_count - controller->commands_processed;
            //printf("%u differ\n", difference);
            memmove(&controller->commands[0], &controller->commands[controller->commands_processed], difference * sizeof(MIDI_Command));
            memset(&controller->commands[difference], 0, controller->commands_processed);

            controller->commands_processed = 0;
            controller->command_count = difference;
        }


        pthread_mutex_unlock(&controller->mutex);
    }



    return NULL;
}



MIDI_INLINE void midi_controller_destrory(MIDI_Controller* controller)
{
    pthread_mutex_lock(&controller->mutex);
    controller->flags |= (MIDI_INTERFACE_DESTORY | MIDI_CLOCK_COMMAND_SENT);
    pthread_cond_signal(&controller->cond);
    pthread_mutex_unlock(&controller->mutex);

    for (uint8_t i = 0; i < MIDI_MAX_CHANNELS; ++i)
    {
        if (controller->active_channels & (1<<i))
        {
            Channel_Node* node = controller->midi_commands.channel[i];
            for (uint8_t j = 0; j < controller->midi_commands.node_count[i]; ++j)
            {
                Channel_Node* node_to_free = node;
                node = node->next;
                free(node_to_free);
            }
        }
    }
}

MIDI_INLINE Channel_Node* midi_command_node(const uint8_t command_byte, const uint8_t param1, const uint8_t param2, uint16_t on_tick)
{
    //allowing for const members to be set
    Channel_Node node_heap = {{command_byte, param1, param2}, on_tick, NULL};
    Channel_Node* node = malloc(sizeof(Channel_Node));
    memcpy(node, &node_heap, sizeof(Channel_Node));

    return node;
}

#define LOOP_SAFETY_TRIGGERED -9999
MIDI_INLINE int midi_place_next_node(Channel_Node* first_node, Channel_Node* next_node)
{
    Channel_Node* node = first_node;
    int safety = 0;

    while (node->next != NULL && safety < 1000)
    {
        node = node->next;
        ++safety;
    }

    if (safety == 1000)
        return LOOP_SAFETY_TRIGGERED;

    node->next = next_node;

    return 0;
}

MIDI_INLINE MIDI_Channels midi_channel_parse(const uint8_t channel)
{
    switch (channel)
    {
    case 1:
        return MIDI_CHANNEL_1;
    case 2:
        return MIDI_CHANNEL_2;
    case 3:
        return MIDI_CHANNEL_3;
    case 4:
        return MIDI_CHANNEL_4;
    case 5:
        return MIDI_CHANNEL_5;
    case 6:
        return MIDI_CHANNEL_6;
    case 7:
        return MIDI_CHANNEL_7;
    case 8:
        return MIDI_CHANNEL_8;
    case 9:
        return MIDI_CHANNEL_9;
    case 10:
        return MIDI_CHANNEL_10;
    case 11:
        return MIDI_CHANNEL_11;
    case 12:
        return MIDI_CHANNEL_12;
    case 13:
        return MIDI_CHANNEL_13;
    case 14:
        return MIDI_CHANNEL_14;
    case 15:
        return MIDI_CHANNEL_15;
    case 16:
        return MIDI_CHANNEL_16;
    default:
        assert(0 && "ERROR - Invalid Channel parsed\n");
        return MIDI_CHANNEL_UNDEFINED;
    }
}

enum
{
    LINE_NOT_DEFINED = 0,
    LINE_CHANNEL = 1,
    LINE_LOOP = 2, //how many bars in the whole loop
    LINE_SEQUENCE = 3
};

MIDI_INLINE MIDI_Command_type midi_get_command_sequence(const char* command)
{
    if (strncmp(command, "ON", 2) == 0)
        return MIDI_NOTE_ON;
    else if (strncmp(command, "OFF", 3) == 0)
        return MIDI_NOTE_OFF;
    else
        return MIDI_COMMAND_INVALID;
}

MIDI_INLINE int midi_parse_commands(MIDI_Controller* controller, const char* filepath)
{
    FILE* file = fopen(filepath, "r");
    if (file == 0)
    {
        printf("ERROR - midi commands file cannot be opened\n");
        return -1;
    }

    char buffer[1000];
    memset(buffer, 0, sizeof(char) * 1000);

    int channel = -1;
    uint8_t line = LINE_NOT_DEFINED;
    uint32_t loop_ticks;

    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        if (buffer[0] == '{')
        {
            line = LINE_CHANNEL;
            continue;
        }
        else if (buffer[0] == '}')
        {
            channel = -1;
            line = LINE_NOT_DEFINED;
            continue;
        }
        //DEBUG_PRINT("\nBuffer: %s", buffer);

        switch (line)
        {
        case LINE_CHANNEL:
        {
            int channel_parse = 0;
            char tmp[50] = {0};
            if (sscanf(buffer, "%s %d,", tmp, &channel_parse) != 2)
                printf("WARNING: wrong amount of data parsed by %d channel?", channel_parse);

            if (channel_parse >= 1 && channel_parse <= 16)
            {
                channel = channel_parse;
                line = LINE_LOOP;
            }
            else
            {
                printf("ERROR - channel parsed incorrectly, check .midi file. %s %d", tmp, channel_parse);
                return -1;
            }
            DEBUG_PRINT("channel_parsed: %u\n", channel);
            break;
        }
        case LINE_LOOP:
        {
            float loop_parse = 0;
            char tmp[50] = {0};
            if (sscanf(buffer, "%s %f,", tmp, &loop_parse) != 2)
                printf("WARNING: wrong amount of data parsed by channel %d - %f loop?", channel, loop_parse);

            if (loop_parse > 0)
            {
                loop_ticks = loop_parse * MIDI_TICKS_PER_BAR;
                line = LINE_SEQUENCE;
            }
            else
            {
                printf("ERROR - channel %d loop parsed incorrectly, check .midi file. %s %f", channel, tmp, loop_parse);
                return -1;
            }
            if (loop_ticks % 24 != 0)
                printf("WARNING - loop is not quater note aligned\n");

            DEBUG_PRINT("loop_bars: %0.3, loop_ticks: %u\n", loop_parse, loop_ticks);


            break;
        }
        case LINE_SEQUENCE:
        {
            uint16_t node_count = 0;
            if (channel < 1 || channel > 16)
            {
                printf("ERROR - channel not know when parsing line sequences");
                return -1;
            }

            Channel_Node* first_node = NULL;


            char* token = strtok(buffer, " ");
            while (token != NULL)
            {
                if (node_count == 65535)
                {
                    printf("ERROR - Maximum number of nodes hit\n");
                    return -1;
                }
                ++node_count;
                Channel_Node* next_node = NULL;
                const uint8_t command = midi_get_command_sequence(token);
                switch(command)
                {
                case MIDI_NOTE_ON:
                {
                    float frequency, placement;
                    uint8_t velocity;
                    sscanf(token, "ON(%f,%hhu,%f)", &frequency, &velocity, &placement);
                    //printf("frequency: %f, velocity: %u, placement: %f\n", frequency, velocity, placement);

                    const uint16_t on_tick = (placement <= 1) ? 0 : (placement - 1) * 24;
                    next_node = midi_command_node(MIDI_NOTE_ON | midi_channel_parse((uint8_t)channel),
                                                  midi_frequency_to_midi_note(frequency), velocity > 127 ? 127 : velocity,
                                                  on_tick);
                    break;
                }
                case MIDI_NOTE_OFF:
                {
                    float placement;
                    sscanf(token, "OFF(%f)", &placement);
                    const uint16_t on_tick = (placement <= 1) ? 0 : (placement - 1) * 24;
                    next_node = midi_command_node(MIDI_NOTE_OFF | midi_channel_parse((uint8_t)channel), 0, 0, on_tick);
                    break;
                }
                case MIDI_COMMAND_INVALID:
                    printf("ERROR - invalid command parsed");
                    return -1;
                }

                DEBUG_PRINT("Node - command: %u, param1: %u, param2: %u, on_tick: %u\n",
                            next_node->command.command_byte,
                            next_node->command.param1,
                            next_node->command.param2,
                            next_node->on_tick);

                if (first_node == NULL)
                {
                    first_node = next_node;
                    controller->midi_commands.next_command[channel -1] = first_node->on_tick;
                }
                else
                {
                    int check = midi_place_next_node(first_node, next_node);
                    if (check == LOOP_SAFETY_TRIGGERED)
                    {
                        assert(0 && "ERROR - LOOP_SAFETY_TRIGGERED\n");
                    }
                    else if (check != 0)
                    {
                        printf("ERROR - couldn't find next free node\n");
                    }
                }
                token = strtok(NULL, " ");
            }
            //complete the loop and initalize the channel settings
            int check = midi_place_next_node(first_node, first_node);
            if (check == LOOP_SAFETY_TRIGGERED)
            {
                assert(0 && "ERROR - LOOP_SAFETY_TRIGGERED\n");
                printf("WARNING - loop saftey triggered\n");
            }
            else if (check != 0)
            {
                printf("ERROR - couldn't find next free node\n");
            }
            controller->midi_commands.node_count[channel -1] = node_count;
            controller->midi_commands.loop_steps[channel -1] = loop_ticks;
            controller->midi_commands.channel[channel -1] = first_node;
            controller->active_channels |= (1<<(channel -1));
            break;
        }
        }
    }
    return 0;
}

MIDI_INLINE void midi_controller_set(MIDI_Controller* controller, const char* filepath)
{
    for (uint8_t i = 0; i < MIDI_COMMAND_MAX_COUNT; ++i)
        memset(&controller->commands[i], 0, sizeof(MIDI_Command));

    controller->command_count = 0;
    controller->commands_processed = 0;
    controller->flags = 0;
    pthread_t midi_interface_thread;
    pthread_create(&midi_interface_thread, NULL, midi_thread_loop, controller);
    pthread_detach(midi_interface_thread);

    if (filepath != NULL)
    {
        midi_parse_commands(controller, filepath);

        // setting the inactive channels to max for simd comparision optimisation
        for (uint8_t i = 0; i < MIDI_MAX_CHANNELS; ++i)
        {
            if (!(controller->active_channels & (1<<i)))
                --controller->midi_commands.loop_steps[i];
        }
    }
}

MIDI_INLINE void midi_command_byte_parse(uint8_t commmand_byte, uint8_t* out_type, uint8_t* out_channel)
{
    *out_type = commmand_byte & MIDI_COMMAND_TYPE_BYTE_MASK;
    *out_channel = commmand_byte & MIDI_CHANNEL_BYTE_MASK;
}

MIDI_INLINE void midi_command_clock(MIDI_Controller* controller)
{
    assert(controller->command_count < MIDI_COMMAND_MAX_COUNT);
    pthread_mutex_lock(&controller->mutex);
    controller->commands[controller->command_count++].command_byte = MIDI_SYSTEM_MESSAGE | MIDI_CLOCK;
    controller->flags |= MIDI_CLOCK_COMMAND_SENT;
    pthread_cond_signal(&controller->cond);
    pthread_mutex_unlock(&controller->mutex);
}

MIDI_INLINE void midi_note_on(MIDI_Controller* controller, MIDI_Channels channel, float frequency, uint8_t velocity)
{
    assert(controller->command_count < MIDI_COMMAND_MAX_COUNT);
    pthread_mutex_lock(&controller->mutex);
    MIDI_Command* command = &controller->commands[controller->command_count++];
    command->command_byte = MIDI_NOTE_ON | channel;
    command->param1 = midi_frequency_to_midi_note(frequency);
    command->param2 = velocity;
    pthread_mutex_unlock(&controller->mutex);
}

MIDI_INLINE void midi_note_off(MIDI_Controller* controller, MIDI_Channels channel)
{
    assert(controller->command_count < MIDI_COMMAND_MAX_COUNT);
    pthread_mutex_lock(&controller->mutex);
    MIDI_Command* command = &controller->commands[controller->command_count++];
    command->command_byte = MIDI_NOTE_OFF | channel;
    pthread_mutex_unlock(&controller->mutex);
}

MIDI_INLINE uint8_t midi_frequency_to_midi_note(float frequency)
{
    if (frequency < 8)
        return 0;

    uint8_t note = 69.0f + 12.0f * log2f(frequency/ 440.0f);
    if (note > 127)
        return 127;
    else
        return note;
}

MIDI_INLINE float midi_note_to_frequence(uint8_t midi_note)
{
    return 440.0f * pow(2.0f, ((float)midi_note - 69.0f) / 12.0f);
}

#endif // MIDI_INTERFACE_IMPLEMENTATION
