#!/bin/sh

# git-ls-remote could be called from outside a git managed repository;
# this would fail in that case and would issue an error message.
GIT_DIR=$(git-rev-parse --git-dir 2>/dev/null) || :;

get_data_source () {
	case "$1" in
	*/*)
		echo ''
		;;
	*)
		if test "$(git-config --get "remote.$1.url")"
		then
			echo config
		elif test -f "$GIT_DIR/remotes/$1"
		then
			echo remotes
		elif test -f "$GIT_DIR/branches/$1"
		then
			echo branches
		else
			echo ''
		fi ;;
	esac
}

get_remote_url () {
	data_source=$(get_data_source "$1")
	case "$data_source" in
	'')
		echo "$1"
		;;
	config)
		git-config --get "remote.$1.url"
		;;
	remotes)
		sed -ne '/^URL: */{
			s///p
			q
		}' "$GIT_DIR/remotes/$1"
		;;
	branches)
		sed -e 's/#.*//' "$GIT_DIR/branches/$1"
		;;
	*)
		die "internal error: get-remote-url $1" ;;
	esac
}

get_default_remote () {
	curr_branch=$(git-symbolic-ref -q HEAD | sed -e 's|^refs/heads/||')
	origin=$(git-config --get "branch.$curr_branch.remote")
	echo ${origin:-origin}
}

get_remote_default_refs_for_push () {
	data_source=$(get_data_source "$1")
	case "$data_source" in
	'' | branches)
		;; # no default push mapping, just send matching refs.
	config)
		git-config --get-all "remote.$1.push" ;;
	remotes)
		sed -ne '/^Push: */{
			s///p
		}' "$GIT_DIR/remotes/$1" ;;
	*)
		die "internal error: get-remote-default-ref-for-push $1" ;;
	esac
}

# Called from get_remote_default_refs_for_fetch to grok
# refspecs that are retrieved from the configuration, but not
# from get_remote_refs_for_fetch when it deals with refspecs
# supplied on the command line.  $ls_remote_result has the list
# of refs available at remote.
#
# The first token returned is either "explicit" or "glob"; this
# is to help prevent randomly "globbed" ref from being chosen as
# a merge candidate
expand_refs_wildcard () {
	echo "$ls_remote_result" |
	git fetch--tool expand-refs-wildcard "-" "$@"
}

# Subroutine to canonicalize remote:local notation.
canon_refs_list_for_fetch () {
	for ref
	do
		force=
		case "$ref" in
		+*)
			ref=$(expr "z$ref" : 'z+\(.*\)')
			force=+
			;;
		esac
		expr "z$ref" : 'z.*:' >/dev/null || ref="${ref}:"
		remote=$(expr "z$ref" : 'z\([^:]*\):')
		local=$(expr "z$ref" : 'z[^:]*:\(.*\)')
		case "$remote" in
		'' | HEAD ) remote=HEAD ;;
		refs/heads/* | refs/tags/* | refs/remotes/*) ;;
		heads/* | tags/* | remotes/* ) remote="refs/$remote" ;;
		*) remote="refs/heads/$remote" ;;
		esac
		case "$local" in
		'') local= ;;
		refs/heads/* | refs/tags/* | refs/remotes/*) ;;
		heads/* | tags/* | remotes/* ) local="refs/$local" ;;
		*) local="refs/heads/$local" ;;
		esac

		if local_ref_name=$(expr "z$local" : 'zrefs/\(.*\)')
		then
		   git-check-ref-format "$local_ref_name" ||
		   die "* refusing to create funny ref '$local_ref_name' locally"
		fi
		echo "${force}${remote}:${local}"
	done
}

# Returns list of src: (no store), or src:dst (store)
get_remote_default_refs_for_fetch () {
	test "$1" = -t && type=yes && shift
	test "$1" = -n && canon=no && shift
	data_source=$(get_data_source "$1")
	case "$data_source" in
	'')
		set explicit "HEAD:" ;;
	config)
		set $(expand_refs_wildcard "$1" \
			$(git-config --get-all "remote.$1.fetch")) ;;
	branches)
		remote_branch=$(sed -ne '/#/s/.*#//p' "$GIT_DIR/branches/$1")
		case "$remote_branch" in '') remote_branch=master ;; esac
		set explicit "refs/heads/${remote_branch}:refs/heads/$1"
		;;
	remotes)
		set $(expand_refs_wildcard "$1" $(sed -ne '/^Pull: */s///p'\
					"$GIT_DIR/remotes/$1"))
		;;
	*)
		die "internal error: get-remote-default-ref-for-push $1" ;;
	esac
	if test "$type" = yes ; then
		echo $1
	elif test "$canon" = no ; then
		shift
		for ref ; do echo "$ref" ; done
	else
		shift
		canon_refs_list_for_fetch "$@"
	fi
}

get_remote_refs_for_push () {
	case "$#" in
	0) die "internal error: get-remote-refs-for-push." ;;
	1) get_remote_default_refs_for_push "$@" ;;
	*) shift; echo "$@" ;;
	esac
}

get_remote_refs_for_fetch () {
	case "$#" in
	0)
	    die "internal error: get-remote-refs-for-fetch." ;;
	1)
	    get_remote_default_refs_for_fetch "$@" ;;
	*)
	    shift
	    tag_just_seen=
	    for ref
	    do
		if test "$tag_just_seen"
		then
		    echo "refs/tags/${ref}:refs/tags/${ref}"
		    tag_just_seen=
		    continue
		else
		    case "$ref" in
		    tag)
			tag_just_seen=yes
			continue
			;;
		    esac
		fi
		canon_refs_list_for_fetch "$ref"
	    done
	    ;;
	esac
}

resolve_alternates () {
	# original URL (xxx.git)
	top_=`expr "z$1" : 'z\([^:]*:/*[^/]*\)/'`
	while read path
	do
		case "$path" in
		\#* | '')
			continue ;;
		/*)
			echo "$top_$path/" ;;
		../*)
			# relative -- ugly but seems to work.
			echo "$1/objects/$path/" ;;
		*)
			# exit code may not be caught by the reader.
			echo "bad alternate: $path"
			exit 1 ;;
		esac
	done
}

get_uploadpack () {
	data_source=$(get_data_source "$1")
	case "$data_source" in
	config)
		uplp=$(git-config --get "remote.$1.uploadpack")
		echo ${uplp:-git-upload-pack}
		;;
	*)
		echo "git-upload-pack"
		;;
	esac
}
