#include <stdio.h>
#define MIDI_INTERFACE_IMPLEMENTATION
#include "MIDI_interface.h"

void process(MIDI_Controller* controller)
{

    pthread_mutex_lock(&controller->mutex);
    DEBUG_PRINT("Processing... command count: %u\n", controller->command_count);
    for (int i = 0; i < controller->command_count; ++i)
    {
        // if (controller->commands[i].command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_CLOCK))
        //     printf("CLOCK CLOCK CLOCK \n");
        // else
        //     printf("ANOTHER COMMAND\n");
        // print_binary_8(controller->commands[i].command_byte);
        // print_binary_8(controller->commands[i].param1);
        // print_binary_8(controller->commands[i].param2);
        ++controller->commands_processed;
    }
    pthread_mutex_unlock(&controller->mutex);
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Input with the first arg the amount of cycles you'd like to simulate\n");
        return -1;
    }
    MIDI_Controller controller = {0};
    midi_controller_set(&controller, "inputs.midi", "/dev/snd/midiC2D0");

    uint32_t count = atoi(argv[1]);
    uint32_t counter = 0;
    midi_start(&controller);
    midi_message_send(&controller, MIDI_CONTINUOUS_CONTROLLER | 0x2, 0b01100110, 0xF0 | 0x0F);
    while (counter < count)
    {
        midi_command_clock(&controller);

        process(&controller);

        ++counter;
        usleep(16666);
    }


    midi_stop(&controller);

    midi_controller_destrory(&controller);

    return 0;
}
