#ifndef OBJECT_H
#define OBJECT_H

/*
 * If USES_OBJECT_ID_OBJECT is defined, then struct object contains a
 * struct object_id instead of unsigned char[20] and all the functions
 * take a struct object_id * instead of an unsigned char *.
 *
 * This is temporary to assist with the conversion and maintain full
 * bisectability.  There are simply too many files to convert all at
 * once.  A file need only #define USES_OBJECT_ID_OBJECT before
 * including any headers and it will get the new version.
 *
 * After all files have been converted, this define will be removed
 * and the other files updated to remove their defines.  Since that's
 * only a one-line change it's much more reasonable to do that to them
 * all at once.
 *
 * Until all files are converted, the functions that are being converted
 * are defined inline to call implementation functions that have been
 * renamed with an _hash suffix.  At the end of the conversion those
 * functions will then be converted and renamed.
 */

struct object_list {
	struct object *item;
	struct object_list *next;
};

struct object_array {
	unsigned int nr;
	unsigned int alloc;
	struct object_array_entry {
		struct object *item;
		/*
		 * name or NULL.  If non-NULL, the memory pointed to
		 * is owned by this object *except* if it points at
		 * object_array_slopbuf, which is a static copy of the
		 * empty string.
		 */
		char *name;
		char *path;
		unsigned mode;
	} *objects;
};

#define OBJECT_ARRAY_INIT { 0, 0, NULL }

#define TYPE_BITS   3
/*
 * object flag allocation:
 * revision.h:      0---------10                                26
 * fetch-pack.c:    0---4
 * walker.c:        0-2
 * upload-pack.c:               11----------------19
 * builtin/blame.c:               12-13
 * bisect.c:                               16
 * bundle.c:                               16
 * http-push.c:                            16-----19
 * commit.c:                               16-----19
 * sha1_name.c:                                     20
 */
#define FLAG_BITS  27

/*
 * The object type is stored in 3 bits.
 */
struct object {
	unsigned parsed : 1;
	unsigned used : 1;
	unsigned type : TYPE_BITS;
	unsigned flags : FLAG_BITS;
#ifdef USES_OBJECT_ID_OBJECT
	/*
	 * Since struct object_id contains only one member and that member
	 * has an alignment of 1, we can get away with this.  Also this
	 * #ifdef alternative situation while ugly is only temporary anyway.
	 */
	struct object_id oid;
#else
	unsigned char sha1[20];
#endif
};

extern const char *typename(unsigned int type);
extern int type_from_string_gently(const char *str, ssize_t, int gentle);
#define type_from_string(str) type_from_string_gently(str, -1, 0)

/*
 * Return the current number of buckets in the object hashmap.
 */
extern unsigned int get_max_object_index(void);

/*
 * Return the object from the specified bucket in the object hashmap.
 */
extern struct object *get_indexed_object(unsigned int);

/*
 * This can be used to see if we have heard of the object before, but
 * it can return "yes we have, and here is a half-initialised object"
 * for an object that we haven't loaded/parsed yet.
 *
 * When parsing a commit to create an in-core commit object, its
 * parents list holds commit objects that represent its parents, but
 * they are expected to be lazily initialized and do not know what
 * their trees or parents are yet.  When this function returns such a
 * half-initialised objects, the caller is expected to initialize them
 * by calling parse_object() on them.
 */
struct object *lookup_object_hash(const unsigned char *sha1);
#ifdef USES_OBJECT_ID_OBJECT
static inline struct object *lookup_object(const struct object_id *oid)
{
	return lookup_object_hash(oid->hash);
}
#else
static inline struct object *lookup_object(const unsigned char *sha1)
{
	return lookup_object_hash(sha1);
}
#endif

extern void *create_object_hash(const unsigned char *sha1, void *obj);
#ifdef USES_OBJECT_ID_OBJECT
static inline void *create_object(const struct object_id *oid, void *obj)
{
	return create_object_hash(oid->hash, obj);
}
#else
static inline void *create_object(const unsigned char *sha1, void *obj)
{
	return create_object_hash(sha1, obj);
}
#endif

void *object_as_type(struct object *obj, enum object_type type, int quiet);

/*
 * Returns the object, having parsed it to find out what it is.
 *
 * Returns NULL if the object is missing or corrupt.
 */
struct object *parse_object_hash(const unsigned char *sha1);
#ifdef USES_OBJECT_ID_OBJECT
static inline struct object *parse_object(const struct object_id *oid)
{
	return parse_object_hash(oid->hash);
}
#else
static inline struct object *parse_object(const unsigned char *sha1)
{
	return parse_object_hash(sha1);
}
#endif

/*
 * Like parse_object, but will die() instead of returning NULL. If the
 * "name" parameter is not NULL, it is included in the error message
 * (otherwise, the sha1 hex is given).
 */
struct object *parse_object_or_die_hash(const unsigned char *sha1, const char *name);
#ifdef USES_OBJECT_ID_OBJECT
static inline struct object *parse_object_or_die(const struct object_id *oid, const char *name)
{
	return parse_object_or_die_hash(oid->hash, name);
}
#else
static inline struct object *parse_object_or_die(const unsigned char *sha1, const char *name)
{
	return parse_object_or_die_hash(sha1, name);
}
#endif

/* Given the result of read_sha1_file(), returns the object after
 * parsing it.  eaten_p indicates if the object has a borrowed copy
 * of buffer and the caller should not free() it.
 */
struct object *parse_object_buffer_hash(const unsigned char *sha1, enum object_type type, unsigned long size, void *buffer, int *eaten_p);
#ifdef USES_OBJECT_ID_OBJECT
static inline struct object *parse_object_buffer(const struct object_id *oid, enum object_type type, unsigned long size, void *buffer, int *eaten_p)
{
	return parse_object_buffer_hash(oid->hash, type, size, buffer, eaten_p);
}
#else
static inline struct object *parse_object_buffer(const unsigned char *sha1, enum object_type type, unsigned long size, void *buffer, int *eaten_p)
{
	return parse_object_buffer_hash(sha1, type, size, buffer, eaten_p);
}
#endif

/** Returns the object, with potentially excess memory allocated. **/
struct object *lookup_unknown_object_hash(const unsigned char *sha1);
#ifdef USES_OBJECT_ID_OBJECT
static inline struct object *lookup_unknown_object(const struct object_id *oid)
{
	return lookup_unknown_object_hash(oid->hash);
}
#else
static inline struct object *lookup_unknown_object(const unsigned char *sha1)
{
	return lookup_unknown_object_hash(sha1);
}
#endif

struct object_list *object_list_insert(struct object *item,
				       struct object_list **list_p);

int object_list_contains(struct object_list *list, struct object *obj);

/* Object array handling .. */
void add_object_array(struct object *obj, const char *name, struct object_array *array);
void add_object_array_with_path(struct object *obj, const char *name, struct object_array *array, unsigned mode, const char *path);

typedef int (*object_array_each_func_t)(struct object_array_entry *, void *);

/*
 * Apply want to each entry in array, retaining only the entries for
 * which the function returns true.  Preserve the order of the entries
 * that are retained.
 */
void object_array_filter(struct object_array *array,
			 object_array_each_func_t want, void *cb_data);

/*
 * Remove from array all but the first entry with a given name.
 * Warning: this function uses an O(N^2) algorithm.
 */
void object_array_remove_duplicates(struct object_array *array);

/*
 * Remove any objects from the array, freeing all used memory; afterwards
 * the array is ready to store more objects with add_object_array().
 */
void object_array_clear(struct object_array *array);

void clear_object_flags(unsigned flags);

#endif /* OBJECT_H */
