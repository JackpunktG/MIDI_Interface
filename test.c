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
    DEBUG_PRINT("Processing... command count: %u, commands processed %u\n", controller->command_count, controller->commands_processed);
    for (uint8_t i = controller->commands_processed; i < controller->command_count; ++i)
    {
        if (controller->commands[i].command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_CLOCK))
            printf("\033[0;34m" "CLOCK CLOCK CLOCK \n" "\033[0m");
        else
            printf("\033[0;35m" "ANOTHER COMMAND\n" "\033[0m");
        // print_binary_8(controller->commands[i].command_byte);
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
        printf("./out <seconds_to_simulate> <external_midi_connection_path*>\n*optional\n");
        return -1;
    }
    MIDI_Controller controller = {0};
    if (argc >= 3)
    {
        printf("Using external MIDI connection: \"%s\"\n", argv[2]);
        if (midi_controller_set(&controller, "inputs.midi", argv[2], EXTERNAL_INPUT_THROUGH| EXTERNAL_INPUT_CLOCK) != MIDI_SETUP_SUCCESS)
        {
            printf("ERROR - failed to set up MIDI controller with external connection. Exiting\n");
            return -1;
        }
    }
    else if (midi_controller_set(&controller, "inputs.midi", NULL, EXTERNAL_INPUT_INACTIVE) != MIDI_SETUP_SUCCESS)
    {
        printf("ERROR - failed to set up MIDI controller without external connection. Exiting\n");
        return -1;
    }


    uint32_t count = 60 * atoi(argv[1]); // roughly 60 ticks per second
    uint32_t counter = 0;
    midi_clock_set(&controller, 130);
    midi_start(&controller);

    //showing different ways you can construct a MIDI message
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

    printf("\033[0;32m" "Test successfuly completed\n" "\033[0m");



    return 0;
}
