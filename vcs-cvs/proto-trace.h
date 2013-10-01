#ifndef PROTO_TRACE_H
#define PROTO_TRACE_H

#include <stddef.h>

enum direction {
	OUT,
	OUT_BLOB,
	IN,
	IN_BLOB
};

void set_proto_trace_tz(int tz);
void proto_trace_kp(const char *trace_key, const char *proto, const char *buf, size_t len, int direction);
void proto_ztrace_kp(const char *trace_key, const char *proto, size_t len, size_t zlen, int direction);

#define proto_trace(buf, len, direction) \
	proto_trace_kp(trace_key, trace_proto, buf, len, direction)

#define proto_ztrace(len, zlen, direction) \
	proto_ztrace_kp(trace_key, trace_proto, len, zlen, direction);

#endif
