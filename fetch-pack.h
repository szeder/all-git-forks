#ifndef FETCH_PACK_H
#define FETCH_PACK_H

#include "string-list.h"
#include "run-command.h"

struct sha1_array;

struct fetch_pack_args {
	const char *uploadpack;
	int transport_version;
	int unpacklimit;
	int depth;
	unsigned quiet:1;
	unsigned keep_pack:1;
	unsigned lock_pack:1;
	unsigned use_thin_pack:1;
	unsigned fetch_all:1;
	unsigned stdin_refs:1;
	unsigned diag_url:1;
	unsigned verbose:1;
	unsigned no_progress:1;
	unsigned include_tag:1;
	unsigned stateless_rpc:1;
	unsigned check_self_contained_and_connected:1;
	unsigned self_contained_and_connected:1;
	unsigned cloning:1;
	unsigned update_shallow:1;
};

/*
 * In version 2 of the pack protocol we negotiate the capabilities
 * before the actual transfer of refs and packs.
 */
void negotiate_capabilities(int fd[2], struct fetch_pack_args *args);

/*
 * sought represents remote references that should be updated from.
 * On return, the names that were found on the remote will have been
 * marked as such.
 */
struct ref *fetch_pack(struct fetch_pack_args *args,
		       int fd[], struct child_process *conn,
		       const struct ref *ref,
		       const char *dest,
		       struct ref **sought,
		       int nr_sought,
		       struct sha1_array *shallow,
		       char **pack_lockfile);

#endif
