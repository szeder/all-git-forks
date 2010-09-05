#ifndef SVNDIFF_H_
#define SVNDIFF_H_
#include "../git-compat-util.h"

enum svndiff_action {
    copyfrom_source,
    copyfrom_target,
    copyfrom_new
};

struct svndiff_instruction
{
  enum svndiff_action action_code;
  size_t offset;
  size_t length;
};

/* An svndiff_window object describes how to reconstruct a
 * contiguous section of the target string (the "target view") using a
 * specified contiguous region of the source string (the "source
 * view").  It contains a series of instructions which assemble the
 * new target string text by pulling together substrings from:
 *
n *   - the source view,
 *
 *   - the previously constructed portion of the target view,
 *
 *   - a string of new data contained within the window structure
 *
 * The source view must always slide forward from one window to the
 * next; that is, neither the beginning nor the end of the source view
 * may move to the left as we read from a window stream.  This
 * property allows us to apply deltas to non-seekable source streams
 * without making a full copy of the source stream.
 */
struct svndiff_window
{
  size_t sview_offset;
  size_t sview_len;
  size_t tview_len;
  size_t ins_len;
  size_t newdata_len;
  struct svndiff_instruction *ops;
  char *newdata;
};

size_t read_header(void);
void svndiff_apply(FILE *src_fd);

#endif
