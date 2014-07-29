git_src := $(call my-dir)

gitprefix = /system
gitexecdir = xbin/git-core
gitmandir = $(gitprefix)/usr/share/man
gitinfodir = $(gitprefix)/usr/share/info
template_dir = usr/share/git-core/templates

ETC_GITCONFIG = /etc/gitconfig
ETC_GITATTRIBUTES = /etc/gitattributes

GIT_SHELL = /system/bin/sh
GIT_EDITOR = vim
GIT_PAGER = more

include $(git_src)/GIT-VERSION-FILE
GIT_USER_AGENT = git/$(GIT_VERSION)

# Test if libcurl folder is present in repo
optional_libcurl = libcurl
ifeq (,$(shell test -e "$(git_src)/../curl" && echo Y))
    optional_libcurl =
endif

# detect "mm -B" (B on darwin, wB else)
# I used that to allow the push of runtime files with "mmp -B"
mm_forced = $(filter B wB, $(MAKEFLAGS))

###############################################################################
# /system/etc/gitconfig

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)

LOCAL_MODULE := gitconfig
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_SRC_FILES := android/gitconfig

include $(BUILD_PREBUILT)


###############################################################################
# /system/xbin/git-core/*.sh

LOCAL_PATH := $(git_src)

GIT_SCRIPTS := \
  git-am.sh \
  git-bisect.sh \
  git-difftool--helper.sh \
  git-filter-branch.sh \
  git-merge-octopus.sh \
  git-merge-one-file.sh \
  git-merge-resolve.sh \
  git-mergetool.sh \
  git-pull.sh \
  git-quiltimport.sh \
  git-rebase.sh \
  git-remote-testgit.sh \
  git-request-pull.sh \
  git-stash.sh \
  git-submodule.sh \
  git-web--browse.sh \

GIT_SCRIPTS_LIB := \
  git-mergetool--lib \
  git-parse-remote \
  git-rebase--am \
  git-rebase--interactive \
  git-rebase--merge \
  git-sh-setup \
  git-sh-i18n

gitshfixup_regex := $(subst /,\/,\#!/bin/sh)/$(subst /,\/,\#!$(GIT_SHELL))

# Script files need a shell header
$(GIT_SCRIPTS):
	@echo -e ${CL_CYN}"Install: $(TARGET_OUT)/$(gitexecdir)/$@"${CL_RST}
	@mkdir -p $(TARGET_OUT)/$(gitexecdir)
	$(hide) cp $(git_src)/$@ $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i 's/$(gitshfixup_regex)/g' $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i 's/@@PERL@@/perl/g' $(TARGET_OUT)/$(gitexecdir)/$@
	@rm -f $(TARGET_OUT)/$(gitexecdir)/$(subst .sh,,$@)
	$(hide) ln -sf $@ $(TARGET_OUT)/$(gitexecdir)/$(subst .sh,,$@)

$(GIT_SCRIPTS_LIB):
	@echo -e ${CL_CYN}"Install: $(TARGET_OUT)/$(gitexecdir)/$@"${CL_RST}
	@mkdir -p $(TARGET_OUT)/$(gitexecdir)
	$(hide) cp $(git_src)/$@.sh $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i 's/@@DIFF@@/diff/g' $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i 's/@@PERL@@/perl/g' $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i 's/@@LOCALEDIR@@//g' $(TARGET_OUT)/$(gitexecdir)/$@
	@sed -i 's/@@[A-Z_]\+@@//g' $(TARGET_OUT)/$(gitexecdir)/$@

# made at the eof...
#ALL_DEFAULT_INSTALLED_MODULES += $(GIT_SCRIPTS) $(GIT_SCRIPTS_LIB)
#ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
#	$(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) \
#	$(GIT_SCRIPTS) $(GIT_SCRIPTS_LIB)

###############################################################################
# static lib and other common objects to build all binaries

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
LIB_OBJS += block-sha1/sha1.c

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


git_INCLUDES := \
	$(LOCAL_PATH)/android \
	$(LOCAL_PATH)/compat \
	$(LOCAL_PATH)/xdiff \
	external/zlib \
	external/expat/lib \
	external/openssl/include \

git_CFLAGS := \
	-DNO_ICONV -DNO_GETTEXT \
	-UNO_EXPAT \
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

# Hide some noisy warnings
git_CFLAGS += -Wno-sign-compare -Wno-missing-field-initializers

ifeq ($(optional_libcurl),libcurl)
    git_INCLUDES += external/curl/include
else
    git_CFLAGS += -DNO_CURL
endif

LOCAL_MODULE := libgit_static
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := $(LIB_OBJS)
include $(BUILD_STATIC_LIBRARY)


###############################################################################
# Binary required to clone from http(s) and ftp(s) (git:// doesnt need that)

ifeq ($(optional_libcurl),libcurl)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)

LOCAL_MODULE := git-remote-http
LOCAL_MODULE_TAGS := optional

LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := remote-curl.c http.c http-walker.c $(XDIFF_OBJS)

LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz libssl libcurl
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

GIT_R_SYMLINKS := git-remote-https git-remote-ftp git-remote-ftps
GIT_R_SYMLINKS := $(addprefix $(TARGET_OUT)/$(gitexecdir)/,$(GIT_R_SYMLINKS))
$(GIT_R_SYMLINKS): git-remote-http
	@echo -e ${CL_CYN}"Symlink:"${CL_RST}" $@ -> git-remote-http"
	@mkdir -p $(dir $@)
	@rm -f $@
	$(hide) ln -sf git-remote-http $@

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-http-fetch
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS) -DCURL_DISABLE_TYPECHECK
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := http-fetch.c http.c http-walker.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz libcurl
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-http-push
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS) -DCURL_DISABLE_TYPECHECK
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := http-push.c http.c $(XDIFF_OBJS)
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz libexpat libcurl
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

endif

###############################################################################
# Other optional git-<tools>


LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-credential-store
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := credential-store.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-daemon
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := daemon.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-fast-import
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := fast-import.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-http-backend
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS) #-DCURL_DISABLE_TYPECHECK
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := http-backend.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-imap-send
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := imap-send.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libcrypto libssl libz
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-remote-testsvn
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES) $(git_src)/vcs-svn
LOCAL_SRC_FILES := remote-testsvn.c \
  vcs-svn/svndiff.c vcs-svn/svndump.c \
  vcs-svn/fast_export.c vcs-svn/line_buffer.c \
  vcs-svn/repo_tree.c vcs-svn/sliding_window.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-shell
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := shell.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)
# note: ~/git-shell-commands folder should be created

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-show-index
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := show-index.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-upload-pack
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := upload-pack.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)
LOCAL_MODULE := git-sh-i18n--envsubst
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)
LOCAL_SRC_FILES := sh-i18n--envsubst.c
LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_MODULE_PATH := $(TARGET_OUT)/$(gitexecdir)
include $(BUILD_EXECUTABLE)


###############################################################################
# Main git binary

LOCAL_PATH := $(git_src)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := git.c \
	$(BUILTIN_OBJS) \
	$(XDIFF_OBJS) \

LOCAL_MODULE := git
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := $(git_CFLAGS)
LOCAL_C_INCLUDES := $(git_INCLUDES)

LOCAL_STATIC_LIBRARIES := libgit_static
LOCAL_SHARED_LIBRARIES := libz
LOCAL_REQUIRED_MODULES := libgit_static gitconfig

# required for git am
LOCAL_REQUIRED_MODULES += git-sh-i18n--envsubst

ifeq ($(optional_libcurl),libcurl)
    LOCAL_REQUIRED_MODULES += git-remote-http
endif

LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
include $(BUILD_EXECUTABLE)

# we want the 'git' package ?
ifeq (git,$(filter git, $(ALL_MODULES)))

GIT_SYMLINKS := git-merge
GIT_SYMLINKS := $(addprefix $(TARGET_OUT)/$(gitexecdir)/,$(GIT_SYMLINKS))
$(GIT_SYMLINKS): git
	@echo -e ${CL_CYN}"Symlink:"${CL_RST}" $@ -> ../git"
	@mkdir -p $(dir $@)
	@rm -f $@
	$(hide) ln -sf ../git $@


###############################################################################
# Template (folders+files) used on "git init"

GIT_TEMPLATES := $(call find-subdir-files, templates/* )
GIT_TEMPLATES := $(filter-out templates/Makefile, $(GIT_TEMPLATES))

gittpldir = $(subst /templates,,$(template_dir))

# templates files uses "--" as directory seperators, subst them...
# branches-- is an empty folder, copy is not required (mkdir do it)
#
$(GIT_TEMPLATES):
	@echo -e ${CL_CYN}"Install: $(TARGET_OUT)/$(gittpldir)/$(subst --,/,$@)"${CL_RST}
	@mkdir -p $(dir $(TARGET_OUT)/$(gittpldir)/$(subst --,/,$@))
	$(hide) $(if $(notdir $(subst --,/,$@)), \
	 dstfile=$(TARGET_OUT)/$(gittpldir)/$(subst --,/,$@) ; \
	 cp -R $(git_src)/$@ $$dstfile ; \
         sed -i 's/$(gitshfixup_regex)/g' $$dstfile ; \
	,)

ifeq ($(optional_libcurl),libcurl)
    GIT_SYMLINKS += $(GIT_R_SYMLINKS)
endif

# for brunch
ALL_DEFAULT_INSTALLED_MODULES += \
  $(GIT_SCRIPTS) $(GIT_SCRIPTS_LIB) \
  $(GIT_TEMPLATES) $(GIT_SYMLINKS) \

ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
  $(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) \
  $(GIT_SCRIPTS) $(GIT_SCRIPTS_LIB) \
  $(GIT_TEMPLATES) $(GIT_SYMLINKS) \

ifneq ($(mm_forced),)
# push on mmp -B
$(LOCAL_MODULE): $(GIT_SCRIPTS) $(GIT_SCRIPTS_LIB) $(GIT_SYMLINKS) $(GIT_TEMPLATES)
endif

endif # filter (prevent rules if 'git' module is not selected)

