#include "cache.h"
#include "pkt-line.h"

char packet_buffer[LARGE_PACKET_MAX];
static const char *packet_trace_prefix = "git";
static const char trace_key[] = "GIT_TRACE_PACKET";

void packet_trace_identity(const char *prog)
{
	packet_trace_prefix = xstrdup(prog);
}

void packet_trace(const char *buf, unsigned int len, int write)
{
	int i;
	struct strbuf out;

	if (!trace_want(trace_key))
		return;

	/* +32 is just a guess for header + quoting */
	strbuf_init(&out, len+32);

	strbuf_addf(&out, "packet: %12s%c ",
		    packet_trace_prefix, write ? '>' : '<');

	if ((len >= 4 && starts_with(buf, "PACK")) ||
	    (len >= 5 && starts_with(buf+1, "PACK"))) {
		strbuf_addstr(&out, "PACK ...");
		unsetenv(trace_key);
	}
	else if (starts_with(buf, "watch ") && len > 70)
		strbuf_addstr(&out, "watch ...");
	else if (starts_with(buf, "changed ") && len > 70)
		strbuf_addstr(&out, "changed ...");
	else if (starts_with(buf, "unchange ") && len > 70)
		strbuf_addstr(&out, "unchange ...");
	else {
		/* XXX we should really handle printable utf8 */
		for (i = 0; i < len; i++) {
			/* suppress newlines */
			if (buf[i] == '\n')
				continue;
			if (buf[i] >= 0x20 && buf[i] <= 0x7e)
				strbuf_addch(&out, buf[i]);
			else
				strbuf_addf(&out, "\\%o", buf[i]);
		}
	}

	strbuf_addch(&out, '\n');
	trace_strbuf(trace_key, &out);
	strbuf_release(&out);
}

/*
 * If we buffered things up above (we don't, but we should),
 * we'd flush it here
 */
void packet_flush(int fd)
{
	packet_trace("0000", 4, 1);
	write_or_die(fd, "0000", 4);
}

void packet_buf_flush(struct strbuf *buf)
{
	packet_trace("0000", 4, 1);
	strbuf_add(buf, "0000", 4);
}

#define hex(a) (hexchar[(a) & 15])
unsigned format_packet(struct strbuf *sb,
		       const char *fmt, va_list args)
{
	static char hexchar[] = "0123456789abcdef";
	unsigned n = sb->len;
	strbuf_grow(sb, 4);
	strbuf_setlen(sb, sb->len + 4);
	strbuf_vaddf(sb, fmt, args);
	n = sb->len - n;
	if (n > 0xffff)
		die("protocol error: impossibly long line");
	sb->buf[sb->len - n + 0] = hex(n >> 12);
	sb->buf[sb->len - n + 1] = hex(n >> 8);
	sb->buf[sb->len - n + 2] = hex(n >> 4);
	sb->buf[sb->len - n + 3] = hex(n);
	return n;
}

void packet_write(int fd, const char *fmt, ...)
{
	static struct strbuf sb = STRBUF_INIT;
	va_list args;
	unsigned n;

	va_start(args, fmt);
	strbuf_reset(&sb);
	n = format_packet(&sb, fmt, args);
	va_end(args);
	packet_trace(sb.buf + 4, sb.len - 4, 1);
	write_or_die(fd, sb.buf, n);
}

void packet_buf_write(struct strbuf *buf, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	format_packet(buf, fmt, args);
	va_end(args);
	packet_trace(buf->buf + 4, buf->len - 4, 1);
}

void packet_buf_write_notrace(struct strbuf *buf, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	format_packet(buf, fmt, args);
	va_end(args);
}

static int get_packet_data(int fd, char **src_buf, size_t *src_size,
			   void *dst, unsigned size, int options,
			   unsigned timeout)
{
	ssize_t ret;

	if (fd >= 0 && src_buf && *src_buf)
		die("BUG: multiple sources given to packet_read");

	/* Read up to "size" bytes from our source, whatever it is. */
	if (src_buf && *src_buf) {
		ret = size < *src_size ? size : *src_size;
		memcpy(dst, *src_buf, ret);
		*src_buf += ret;
		*src_size -= ret;
	} else {
		if (!timeout)
			ret = read_in_full(fd, dst, size);
		else {
			struct pollfd pfd;
			pfd.fd = fd;
			pfd.events = POLLIN;
			if (poll(&pfd, 1, timeout) > 0 &&
			    (pfd.revents & POLLIN))
				ret = xread(fd, dst, size);
			else
				ret = -1;
		}
		if (ret < size) {
			if (options & PACKET_READ_GENTLE)
				return error("read error: %s",
					     strerror(errno));
			die_errno("read error");
		}
	}

	/* And complain if we didn't get enough bytes to satisfy the read. */
	if (ret < size) {
		if (options & PACKET_READ_GENTLE_ON_EOF)
			return -1;

		if (options & PACKET_READ_GENTLE)
			return error("The remote end hung up unexpectedly");
		die("The remote end hung up unexpectedly");
	}

	return ret;
}

int packet_length(const char *linelen)
{
	int n;
	int len = 0;

	for (n = 0; n < 4; n++) {
		unsigned char c = linelen[n];
		len <<= 4;
		if (c >= '0' && c <= '9') {
			len += c - '0';
			continue;
		}
		if (c >= 'a' && c <= 'f') {
			len += c - 'a' + 10;
			continue;
		}
		if (c >= 'A' && c <= 'F') {
			len += c - 'A' + 10;
			continue;
		}
		return -1;
	}
	return len;
}

int packet_read_timeout(int fd, char **src_buf, size_t *src_len,
			char *buffer, unsigned size, int options,
			unsigned timeout)
{
	int len, ret;
	char linelen[4];

	ret = get_packet_data(fd, src_buf, src_len, linelen, 4,
			      options, timeout);
	if (ret < 0)
		return ret;
	len = packet_length(linelen);
	if (len < 0) {
		if (options & PACKET_READ_GENTLE)
			return error("protocol error: bad line length character: %.4s",
				     linelen);
		die("protocol error: bad line length character: %.4s", linelen);
	}
	if (!len) {
		packet_trace("0000", 4, 0);
		return 0;
	}
	len -= 4;
	if (len >= size) {
		if (options & PACKET_READ_GENTLE)
			return error("protocol error: bad line length %d", len);
		die("protocol error: bad line length %d", len);
	}
	ret = get_packet_data(fd, src_buf, src_len, buffer, len,
			      options, timeout);
	if (ret < 0)
		return ret;

	if ((options & PACKET_READ_CHOMP_NEWLINE) &&
	    len && buffer[len-1] == '\n')
		len--;

	buffer[len] = 0;
	packet_trace(buffer, len, 0);
	return len;
}

int packet_read(int fd, char **src_buf, size_t *src_len,
			char *buffer, unsigned size, int options)
{
	return packet_read_timeout(fd, src_buf, src_len,
				   buffer, size, options, 0);
}

static char *packet_read_line_generic(int fd,
				      char **src, size_t *src_len,
				      int *dst_len)
{
	int len = packet_read(fd, src, src_len,
			      packet_buffer, sizeof(packet_buffer),
			      PACKET_READ_CHOMP_NEWLINE);
	if (dst_len)
		*dst_len = len;
	return len ? packet_buffer : NULL;
}

char *packet_read_line(int fd, int *len_p)
{
	return packet_read_line_generic(fd, NULL, NULL, len_p);
}

char *packet_read_line_buf(char **src, size_t *src_len, int *dst_len)
{
	return packet_read_line_generic(-1, src, src_len, dst_len);
}
