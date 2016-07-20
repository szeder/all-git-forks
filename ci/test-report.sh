#!/bin/sh
#
# Print test results and run Git bisect on failed tests.
#
REPO_ORG_NAME=$1
CURRENT_BRANCH_NAME=$2


#
# Print test results
#
for TEST_EXIT in t/test-results/*.exit
do
	if test "$(cat "$TEST_EXIT")" != "0"
	then
		TEST="${TEST_EXIT%.exit}"
		TEST_OUT="${TEST}.out"
		echo "------------------------------------------------------------------------"
		echo "  $(tput setaf 1)${TEST} Output$(tput sgr0)"
		echo "------------------------------------------------------------------------"
		cat "$TEST_OUT"
		echo ""
		echo ""
	fi
done


#
# Run Git bisect
#
run_bisect () {
	TEST_SCRIPT=$1
	BAD_REV=$2
	GOOD_RV=$3
	TMPDIR=$(mktemp -d -t "ci-report-bisect-XXXXXX" 2>/dev/null)
	cat > "$TMPDIR/bisect-run.sh" <<EOF
		#!/bin/sh
		if test -e ./t/$TEST_SCRIPT.sh && make --jobs=2 >/dev/null 2>&1
		then
				cd t && ./$TEST_SCRIPT.sh >/dev/null 2>&1
		else
				exit 125
		fi
EOF
	chmod +x "$TMPDIR/bisect-run.sh"
	git bisect start $BAD_REV $GOOD_RV
	git bisect run "$TMPDIR/bisect-run.sh"
	git bisect reset >/dev/null 2>&1
}

case "$CURRENT_BRANCH_NAME" in
	master) STABLE_BRANCH="maint";;
	bisect-ci/v2)   STABLE_BRANCH="master";;
esac

if test "$REPO_ORG_NAME" = "larsxschneider/git" && test -n $STABLE_BRANCH
then
	BAD_REV=$(git rev-parse HEAD)

	# Travis CI clones are shallow. It is possible that the last good revision
	# was not fetched, yet. Therefore we need to fetch all commits on the
	# stable branch.
	git config remote.origin.fetch "+refs/heads/$STABLE_BRANCH:refs/remotes/origin/$STABLE_BRANCH"
	git fetch --unshallow --quiet
	LAST_GOOD_REV=$(git merge-base $BAD_REV "remotes/origin/$STABLE_BRANCH")

	for TEST_EXIT in t/test-results/*.exit
	do
		if test "$(cat "$TEST_EXIT")" != "0"
		then
			TEST="${TEST_EXIT%.exit}"
			TEST_SCRIPT=${TEST#t/test-results/}
			echo "------------------------------------------------------------------------"
			echo "  $(tput setaf 1)${TEST} Bisect$(tput sgr0)"
			echo "------------------------------------------------------------------------"
			run_bisect $TEST_SCRIPT $BAD_REV $LAST_GOOD_REV
			echo ""
			echo ""
		fi
	done
fi
