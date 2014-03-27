#!/bin/sh

test_description='log --function-name'
. ./test-lib.sh

test_expect_success setup '
  echo "* diff=cpp" > .gitattributes

	>file &&
	git add file &&
	test_tick &&
	git commit -m initial &&

  printf "int main(){\n\treturn 0;\n}\n" >> file &&
  test_tick &&
  git commit -am second

  printf "void newfunc(){\n\treturn;\n}\n" >> file &&
  test_tick &&
  git commit -am third

  printf "void newfunc2(){\n\treturn;\n}\n" | cat - file > temp &&
  mv temp file &&
  test_tick &&
  git commit -am fourth

  printf "void newfunc3(){\n\treturn;\n}\n" | cat - file > temp &&
  mv temp file &&
  test_tick &&
  git commit -am fifth

  sed -i -e "s/void newfunc2/void newfunc4/" file &&
  test_tick &&
  git commit -am sixth
'

test_expect_success 'log --function-name=main' '
  git log --function-name=main >actual &&
  git log --grep=second >expect &&
  test_cmp expect actual
'

test_expect_success 'log --function-name "newfunc\W"' '
  git log --function-name "newfunc\W" >actual &&
  git log --grep=third >expect &&
  test_cmp expect actual
'

test_expect_success 'log --function-name "newfunc2"' '
  git log --function-name newfunc2 >actual &&
  git log -E --grep "sixth|fourth" >expect &&
  test_cmp expect actual
'

test_expect_success 'log --function-name "newfunc3"' '
  git log --function-name newfunc3 >actual &&
  git log --grep=fifth >expect &&
  test_cmp expect actual
'

test_expect_success 'log --function-name "newfunc4"' '
  git log --function-name newfunc4 >actual &&
  git log --grep=sixth >expect &&
  test_cmp expect actual
'

test_expect_success 'log --function-name "newfunc"' '
  git log --function-name newfunc >actual &&
  git log -E --grep "third|fourth|fifth|sixth" >expect &&
  test_cmp expect actual
'

test_done
