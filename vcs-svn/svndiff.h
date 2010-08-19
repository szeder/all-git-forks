enum svndiff_action {
    svn_txdelta_source,
    svn_txdelta_target,
    svn_txdelta_new
};

struct svndiff_op
{
  enum svndiff_action action_code;
  int offset;
  int length;
};

/** An #svn_txdelta_window_t object describes how to reconstruct a
 * contiguous section of the target string (the "target view") using a
 * specified contiguous region of the source string (the "source
 * view").  It contains a series of instructions which assemble the
 * new target string text by pulling together substrings from:
 *
 *   - the source view,
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
typedef struct svn_txdelta_window_t
{
  /** The offset of the source view for this window.  */
  svn_filesize_t sview_offset;

  /** The length of the source view for this window.  */
  apr_size_t sview_len;

  /** The length of the target view for this window, i.e. the number of
   * bytes which will be reconstructed by the instruction stream.  */
  apr_size_t tview_len;

  /** The number of instructions in this window.  */
  int num_ops;

  /** The number of svn_txdelta_source instructions in this window. If
   * this number is 0, we don't need to read the source in order to
   * reconstruct the target view.
   */
  int src_ops;

  /** The instructions for this window.  */
  const svn_txdelta_op_t *ops;

  /** New data, for use by any `svn_txdelta_new' instructions.  */
  const svn_string_t *new_data;
};
