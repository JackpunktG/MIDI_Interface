#include <stdio.h>
#define MIDI_INTERFACE_IMPLEMENTATION
#include "MIDI_interface.h"

void process(MIDI_Controller* controller)
{
    pthread_mutex_lock(&controller->mutex);
    if (controller->command_count == controller->commands_processed)
    {
        pthread_mutex_unlock(&controller->mutex);
        return;
    }
    DEBUG_PRINT("Processing... command count: %u, commands processed %u\n", controller->command_count - controller->commands_processed);
    for (uint8_t i = controller->commands_processed; i < controller->command_count; ++i)
    {
        // if (controller->commands[i].command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_CLOCK))
        //     printf("CLOCK CLOCK CLOCK \n");
        // else
        //     printf("ANOTHER COMMAND\n");
        // // print_binary_8(controller->commands[i].command_byte);
        // print_binary_8(controller->commands[i].param1);
        // print_binary_8(controller->commands[i].param2);
        ++controller->commands_processed;
    }
    pthread_mutex_unlock(&controller->mutex);
}

int main(int argc, char* argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2)
    {
        printf("Input with the first arg the amount of cycles you'd like to simulate\n");
        return -1;
    }
    MIDI_Controller controller = {0};
    midi_controller_set(&controller, "inputs.midi", "/dev/snd/midiC1D0", EXTERNAL_INPUT_INACTIVE);

    uint32_t count = atoi(argv[1]);
    uint32_t counter = 0;
    midi_clock_set(&controller, 130);
    midi_start(&controller);
    midi_message_send(&controller, MIDI_CONTINUOUS_CONTROLLER | 0x2, 0b01100110, 0xF0 | 0x0F);
    while (counter < count)
    {
        //midi_command_clock(&controller);

        process(&controller);

        ++counter;
        usleep(16666);
    }


    midi_stop(&controller);

    midi_controller_destrory(&controller);

    return 0;
}
