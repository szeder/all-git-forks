#!/bin/sh
#
# Copyright (c) 2010 Ævar Arnfjörð Bjarmason
#

test_description='Gettext Shell fallbacks'

GIT_INTERNAL_GETTEXT_TEST_FALLBACKS=YesPlease
export GIT_INTERNAL_GETTEXT_TEST_FALLBACKS

. ./lib-gettext.sh

test_expect_success NO_GETTEXT_POISON "sanity: \$GIT_INTERNAL_GETTEXT_SH_SCHEME is set (to $GIT_INTERNAL_GETTEXT_SH_SCHEME)" '
    test -n "$GIT_INTERNAL_GETTEXT_SH_SCHEME"
'

test_expect_success NO_GETTEXT_POISON 'sanity: $GIT_INTERNAL_GETTEXT_TEST_FALLBACKS is set' '
    test -n "$GIT_INTERNAL_GETTEXT_TEST_FALLBACKS"
'

test_expect_success NO_GETTEXT_POISON 'sanity: $GIT_INTERNAL_GETTEXT_SH_SCHEME" is fallthrough' '
    test "$GIT_INTERNAL_GETTEXT_SH_SCHEME" = "fallthrough"
'

test_expect_success NO_GETTEXT_POISON 'gettext: our gettext() fallback has pass-through semantics' '
    printf "test" >expect &&
    gettext "test" >actual &&
    test_cmp expect actual &&
    printf "test more words" >expect &&
    gettext "test more words" >actual &&
    test_cmp expect actual
'

test_expect_success NO_GETTEXT_POISON 'eval_gettext: our eval_gettext() fallback has pass-through semantics' '
    printf "test" >expect &&
    eval_gettext "test" >actual &&
    test_cmp expect actual &&
    printf "test more words" >expect &&
    eval_gettext "test more words" >actual &&
    test_cmp expect actual
'

test_expect_success NO_GETTEXT_POISON 'eval_gettext: our eval_gettext() fallback can interpolate variables' '
    printf "test YesPlease" >expect &&
    eval_gettext "test \$GIT_INTERNAL_GETTEXT_TEST_FALLBACKS" >actual &&
    test_cmp expect actual
'

test_done
