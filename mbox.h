#ifndef MBOX_H
#define MBOX_H

/*
 * Returns true if the line appears to be an mbox "From" line starting a new
 * message.
 */
int is_from_line(const char *line, int len);

#endif /* MBOX_H */
