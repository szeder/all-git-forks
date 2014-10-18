#ifndef STRBUF_H
#define STRBUF_H

/* XXX transition */
#ifndef xrealloc
#define xrealloc(X, Y) realloc(X, Y)
#endif
#if 0
extern void *xrealloc(void *, size_t);
#endif

/* From git-compat-util.h */

#define REALLOC_ARRAY(x, alloc) (x) = xrealloc((x), (alloc) * sizeof(*(x)))

/* From cache.h: */

#define alloc_nr(x) (((x)+16)*3/2)

/*
 * Realloc the buffer pointed at by variable 'x' so that it can hold
 * at least 'nr' entries; the number of entries currently allocated
 * is 'alloc', using the standard growing factor alloc_nr() macro.
 *
 * DO NOT USE any expression with side-effect for 'x', 'nr', or 'alloc'.
 */
#define ALLOC_GROW(x, nr, alloc) \
	do { \
		if ((nr) > alloc) { \
			if (alloc_nr(alloc) < (nr)) \
				alloc = (nr); \
			else \
				alloc = alloc_nr(alloc); \
			REALLOC_ARRAY(x, alloc); \
		} \
	} while (0)

/* See Documentation/technical/api-strbuf.txt */

extern char strbuf_slopbuf[];
struct strbuf {
	size_t alloc;
	size_t len;
	char *buf;
};

#define STRBUF_INIT  { 0, 0, strbuf_slopbuf }

/*----- strbuf life cycle -----*/
extern void strbuf_init(struct strbuf *, size_t);
extern void strbuf_release(struct strbuf *);

/*----- strbuf size related -----*/
extern void strbuf_grow(struct strbuf *, size_t);

static inline void strbuf_setlen(struct strbuf *sb, size_t len)
{
	if (len > (sb->alloc ? sb->alloc - 1 : 0)) {
		printf("BUG: strbuf_setlen() beyond buffer\n");
		abort();
	}
	sb->len = len;
	sb->buf[len] = '\0';
}
#define strbuf_reset(sb)  strbuf_setlen(sb, 0)

/*----- add data in your buffer -----*/
static inline void strbuf_addch(struct strbuf *sb, int c)
{
	strbuf_grow(sb, 1);
	sb->buf[sb->len++] = c;
	sb->buf[sb->len] = '\0';
}

extern void strbuf_remove(struct strbuf *, size_t pos, size_t len);

extern void strbuf_add(struct strbuf *, const void *, size_t);

extern size_t strbuf_fread(struct strbuf *, size_t, FILE *);

#endif /* STRBUF_H */
