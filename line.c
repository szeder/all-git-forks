#include "git-compat-util.h"
#include "cache.h"
#include "tag.h"
#include "blob.h"
#include "tree.h"
#include "diff.h"
#include "commit.h"
#include "decorate.h"
#include "revision.h"
#include "xdiff-interface.h"
#include "strbuf.h"
#include "log-tree.h"
#include "graph.h"
#include "line.h"

struct print_range {
	int start, end;		/* Line range of post-image */
	int pstart, pend;	/* Line range of pre-image */
	int line_added : 1;	/* whether this range is added */
};

struct print_pair {
	int alloc, nr;
	struct print_range *ranges;
};

struct line_range {
	const char *arg;	/* The argument to specify this line range */
	long start, end;	/* The interesting line range of current commit */
	long pstart, pend;	/* The corresponding range of parent commit */
	struct print_pair pair;
			/* The changed lines inside this range */
	unsigned int diff:1;
};

/*
 * These could be in line.h but we put them here so the functions can
 * be static.
 */

static struct line_range *diff_line_range_insert(struct diff_line_range *r,
	       const char *arg, int start, int end);

static void diff_line_range_clear(struct diff_line_range *r);

static struct diff_line_range *diff_line_range_merge(
	       struct diff_line_range *out,
	       struct diff_line_range *other);

static struct diff_line_range *diff_line_range_clone(struct diff_line_range *r);

static struct diff_line_range *diff_line_range_clone_deeply(struct diff_line_range *r);

static void add_line_range(struct rev_info *revs, struct commit *commit,
	       struct diff_line_range *r);

static struct diff_line_range *lookup_line_range(struct rev_info *revs,
	       struct commit *commit);

/*
 * Data structure helpers
 */

static inline void print_range_init(struct print_range *r)
{
	r->start = r->end = 0;
	r->pstart = r->pend = 0;
	r->line_added = 0;
}

static inline void print_pair_init(struct print_pair *p)
{
	p->alloc = p->nr = 0;
	p->ranges = NULL;
}

static inline void print_pair_grow(struct print_pair *p)
{
	p->nr++;
	ALLOC_GROW(p->ranges, p->nr, p->alloc);
}

static inline void print_pair_clear(struct print_pair *p)
{
	p->alloc = p->nr = 0;
	if (p->ranges)
		free(p->ranges);
	p->ranges = NULL;
}

static inline void line_range_init(struct line_range *r)
{
	r->arg = NULL;
	r->start = r->end = 0;
	r->pstart = r->pend = 0;
	print_pair_init(&(r->pair));
	r->diff = 0;
}

static inline void line_range_clear(struct line_range *r)
{
	r->arg = NULL;
	r->start = r->end = 0;
	r->pstart = r->pend = 0;
	print_pair_clear(&r->pair);
	r->diff = 0;
}

static inline void diff_line_range_grow(struct diff_line_range *r)
{
	r->nr++;
	ALLOC_GROW(r->ranges, r->nr, r->alloc);
	line_range_init((r->ranges + r->nr - 1));
}

/*
 * Parse one item in the -L option
 */
const char *parse_loc(const char *spec, nth_line_fn_t nth_line,
		void *data, long lines, long begin, long *ret)
{
	char *term;
	const char *line;
	long num;
	int reg_error;
	regex_t regexp;
	regmatch_t match[1];

	/*
	 * $ is a synonym for "the end of the file".
	 */
	if (spec[0] == '$') {
		*ret = lines;
		return spec + 1;
	}

	/*
	 * Allow "-L <something>,+20" to mean starting at <something>
	 * for 20 lines, or "-L <something>,-5" for 5 lines ending at
	 * <something>.
	 */
	if (begin != -1 && (spec[0] == '+' || spec[0] == '-')) {
		num = strtol(spec + 1, &term, 10);
		if (term != spec + 1) {
			if (!ret)
				return term;
			if (spec[0] == '-')
				num = 0 - num;
			if (0 < num)
				*ret = begin + num - 2;
			else if (!num)
				*ret = begin;
			else
				*ret = begin + num;
			return term;
		}
		return spec;
	}
	num = strtol(spec, &term, 10);
	if (term != spec) {
		if (ret)
			*ret = num;
		return term;
	}
	if (spec[0] != '/')
		return spec;

	/* it could be a regexp of form /.../ */
	for (term = (char *) spec + 1; *term && *term != '/'; term++) {
		if (*term == '\\')
			term++;
	}
	if (*term != '/')
		return spec;

	/* in the scan-only case we are not interested in the regex */
	if (!ret)
		return term+1;

	/* try [spec+1 .. term-1] as regexp */
	*term = 0;
	if (begin == -1)
		begin = 1;
	begin--; /* input is in human terms */
	line = nth_line(data, begin);

	if (!(reg_error = regcomp(&regexp, spec + 1, REG_NEWLINE)) &&
	    !(reg_error = regexec(&regexp, line, 1, match, 0))) {
		const char *cp = line + match[0].rm_so;
		const char *nline;

		while (begin++ < lines) {
			nline = nth_line(data, begin);
			if (line <= cp && cp < nline)
				break;
			line = nline;
		}
		*ret = begin;
		regfree(&regexp);
		*term++ = '/';
		return term;
	} else {
		char errbuf[1024];
		regerror(reg_error, &regexp, errbuf, 1024);
		die("-L parameter '%s': %s", spec + 1, errbuf);
	}
}

int parse_range_arg(const char *arg, nth_line_fn_t nth_line_cb,
		void *cb_data, long lines, long *begin, long *end)
{
	arg = parse_loc(arg, nth_line_cb, cb_data, lines, -1, begin);

	if (*arg == ',') {
		arg = parse_loc(arg+1, nth_line_cb, cb_data, lines, *begin+1, end);
		if (*begin > *end) {
			long tmp = *begin;
			*begin = *end;
			*end = tmp;
		}
	}

	if (*arg)
		return -1;

	return 0;
}

const char *skip_range_arg(const char *arg)
{
	arg = parse_loc(arg, NULL, NULL, 0, -1, 0);

	if (*arg == ',')
		arg = parse_loc(arg+1, NULL, NULL, 0, 0, 0);

	return arg;
}

static void free_diff_line_ranges(struct diff_line_range *r)
{
	while (r) {
		struct diff_line_range *next = r->next;
		diff_line_range_clear(r);
		free(r);
		r = next;
	}
}

static struct object *verify_commit(struct rev_info *revs)
{
	struct object *commit = NULL;
	int found = -1;
	int i;

	for (i = 0; i < revs->pending.nr; i++) {
		struct object *obj = revs->pending.objects[i].item;
		if (obj->flags & UNINTERESTING)
			continue;
		while (obj->type == OBJ_TAG)
			obj = deref_tag(obj, NULL, 0);
		if (obj->type != OBJ_COMMIT)
			die("Non commit %s?", revs->pending.objects[i].name);
		if (commit)
			die("More than one commit to dig from: %s and %s?",
			    revs->pending.objects[i].name,
				revs->pending.objects[found].name);
		commit = obj;
		found = i;
	}

	if (!commit)
		die("No commit specified?");

	return commit;
}

static void fill_blob_sha1(struct commit *commit, struct diff_line_range *r)
{
	unsigned mode;
	unsigned char sha1[20];

	while (r) {
		if (get_tree_entry(commit->object.sha1, r->spec->path,
			sha1, &mode))
			goto error;
		fill_filespec(r->spec, sha1, mode);
		r = r->next;
	}

	return;
error:
	die("There is no path %s in the commit", r->spec->path);
}

static void fill_line_ends(struct diff_filespec *spec, long *lines,
	unsigned long **line_ends)
{
	int num = 0, size = 50;
	long cur = 0;
	unsigned long *ends = NULL;
	char *data = NULL;

	if (diff_populate_filespec(spec, 0))
		die("Cannot read blob %s", sha1_to_hex(spec->sha1));

	ends = xmalloc(size * sizeof(*ends));
	ends[cur++] = 0;
	data = spec->data;
	while (num < spec->size) {
		if (data[num] == '\n' || num == spec->size - 1) {
			ALLOC_GROW(ends, (cur + 1), size);
			ends[cur++] = num;
		}
		num++;
	}

	/* shrink the array to fit the elements */
	ends = xrealloc(ends, cur * sizeof(*ends));
	*lines = cur;
	*line_ends = ends;
}

struct nth_line_cb {
	struct diff_filespec *spec;
	long lines;
	unsigned long *line_ends;
};

static const char *nth_line(void *data, long line)
{
	struct nth_line_cb *d = data;
	assert(d && line < d->lines);
	assert(d->spec && d->spec->data);

	if (line == 0)
		return (char *)d->spec->data;
	else
		return (char *)d->spec->data + d->line_ends[line] + 1;
}

static void parse_lines(struct commit *commit, struct diff_line_range *r)
{
	int i;
	struct line_range *old_range = NULL;
	long lines = 0;
	unsigned long *ends = NULL;
	struct nth_line_cb cb_data;

	while (r) {
		struct diff_filespec *spec = r->spec;
		int num = r->nr;
		assert(spec);
		fill_blob_sha1(commit, r);
		old_range = r->ranges;
		r->ranges = NULL;
		r->nr = r->alloc = 0;
		fill_line_ends(spec, &lines, &ends);
		cb_data.spec = spec;
		cb_data.lines = lines;
		cb_data.line_ends = ends;
		for (i = 0; i < num; i++) {
			long begin, end;
			if (parse_range_arg(old_range[i].arg, nth_line, &cb_data,
					    lines-1, &begin, &end))
				die("malformed -L argument '%s'", old_range[i].arg);
			diff_line_range_insert(r, old_range[i].arg, begin, end);
		}

		free(ends);
		ends = NULL;

		r = r->next;
		free(old_range);
	}
}

/*
 * Insert a new line range into a diff_line_range struct, and keep the
 * r->ranges sorted by their starting line number.
 */
static struct line_range *diff_line_range_insert(struct diff_line_range *r,
		const char *arg, int start, int end)
{
	int i = 0;
	struct line_range *rs = r->ranges;
	int left_merge = 0, right_merge = 0;

	assert(r);
	assert(start <= end);

	if (r->nr == 0 || rs[r->nr - 1].end < start - 1) {
		int num = 0;
		diff_line_range_grow(r);
		rs = r->ranges;
		num = r->nr - 1;
		rs[num].arg = arg;
		rs[num].start = start;
		rs[num].end = end;
		return rs + num;
	}

	for (; i < r->nr; i++) {
		if (rs[i].end < start - 1)
			continue;
		if (rs[i].end == start - 1) {
			rs[i].end = end;
			right_merge = 1;
			goto out;
		}

		assert(rs[i].end > start - 1);
		if (rs[i].start <= start) {
			if (rs[i].end < end) {
				rs[i].end = end;
				right_merge = 1;
			}
			goto out;
		} else if (rs[i].start <= end + 1) {
			rs[i].start = start;
			left_merge = 1;
			if (rs[i].end < end) {
				rs[i].end = end;
				right_merge = 1;
			}
			goto out;
		} else {
			int num = r->nr - i;
			diff_line_range_grow(r);
			rs = r->ranges;
			memmove(rs + i + 1, rs + i, num * sizeof(struct line_range));
			rs[i].arg = arg;
			rs[i].start = start;
			rs[i].end = end;
			goto out;
		}
	}

out:
	assert(r->nr != i);
	if (left_merge) {
		int j = i;
		for (; j > -1; j--) {
			if (rs[j].end >= rs[i].start - 1)
				if (rs[j].start < rs[i].start)
					rs[i].start = rs[j].start;
		}
		memmove(rs + j + 1, rs + i, (r->nr - i) * sizeof(struct line_range));
		r->nr -= i - j - 1;
	}
	if (right_merge) {
		int j = i;
		for (; j < r->nr; j++) {
			if (rs[j].start <= rs[i].end + 1)
				if (rs[j].end > rs[i].end)
					rs[i].end = rs[j].end;
		}
		if (j < r->nr)
			memmove(rs + i + 1, rs + j, (r->nr - j) * sizeof(struct line_range));
		r->nr -= j - i - 1;
	}
	assert(r->nr);

	return rs + i;
}

static void diff_line_range_clear(struct diff_line_range *r)
{
	int i = 0, zero = 0;

	for (; i < r->nr; i++) {
		struct line_range *rg = r->ranges + i;
		line_range_clear(rg);
	}

	if (r->prev) {
		zero = 0;
		if (r->prev->count == 1)
			zero = 1;
		free_filespec(r->prev);
		if (zero)
			r->prev = NULL;
	}
	if (r->spec) {
		zero = 0;
		if (r->spec->count == 1)
			zero = 1;
		free_filespec(r->spec);
		if (zero)
			r->spec = NULL;
	}

	r->status = '\0';
	r->alloc = r->nr = 0;

	if (r->ranges)
		free(r->ranges);
	r->ranges = NULL;
	r->next = NULL;
}

void diff_line_range_append(struct diff_line_range *r, const char *arg)
{
	diff_line_range_grow(r);
	r->ranges[r->nr - 1].arg = arg;
}

static struct diff_line_range *diff_line_range_clone(struct diff_line_range *r)
{
	struct diff_line_range *ret = xmalloc(sizeof(*ret));
	int i = 0;

	assert(r);
	diff_line_range_init(ret);
	ret->ranges = xcalloc(r->nr, sizeof(struct line_range));
	memcpy(ret->ranges, r->ranges, sizeof(struct line_range) * r->nr);

	ret->alloc = ret->nr = r->nr;

	for (; i < ret->nr; i++)
		print_pair_init(&ret->ranges[i].pair);

	ret->spec = r->spec;
	assert(ret->spec);
	ret->spec->count++;

	return ret;
}

static struct diff_line_range *
diff_line_range_clone_deeply(struct diff_line_range *r)
{
	struct diff_line_range *ret = NULL;
	struct diff_line_range *tmp = NULL, *prev = NULL;

	assert(r);
	ret = tmp = prev = diff_line_range_clone(r);
	r = r->next;
	while (r) {
		tmp = diff_line_range_clone(r);
		prev->next = tmp;
		prev = tmp;
		r = r->next;
	}

	return ret;
}

static struct diff_line_range *diff_line_range_merge(struct diff_line_range *out,
		struct diff_line_range *other)
{
	struct diff_line_range *one = out, *two = other;
	struct diff_line_range *pone = NULL;

	while (one) {
		struct diff_line_range *ptwo;
		two = other;
		ptwo = other;
		while (two) {
			if (!strcmp(one->spec->path, two->spec->path)) {
				int i = 0;
				for (; i < two->nr; i++) {
					diff_line_range_insert(one, NULL,
						two->ranges[i].start,
						two->ranges[i].end);
				}
				if (two == other)
					other = other->next;
				else
					ptwo->next = two->next;
				diff_line_range_clear(two);
				free(two);
				two = NULL;

				break;
			}

			ptwo = two;
			two = two->next;
		}

		pone = one;
		one = one->next;
	}
	pone->next = other;

	return out;
}

static void add_line_range(struct rev_info *revs, struct commit *commit,
		struct diff_line_range *r)
{
	struct diff_line_range *ret = NULL;

	ret = lookup_decoration(&revs->line_range, &commit->object);
	if (ret && r)
		diff_line_range_merge(ret, r);
	else
		add_decoration(&revs->line_range, &commit->object, r);

	if (r)
		commit->object.flags |= RANGE_UPDATE;
}

static void clear_commit_line_range(struct rev_info *revs, struct commit *commit)
{
	struct diff_line_range *r;
	r = lookup_decoration(&revs->line_range, &commit->object);
	if (!r)
		return;
	free_diff_line_ranges(r);
	add_decoration(&revs->line_range, &commit->object, NULL);
}

static struct diff_line_range *lookup_line_range(struct rev_info *revs,
		struct commit *commit)
{
	struct diff_line_range *ret = NULL;

	ret = lookup_decoration(&revs->line_range, &commit->object);
	return ret;
}

void line_log_init(struct rev_info *rev, struct diff_line_range *r)
{
	struct commit *commit = NULL;
	struct diff_options *opt = &rev->diffopt;

	commit = (struct commit *)verify_commit(rev);
	parse_lines(commit, r);

	add_line_range(rev, commit, r);
}

struct take_range_cb_data {
	struct diff_line_range *interesting;	/* currently interesting ranges */
	struct diff_line_range *range;
		/* the ranges corresponds to the interesting ranges of parent commit */
	long plno, tlno;
		/* the last line number of diff hunk */
	int diff;
		/* whether there is some line changes between the current
		 * commit and its parent */
};

#define SCALE_FACTOR 4
/*
 * [p_start, p_end] represents the pre-image of current diff hunk,
 * [t_start, t_end] represents the post-image of the current diff hunk,
 * [start, end] represents the currently interesting line range in
 * post-image,
 * [o_start, o_end] represents the original line range that coresponds
 * to current line range.
 */
void map_lines(long p_start, long p_end, long t_start, long t_end,
		long start, long end, long *o_start, long *o_end)
{
	/*
	 * Normally, p_start should be less than p_end, so does the
	 * t_start and t_end. But when the line range is added from
	 * scratch, p_start will be greater than p_end. When the line
	 * range is deleted, t_start will be greater than t_end.
	 */
	if (p_start > p_end) {
		*o_start = *o_end = 0;
		return;
	}
	/* A deletion */
	if (t_start > t_end) {
		*o_start = p_start;
		*o_end = p_end;
		return;
	}

	if (start == t_start && end == t_end) {
		*o_start = p_start;
		*o_end = p_end;
		return;
	}

	if (start == t_start) {
		*o_start = p_start;
		*o_end = p_start + (end - start);
		if (*o_end > p_end)
			*o_end = p_end;
		return;
	}

	if (end == t_end) {
		*o_start = p_end - (end - start);
		if (*o_start < p_start)
			*o_start = p_start;
		*o_end = p_end;
		return;
	}

	/*
	 * A heuristic for lines mapping:
	 *
	 * When the pre-image is no more than 1/SCALE_FACTOR of the post-image,
	 * there is no effective way to find out which part of pre-image
	 * corresponds to the currently interesting range of post-image.
	 * And we are in the danger of tracking totally useless lines.
	 * So, we just treat all the post-image lines as added from scratch.
	 */
	if (SCALE_FACTOR * (p_end - p_start + 1) < (t_end - t_start + 1)) {
		*o_start = *o_end = 0;
		return;
	}

	*o_start = p_start + start - t_start;
	*o_end = p_end - (t_end - end);

	if (*o_start > *o_end) {
		int temp = *o_start;
		*o_start = *o_end;
		*o_end = temp;
	}

	if (*o_start < p_start)
		*o_start = p_start;
	if (*o_end > p_end)
		*o_end = p_end;
}

/*
 * When same == 1:
 * [p_start, p_end] represents the diff hunk line range of pre-image,
 * [t_start, t_end] represents the diff hunk line range of post-image.
 * When same == 0, they represent a range of identical lines between
 * two images.
 *
 * This function find out the corresponding line ranges of currently
 * interesting ranges which this diff hunk touches.
 */
static void map_range(struct take_range_cb_data *data, int same,
		long p_start, long p_end, long t_start, long t_end)
{
	struct line_range *ranges = data->interesting->ranges;
	long takens, takene, start, end;
	int i = 0, out = 0, added = 0;
	long op_start = p_start, op_end = p_end, ot_start = t_start, ot_end = t_end;

	for (; i < data->interesting->nr; i++) {
		added = 0;
		if (t_start > ranges[i].end)
			continue;
		if (t_end < ranges[i].start)
			break;

		if (t_start > ranges[i].start) {
			start = t_start;
			takens = p_start;
		} else {
			start = ranges[i].start;
			takens = p_start + start - t_start;
		}

		if (t_end >= ranges[i].end) {
			end = ranges[i].end;
			takene = p_start + end - t_start;
		} else {
			end = t_end;
			takene = p_end;
			out = 1;
		}

		if (!same) {
			struct print_pair *pair = &ranges[i].pair;
			struct print_range *rr = NULL;
			print_pair_grow(pair);
			rr = pair->ranges + pair->nr - 1;
			print_range_init(rr);
			rr->start = start;
			rr->end = end;
			map_lines(op_start, op_end, ot_start, ot_end, start, end,
					&takens, &takene);
			if (takens == 0 && takene == 0) {
				added = 1;
				rr->line_added = 1;
			}
			rr->pstart = takens;
			rr->pend = takene;
			data->diff = 1;
			data->interesting->diff = 1;
			ranges[i].diff = 1;
		}
		if (added) {
			/* Code movement/copy goes here */
		} else {
			struct line_range *added_range = diff_line_range_insert(data->range,
					NULL, takens, takene);
			assert(added_range);
			ranges[i].pstart = added_range->start;
			ranges[i].pend = added_range->end;
		}

		t_start = end + 1;
		p_start = takene + 1;

		if (out)
			break;
	}
}

/*
 * [p_start, p_end] represents the line range of pre-image,
 * [t_start, t_end] represents the line range of post-image,
 * and they are identical lines.
 *
 * This function substracts out the identical lines between current
 * commit and its parent, from currently interesting ranges.
 */
static void take_range(struct take_range_cb_data *data,
		long p_start, long p_end, long t_start, long t_end)
{
	struct line_range *ranges = data->interesting->ranges;
	long takens, takene, start, end;
	int i = 0, out = 0, added = 0;

	for (; i < data->interesting->nr; i++) {
		added = 0;
		if (t_start > ranges[i].end)
			continue;
		if (t_end < ranges[i].start)
			break;

		if (t_start > ranges[i].start) {
			long tmp = ranges[i].end;
			ranges[i].end = t_start - 1;
			start = t_start;
			takens = p_start;
			if (t_end >= tmp) {
				end = tmp;
				takene = p_start + end - t_start;
				p_start = takene + 1;
				t_start = end + 1;
			} else {
				end = t_end;
				takene = p_end;
				diff_line_range_insert(data->interesting, NULL,
					t_end + 1, tmp);
				out = 1;
			}
		} else {
			start = ranges[i].start;
			takens = p_start + start - t_start;
			if (t_end >= ranges[i].end) {
				int num = data->interesting->nr - 1;
				end = ranges[i].end;
				takene = p_start + end - t_start;
				t_start = end + 1;
				p_start = takene + 1;
				memmove(ranges + i, ranges + i + 1, (num - i) * sizeof(*ranges));
				data->interesting->nr = num;
				i--;
			} else {
				end = t_end;
				takene = p_end;
				ranges[i].start = t_end + 1;
				out = 1;
			}
		}

		diff_line_range_insert(data->range, NULL, takens, takene);

		if (out)
			break;
	}
}

static void take_range_cb(void *data, long same, long p_next, long t_next)
{
	struct take_range_cb_data *d = data;
	long p_start = d->plno + 1, t_start = d->tlno + 1;
	long p_end = p_start + same - t_start, t_end = same;

	/* If one file is added from scratch, we should not bother to call
	 * take_range, since there is nothing to take
	 */
	if (t_end >= t_start)
		take_range(d, p_start, p_end, t_start, t_end);
	d->plno = p_next;
	d->tlno = t_next;
}

static void map_range_cb(void *data, long same, long p_next, long t_next)
{
	struct take_range_cb_data *d = data;

	long p_start = d->plno + 1;
	long t_start = d->tlno + 1;
	long p_end = same - t_start + p_start;
	long t_end = same;

	/* Firstly, take the unchanged lines from child */
	if (t_end >= t_start)
		map_range(d, 1, p_start, p_end, t_start, t_end);

	/* find out which lines to print */
	t_start = same + 1;
	p_start = d->plno + t_start - d->tlno;
	map_range(d, 0, p_start, p_next, t_start, t_next);

	d->plno = p_next;
	d->tlno = t_next;
}

static void load_tree_desc(struct tree_desc *desc, void **tree,
		const unsigned char *sha1)
{
	unsigned long size;
	*tree = read_object_with_reference(sha1, tree_type, &size, NULL);
	if (!tree)
		die("Unable to read tree (%s)", sha1_to_hex(sha1));
	init_tree_desc(desc, *tree, size);
}

/*
 * We support two kinds of operation in this function:
 * 1. map == 0, take the same lines from the current commit and assign it
 *              to parent;
 * 2. map == 1, in addition to the same lines, we also map the changed lines
 *              from the current commit to the parent according to the
 *              diff output.
 * take_range_cb and take_range are used to take same lines from current commit
 * to parents.
 * map_range_cb and map_range are used to map line ranges to the parent.
 */
static int assign_range_to_parent(struct rev_info *rev, struct commit *commit,
		struct commit *parent, struct diff_line_range *range,
		struct diff_options *opt, int map)
{
	struct diff_line_range *final_range = xmalloc(sizeof(*final_range));
	struct diff_line_range *cur_range = final_range;
	struct diff_line_range *prev_range = final_range;
	struct diff_line_range *rg = NULL;
	void *tree1 = NULL, *tree2 = NULL;
	struct tree_desc desc1, desc2;
	struct diff_queue_struct *queue;
	struct take_range_cb_data cb_data = {NULL, cur_range, 0, 0};
	xpparam_t xpp;
	xdemitconf_t xecfg;
	int i, diff = 0;
	xdiff_emit_hunk_consume_fn fn = map ? map_range_cb : take_range_cb;

	diff_line_range_init(cur_range);
	memset(&xpp, 0, sizeof(xpp));
	memset(&xecfg, 0, sizeof(xecfg));
	xecfg.ctxlen = xecfg.interhunkctxlen = 0;

	/*
	 * Compose up two trees, for root commit, we make up a empty tree.
	 */
	assert(commit);
	load_tree_desc(&desc2, &tree2, commit->tree->object.sha1);
	if (parent) {
		load_tree_desc(&desc1, &tree1, parent->tree->object.sha1);
	} else {
		init_tree_desc(&desc1, "", 0);
	}

	DIFF_QUEUE_CLEAR(&diff_queued_diff);
	diff_tree(&desc1, &desc2, "", opt);
	diffcore_std(opt);

	queue = &diff_queued_diff;
	for (i = 0; i < queue->nr; i++) {
		struct diff_filepair *pair = queue->queue[i];
		struct diff_line_range *rg = range;
		mmfile_t file_parent, file_t;
		assert(pair->two->path);
		while (rg) {
			assert(rg->spec->path);
			if (!strcmp(rg->spec->path, pair->two->path))
				break;
			rg = rg->next;
		}

		if (!rg)
			continue;
		rg->touched = 1;
		if (rg->nr == 0)
			continue;

		rg->status = pair->status;
		assert(pair->two->sha1_valid);
		diff_populate_filespec(pair->two, 0);
		file_t.ptr = pair->two->data;
		file_t.size = pair->two->size;

		if (rg->prev)
			free_filespec(rg->prev);
		rg->prev = pair->one;
		rg->prev->count++;
		if (pair->one->sha1_valid) {
			diff_populate_filespec(pair->one, 0);
			file_parent.ptr = pair->one->data;
			file_parent.size = pair->one->size;
		} else {
			file_parent.ptr = "";
			file_parent.size = 0;
		}

		if (cur_range->nr != 0) {
			struct diff_line_range *tmp = xmalloc(sizeof(*tmp));
			cur_range->next = tmp;
			prev_range = cur_range;
			cur_range = tmp;
		} else if (cur_range->spec)
			diff_line_range_clear(cur_range);

		diff_line_range_init(cur_range);
		if (pair->one->sha1_valid) {
			cur_range->spec = pair->one;
			cur_range->spec->count++;
		} else {
			assert(is_null_sha1(pair->one->sha1));
			cur_range->spec = pair->two;
			cur_range->spec->count++;
		}

		cb_data.interesting = rg;
		cb_data.range = cur_range;
		cb_data.diff = 0;
		cb_data.plno = cb_data.tlno = 0;
		xdi_diff_hunks(&file_parent, &file_t, fn, &cb_data, &xpp, &xecfg);
		if (cb_data.diff)
			diff = 1;
		/*
		 * The remain part is the same part.
		 * Instead of calculating the true line number of the two files,
		 * use the biggest integer.
		 */
		if (map)
			map_range(&cb_data, 1, cb_data.plno + 1,
				  INT_MAX, cb_data.tlno + 1, INT_MAX);
		else
			take_range(&cb_data, cb_data.plno + 1,
				   INT_MAX, cb_data.tlno + 1, INT_MAX);
	}
	opt->output_format = DIFF_FORMAT_NO_OUTPUT;
	diff_flush(opt);

	/*
	 * Collect the untouched ranges, this comes from the files not changed
	 * between two commit.
	 */
	rg = range;
	while (rg) {
		/* clear the touched one to make it usable in next round */
		if (rg->touched) {
			rg->touched = 0;
		} else {
			struct diff_line_range *untouched = diff_line_range_clone(rg);
			if (prev_range == final_range && final_range->nr == 0) {
				final_range = prev_range = untouched;
			} else {
				prev_range->next = untouched;
				prev_range = untouched;
			}
		}
		rg = rg->next;
	}

	if (cur_range->nr == 0) {
		diff_line_range_clear(cur_range);
		free(cur_range);
		if (prev_range == cur_range)
			final_range = NULL;
		else
			prev_range->next = NULL;
	}

	if (!map)
		goto out;

	if (final_range) {
		assert(parent);
		assert(final_range->spec);
		add_line_range(rev, parent, final_range);
	} else {
		/*
		 * If there is no new ranges assigned to the parent,
		 * we should mark it as a 'root' commit.
		 */
		if (commit->parents && !commit->parents->next) {
			free(commit->parents);
			commit->parents = NULL;
		}
	}

	/* and the ranges of current commit is updated */
	commit->object.flags &= ~RANGE_UPDATE;
	if (diff)
		commit->object.flags |= NEED_PRINT;

out:
	if (tree1)
		free(tree1);
	if (tree2)
		free(tree2);

	return diff;
}

static void diff_update_parent_range(struct rev_info *rev,
		struct commit *commit)
{
	struct diff_line_range *r = lookup_line_range(rev, commit);
	struct commit_list *parents = commit->parents;
	struct commit *c = NULL;
	if (parents) {
		assert(!parents->next);
		c = parents->item;
	}

	assign_range_to_parent(rev, commit, c, r, &rev->diffopt, 1);
}

struct commit_state {
	struct diff_line_range *range;
	struct object obj;
};

static void assign_parents_range(struct rev_info *rev, struct commit *commit)
{
	struct commit_list *parents = commit->parents;
	struct diff_line_range *r = lookup_line_range(rev, commit);
	struct diff_line_range *evil = NULL, *range = NULL;
	struct decoration parents_state;
	struct commit_state *state = NULL;
	int nontrivial = 0;

	memset(&parents_state, 0, sizeof(parents_state));
	/*
	 * If we are in linear history, update range and flush the patch if
	 * necessary
	 */
	if (!parents || !parents->next)
		return diff_update_parent_range(rev, commit);

	/*
	 * Loop on the parents and assign the ranges to different
	 * parents, if there is any range left, this commit must
	 * be an evil merge.
	 */
	evil = diff_line_range_clone_deeply(r);
	parents = commit->parents;
	while (parents) {
		struct commit *p = parents->item;
		int diff = 0;
		struct diff_line_range *origin_range = lookup_line_range(rev, p);
		if (origin_range)
			origin_range = diff_line_range_clone_deeply(origin_range);

		state = xmalloc(sizeof(*state));
		state->range = origin_range;
		state->obj = p->object;
		add_decoration(&parents_state, &p->object, state);
		diff = assign_range_to_parent(rev, commit, p, r, &rev->diffopt, 1);
		/* Since all the ranges comes from this parent, we can ignore others */
		if (diff == 0) {
			/* restore the state of parents before this one */
			parents = commit->parents;
			while (parents->item != p) {
				struct commit_list *list = parents;
				parents = parents->next;
				clear_commit_line_range(rev, list->item);
				state = lookup_decoration(&parents_state, &list->item->object);
				add_decoration(&parents_state, &list->item->object, NULL);
				add_line_range(rev, list->item, state->range);
				list->item->object = state->obj;
				free(state);
				free(list);
			}

			commit->parents = parents;
			parents = parents->next;
			commit->parents->next = NULL;

			/* free the non-use commit_list */
			while (parents) {
				struct commit_list *list = parents;
				parents = parents->next;
				free(list);
			}
			goto out;
		}
		/* take the ranges from 'commit', try to detect nontrivial merge */
		assign_range_to_parent(rev, commit, p, evil, &rev->diffopt, 0);
		parents = parents->next;
	}

	commit->object.flags |= NONTRIVIAL_MERGE;
	/*
	 * yes, this must be an evil merge.
	 */
	range = evil;
	while (range) {
		if (range->nr) {
			commit->object.flags |= EVIL_MERGE;
			nontrivial = 1;
		}
		range = range->next;
	}

out:
	/* Never print out any diff for a merge commit */
	commit->object.flags &= ~NEED_PRINT;

	parents = commit->parents;
	while (parents) {
		state = lookup_decoration(&parents_state, &parents->item->object);
		if (state) {
			free_diff_line_ranges(state->range);
			free(state);
		}
		parents = parents->next;
	}

	if (nontrivial)
		add_decoration(&rev->nontrivial_merge, &commit->object, evil);
	else
		free_diff_line_ranges(evil);
}

struct line_chunk {
	int lone, ltwo;
	const char *one, *two;
	const char *one_end, *two_end;
	struct diff_line_range *range;
};

static void flush_lines(struct diff_options *opt, const char **ptr, const char *end,
		int slno, int elno, int *lno, const char *color, const char heading)
{
	const char *p = *ptr;
	struct strbuf buf = STRBUF_INIT;
	const char *reset;
	char *line_prefix = "";
	struct strbuf *msgbuf;

	if (opt && opt->output_prefix) {
		msgbuf = opt->output_prefix(opt, opt->output_prefix_data);
		line_prefix = msgbuf->buf;
	}

	if (*color)
		reset = diff_get_color_opt(opt, DIFF_RESET);
	else
		reset = "";

	strbuf_addf(&buf, "%s%c", color, heading);
	while (*ptr < end && *lno < slno) {
		if (**ptr == '\n') {
			(*lno)++;
			if (*lno == slno) {
				(*ptr)++;
				break;
			}
		}
		(*ptr)++;
	}
	assert(*ptr <= end);
	p = *ptr;

	while (*ptr < end && *lno <= elno) {
		if (**ptr == '\n') {
			fprintf(opt->file, "%s%s", line_prefix, buf.buf);
			if (*ptr - p)
				fwrite(p, *ptr - p, 1, opt->file);
			fprintf(opt->file, "%s\n", reset);
			p = *ptr + 1;
			(*lno)++;
		}
		(*ptr)++;
	}
	if (*lno <= elno) {
		fprintf(opt->file, "%s%s", line_prefix, buf.buf);
		if (*ptr - p)
			fwrite(p, *ptr - p, 1, opt->file);
		fprintf(opt->file, "%s\n", reset);
	}
	strbuf_release(&buf);
}

static void diff_flush_range(struct diff_options *opt, struct line_chunk *chunk,
		struct line_range *range)
{
	struct print_pair *pair = &range->pair;
	const char *old = diff_get_color_opt(opt, DIFF_FILE_OLD);
	const char *new = diff_get_color_opt(opt, DIFF_FILE_NEW);
	int i, cur = range->start;

	for (i = 0; i < pair->nr; i++) {
		struct print_range *pr = pair->ranges + i;
		if (cur < pr->start)
			flush_lines(opt, &chunk->two, chunk->two_end,
				cur, pr->start - 1, &chunk->ltwo, "", ' ');

		if (!pr->line_added)
			flush_lines(opt, &chunk->one, chunk->one_end,
				pr->pstart, pr->pend, &chunk->lone, old, '-');
		flush_lines(opt, &chunk->two, chunk->two_end,
			pr->start, pr->end, &chunk->ltwo, new, '+');

		cur = pr->end + 1;
	}

	if (cur <= range->end) {
		flush_lines(opt, &chunk->two, chunk->two_end,
			cur, range->end, &chunk->ltwo, "", ' ');
	}
}

static void diff_flush_chunks(struct diff_options *opt, struct line_chunk *chunk)
{
	struct diff_line_range *range = chunk->range;
	const char *set = diff_get_color_opt(opt, DIFF_FRAGINFO);
	const char *reset = diff_get_color_opt(opt, DIFF_RESET);
	char *line_prefix = "";
	struct strbuf *msgbuf;
	int i;

	if (opt && opt->output_prefix) {
		msgbuf = opt->output_prefix(opt, opt->output_prefix_data);
		line_prefix = msgbuf->buf;
	}

	for (i = 0; i < range->nr; i++) {
		struct line_range *r = range->ranges + i;
		long lenp = r->pend - r->pstart + 1, pstart = r->pstart;
		long len = r->end - r->start + 1;
		if (pstart == 0)
			lenp = 0;

		fprintf(opt->file, "%s%s@@ -%ld,%ld +%ld,%ld @@%s\n",
			line_prefix, set, pstart, lenp, r->start, len, reset);

		diff_flush_range(opt, chunk, r);
	}
}

static void diff_flush_filepair(struct rev_info *rev, struct diff_line_range *range)
{
	struct diff_options *opt = &rev->diffopt;
	struct diff_filespec *one = range->prev, *two = range->spec;
	struct diff_filepair p = {one, two, range->status, 0};
	struct strbuf header = STRBUF_INIT, meta = STRBUF_INIT;
	const char *a_prefix, *b_prefix;
	const char *name_a, *name_b, *a_one, *b_two;
	const char *lbl[2];
	const char *set = diff_get_color_opt(opt, DIFF_METAINFO);
	const char *reset = diff_get_color_opt(opt, DIFF_RESET);
	struct line_chunk chunk;
	int must_show_header;
	char *line_prefix = "";
	struct strbuf *msgbuf;

	if (opt && opt->output_prefix) {
		msgbuf = opt->output_prefix(opt, opt->output_prefix_data);
		line_prefix = msgbuf->buf;
	}

	/*
	 * the ranges that touch no different file, in this case
	 * the line number will not change, and of course we have
	 * no sensible range->pair since there is no diff run.
	 */
	if (!one) {
		if (rev->full_line_diff) {
			chunk.two = two->data;
			chunk.two_end = (const char *)two->data + two->size;
			chunk.ltwo = 1;
			chunk.range = range;
			diff_flush_chunks(&rev->diffopt, &chunk);
		}
		return;
	}

	if (range->status == DIFF_STATUS_DELETED)
		die("We are following an nonexistent file, interesting!");

	name_a  = one->path;
	name_b = two->path;
	fill_metainfo(&meta, name_a, name_b, one, two, opt, &p, &must_show_header,
			DIFF_OPT_TST(opt, COLOR_DIFF));

	diff_set_mnemonic_prefix(opt, "a/", "b/");
	if (DIFF_OPT_TST(opt, REVERSE_DIFF)) {
		a_prefix = opt->b_prefix;
		b_prefix = opt->a_prefix;
	} else {
		a_prefix = opt->a_prefix;
		b_prefix = opt->b_prefix;
	}

	name_a = DIFF_FILE_VALID(one) ? name_a : name_b;
	name_b = DIFF_FILE_VALID(two) ? name_b : name_a;

	a_one = quote_two(a_prefix, name_a + (*name_a == '/'));
	b_two = quote_two(b_prefix, name_b + (*name_b == '/'));
	lbl[0] = DIFF_FILE_VALID(one) ? a_one : "/dev/null";
	lbl[1] = DIFF_FILE_VALID(two) ? b_two : "/dev/null";
	strbuf_addf(&header, "%s%sdiff --git %s %s%s\n", line_prefix,
			set, a_one, b_two, reset);
	if (lbl[0][0] == '/') {
		strbuf_addf(&header, "%s%snew file mode %06o%s\n",
			line_prefix, set, two->mode, reset);
	} else if (lbl[1][0] == '/') {
		strbuf_addf(&header, "%s%sdeleted file mode %06o%s\n",
			line_prefix, set, one->mode, reset);
	} else if (one->mode != two->mode) {
			strbuf_addf(&header, "%s%sold mode %06o%s\n",
				line_prefix, set, one->mode, reset);
			strbuf_addf(&header, "%s%snew mode %06o%s\n",
				line_prefix, set, two->mode, reset);
	}

	fprintf(opt->file, "%s%s", header.buf, meta.buf);
	strbuf_release(&meta);
	strbuf_release(&header);
	fprintf(opt->file, "%s%s--- %s%s\n", line_prefix, set, lbl[0], reset);
	fprintf(opt->file, "%s%s+++ %s%s\n", line_prefix, set, lbl[1], reset);
	free((void *)a_one);
	free((void *)b_two);

	chunk.one = one->data;
	chunk.one_end = (const char *)one->data + one->size;
	chunk.lone = 1;
	chunk.two = two->data;
	chunk.two_end = (const char *)two->data + two->size;
	chunk.ltwo = 1;
	chunk.range = range;
	diff_flush_chunks(&rev->diffopt, &chunk);
}

static void flush_nontrivial_merge(struct rev_info *rev,
		struct diff_line_range *range)
{
	struct diff_options *opt = &rev->diffopt;
	const char *reset = diff_get_color_opt(opt, DIFF_RESET);
	const char *frag = diff_get_color_opt(opt, DIFF_FRAGINFO);
	const char *meta = diff_get_color_opt(opt, DIFF_METAINFO);
	const char *new = diff_get_color_opt(opt, DIFF_FILE_NEW);
	char *line_prefix = "";
	struct strbuf *msgbuf;
	int evil = 0;
	struct diff_line_range *r = range;

	if (opt && opt->output_prefix) {
		msgbuf = opt->output_prefix(opt, opt->output_prefix_data);
		line_prefix = msgbuf->buf;
	}

	while (r) {
		if (r->nr)
			evil = 1;
		r = r->next;
	}

	if (!evil)
		return;

	fprintf(opt->file, "%s%s%s%s\n", line_prefix, meta,
		"nontrivial merge found", reset);

	while (range) {
		if (range->nr) {
			int lno = 1;
			const char *ptr = range->spec->data;
			const char *end = (const char *)range->spec->data + range->spec->size;
			int i = 0;
			fprintf(opt->file, "%s%s%s%s\n", line_prefix,
				meta, range->spec->path, reset);
			for (; i < range->nr; i++) {
				struct line_range *r = range->ranges + i;
				fprintf(opt->file, "%s%s@@ %ld,%ld @@%s\n",
					line_prefix, frag, r->start,
					r->end - r->start + 1, reset);
				flush_lines(opt, &ptr, end, r->start, r->end,
					&lno, new, ' ');
			}
			fprintf(opt->file, "%s\n", line_prefix);
		}
		range = range->next;
	}
}

static void line_log_flush(struct rev_info *rev, struct commit *c)
{
	struct diff_line_range *range = lookup_line_range(rev, c);
	struct diff_line_range *nontrivial = lookup_decoration(&rev->nontrivial_merge,
							&c->object);
	struct log_info log;
	struct diff_options *opt = &rev->diffopt;
	char *line_prefix = "";
	struct strbuf *msgbuf;

	if (!range || !(c->object.flags & NONTRIVIAL_MERGE ||
			c->object.flags & NEED_PRINT ||
			rev->full_line_diff))
		return;

	if (rev->graph)
		graph_update(rev->graph, c);
	log.commit = c;
	log.parent = NULL;
	rev->loginfo = &log;
	show_log(rev);
	rev->loginfo = NULL;

	if (opt && opt->output_prefix) {
		msgbuf = opt->output_prefix(opt, opt->output_prefix_data);
		line_prefix = msgbuf->buf;
	}
	fprintf(rev->diffopt.file, "%s\n", line_prefix);

	if (c->object.flags & NONTRIVIAL_MERGE)
		flush_nontrivial_merge(rev, nontrivial);
	else {
		while (range) {
			if (range->diff || (range->nr && rev->full_line_diff))
				diff_flush_filepair(rev, range);
			range = range->next;
		}
	}

	while (rev->graph && !graph_is_commit_finished(rev->graph)) {
		struct strbuf sb;
		strbuf_init(&sb, 0);
		graph_next_line(rev->graph, &sb);
		fputs(sb.buf, opt->file);
	}
}

static enum rewrite_result rewrite_one(struct rev_info *rev, struct commit **pp)
{
	struct diff_line_range *r = NULL;
	struct commit *p;
	while (1) {
		p = *pp;
		if (p->object.flags & RANGE_UPDATE)
			assign_parents_range(rev, p);
		if (p->object.flags & NEED_PRINT ||
		    p->object.flags & NONTRIVIAL_MERGE)
			return rewrite_one_ok;
		if (!p->parents)
			return rewrite_one_noparents;

		r = lookup_line_range(rev, p);
		if (!r)
			return rewrite_one_noparents;
		*pp = p->parents->item;
	}
}

int line_log_walk(struct rev_info *rev)
{
	struct commit *commit;
	struct commit_list *list = NULL;
	struct diff_line_range *r = NULL;

	if (prepare_revision_walk(rev))
		die("revision walk prepare failed");

	/*
	 * Note that -L automatically turns on --topo-order, so
	 * rev->commits already holds all commits in the range.  The
	 * first commit is our starting point.
	 */
	list = rev->commits;
	if (list) {
		list->item->object.flags |= RANGE_UPDATE;
		list = list->next;
	}
	/* Clear the flags */
	while (list) {
		list->item->object.flags &= ~(RANGE_UPDATE | NONTRIVIAL_MERGE |
				NEED_PRINT | EVIL_MERGE);
		list = list->next;
	}

	list = rev->commits;
	while (list) {
		struct commit_list *need_free = list;
		commit = list->item;

		if (commit->object.flags & RANGE_UPDATE)
			assign_parents_range(rev, commit);

		if (commit->object.flags & NEED_PRINT ||
		    commit->object.flags & NONTRIVIAL_MERGE) {
			if (rewrite_parents(rev, commit, rewrite_one))
				die("Can't rewrite parent for commit %s",
					sha1_to_hex(commit->object.sha1));
		}

		if (commit->object.flags & NEED_PRINT ||
		    commit->object.flags & NONTRIVIAL_MERGE ||
		    rev->full_line_diff)
			line_log_flush(rev, commit);

		clear_commit_line_range(rev, commit);

		r = lookup_decoration(&rev->nontrivial_merge, &commit->object);
		if (r) {
			free_diff_line_ranges(r);
			add_decoration(&rev->nontrivial_merge, &commit->object,
				       NULL);
		}

		list = list->next;
		free(need_free);
	}

	return 0;
}
