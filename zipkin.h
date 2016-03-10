#ifndef _ZIPKIN_H
#define _ZIPKIN_H

#include <jansson.h>
#include <stdint.h>
#include <sys/time.h>

struct binary_annotation
{
	struct timeval timestamp;
	char *key;
	char *value;
	struct binary_annotation *next;
};

struct annotation
{
	struct timeval timestamp;
	char *annotation;
	struct annotation *next;
};

/**
 * we only need one span, the current span of this process, thus it is
 * stored as a global variable.
 */
struct zipkin_span {
	uint64_t trace_id; /* zipkin trace id */
	uint64_t span_id; /* zipkin span id for current span */
	uint64_t parent_id; /* zipkin span id for parent span */
	/* sampled, flags fields are omitted for the moment */

	struct annotation *annotations;
	struct binary_annotation *binary_annotations;
};

extern struct zipkin_span TRACE;

/**
 * record the given string as an annotation, timestamped to now
 */
int trace_record(char *annotation);

/**
 * record the given key/value strings as a binary annotation, timestamped to now
 */
int trace_record_binary(char *key, char *value);

/**
 * iterate to the next random trace id, setting appropriate environment variables
 */
void trace_next_id(void);

/**
 * return the current span serialized to json
 */
json_t *trace_to_json(void);

#endif /* _ZIPKIN_H */
