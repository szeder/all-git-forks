#!/bin/sh
#
# Copyright (c) 2015 Twitter, Inc
#

test_description='git combine-pack'

. ./test-lib.sh

create_a_pack() {
	test_commit "$1" &&
	git repack -d &&
	objects=$(ls .git/objects/) &&
	! echo "$objects" | egrep -v "info|pack"
}

restore_packs() {
	rm -r .git/objects/pack &&
	cp -r saved-packs .git/objects/pack
}

test_expect_success 'setup' '
	create_a_pack a &&
	create_a_pack bananas &&
	create_a_pack cookie_monster &&
	create_a_pack diamonds_are_forever &&
	packs=$(ls .git/objects/pack/*.pack | wc -l) &&
	test "$packs" -eq 4
'

test_expect_success 'save off packs' '
	cp -r .git/objects/pack saved-packs
'

test_expect_success 'combine-pack --size-lower-bound' '
	# Combine the two largest packs
	git combine-pack --size-lower-bound 281 &&
	packs=$(ls .git/objects/pack/*.pack | wc -l) &&
	test "$packs" -eq 3 &&
	! test -e .git/objects/pack/pack-a6aa2ba1e26b06b443b0fd9c8c7950867df70eae.pack && # a big one
	test -e .git/objects/pack/pack-19b7530f87801417f24b9f896adaa7c717299e2c.pack # a small one
'

test_expect_success 'combine-pack --size-upper-bound' '
	restore_packs &&
	# Combine the two smallest packs
	git combine-pack --size-upper-bound 281 &&
	packs=$(ls .git/objects/pack/*.pack | wc -l) &&
	test "$packs" -eq 3 &&
	test -e .git/objects/pack/pack-a6aa2ba1e26b06b443b0fd9c8c7950867df70eae.pack && # a big one
	! test -e .git/objects/pack/pack-19b7530f87801417f24b9f896adaa7c717299e2c.pack # a small one
'

cat <<EOF >expect
Adding objects from pack 19b7530f87801417f24b9f896adaa7c717299e2c
Adding objects from pack a51a9b2f89169414e5144623b44948c5ff9555eb
Adding objects from pack a6aa2ba1e26b06b443b0fd9c8c7950867df70eae
Adding objects from pack fc8046cb0325fbc70f2d42a5bbb5ed4d32d8ac44
Moving
Removing redundant pack pack-19b7530f87801417f24b9f896adaa7c717299e2c
Removing redundant pack pack-a51a9b2f89169414e5144623b44948c5ff9555eb
Removing redundant pack pack-a6aa2ba1e26b06b443b0fd9c8c7950867df70eae
Removing redundant pack pack-fc8046cb0325fbc70f2d42a5bbb5ed4d32d8ac44
EOF

test_expect_success 'combine-pack -v is verbose' '
	restore_packs &&
	# We munge the "Moving" command because it contains an unpredictable pid
	git combine-pack -v | sort | sed "s/Moving .*/Moving/" >actual &&
	packs=$(ls .git/objects/pack/*.pack | wc -l) &&
	test "$packs" -eq 1 &&
	test_cmp actual expect
'

test_expect_success 'combine-pack -q is quiet' '
	restore_packs &&
	git combine-pack -q >actual &&
	packs=$(ls .git/objects/pack/*.pack | wc -l) &&
	test "$packs" -eq 1 &&
	! test -s actual
'

test_expect_success "combine-pack doesn't do anything when only one pack" '
	restore_packs &&
	git combine-pack -q &&
	packs=$(ls .git/objects/pack/*.pack | wc -l) &&
	test "$packs" -eq 1 &&
	git combine-pack 2> output &&
	grep -q "Not enough packs exist to combine." output
'

test_done
