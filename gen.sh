#!/bin/sh
# Generates a coverage report from the checkout dir $1
git_dir="$1"
if test -z "$git_dir"; then
    echo "$0 git_dir"
    exit 1
fi
GIT_TEST_OPTS='--kcov'
export GIT_TEST_OPTS
GIT_SKIP_TESTS='t3404 t3409 t3410 t3411 t3412 t3414 t3415 t3421 t3425 t3903 t5150 t5801 t6030 t7003 t7401 t7508 t7800 t9901'
export GIT_SKIP_TESTS
name="$(date -u +%Y%m%d%H%M%S)-$(git -C "$git_dir" rev-parse --short HEAD)"
(
    cd "$git_dir/t" &&
    git clean -df kcov || exit 1
    make -j8 || exit 1
) || exit 1
mkdir "$name"  || exit 1
cp -r "$git_dir"/t/kcov/* "$name"
# Generate file listing in index.html
cat >index.html <<EOF
<html>
<head>
<title>kcov test coverage reports</title>
</head>
<body>
<h1>kcov test coverage reports</h1>
<p>The test suite is currently being run with the following environment variables:</p>
<ul>
<li><pre>GIT_TEST_OPTS='$GIT_TEST_OPTS'</pre></li>
<li><pre>GIT_SKIP_TESTS='$GIT_SKIP_TESTS'</pre></li>
</ul>
<p>Test coverage reports, sorted by date in reverse:</p>
<ul>
EOF
for x in $(ls -d */ | sort -r); do
    echo '<li><a href="'"$x/index.html"'">'"$x"'</a></li>' >>index.html
done
cat >>index.html <<EOF
</ul>
</body>
</html>
EOF
git add "$name" index.html &&
git commit -m "$name"
