git_src := $(call my-dir)

gitprefix = /system
gitexecdir = lib/git-core
gitmandir = $(gitprefix)/usr/share/man
gitinfodir = $(gitprefix)/usr/share/info
template_dir = usr/share/git-core/templates

GIT_SHELL = /system/bin/sh
GIT_EDITOR = vim
GIT_PAGER = more

# =============================================================================
# /system/etc/gitconfig
# =============================================================================

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)

LOCAL_MODULE := gitconfig
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := android/gitconfig

include $(BUILD_PREBUILT)

# =============================================================================
# /system/lib/git-core/*.sh
# =============================================================================

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)

include $(LOCAL_PATH)/GIT-VERSION-FILE
GIT_USER_AGENT = git/$(GIT_VERSION)

### SCRIPT FILES ###

SCRIPT_SH =
SCRIPT_LIB =

SCRIPT_SH += git-am.sh
SCRIPT_SH += git-bisect.sh
SCRIPT_SH += git-difftool--helper.sh
SCRIPT_SH += git-filter-branch.sh
SCRIPT_SH += git-merge-octopus.sh
SCRIPT_SH += git-merge-one-file.sh
SCRIPT_SH += git-merge-resolve.sh
SCRIPT_SH += git-mergetool.sh
SCRIPT_SH += git-pull.sh
SCRIPT_SH += git-quiltimport.sh
SCRIPT_SH += git-rebase.sh
SCRIPT_SH += git-remote-testgit.sh
SCRIPT_SH += git-request-pull.sh
SCRIPT_SH += git-stash.sh
SCRIPT_SH += git-submodule.sh
SCRIPT_SH += git-web--browse.sh

SCRIPT_LIB += git-mergetool--lib
SCRIPT_LIB += git-parse-remote
SCRIPT_LIB += git-rebase--am
SCRIPT_LIB += git-rebase--interactive
SCRIPT_LIB += git-rebase--merge
SCRIPT_LIB += git-sh-setup
#SCRIPT_LIB += git-sh-i18n

####################################

# All other scripts + lib to /system/lib/git-core/

gitshfixup_regex := $(subst /,\/,\#!/bin/sh)/$(subst /,\/,\#!$(GIT_SHELL))

GIT_SCRIPTS := $(SCRIPT_SH)
$(GIT_SCRIPTS): GIT_BINARY := $(LOCAL_MODULE)
$(GIT_SCRIPTS): $(LOCAL_INSTALLED_MODULE)
	@echo -e ${CL_CYN}"Install: $(gitprefix)/$(gitexecdir)/$@"${CL_RST}
	@mkdir -p $(TARGET_OUT)/$(gitexecdir)
	$(hide) cp $(git_src)/$@ $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i 's/$(gitshfixup_regex)/g' $(TARGET_OUT)/$(gitexecdir)/$@

ALL_DEFAULT_INSTALLED_MODULES += $(GIT_SCRIPTS)

# other script lib files need a shell header (or not, often included)

GIT_SCRIPT_LIB := $(SCRIPT_LIB)
$(GIT_SCRIPT_LIB): GIT_BINARY := $(LOCAL_MODULE)
$(GIT_SCRIPT_LIB): $(LOCAL_INSTALLED_MODULE)
	@echo -e ${CL_CYN}"Install: $(gitprefix)/$(gitexecdir)/$@"${CL_RST}
	@mkdir -p $(TARGET_OUT)/$(gitexecdir)
	$(hide) cp $(git_src)/$@.sh $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i '1i#!$(GIT_SHELL)' $(TARGET_OUT)/$(gitexecdir)/$@

ALL_DEFAULT_INSTALLED_MODULES += $(GIT_SCRIPT_LIB)

ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
        $(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(GIT_SCRIPTS) $(GIT_SCRIPT_LIB)


# =============================================================================
# /system/xbin/git
# =============================================================================

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)

LIB_OBJS =
BUILTIN_OBJS =
XDIFF_OBJS =

LIB_OBJS += abspath.c
LIB_OBJS += advice.c
LIB_OBJS += alias.c
LIB_OBJS += alloc.c
LIB_OBJS += archive.c
LIB_OBJS += archive-tar.c
LIB_OBJS += archive-zip.c
LIB_OBJS += argv-array.c
LIB_OBJS += attr.c
LIB_OBJS += base85.c
LIB_OBJS += bisect.c
LIB_OBJS += blob.c
LIB_OBJS += branch.c
LIB_OBJS += bulk-checkin.c
LIB_OBJS += bundle.c
LIB_OBJS += cache-tree.c
LIB_OBJS += color.c
LIB_OBJS += column.c
LIB_OBJS += combine-diff.c
LIB_OBJS += commit.c
LIB_OBJS += compat/obstack.c
LIB_OBJS += compat/terminal.c
LIB_OBJS += config.c
LIB_OBJS += connect.c
LIB_OBJS += connected.c
LIB_OBJS += convert.c
LIB_OBJS += copy.c
LIB_OBJS += credential.c
LIB_OBJS += csum-file.c
LIB_OBJS += ctype.c
LIB_OBJS += date.c
LIB_OBJS += decorate.c
LIB_OBJS += diffcore-break.c
LIB_OBJS += diffcore-delta.c
LIB_OBJS += diffcore-order.c
LIB_OBJS += diffcore-pickaxe.c
LIB_OBJS += diffcore-rename.c
LIB_OBJS += diff-delta.c
LIB_OBJS += diff-lib.c
LIB_OBJS += diff-no-index.c
LIB_OBJS += diff.c
LIB_OBJS += dir.c
LIB_OBJS += editor.c
LIB_OBJS += entry.c
LIB_OBJS += environment.c
LIB_OBJS += ewah/bitmap.c
LIB_OBJS += ewah/ewah_bitmap.c
LIB_OBJS += ewah/ewah_io.c
LIB_OBJS += ewah/ewah_rlw.c
LIB_OBJS += exec_cmd.c
LIB_OBJS += fetch-pack.c
LIB_OBJS += fsck.c
LIB_OBJS += gettext.c
LIB_OBJS += gpg-interface.c
LIB_OBJS += graph.c
LIB_OBJS += grep.c
LIB_OBJS += hashmap.c
LIB_OBJS += help.c
LIB_OBJS += hex.c
LIB_OBJS += ident.c
LIB_OBJS += kwset.c
LIB_OBJS += levenshtein.c
LIB_OBJS += line-log.c
LIB_OBJS += line-range.c
LIB_OBJS += list-objects.c
LIB_OBJS += ll-merge.c
LIB_OBJS += lockfile.c
LIB_OBJS += log-tree.c
LIB_OBJS += mailmap.c
LIB_OBJS += match-trees.c
LIB_OBJS += merge.c
LIB_OBJS += merge-blobs.c
LIB_OBJS += merge-recursive.c
LIB_OBJS += mergesort.c
LIB_OBJS += name-hash.c
LIB_OBJS += notes.c
LIB_OBJS += notes-cache.c
LIB_OBJS += notes-merge.c
LIB_OBJS += notes-utils.c
LIB_OBJS += object.c
LIB_OBJS += pack-bitmap.c
LIB_OBJS += pack-bitmap-write.c
LIB_OBJS += pack-check.c
LIB_OBJS += pack-objects.c
LIB_OBJS += pack-revindex.c
LIB_OBJS += pack-write.c
LIB_OBJS += pager.c
LIB_OBJS += parse-options.c
LIB_OBJS += parse-options-cb.c
LIB_OBJS += patch-delta.c
LIB_OBJS += patch-ids.c
LIB_OBJS += path.c
LIB_OBJS += pathspec.c
LIB_OBJS += pkt-line.c
LIB_OBJS += preload-index.c
LIB_OBJS += pretty.c
LIB_OBJS += prio-queue.c
LIB_OBJS += progress.c
LIB_OBJS += prompt.c
LIB_OBJS += quote.c
LIB_OBJS += reachable.c
LIB_OBJS += read-cache.c
LIB_OBJS += reflog-walk.c
LIB_OBJS += refs.c
LIB_OBJS += remote.c
LIB_OBJS += replace_object.c
LIB_OBJS += rerere.c
LIB_OBJS += resolve-undo.c
LIB_OBJS += revision.c
LIB_OBJS += run-command.c
LIB_OBJS += send-pack.c
LIB_OBJS += sequencer.c
LIB_OBJS += server-info.c
LIB_OBJS += setup.c
LIB_OBJS += sha1-array.c
LIB_OBJS += sha1-lookup.c
LIB_OBJS += sha1_file.c
LIB_OBJS += sha1_name.c
LIB_OBJS += shallow.c
LIB_OBJS += sideband.c
LIB_OBJS += sigchain.c
LIB_OBJS += split-index.c
LIB_OBJS += strbuf.c
LIB_OBJS += streaming.c
LIB_OBJS += string-list.c
LIB_OBJS += submodule.c
LIB_OBJS += symlinks.c
LIB_OBJS += tag.c
LIB_OBJS += trace.c
LIB_OBJS += transport.c
LIB_OBJS += transport-helper.c
LIB_OBJS += tree-diff.c
LIB_OBJS += tree.c
LIB_OBJS += tree-walk.c
LIB_OBJS += unpack-trees.c
LIB_OBJS += url.c
LIB_OBJS += urlmatch.c
LIB_OBJS += usage.c
LIB_OBJS += userdiff.c
LIB_OBJS += utf8.c
LIB_OBJS += varint.c
LIB_OBJS += version.c
LIB_OBJS += versioncmp.c
LIB_OBJS += walker.c
LIB_OBJS += wildmatch.c
LIB_OBJS += wrapper.c
LIB_OBJS += write_or_die.c
LIB_OBJS += ws.c
LIB_OBJS += wt-status.c
LIB_OBJS += xdiff-interface.c
LIB_OBJS += zlib.c

LIB_OBJS += thread-utils.c

BUILTIN_OBJS += builtin/add.c
BUILTIN_OBJS += builtin/annotate.c
BUILTIN_OBJS += builtin/apply.c
BUILTIN_OBJS += builtin/archive.c
BUILTIN_OBJS += builtin/bisect--helper.c
BUILTIN_OBJS += builtin/blame.c
BUILTIN_OBJS += builtin/branch.c
BUILTIN_OBJS += builtin/bundle.c
BUILTIN_OBJS += builtin/cat-file.c
BUILTIN_OBJS += builtin/check-attr.c
BUILTIN_OBJS += builtin/check-ignore.c
BUILTIN_OBJS += builtin/check-mailmap.c
BUILTIN_OBJS += builtin/check-ref-format.c
BUILTIN_OBJS += builtin/checkout-index.c
BUILTIN_OBJS += builtin/checkout.c
BUILTIN_OBJS += builtin/clean.c
BUILTIN_OBJS += builtin/clone.c
BUILTIN_OBJS += builtin/column.c
BUILTIN_OBJS += builtin/commit-tree.c
BUILTIN_OBJS += builtin/commit.c
BUILTIN_OBJS += builtin/config.c
BUILTIN_OBJS += builtin/count-objects.c
BUILTIN_OBJS += builtin/credential.c
BUILTIN_OBJS += builtin/describe.c
BUILTIN_OBJS += builtin/diff-files.c
BUILTIN_OBJS += builtin/diff-index.c
BUILTIN_OBJS += builtin/diff-tree.c
BUILTIN_OBJS += builtin/diff.c
BUILTIN_OBJS += builtin/fast-export.c
BUILTIN_OBJS += builtin/fetch-pack.c
BUILTIN_OBJS += builtin/fetch.c
BUILTIN_OBJS += builtin/fmt-merge-msg.c
BUILTIN_OBJS += builtin/for-each-ref.c
BUILTIN_OBJS += builtin/fsck.c
BUILTIN_OBJS += builtin/gc.c
BUILTIN_OBJS += builtin/get-tar-commit-id.c
BUILTIN_OBJS += builtin/grep.c
BUILTIN_OBJS += builtin/hash-object.c
BUILTIN_OBJS += builtin/help.c
BUILTIN_OBJS += builtin/index-pack.c
BUILTIN_OBJS += builtin/init-db.c
BUILTIN_OBJS += builtin/log.c
BUILTIN_OBJS += builtin/ls-files.c
BUILTIN_OBJS += builtin/ls-remote.c
BUILTIN_OBJS += builtin/ls-tree.c
BUILTIN_OBJS += builtin/mailinfo.c
BUILTIN_OBJS += builtin/mailsplit.c
BUILTIN_OBJS += builtin/merge.c
BUILTIN_OBJS += builtin/merge-base.c
BUILTIN_OBJS += builtin/merge-file.c
BUILTIN_OBJS += builtin/merge-index.c
BUILTIN_OBJS += builtin/merge-ours.c
BUILTIN_OBJS += builtin/merge-recursive.c
BUILTIN_OBJS += builtin/merge-tree.c
BUILTIN_OBJS += builtin/mktag.c
BUILTIN_OBJS += builtin/mktree.c
BUILTIN_OBJS += builtin/mv.c
BUILTIN_OBJS += builtin/name-rev.c
BUILTIN_OBJS += builtin/notes.c
BUILTIN_OBJS += builtin/pack-objects.c
BUILTIN_OBJS += builtin/pack-redundant.c
BUILTIN_OBJS += builtin/pack-refs.c
BUILTIN_OBJS += builtin/patch-id.c
BUILTIN_OBJS += builtin/prune-packed.c
BUILTIN_OBJS += builtin/prune.c
BUILTIN_OBJS += builtin/push.c
BUILTIN_OBJS += builtin/read-tree.c
BUILTIN_OBJS += builtin/receive-pack.c
BUILTIN_OBJS += builtin/reflog.c
BUILTIN_OBJS += builtin/remote.c
BUILTIN_OBJS += builtin/remote-ext.c
BUILTIN_OBJS += builtin/remote-fd.c
BUILTIN_OBJS += builtin/repack.c
BUILTIN_OBJS += builtin/replace.c
BUILTIN_OBJS += builtin/rerere.c
BUILTIN_OBJS += builtin/reset.c
BUILTIN_OBJS += builtin/rev-list.c
BUILTIN_OBJS += builtin/rev-parse.c
BUILTIN_OBJS += builtin/revert.c
BUILTIN_OBJS += builtin/rm.c
BUILTIN_OBJS += builtin/send-pack.c
BUILTIN_OBJS += builtin/shortlog.c
BUILTIN_OBJS += builtin/show-branch.c
BUILTIN_OBJS += builtin/show-ref.c
BUILTIN_OBJS += builtin/stripspace.c
BUILTIN_OBJS += builtin/symbolic-ref.c
BUILTIN_OBJS += builtin/tag.c
BUILTIN_OBJS += builtin/unpack-file.c
BUILTIN_OBJS += builtin/unpack-objects.c
BUILTIN_OBJS += builtin/update-index.c
BUILTIN_OBJS += builtin/update-ref.c
BUILTIN_OBJS += builtin/update-server-info.c
BUILTIN_OBJS += builtin/upload-archive.c
BUILTIN_OBJS += builtin/var.c
BUILTIN_OBJS += builtin/verify-commit.c
BUILTIN_OBJS += builtin/verify-pack.c
BUILTIN_OBJS += builtin/verify-tag.c
BUILTIN_OBJS += builtin/write-tree.c

XDIFF_OBJS += xdiff/xdiffi.c
XDIFF_OBJS += xdiff/xprepare.c
XDIFF_OBJS += xdiff/xutils.c
XDIFF_OBJS += xdiff/xemit.c
XDIFF_OBJS += xdiff/xmerge.c
XDIFF_OBJS += xdiff/xpatience.c
XDIFF_OBJS += xdiff/xhistogram.c

LOCAL_SRC_FILES:= \
	git.c \
	block-sha1/sha1.c \
	$(LIB_OBJS) \
	$(BUILTIN_OBJS) \
	$(XDIFF_OBJS) \

# LOCAL_SRC_FILES += src/utf8_mbfuncs.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/android \
	$(LOCAL_PATH)/compat \
	$(LOCAL_PATH)/xdiff \
	external/zlib \
	external/curl/include \
	external/openssl/include \

ETC_GITCONFIG = /etc/gitconfig
ETC_GITATTRIBUTES = /etc/gitattributes

LOCAL_CFLAGS += \
	-DNO_ICONV -DNO_GETTEXT \
	-DNO_GECOS_IN_PWENT -DHAVE_DEV_TTY \
	-DPREFIX=\"$(gitprefix)\" \
	-DBINDIR=\"$(gitbindir)\" \
	-DGIT_EXEC_PATH=\"$(gitexecdir)\" \
	-DETC_GITCONFIG=\"$(ETC_GITCONFIG)\" \
	-DETC_GITATTRIBUTES=\"$(ETC_GITATTRIBUTES)\" \
	-DDEFAULT_GIT_TEMPLATE_DIR=\"$(template_dir)\" \
	-DSHA1_HEADER=\"block-sha1/sha1.h\" \
	-DGIT_VERSION=\"$(GIT_VERSION)\" \
	-DGIT_USER_AGENT=\"$(GIT_USER_AGENT)\" \
	-DGIT_MAN_PATH=\"$(gitmandir)\" \
	-DGIT_INFO_PATH=\"$(gitinfodir)\" \
	-DGIT_HTML_PATH=\"\" \
	-DDEFAULT_PAGER=\"$(GIT_PAGER)\" \
	-DDEFAULT_EDITOR=\"$(GIT_EDITOR)\" \

LOCAL_STATIC_LIBRARIES +=
LOCAL_SHARED_LIBRARIES += libcurl libz libssl libcrypto

LOCAL_MODULE := git
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := gitconfig
include $(BUILD_EXECUTABLE)


### RUNTIME FILES (TEMPLATES) #####################################################################

GIT_TEMPLATES := $(call find-subdir-files, templates/* )
GIT_TEMPLATES := $(filter-out templates/Makefile, $(GIT_TEMPLATES))

gittpldir = $(subst /templates,,$(template_dir))

# templates files uses "--" as directory seperators, subst them...
# todo: fix /bin/sh header
#
$(GIT_TEMPLATES): GIT_BINARY := $(LOCAL_MODULE)
$(GIT_TEMPLATES): $(LOCAL_INSTALLED_MODULE)
	@echo -e ${CL_CYN}"Install: $(subst --,/,$@) -> $(gitprefix)/$(gittpldir)/"${CL_RST}
	@mkdir -p $(shell dirname $(TARGET_OUT)/$(gittpldir)/$(subst --,/,$@))
	$(hide) cp $(git_src)/$@ $(TARGET_OUT)/$(gittpldir)/$(subst --,/,$@) || echo "ignore $@"

ALL_DEFAULT_INSTALLED_MODULES += $(GIT_TEMPLATES)

ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
	$(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(GIT_TEMPLATES)


#include $(call all-makefiles-under,$(LOCAL_PATH))
