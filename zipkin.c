#include <assert.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "zipkin.h"
#include "git-compat-util.h"

#define TRACE_ID_VAR "X_B3_TRACEID"
#define SPAN_ID_VAR "X_B3_SPANID"
#define PARENT_ID_VAR "X_B3_PARENTSPANID"
#define VAR_FORMAT "%#"PRIx64

/* borrowed from stats-report.c */
extern double timeval_double(struct timeval *t);

/**
 * The current trace is global as it is in finagle
 */
struct zipkin_span TRACE = {0,0,0,NULL,NULL};

static void set_env_vars(void)
{
	char buf[19]; /* 16 digits, 2 "0x" 1 null */
	snprintf(buf, sizeof(buf), VAR_FORMAT, TRACE.trace_id);
	setenv(TRACE_ID_VAR, buf, 1);
	snprintf(buf, sizeof(buf), VAR_FORMAT, TRACE.span_id);
	setenv(SPAN_ID_VAR, buf, 1);
	snprintf(buf, sizeof(buf), VAR_FORMAT, TRACE.parent_id);
	setenv(PARENT_ID_VAR, buf, 1);
}

static uint64_t numeric_getenv(const char *env)
{
	uint64_t num = 0;
	char* val = getenv(env);
	if (val) {
		num = strtoul(val, NULL, 0);
	}

	return num;
}

static void read_env_vars(void)
{
	TRACE.trace_id = numeric_getenv(TRACE_ID_VAR);
	TRACE.span_id = numeric_getenv(SPAN_ID_VAR);
	TRACE.parent_id = numeric_getenv(PARENT_ID_VAR);
}

static uint64_t new_trace_id(void)
{
	uint64_t id;
	ssize_t ret;
	int fd = open("/dev/urandom", O_RDONLY);
	assert(fd);
	ret = read_in_full(fd, &id, sizeof(id));
	assert(ret == sizeof(id));
	close(fd);

	return id;
}

static void free_annotations(void)
{
	struct annotation *nextann, *ann = TRACE.annotations;
	struct binary_annotation *nextbann, *bann = TRACE.binary_annotations;

	while (ann != NULL) {
		nextann = ann->next;
		free(ann->annotation);
		free(ann);
		ann = nextann;
	}

	while (bann != NULL) {
		nextbann = bann->next;
		free(bann->key);
		free(bann->value);
		free(bann);
		bann = nextbann;
	}

	TRACE.annotations = NULL;
	TRACE.binary_annotations = NULL;
}

void trace_next_id(void)
{
	read_env_vars();
	if (!TRACE.trace_id) /* start of the trace */
		TRACE.trace_id = new_trace_id();
	TRACE.parent_id = TRACE.span_id;
	if (TRACE.span_id)
		TRACE.span_id = new_trace_id();
	else
		TRACE.span_id = TRACE.trace_id; /* first span_id should == trace_id */
	set_env_vars();

	free_annotations(); /* wipe the slate for the next span */
}

int trace_record(char *annotation)
{
	struct annotation *newann = xcalloc(1, sizeof(struct annotation));
	if (!newann)
		return -1;

	gettimeofday(&newann->timestamp, NULL);

	newann->annotation = strdup(annotation);
	if (!newann->annotation)
		return -1;

	newann->next = TRACE.annotations;
	TRACE.annotations = newann;

	return 0;
}


int trace_record_binary(char *key, char *value)
{
	struct binary_annotation *newann = xcalloc(1, sizeof(struct binary_annotation));
	if (!newann)
		return -1;

	gettimeofday(&newann->timestamp, NULL);

	newann->key = strdup(key);
	newann->value = strdup(value);
	if (! (newann->key && newann->value))
		return -1;

	newann->next = TRACE.binary_annotations;
	TRACE.binary_annotations = newann;

	return 0;
}

json_t *trace_to_json(void)
{
	char trace_id_buf[19]; /* 16 digits, 2 "0x" 1 null */

	json_t *json = json_object();
	json_t *json_banns = json_array();
	json_t *json_anns = json_array();

	struct annotation *ann;
	struct binary_annotation *bann;

	snprintf(trace_id_buf, sizeof(trace_id_buf)-1, VAR_FORMAT, TRACE.trace_id);
	json_object_set_new(json, "trace_id", json_string(trace_id_buf));
	snprintf(trace_id_buf, sizeof(trace_id_buf)-1, VAR_FORMAT, TRACE.span_id);
	json_object_set_new(json, "span_id", json_string(trace_id_buf));
	snprintf(trace_id_buf, sizeof(trace_id_buf)-1, VAR_FORMAT, TRACE.parent_id);
	json_object_set_new(json, "parent_id", json_string(trace_id_buf));

	for (ann = TRACE.annotations; ann != NULL; ann = ann->next) {
		json_t *json_annotation = json_object();
		json_object_set_new(json_annotation, "timestamp", json_real(timeval_double(&ann->timestamp)));
		json_object_set_new(json_annotation, "annotation", json_string(ann->annotation));
		json_array_append_new(json_anns, json_annotation);
	}
	json_object_set_new(json, "annotations", json_anns);

	for (bann = TRACE.binary_annotations; bann != NULL; bann = bann->next) {
		json_t *json_annotation = json_object();
		json_object_set_new(json_annotation, "timestamp", json_real(timeval_double(&bann->timestamp)));
		json_object_set_new(json_annotation, "key", json_string(bann->key));
		json_object_set_new(json_annotation, "value", json_string(bann->value));
		json_array_append_new(json_banns, json_annotation);
	}
	json_object_set_new(json, "binary_annotations", json_banns);

	return json;
}
