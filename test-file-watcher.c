#include "git-compat-util.h"
#include "unix-socket.h"
#include "pkt-line.h"
#include "strbuf.h"

int main(int ac, char **av)
{
	struct strbuf sb = STRBUF_INIT;
	struct strbuf packed = STRBUF_INIT;
	int packing_mode = 0, last_command_is_reply = 0;
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
			if (!packing_mode) {
				strbuf_reset(&packed);
				strbuf_addf(&packed, "%s ", sb.buf + 2);
				packing_mode = 1;
				continue;
			}
			if (!strcmp(sb.buf + 2, "NULL"))
				strbuf_addstr(&packed, "0000");
			else if (sb.buf[2])
				packet_buf_write_notrace(&packed, "%s", sb.buf + 2);
			else {
				packet_write(fd, "%s", packed.buf);
				packing_mode = 0;
			}
			continue;
		}
		if (sb.buf[0] == '<') {
			packet_write(fd, "%s", sb.buf + 1);
			puts(sb.buf);
			continue;
		}
		if (sb.buf[0] == '>' && sb.buf[1] == '>') {
			int len;
			char *reply = packet_read_line(fd, &len);
			char *p = strchr(reply, ' ');
			char *end = reply + len;
			if (!p) {
				printf(">%s\n", reply);
				continue;
			}
			printf(">>%.*s\n", (int)(p - reply), reply);
			p++;
			for (; p < end; p += len) {
				len = packet_length(p);
				if (!len) {
					printf(">>NULL\n");
					len += 4;
				} else
					printf(">>%.*s\n", len - 4, p + 4);
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
