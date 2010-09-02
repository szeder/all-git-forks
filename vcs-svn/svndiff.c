#include <stdio.h>
#include "svndiff.h"
#include <stdarg.h>
#include <stdlib.h>

#define MAX_ENCODED_INT_LEN 10

/* Remove when linking to gitcore */
void die(const char *err, ...)
{
	va_list params;
	va_start(params, err);
	vfprintf(stderr, err, params);
	va_end(params);
	exit(128);
}

/* Decode an svndiff-encoded integer into VAL and return a pointer to
   the byte after the integer.  The bytes to be decoded live in the
   range [P..END-1]. */

/* This encoding uses the high bit of each byte as a continuation bit
   and the other seven bits as data bits.  High-order data bits are
   encoded first, followed by lower-order bits, so the value can be
   reconstructed by concatenating the data bits from left to right and
   interpreting the result as a binary number.  Examples (brackets
   denote byte boundaries, spaces are for clarity only):

   1 encodes as [0 0000001]
   33 encodes as [0 0100001]
   129 encodes as [1 0000001] [0 0000001]
   2000 encodes as [1 0001111] [0 1010000]
*/

const unsigned char *decode_size(int *val,
				 const unsigned char *p,
				 const unsigned char *end)
{
	int temp = 0;

	if (p + MAX_ENCODED_INT_LEN < end)
		end = p + MAX_ENCODED_INT_LEN;

	while (p < end)
	{
		int c = *p++;

		temp = (temp << 7) | (c & 0x7f);
		if (c < 0x80)
		{
			*val = temp;
			return p;
		}
	}

	*val = temp;
	return NULL;
}

/* Decode an instruction into OP, returning a pointer to the text
   after the instruction.  Note that if the action code is
   svn_txdelta_new, the offset field of *OP will not be set.  */
const unsigned char *decode_instruction(struct svndiff_op *op,
					const unsigned char *p,
					const unsigned char *end)
{
	int c, action;

	if (p == end)
		return NULL;

	/* We need this more than once */
	c = *p++;

	/* Decode the instruction selector */
	action = (c >> 6) & 0x3;
	if (action >= 0x3)
		return NULL;

	op->action_code = (enum svndiff_action)(action);

	/* Decode the length and offset */
	op->length = c & 0x3f;
	if (op->length == 0)
	{
		p = decode_size(&op->length, p, end);
		if (p == NULL)
			return NULL;
	}
	if (action != svn_txdelta_new)
	{
		p = decode_size(&op->offset, p, end);
		if (p == NULL)
			return NULL;
	}

	return p;
}

int count_instructions(const unsigned char *p,
		       const unsigned char *end,
		       int sview_len,
		       int tview_len,
		       int new_len)
{
	int n = 0;
	struct svndiff_op *op;
	int tpos = 0, npos = 0;

	while (p < end)
	{
		p = decode_instruction(op, p, end);

		if (p == NULL)
			die("Invalid diff stream: insn %d cannot be decoded", n);
		else if (op->length == 0)
			die("Invalid diff stream: insn %d has length zero", n);
		else if (op->length > tview_len - tpos)
			die("Invalid diff stream: insn %d overflows the target view", n);

		switch (op->action_code)
		{
		case svn_txdelta_source:
			if (op->length > sview_len - op->offset ||
			    op->offset > sview_len)
				die("Invalid diff stream: [src] insn %d overflows the source view", n);
			break;
		case svn_txdelta_target:
			if (op->offset >= tpos)
				die("Invalid diff stream: [tgt] insn %d starts beyond the target view position", n);
			break;
		case svn_txdelta_new:
			if (op->length > new_len - npos)
				die("Invalid diff stream: [new] insn %d overflows the new data section", n);
			npos += op->length;
			break;
		}
		tpos += op->length;
		n++;
	}
	if (tpos != tview_len)
		die("Delta does not fill the target window");
	if (npos != new_len)
		die("Delta does not contain enough new data");

	return n;
}

int decode_window(struct svndiff_window *window, int sview_offset,
		  int sview_len, int tview_len, int inslen,
		  int newlen, const unsigned char *data)
{
	const unsigned char *insend;
	int ninst;
	int npos;
	struct svndiff_op *ops, *op;

	window->sview_offset = sview_offset;
	window->sview_len = sview_len;
	window->tview_len = tview_len;

	insend = data + inslen;

	/* Count the instructions and make sure they are all valid.  */
	ninst = count_instructions(data, insend, sview_len, tview_len, newlen);

	/* Allocate a buffer for the instructions and decode them. */
	ops = malloc(ninst * sizeof(*ops));
	npos = 0;
	window->src_ops = 0;
	for (op = ops; op < ops + ninst; op++)
	{
		data = decode_instruction(op, data, insend);
		if (op->action_code == svn_txdelta_source)
			++window->src_ops;
		else if (op->action_code == svn_txdelta_new)
		{
			op->offset = npos;
			npos += op->length;
		}
	}
	if (data != insend)
		die("data != insend");

	window->ops = ops;
	window->num_ops = ninst;

	return 0;
}

int main()
{
	char buf[100];
	int version;

	/* Read off the 4-byte header */
	size_t hdr_size = 4, instruction_size = 8;
	fread(&buf, hdr_size, 1, stdin);
	version = atoi(buf + 3);
	if (version != 0)
		die("Version %d unsupported", version);
	return 0;
}
