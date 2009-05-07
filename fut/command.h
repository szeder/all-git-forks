#ifndef COMMAND_H
#define COMMAND_H

typedef int (*command_func)(int, char const* const*);

typedef struct {
    char const* name;
    command_func func;
    char const* short_help;
    char const* long_help;
} command_t;

#endif // COMMAND_H

