#ifndef MBOX_H
#define MBOX_H

/*
 * Returns true if the line appears to be an mbox "From" line starting a new
 * message.
 */
int is_from_line(const char *line, int len);

/*
 * Returns true if the line appears to be a "From" line starting a new
 * message in the format-patch output.
 */
int is_format_patch_separator(const char *line, int len);

#endif /* MBOX_H */
