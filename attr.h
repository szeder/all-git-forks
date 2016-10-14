#ifndef ATTR_H
#define ATTR_H

/* An attribute is a pointer to this opaque structure */
struct git_attr;

/*
 * Given a string, return the gitattribute object that
 * corresponds to it.
 */
extern struct git_attr *git_attr(const char *);

/*
 * Return the name of the attribute represented by the argument.  The
 * return value is a pointer to a null-delimited string that is part
 * of the internal data structure; it should not be modified or freed.
 */
extern const char *git_attr_name(const struct git_attr *);

extern int attr_name_valid(const char *name, size_t namelen);
extern void invalid_attr_name_message(struct strbuf *, const char *, int);

/* Internal use */
extern const char git_attr__true[];
extern const char git_attr__false[];

/* For public to check git_attr_check results */
#define ATTR_TRUE(v) ((v) == git_attr__true)
#define ATTR_FALSE(v) ((v) == git_attr__false)
#define ATTR_UNSET(v) ((v) == NULL)

struct git_attr_check {
	int finalized;
	int check_nr;
	int check_alloc;
	const struct git_attr **attr;
};
#define GIT_ATTR_CHECK_INIT {0, 0, 0, NULL}

struct git_attr_result {
	int check_nr;
	int check_alloc;
	const char **value;
};
#define GIT_ATTR_RESULT_INIT {0, 0, NULL}

/*
 * Initialize the `git_attr_check` via one of the following three functions:
 *
 * git_attr_check_alloc allocates an empty check, add more attributes via
 *                      git_attr_check_append.
 * git_all_attrs        allocates a check and fills in all attributes that
 *                      are set for the given path.
 * git_attr_check_initl takes a pointer to where the check will be initialized,
 *                      followed by all attributes that are to be checked.
 *                      This makes it potentially thread safe as it could
 *                      internally have a mutex for that memory location.
 *                      Currently it is not thread safe!
 */
extern struct git_attr_check *git_attr_check_alloc(void);
extern void git_attr_check_append(struct git_attr_check *, const struct git_attr *);
extern void git_all_attrs(const char *path, struct git_attr_check *, struct git_attr_result *);
extern void git_attr_check_initl(struct git_attr_check **, const char *, ...);

/* Query a path for its attributes */
extern struct git_attr_result *git_check_attr(const char *path,
					      struct git_attr_check *);

/**
 * Free or clear the result struct. The underlying strings are not free'd
 * as they are globally known.
 */
extern void git_attr_result_free(struct git_attr_result *);
extern void git_attr_result_clear(struct git_attr_result *);

extern void git_attr_check_clear(struct git_attr_check *);


enum git_attr_direction {
	GIT_ATTR_CHECKIN,
	GIT_ATTR_CHECKOUT,
	GIT_ATTR_INDEX
};
void git_attr_set_direction(enum git_attr_direction, struct index_state *);

#endif /* ATTR_H */
