#include "other.h"

const char *other_type = "other";

int parse_other_buffer(struct other* item, const void* buffer, unsigned long size) {
	int i;
	char* type;

	if (size < strlen("type \n") || memcmp(buffer, "type ", strlen("type "))) {
		return error("other buffer does not start with type ");
	}

	type = (char*) buffer + strlen("type ");

	for (i = 0; i < sizeof(item->type) - 1; i++) {
		if (!type[i] || type[i] == '\n') {
			break;
		}
		item->type[i] = type[i];
	}

	item->type[i] = '\0';

	return 0;
}

int parse_other(struct other* item) {
	enum object_type type;
	void* buffer;
	unsigned long size;
	const unsigned char* sha1 = item->object.sha1;

	if (!item) return -1;
	if (item->object.parsed) return 0;

	buffer = read_sha1_file(sha1, &type, &size);
	if (!buffer)
		return error("could not read %s", sha1_to_hex(sha1));
	if (type != OBJ_OTHER)
		return error("object %s not an other", sha1_to_hex(sha1));

	item->buffer = buffer;
	return parse_other_buffer(item, buffer, size);
}

struct other *lookup_other(const unsigned char* sha1) {
	struct object* obj = lookup_object(sha1);
	if (!obj)
		return create_object(sha1, OBJ_OTHER, alloc_object_node());
	obj->type = OBJ_OTHER;
	return (struct other*) obj;
}

int parse_other_header(char** data, struct other_header* hdr) {
	char* p = strchr(*data, '\n');
	if (!p || p == *data) return 1;
	*p = '\0';

	hdr->key = *data;
	*data = p + 1;

	hdr->value = "";
	hashcpy(hdr->sha1, null_sha1);

	p = strchr(hdr->key, ' ');
	if (!p) return 0;
	*(p++) = '\0';

	if (hdr->key[0] == '+') {
		if (get_sha1_hex(p, hdr->sha1)) return -1;

		p += 40;

		if (*p == ' ')
			p++;
		else if (*p)
			return -1;

	}

	hdr->value = p;
	return 0;
}

