#ifndef TERMIOS_H
#define TERMIOS_H

#define TCSAFLUSH 2

#define ECHO 1
#define ISIG 2

typedef unsigned int tcflag_t;

struct termios {
	tcflag_t c_lflag;
};

int tcgetattr(int fd, struct termios *term);
int tcsetattr(int fd, int opt_acts, const struct termios *term);

#endif /* TERMIOS_H */
