#!/bin/sh
TRAVIS_REPO_SLUG="larsxschneider/git"
TRAVIS_BRANCH="bisect"

if test "$TRAVIS_REPO_SLUG" = "larsxschneider/git"
then
    case "$TRAVIS_BRANCH" in
        master) GOOD_REV=maint;;
        next)   GOOD_REV=master;;
        *)      GOOD_REV=next;;
    esac
    git fetch --quiet --unshallow
    GOOD_REV="$(git merge-base $TRAVIS_BRANCH $GOOD_REV)"
fi

for TEST_EXIT in t/test-results/*.exit
do
  if test "$(cat "$TEST_EXIT")" != "0"
  then
    TEST="${TEST_EXIT%.exit}"
    TEST_SCRIPT=${TEST#t/test-results/}
    TEST_OUT="${TEST}.out"
    echo "------------------------------------------------------------------------"
    echo "$(tput setaf 1)${TEST_OUT}...$(tput sgr0)"
    echo "------------------------------------------------------------------------"
    if test -n "$GOOD_REV"
    then
        git bisect start $TRAVIS_BRANCH $GOOD_REV
        git bisect run sh -c "\
            if NO_OPENSSL=YesPlease NO_GETTEXT=YesPlease DEVELOPER=1 make --jobs=8 >/dev/null 2>&1;
            then cd t && ./$TEST_SCRIPT.sh --immediate >/dev/null 2>&1;
            else exit 125;
            fi"
        git bisect reset >/dev/null 2>&1
        echo ""
        echo "------------------------------------------------------------------------"
    fi
    cat "$TEST_OUT"
  fi
done
echo "done"
