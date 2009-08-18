: included from t2016 and others

. ./test-lib.sh

set_state () {
	echo "$3" > "$1" &&
	git add "$1" &&
	echo "$2" > "$1"
}

save_state () {
	noslash="$(echo "$1" | tr / _)" &&
	cat "$1" > _worktree_"$noslash" &&
	git show :"$1" > _index_"$noslash"
}

set_and_save_state () {
	set_state "$@" &&
	save_state "$1"
}

verify_state () {
	echo $2 >expect &&
	test_cmp expect "$1" &&
	echo "$3" >expect.index &&
	git show :"$1" >"$1".index &&
	test_cmp expect.index "$1".index
}

verify_saved_state () {
	noslash="$(echo "$1" | tr / _)" &&
	verify_state "$1" "$(cat _worktree_"$noslash")" "$(cat _index_"$noslash")"
}

save_head () {
	git rev-parse HEAD > _head
}

verify_saved_head () {
	test "$(cat _head)" = "$(git rev-parse HEAD)"
}
