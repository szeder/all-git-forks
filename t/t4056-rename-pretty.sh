#!/bin/sh

test_description='Rename pretty print

'

. ./test-lib.sh

test_expect_success nothing_common '
	mkdir -p a/b/ &&
	: >a/b/c &&
	git add a/b/c &&
	git commit -m. &&
	mkdir -p c/b/ &&
	git mv a/b/c c/b/a &&
	git commit -m. &&
	git show -M --summary >output &&
	test_i18ngrep "a/b/c => c/b/a" output
'

test_expect_success common_prefix '
	mkdir -p c/d &&
	git mv c/b/a c/d/e &&
	git commit -m. &&
	git show -M --summary >output &&
	test_i18ngrep "c/{b/a => d/e}" output
'

test_expect_success common_suffix '
	mkdir d &&
	git mv c/d/e d/e &&
	git commit -m. &&
	git show -M --summary >output &&
	test_i18ngrep "{c/d => d}/e" output
'

test_expect_success common_suffix_prefix '
	mkdir d/f &&
	git mv d/e d/f/e &&
	git commit -m. &&
	git show -M --summary >output &&
	test_i18ngrep "d/{ => f}/e" output
'

test_expect_success common_overlap '
	mkdir d/f/f &&
	git mv d/f/e d/f/f/e &&
	git commit -m. &&
	git show -M --summary >output &&
	test_i18ngrep "d/f/{ => f}/e" output
'


test_done
