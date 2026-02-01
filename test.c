#include <stdio.h>
#define MIDI_INTERFACE_IMPLEMENTATION
#include "MIDI_interface.h"
#include <unistd.h>

void process(MIDI_Controller* controller)
{

    pthread_mutex_lock(&controller->mutex);
    DEBUG_PRINT("Processing... command count: %u\n", controller->command_count);
    for (int i = 0; i < controller->command_count; ++i)
    {
        if (controller->commands[i].command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_CLOCK))
            printf("CLOCK CLOCK CLOCK \n");
        else
            printf("ANOTHER COMMAND\n");
        ++controller->commands_processed;
    }
    pthread_mutex_unlock(&controller->mutex);
}

int main(int argc, char* argv[])
{
    MIDI_Controller controller = {0};
    midi_controller_set(&controller, "inputs.midi");

    uint32_t count = atoi(argv[1]);
    uint32_t counter = 0;
    while (counter < count)
    {
        midi_command_clock(&controller);

        process(&controller);

        ++counter;
        usleep(16666);
    }

    midi_controller_destrory(&controller);

    return 0;
}
