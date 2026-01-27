#include <stdio.h>
//#define MIDI_INTERFACE_IMPLEMENTATION
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

    print_binary(NULL, controller.active_channels, NULL);
    uint8_t i = 0;

    for (int i = 0; i < 16; ++i)
    {
        if (controller.active_channels & (1<<i))
        {
            printf("Loop ticks %u on channel %d first tick on: %u\n", controller.midi_commands.loop_steps[i], i +1, controller.midi_commands.next_command[i]);
            Channel_Node* command = controller.midi_commands.channel[i];
            while(1)
            {
                print_binary(NULL, NULL, command->command.command_byte);
                print_binary(NULL, NULL, command->command.param1);
                print_binary(NULL, NULL, command->command.param2);
                printf("step count: %u\n\n", command->on_tick);
                command = command->next;
                if (command->on_tick == 0)
                    break;
            }
        }
    }

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
