#include "cache.h"
#include "unix-socket.h"
#include "pkt-line.h"
#include "strbuf.h"

int main(int ac, char **av)
{
	struct strbuf sb = STRBUF_INIT;
	struct strbuf packed = STRBUF_INIT;
	char *packing = NULL;
	int last_command_is_reply = 0;
	int fd;

	strbuf_addf(&sb, "%s/socket", av[1]);
	fd = unix_stream_connect(sb.buf);
	if (fd < 0)
		die_errno("connect");
	strbuf_reset(&sb);

	/*
	 * test-file-watcher crashes sometimes, make sure to flush
	 */
	setbuf(stdout, NULL);

	while (!strbuf_getline(&sb, stdin, '\n')) {
		if (sb.buf[0] == '#') {
			puts(sb.buf);
			continue;
		}
		if (sb.buf[0] == '>') {
			if (last_command_is_reply)
				continue;
			last_command_is_reply = 1;
		} else
			last_command_is_reply = 0;

		if (sb.buf[0] == '<' && sb.buf[1] == '<') {
			puts(sb.buf);
			if (!packing) {
				packing = xstrdup(sb.buf + 2);
				strbuf_reset(&packed);
				continue;
			}
			if (!sb.buf[2]) {
				packet_write(fd, "%s %d", packing, (int)packed.len);
				if (packed.len)
					write_in_full(fd, packed.buf, packed.len);
				free(packing);
				packing = NULL;
			} else
				strbuf_add(&packed, sb.buf + 2, sb.len - 2 + 1);
			continue;
		}
		if (sb.buf[0] == '<') {
			packet_write(fd, "%s", sb.buf + 1);
			puts(sb.buf);
			continue;
		}
		if (sb.buf[0] == '>' && sb.buf[1] == '>') {
			int len;
			char *p, *reply = packet_read_line(fd, &len);
			if (!starts_with(reply, sb.buf + 2) ||
			    reply[sb.len - 2] != ' ') {
				printf(">%s\n", reply);
				continue;
			} else {
				p = reply + sb.len - 2;
				printf(">>%.*s\n", (int)(p - reply), reply);
				len = atoi(p + 1);
				if (!len)
					continue;
			}
			strbuf_reset(&packed);
			strbuf_grow(&packed, len);
			if (read_in_full(fd, packed.buf, len) <= 0)
				return 1;
			strbuf_setlen(&packed, len);
			for (p = packed.buf; p - packed.buf < packed.len; p += len + 1) {
				len = strlen(p);
				printf(">>%s\n", p);
			}
			continue;
		}
		if (sb.buf[0] == '>') {
			int len;
			char *reply = packet_read_line(fd, &len);
			if (!reply)
				puts(">");
			else
				printf(">%s\n", reply);
			continue;
		}
		die("unrecognize command %s", sb.buf);
	}
	return 0;
}
