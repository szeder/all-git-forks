#!/bin/sh
#
#

test_description='git-mktag: tag object verify test'

. ./test-lib.sh

###########################################################
# check the tag.sig file, expecting verify_tag() to fail,
# and checking that the error message matches the pattern
# given in the expect.pat file.

check_verify_failure () {
    test_expect_success \
        "$1" \
        'git-mktag <tag.sig 2>message ||
         egrep -q -f expect.pat message'
}

###########################################################
# first create a commit, so we have a valid object/type
# for the tag.
echo Hello >A
git-update-index --add A
git-commit -m "Initial commit"
head=$(git-rev-parse --verify HEAD)

############################################################
#  1. length check

cat >tag.sig <<EOF
too short for a tag
EOF

cat >expect.pat <<EOF
^error: .* size .*$
EOF

check_verify_failure 'Tag object length check'

############################################################
#  2. object line label check

cat >tag.sig <<EOF
xxxxxx 139e9b33986b1c2670fff52c5067603117b3e895
type tag
tag mytag
tagger foo

EOF

cat >expect.pat <<EOF
^error: char0: .*"object "$
EOF

check_verify_failure '"object" line label check'

############################################################
#  3. object line SHA1 check

cat >tag.sig <<EOF
object zz9e9b33986b1c2670fff52c5067603117b3e895
type tag
tag mytag
tagger foo

EOF

cat >expect.pat <<EOF
^error: char7: .*SHA1 hash$
EOF

check_verify_failure '"object" line SHA1 check'

############################################################
#  4. type line label check

cat >tag.sig <<EOF
object 779e9b33986b1c2670fff52c5067603117b3e895
xxxx tag
tag mytag
tagger foo

EOF

cat >expect.pat <<EOF
^error: char47: .*"[\]ntype "$
EOF

check_verify_failure '"type" line label check'

############################################################
#  5. type line eol check

echo "object 779e9b33986b1c2670fff52c5067603117b3e895" >tag.sig
printf "type tagsssssssssssssssssssssssssssssss" >>tag.sig

cat >expect.pat <<EOF
^error: char48: .*"[\]n" after "type"$
EOF

check_verify_failure '"type" line eol check'

############################################################
#  6. tag line label check #1

cat >tag.sig <<EOF
object 779e9b33986b1c2670fff52c5067603117b3e895
type tag
xxx mytag
tagger foo

EOF

cat >expect.pat <<EOF
^error: char57: .*$
EOF

check_verify_failure '"tag" line label check #1'

############################################################
#  7. tag line label check #2

cat >tag.sig <<EOF
object 779e9b33986b1c2670fff52c5067603117b3e895
type taggggggggggggggggggggggggggggggg
tag
keywords foo
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char87: .*$
EOF

check_verify_failure '"tag" line label check #2'

############################################################
#  8. type line type name length check

cat >tag.sig <<EOF
object 779e9b33986b1c2670fff52c5067603117b3e895
type taggggggggggggggggggggggggggggggg
tag mytag
keywords foo
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char53: type too long$
EOF

check_verify_failure '"type" line type-name length check'

############################################################
#  9. verify object (SHA1/type) check

cat >tag.sig <<EOF
object 779e9b33986b1c2670fff52c5067603117b3e895
type tagggg
tag mytag
keywords foo
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char7: could not verify object.*$
EOF

check_verify_failure 'verify object (SHA1/type) check'

############################################################
# 10. verify tag name check

cat >tag.sig <<EOF
object $head
type commit
tag my	tag
keywords foo
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char67: could not verify tag name$
EOF

check_verify_failure 'verify tag-name check'

############################################################
# 11. keywords line label check #1

cat >tag.sig <<EOF
object $head
type commit
tag mytag
xxxxxxxx foo
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char70: .*$
EOF

check_verify_failure '"keywords" line label check #1'

############################################################
# 12. keywords line label check #2

cat >tag.sig <<EOF
object $head
type commit
tag mytag
keywords
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char70: .*$
EOF

check_verify_failure '"keywords" line label check #2'

############################################################
# 13. keywords line check #1

cat >tag.sig <<EOF
object $head
type commit
tag mytag
keywords foo bar	baz
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char83: .*$
EOF

check_verify_failure '"keywords" line check #1'

############################################################
# 14. keywords line check #2

cat >tag.sig <<EOF
object $head
type commit
tag mytag
keywords foo,bar	baz
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char87: .*$
EOF

check_verify_failure '"keywords" line check #2'

############################################################
# 15. keywords line check #3

cat >tag.sig <<EOF
object $head
type commit
tag mytag
keywords foo,,bar
tagger bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char83: .*$
EOF

check_verify_failure '"keywords" line check #3'

############################################################
# 16. tagger line label check #1

cat >tag.sig <<EOF
object $head
type commit
tag mytag

EOF

cat >expect.pat <<EOF
^error: char70: .*$
EOF

check_verify_failure '"tagger" line label check #1'

############################################################
# 17. tagger line label check #2

cat >tag.sig <<EOF
object $head
type commit
tag mytag
xxxxxx bar@baz.com

EOF

cat >expect.pat <<EOF
^error: char70: .*$
EOF

check_verify_failure '"tagger" line label check #2'

############################################################
# 18. tagger line label check #3

cat >tag.sig <<EOF
object $head
type commit
tag mytag
keywords foo
tagger

EOF

cat >expect.pat <<EOF
^error: char83: .*$
EOF

check_verify_failure '"tagger" line label check #3'

############################################################
# 19. create valid tag #1

cat >tag.sig <<EOF
object $head
type commit
tag mytag
tagger another@example.com

EOF

test_expect_success \
    'create valid tag #1' \
    'git-mktag <tag.sig >.git/refs/tags/mytag 2>message'

############################################################
# 20. check mytag

test_expect_success \
    'check mytag' \
    'git-tag -l | grep mytag'

############################################################
# 21. create valid tag #2

cat >tag.sig <<EOF
object $head
type commit
tagger another@example.com

EOF

test_expect_success \
    'create valid tag #2' \
    'git-mktag <tag.sig >.git/refs/tags/mytag 2>message'

############################################################
# 22. create valid tag #3

cat >tag.sig <<EOF
object $head
type commit
keywords foo,bar,baz,spam,spam,spam,spam,spam,spam,spam,spam
tagger another@example.com

EOF

test_expect_success \
    'create valid tag #3' \
    'git-mktag <tag.sig >.git/refs/tags/mytag 2>message'

############################################################
# 23. create valid tag #4

cat >tag.sig <<EOF
object $head
type commit
tag mytag
keywords note
tagger another@example.com

EOF

test_expect_success \
    'create valid tag #4' \
    'git-mktag <tag.sig >.git/refs/tags/mytag 2>message'


test_done
