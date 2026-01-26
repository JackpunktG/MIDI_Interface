#include <stdio.h>
#include "MIDI_interface.h"
#include <unistd.h>
#include <time.h>

void process(MIDI_Controller* controller)
{

    pthread_mutex_lock(&controller->mutex);
    for (int i = 0; i < controller->command_count; ++i)
    {
        if (controller->commands[i].command_byte == (MIDI_NOTE_ON | MIDI_CHANNEL_1))
            printf("NOTE ON %u\n", controller->commands[i].param1);
        else if (controller->commands[i].command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_CLOCK))
            printf("CLOCK CLOCK CLOCK \n");
        ++controller->commands_processed;
    }
    pthread_mutex_unlock(&controller->mutex);
}

int main()
{
    MIDI_Controller controller = {};
    midi_controller_set(&controller, "inputs.midi");
    srand(time(NULL));

    for (int i = 0; i < 16; ++i)
        controller.midi_commands.steps_to_next[i] = (rand() % 25) + 2;

    for (int i = 0; i< 16; ++i)
        controller.active_channels |= (1<<i);

    short counter = 1;
    while (counter < 50)
    {




        midi_command_clock(&controller);


        ++counter;
        usleep(16666);
    }

    midi_controller_destrory(&controller);

    return 0;
}
