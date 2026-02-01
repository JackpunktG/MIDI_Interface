# MIDI_Interface
A single file header-only 'stb - style' C library for interfacing with MIDI devices. This library provides functions to send and receive MIDI messages, making it easy to integrate MIDI functionality into your C projects.
Being able to parse MIDI instructions for synthesizers, drum machines, and other MIDI-capable devices.

## Status -> Work in Progress
This library is currently a work in progress. Basic functionality for sending MIDI Clock, Note On, and Note Off has been implemented, but additional features and improvements are in the works. 

## Installation
To use the MIDI_Interface library, simply include the `MIDI_Interface.h` header file in your project and define `MIDI_INTERFACE_IMPLEMENTATION` in one source file before including it:

```c
#define MIDI_INTERFACE_IMPLEMENTATION
#include "path_to/MIDI_Interface.h"
```

## Usage
Firstly set up the MIDI Interface of the stack and pass the structure into the set up function
```c 
MIDI_Controller controller = {0};
midi_controller_set(&controller, "path_to_midi_commands"); // Filepath can be NULL if only using internal commands and clock
```

Then in your audio loop, call the midi_command_clock 24 times per quarter note to keep the MIDI clock running and in sync with your audio engine.
```c
MIDI_INLINE void midi_command_clock(MIDI_Controller* controller);
```

Once the program is over call the cleanup function to free any allocated memory and kill the midi thread
```c
midi_controller_destrory(&controller);
```

### Interpreting MIDI Commands 
The power is in your hands how you want to interpret the MIDI commands. In the controller is a stack of MIDI commands which will be filled by the midi thread. A simple example of interpreting the commands is shown below:
```c
    if (midi_controller->command_count == 0)
        return;

    pthread_mutex_lock(midi_controller->mutex);

    for(uint8_t i = midi_controller->commands_processed; i < midi_controller->command_count; ++i)
    {
        MIDI_Command command = midi_controller->commands[i];

        uint8_t command_nibble = 0;
        uint8_t channel = 0;
        midi_command_byte_parse(command.command_byte, &command_nibble, &channel); // Helper function to parse command byte
        switch(command_nibble)
        {
        case MIDI_SYSTEM_MESSAGE:
            if (command.command_byte == (MIDI_SYSTEM_MESSAGE | MIDI_CLOCK))
            beat_sync(...)    ; // DO ACTION WITH MIDI CLOCK
            else
                printf("WARNING - MIDI system message not recognised\n");
            break;
        case MIDI_NOTE_OFF:
            note_off(..., channel); // DO ACTION WITH NOTE OFF
            break;
        case MIDI_NOTE_ON:
            note_on(..., channel, command.param1, command.param2); // DO ACTION WITH NOTE ON
            break;
        case MIDI_AFTERTOUCH:
        case MIDI_CONTINUOUS_CONTROLLER:
        case MIDI_PATCH_CHANGE:
        case MIDI_CHANEL_PRESSURE:
        case MIDI_PITCH_BEND:
            printf("WARNING - midi command not yet implmented\n");
            break;
        default:
            assert(false && "ERROR - unknown MIDI command\n");
        }
        ++midi_controller->commands_processed; // IMPORTANT TO INCREMENT THE PROCESSED COMMAND COUNT
    }
    pthread_mutex_unlock(&midi_controller->mutex);
```
In this example, the MIDI commands are processed in a loop, and actions are taken based on the command type (e.g., Note On, Note Off, MIDI Clock). Make sure to handle thread safety by locking the mutex while accessing the command stack.
Most importantly, remember to increment the `commands_processed` count to keep track of which commands have been handled, so next the midi thread wakes up from a clock it removes them off the stack correctly.

### Helper Functions
The library includes some helper functions to assist with MIDI command parsing:

```c
MIDI_INLINE void midi_command_byte_parse(uint8_t command_byte, uint8_t* command_nibble, uint8_t* channel);
```
This function extracts the command nibble and channel from a MIDI command byte. 

```c
MIDI_INLINE float midi_note_to_frequence(uint8_t midi_note);
MIDI_INLINE uint8_t midi_frequency_to_midi_note(float frequency);
```
These functions convert between MIDI note numbers (0 - 127) and their corresponding frequencies in Hertz. Or vice versa.

## MIDI Parser File Format
The MIDI parser can read commands from a text file. Commands are in the following format (In this order):
```midi_command
{
CHANNEL: 2  
loop_bars: 1
ON(4000,64,1.5) OFF(2.25)  ON(4500,127,3) OFF(4)
}
```
- `CHANNEL`: Specifies the MIDI channel (1-16).
- `loop_bars`: Number of bars to loop the sequence.
- `ON(frequency,velocity,placement)`: frequency in Hz, velocity (0-127), and placment (1 - loop_bars end) in a decimal format for inbetween beats.
- `OFF(placement)`: placement (1 - loop_bars end) in a float format.
- `placement` end is calculated based on a 4/4 time signature. so for example, in 1 bar of 4/4 time, placement 1.5 would be on the "and" of the first. and 4.999 would be just before the downbeat of the next bar and the last possible vaule for a 1 bar loop.


## test.c
A simple test program is included to demonstrate the usage of the MIDI_Interface library. The test program initializes the MIDI controller, processes MIDI commands, and cleans up resources.
To compile the test program, use the following command:
```bash
debug: gcc -march=native -DDEBUG -Wall -Wextra -g -O0 test.c -lm -o out
```
Have fun <3

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
