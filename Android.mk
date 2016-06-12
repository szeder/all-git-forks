LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
		git.c \

LOCAL_SRC_FILES += \
		abspath.c \
		advice.c \
		alias.c \
		alloc.c \
		archive.c \
		archive-tar.c \
		archive-zip.c \
		argv-array.c \
		attr.c \
		base85.c \
		bisect.c \
		blob.c \
		branch.c \
		bulk-checkin.c \
		bundle.c \
		cache-tree.c \
		column.c \
		color.c \
		combine-diff.c \
		commit.c \
		compat/obstack.c \
		compat/terminal.c \
		config.c \
		connect.c \
		connected.c \
		convert.c \
		copy.c \
		credential.c \
		csum-file.c \
		ctype.c \
		date.c \
		decorate.c \
		diff.c \
		diffcore-break.c \
		diffcore-delta.c \
		diffcore-order.c \
		diffcore-pickaxe.c \
		diffcore-rename.c \
		diff-delta.c \
		diff-lib.c \
		diff-no-index.c \
		dir.c \
		editor.c \
		entry.c \
		environment.c \
		exec_cmd.c \
		ewah/bitmap.c \
		ewah/ewah_bitmap.c \
		ewah/ewah_io.c \
		ewah/ewah_rlw.c \
		fsck.c \
		fetch-pack.c \
		gpg-interface.c \
		graph.c \
		grep.c \
		hashmap.c \
		help.c \
		hex.c \
		ident.c \
		kwset.c \
		levenshtein.c \
		line-log.c \
		line-range.c \
		list-objects.c \
		ll-merge.c \
		lockfile.c \
		log-tree.c \
		mailinfo.c \
		mailmap.c \
		match-trees.c \
		merge.c \
		merge-blobs.c \
		merge-recursive.c \
		mergesort.c \
		name-hash.c \
		notes.c \
		notes-cache.c \
		notes-merge.c \
		notes-utils.c \
		unpack-trees.c \
		object.c \
		pack-bitmap.c \
		pack-bitmap-write.c \
		pack-check.c \
		pack-objects.c \
		pack-revindex.c \
		pack-write.c \
		pager.c \
		parse-options.c \
		parse-options-cb.c \
		patch-delta.c \
		patch-ids.c \
		path.c \
		pathspec.c \
		pkt-line.c \
		preload-index.c \
		pretty.c \
		prio-queue.c \
		progress.c \
		prompt.c \
		quote.c \
		reachable.c \
		read-cache.c \
		refs.c \
		ref-filter.c \
		reflog-walk.c \
		refs/files-backend.c \
		remote.c \
		replace_object.c \
		rerere.c \
		resolve-undo.c \
		revision.c \
		run-command.c \
		send-pack.c \
		sequencer.c \
		setup.c \
		server-info.c \
		sha1-array.c \
		sha1_file.c \
		sha1-lookup.c \
		sha1_name.c \
		shallow.c \
		sideband.c \
		sigchain.c \
		split-index.c \
		strbuf.c \
		streaming.c \
		string-list.c \
		submodule.c \
		submodule-config.c \
		symlinks.c \
		tag.c \
		tempfile.c \
		thread-utils.c \
		trailer.c \
		trace.c \
		transport.c \
		transport-helper.c \
		tree.c \
		tree-diff.c \
		tree-walk.c \
		url.c \
		urlmatch.c \
		usage.c \
		userdiff.c \
		utf8.c \
		varint.c \
		version.c \
		versioncmp.c \
		wildmatch.c \
		worktree.c \
		wrapper.c \
		write_or_die.c \
		ws.c \
		wt-status.c \
		xdiff-interface.c \
		xdiff/xdiffi.c \
		xdiff/xemit.c \
		xdiff/xhistogram.c \
		xdiff/xmerge.c \
		xdiff/xpatience.c \
		xdiff/xprepare.c \
		xdiff/xutils.c \
		zlib.c \

LOCAL_SRC_FILES += \
		builtin/add.c \
		builtin/am.c \
		builtin/annotate.c \
		builtin/apply.c \
		builtin/archive.c \
		builtin/bisect--helper.c \
		builtin/blame.c \
		builtin/branch.c \
		builtin/bundle.c \
		builtin/cat-file.c \
		builtin/check-attr.c \
		builtin/check-ignore.c \
		builtin/check-mailmap.c \
		builtin/checkout.c \
		builtin/checkout-index.c \
		builtin/check-ref-format.c \
		builtin/clean.c \
		builtin/clone.c \
		builtin/column.c \
		builtin/commit.c \
		builtin/commit-tree.c \
		builtin/config.c \
		builtin/count-objects.c \
		builtin/credential.c \
		builtin/describe.c \
		builtin/diff.c \
		builtin/diff-files.c \
		builtin/diff-index.c \
		builtin/diff-tree.c \
		builtin/fast-export.c \
		builtin/fetch.c \
		builtin/fetch-pack.c \
		builtin/fmt-merge-msg.c \
		builtin/for-each-ref.c \
		builtin/fsck.c \
		builtin/gc.c \
		builtin/get-tar-commit-id.c \
		builtin/grep.c \
		builtin/hash-object.c \
		builtin/help.c \
		builtin/index-pack.c \
		builtin/init-db.c \
		builtin/interpret-trailers.c \
		builtin/log.c \
		builtin/ls-files.c \
		builtin/ls-remote.c \
		builtin/ls-tree.c \
		builtin/mailinfo.c \
		builtin/mailsplit.c \
		builtin/merge-base.c \
		builtin/merge.c \
		builtin/merge-file.c \
		builtin/merge-index.c \
		builtin/merge-ours.c \
		builtin/merge-recursive.c \
		builtin/merge-tree.c \
		builtin/mktag.c \
		builtin/mktree.c \
		builtin/mv.c \
		builtin/name-rev.c \
		builtin/notes.c \
		builtin/pack-objects.c \
		builtin/pack-redundant.c \
		builtin/pack-refs.c \
		builtin/patch-id.c \
		builtin/prune.c \
		builtin/prune-packed.c \
		builtin/pull.c \
		builtin/push.c \
		builtin/read-tree.c \
		builtin/receive-pack.c \
		builtin/reflog.c \
		builtin/remote.c \
		builtin/remote-ext.c \
		builtin/remote-fd.c \
		builtin/repack.c \
		builtin/replace.c \
		builtin/rerere.c \
		builtin/reset.c \
		builtin/revert.c \
		builtin/rev-list.c \
		builtin/rev-parse.c \
		builtin/rm.c \
		builtin/send-pack.c \
		builtin/shortlog.c \
		builtin/show-branch.c \
		builtin/show-ref.c \
		builtin/stripspace.c \
		builtin/submodule--helper.c \
		builtin/symbolic-ref.c \
		builtin/tag.c \
		builtin/unpack-file.c \
		builtin/unpack-objects.c \
		builtin/update-index.c \
		builtin/update-ref.c \
		builtin/update-server-info.c \
		builtin/upload-archive.c \
		builtin/var.c \
		builtin/verify-commit.c \
		builtin/verify-pack.c \
		builtin/verify-tag.c \
		builtin/worktree.c \
		builtin/write-tree.c \

LOCAL_MODULE := git
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_CLASS := EXECUTABLES

SHA1_HEADER_SQ := <openssl/sha.h>
GIT_VERSION := $(shell $(LOCAL_PATH)/GIT-VERSION-GEN|sed 's/GIT_VERSION = //' 2>/dev/null)
GIT_USER_AGENT = git/$(GIT_VERSION)
LOCAL_CFLAGS += \
		-DNO_ICONV \
		-DNO_GETTEXT \
		-DHAVE_DEV_TTY \
		-DSHA1_HEADER='$(SHA1_HEADER_SQ)' \
		-DGIT_VERSION=\"$(GIT_VERSION)\" \
		-DGIT_USER_AGENT=\"$(GIT_USER_AGENT)\" \


LOCAL_CFLAGS += \
		-DGIT_HTML_PATH=\"doc\" \
		-DGIT_MAN_PATH=\"man\" \
		-DGIT_INFO_PATH=\"info\" \
		-DGIT_EXEC_PATH=\"bin\" \
		-DPREFIX=\"/system/bin/git\" \
		-DBINDIR=\"bin\" \
		-DETC_GITCONFIG=\"etc\" \
		-DETC_GITCONFIG=\"etc/gitconfig\" \
		-DETC_GITATTRIBUTES=\"etc/gitattributes\" \


LOCAL_C_INCLUDES := \
		external/zlib \
		external/boringssl/include \

intermediates:= $(local-generated-sources-dir)
GEN := $(intermediates)/common-cmds.h
$(GEN): PRIVATE_GENERATE_CMDLIST_SH := $(LOCAL_PATH)/generate-cmdlist.sh
$(GEN): PRIVATE_COMMAND_LIST_TXT := $(LOCAL_PATH)/command-list.txt
$(GEN): PRIVATE_CUSTOM_TOOL = $(PRIVATE_GENERATE_CMDLIST_SH) $(PRIVATE_COMMAND_LIST_TXT) > $@
$(GEN): $(PRIVATE_GENERATE_CMDLIST_SH) $(PRIVATE_COMMAND_LIST_TXT) $(wildcard Documentation/git-*.txt)
	$(transform-generated-source)
LOCAL_GENERATED_SOURCES += $(GEN)


LOCAL_SHARED_LIBRARIES := \
		libz \
		libcrypto \


BUILT_INS := \
BUILT_INS += git-cherry
BUILT_INS += git-cherry-pick
BUILT_INS += git-format-patch
BUILT_INS += git-fsck-objects
BUILT_INS += git-init
BUILT_INS += git-merge-subtree
BUILT_INS += git-show
BUILT_INS += git-stage
BUILT_INS += git-status
BUILT_INS += git-whatchanged

BUILT_INS += git-add
BUILT_INS += git-am
BUILT_INS += git-annotate
BUILT_INS += git-apply
BUILT_INS += git-archive
BUILT_INS += git-bisect--helper
BUILT_INS += git-blame
BUILT_INS += git-branch
BUILT_INS += git-bundle
BUILT_INS += git-cat-file
BUILT_INS += git-check-attr
BUILT_INS += git-check-ignore
BUILT_INS += git-check-mailmap
BUILT_INS += git-check-ref-format
BUILT_INS += git-checkout-index
BUILT_INS += git-checkout
BUILT_INS += git-clean
BUILT_INS += git-clone
BUILT_INS += git-column
BUILT_INS += git-commit-tree
BUILT_INS += git-commit
BUILT_INS += git-config
BUILT_INS += git-count-objects
BUILT_INS += git-credential
BUILT_INS += git-describe
BUILT_INS += git-diff-files
BUILT_INS += git-diff-index
BUILT_INS += git-diff-tree
BUILT_INS += git-diff
BUILT_INS += git-fast-export
BUILT_INS += git-fetch-pack
BUILT_INS += git-fetch
BUILT_INS += git-fmt-merge-msg
BUILT_INS += git-for-each-ref
BUILT_INS += git-fsck
BUILT_INS += git-gc
BUILT_INS += git-get-tar-commit-id
BUILT_INS += git-grep
BUILT_INS += git-hash-object
BUILT_INS += git-help
BUILT_INS += git-index-pack
BUILT_INS += git-init-db
BUILT_INS += git-interpret-trailers
BUILT_INS += git-log
BUILT_INS += git-ls-files
BUILT_INS += git-ls-remote
BUILT_INS += git-ls-tree
BUILT_INS += git-mailinfo
BUILT_INS += git-mailsplit
BUILT_INS += git-merge
BUILT_INS += git-merge-base
BUILT_INS += git-merge-file
BUILT_INS += git-merge-index
BUILT_INS += git-merge-ours
BUILT_INS += git-merge-recursive
BUILT_INS += git-merge-tree
BUILT_INS += git-mktag
BUILT_INS += git-mktree
BUILT_INS += git-mv
BUILT_INS += git-name-rev
BUILT_INS += git-notes
BUILT_INS += git-pack-objects
BUILT_INS += git-pack-redundant
BUILT_INS += git-pack-refs
BUILT_INS += git-patch-id
BUILT_INS += git-prune-packed
BUILT_INS += git-prune
BUILT_INS += git-pull
BUILT_INS += git-push
BUILT_INS += git-read-tree
BUILT_INS += git-receive-pack
BUILT_INS += git-reflog
BUILT_INS += git-remote
BUILT_INS += git-remote-ext
BUILT_INS += git-remote-fd
BUILT_INS += git-repack
BUILT_INS += git-replace
BUILT_INS += git-rerere
BUILT_INS += git-reset
BUILT_INS += git-rev-list
BUILT_INS += git-rev-parse
BUILT_INS += git-revert
BUILT_INS += git-rm
BUILT_INS += git-send-pack
BUILT_INS += git-shortlog
BUILT_INS += git-show-branch
BUILT_INS += git-show-ref
BUILT_INS += git-stripspace
BUILT_INS += git-submodule--helper
BUILT_INS += git-symbolic-ref
BUILT_INS += git-tag
BUILT_INS += git-unpack-file
BUILT_INS += git-unpack-objects
BUILT_INS += git-update-index
BUILT_INS += git-update-ref
BUILT_INS += git-update-server-info
BUILT_INS += git-upload-archive
BUILT_INS += git-var
BUILT_INS += git-verify-commit
BUILT_INS += git-verify-pack
BUILT_INS += git-verify-tag
BUILT_INS += git-worktree
BUILT_INS += git-write-tree

LOCAL_POST_INSTALL_CMD := $(hide) $(foreach t,$(BUILT_INS),ln -sf git $(TARGET_OUT)/bin/$(t);)

include $(BUILD_EXECUTABLE)

#git-remote-http
#git-remote-https
#git-remote-ftp
#git-remote-ftps

# http-fetch.o
# http-push.o
# -DUSE_CURL_FOR_IMAP_SEND
#  http.o
#	 REMOTE_CURL_ALIASES = git-remote-https$X git-remote-ftp$X git-remote-ftps$X
#	 REMOTE_CURL_NAMES = $(REMOTE_CURL_PRIMARY) $(REMOTE_CURL_ALIASES)
#	 PROGRAM_OBJS += http-fetch.o
#	 PROGRAMS += $(REMOTE_CURL_NAMES)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
		http.c \
		http-walker.c \
		remote-curl.c \

LOCAL_SRC_FILES += \
		argv-array.c \
		hex.c \
		zlib.c \
		strbuf.c \
		setup.c \
		exec_cmd.c \
		string-list.c \
		transport.c \
		config.c \
		credential.c \
		wrapper.c \
		abspath.c \
		run-command.c \
		prompt.c \
		url.c \
		hashmap.c \
		path.c \
		environment.c \
		shallow.c \
		refs.c \
		trace.c \
		sha1_file.c \
		sigchain.c \
		usage.c \
		utf8.c \
		object.c \
		dir.c \
		quote.c \
		mailmap.c \
		ws.c \
		advice.c \
		ident.c \
		remote.c \
		commit.c \
		date.c \
		tag.c \
		tree.c \
		alloc.c \
		blob.c \
		mergesort.c \
		varint.c \
		connect.c \
		streaming.c \
		walker.c \
		ctype.c \
		gettext.c \
		urlmatch.c \
		lockfile.c \
		tempfile.c \
		convert.c \
		version.c \
		sha1-array.c \
		ewah/ewah_io.c \
		ewah/ewah_bitmap.c \
		resolve-undo.c \
		name-hash.c \
		cache-tree.c \
		split-index.c \
		refs/files-backend.c \
		read-cache.c \
		tree-walk.c \
		sha1_name.c \
		pkt-line.c \
		write_or_die.c \
		pack-check.c \
		pack-revindex.c \
		patch-delta.c \
		sha1-lookup.c \
		replace_object.c \
		compat/terminal.c \

LOCAL_MODULE := git-remote-http
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_CLASS := EXECUTABLES

SHA1_HEADER_SQ := <openssl/sha.h>
LOCAL_CFLAGS += \
		-DNO_ICONV \
		-DNO_GETTEXT \
		-DHAVE_DEV_TTY \
		-DSHA1_HEADER='$(SHA1_HEADER_SQ)' \

GIT_VERSION := $(shell $(LOCAL_PATH)/GIT-VERSION-GEN|sed 's/GIT_VERSION = //' 2>/dev/null)
GIT_USER_AGENT = git/$(GIT_VERSION)
LOCAL_CFLAGS += \
		-DGIT_VERSION=\"$(GIT_VERSION)\" \
		-DGIT_USER_AGENT=\"$(GIT_USER_AGENT)\" \

LOCAL_CFLAGS += \
		-DGIT_HTML_PATH=\"doc\" \
		-DGIT_MAN_PATH=\"man\" \
		-DGIT_INFO_PATH=\"info\" \
		-DGIT_EXEC_PATH=\"bin\" \
		-DPREFIX=\"/system/bin/git\" \
		-DBINDIR=\"bin\" \
		-DETC_GITCONFIG=\"etc\" \
		-DETC_GITCONFIG=\"etc/gitconfig\" \
		-DETC_GITATTRIBUTES=\"etc/gitattributes\" \

LOCAL_C_INCLUDES := \
		external/boringssl/include \
		external/curl/include \
		external/expat/lib \
		external/zlib \

LOCAL_SHARED_LIBRARIES := \
		libcurl \
		libexpat \
		libz \
		libcrypto \

REMOTE_CURL_ALIASES := git-remote-https git-remote-ftp git-remote-ftps
LOCAL_POST_INSTALL_CMD := $(hide) $(foreach t,$(REMOTE_CURL_ALIASES),ln -sf git-remote-http $(TARGET_OUT)/bin/$(t);)

include $(BUILD_EXECUTABLE)
