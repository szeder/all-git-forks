#!/bin/bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

#XXX SRC-850 regular http tests don't work in CI
export GIT_TEST_HTTPD=false

# set 'true' so simplified http tests *must* succeed
export GIT_TEST_HTTPD_SIMPLE=true

# malloc debug output breaks tests on linux
export TEST_NO_MALLOC_CHECK=1

# determine our local os
os="$(uname -s)"

# for starters, skip tests tagged for our os
skip_tags="$os"

# act on command line flags
flagpat='^--(.*)'
while [[ ${1:-} =~ $flagpat ]] ; do
	flag="${BASH_REMATCH[1]}"
	case "$flag" in
	"image")
		echo "$0: Will run tests on .STAGE image"
		CWD="$(cd "$(dirname $0)" && pwd -P)"
		GIT_TEST_INSTALLED="${CWD}/../../.STAGE/git.$(uname -s).x86_64/bin"
		if [[ ! -d "$GIT_TEST_INSTALLED" ]] ; then
			echo "$GIT_TEST_INSTALLED doesn't exist" >&2
			exit 1
		fi
		export GIT_TEST_INSTALLED
		;;
	"watchman")
		echo "$0: Will test with watchman"
		export GIT_TEST_WITH_WATCHMAN=1
		skip_tags="${skip_tags} watchman+${os}"
		;;
	*)
		echo "$0: unknown flag --$flag" >&2
		exit 1
		;;
	esac
	shift
done

# get the list of tests to skip
oracle='tests-to-skip.py'
echo "$0: Finding tests to skip using: $oracle $skip_tags"
GIT_SKIP_TESTS="$(./$oracle $skip_tags)"
export GIT_SKIP_TESTS
if [[ -n "$GIT_SKIP_TESTS" ]] ; then cat <<EOT
*** SKIPPING TESTS: $GIT_SKIP_TESTS
*** To learn why, see $oracle
EOT
fi

# choose some amount of parallelism for prove
case "$os" in
	"Darwin") ncpu="$(sysctl -n hw.ncpu)" ;;
	"Linux") ncpu="$(grep -c ^processor /proc/cpuinfo)" ;;
	*) ncpu=1 ;;
esac
((njobs=($ncpu+1)/2))

# install (if necessary) and run prove
test_harness='Test-Harness-3.35'
if [[ ! -d "$test_harness" ]] ; then
	curl -s -o - "https://artifactory.twitter.biz/simple/tools/devprod/ci/prove/${test_harness}.tar.gz" | tar xzf -
fi
exec perl -I ./${test_harness}/lib/ ./${test_harness}/bin/prove --jobs $njobs ./t[0-8]*.sh

