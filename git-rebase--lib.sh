write_basic_state () {
	echo "$head_name" > "$state_dir"/head-name &&
	echo "$onto" > "$state_dir"/onto &&
	echo "$orig_head" > "$state_dir"/orig-head &&
	echo "$GIT_QUIET" > "$state_dir"/quiet &&
	test t = "$verbose" && : > "$state_dir"/verbose
	test -n "$strategy" && echo "$strategy" > "$state_dir"/strategy
	test -n "$strategy_opts" && echo "$strategy_opts" > \
		"$state_dir"/strategy_opts
	test -n "$allow_rerere_autoupdate" && echo "$allow_rerere_autoupdate" > \
		"$state_dir"/allow_rerere_autoupdate
	test -n "$gpg_sign_opt" && echo "$gpg_sign_opt" > "$state_dir"/gpg_sign_opt
}

output () {
	case "$verbose" in
	'')
		output=$("$@" 2>&1 )
		status=$?
		test $status != 0 && printf "%s\n" "$output"
		return $status
		;;
	*)
		"$@"
		;;
	esac
}

move_to_original_branch () {
	case "$head_name" in
	refs/*)
		message="rebase finished: $head_name onto $onto"
		git update-ref -m "$message" \
			$head_name $(git rev-parse HEAD) $orig_head &&
		git symbolic-ref \
			-m "rebase finished: returning to $head_name" \
			HEAD $head_name ||
		die "$(gettext "Could not move back to $head_name")"
		;;
	esac
}

apply_autostash () {
	if test -f "$state_dir/autostash"
	then
		stash_sha1=$(cat "$state_dir/autostash")
		if git stash apply $stash_sha1 2>&1 >/dev/null
		then
			echo "$(gettext 'Applied autostash.')"
		else
			git stash store -m "autostash" -q $stash_sha1 ||
			die "$(eval_gettext "Cannot store \$stash_sha1")"
			gettext 'Applying autostash resulted in conflicts.
Your changes are safe in the stash.
You can run "git stash pop" or "git stash drop" at any time.
'
		fi
	fi
}

finish_rebase () {
	apply_autostash &&
	{ git gc --auto || true; } &&
	rm -rf "$state_dir"
}

exit_rebase () {
	ret=$1
	if test $ret -eq 0
	then
		finish_rebase
	elif test $ret -eq 2 # special exit status for rebase -i
	then
		apply_autostash &&
		rm -rf "$state_dir" &&
		die "Nothing to do"
	fi
	exit $ret
}
