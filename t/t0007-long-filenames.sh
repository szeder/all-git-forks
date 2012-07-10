
#!/bin/sh

test_description='very long file names in the index handled correctly'
. ./test-lib.sh

test_expect_success 'setup' '
	git init &&

	a=a && # 1
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 16
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 256
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 4096
	a=${a}q &&

	>path1 &&
	git update-index --add path1 &&
	(
		git ls-files -s path1 |
		sed -e "s/	.*/	/" |
		tr -d "\012"
		echo "$a"
	) | git update-index --index-info &&

	a=a && # 1
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 16
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 256
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 4096
	a=${a}e &&

	>path2 &&
	git update-index --add path2 &&
	(
		git ls-files -s path2 |
		sed -e "s/	.*/	/" |
		tr -d "\012"
		echo "$a"
	) | git update-index --index-info &&
	len=$(git ls-files "a*" | wc -c) &&
	test $len = 8196
'

test_expect_success 'validate git ls-files output for a known tree' '
	git ls-files "a*" >current
	a=a && # 1
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 16
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 256
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 4096
	a=${a}e &&
	
	echo $a >expected
	a=a && # 1
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 16
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 256
	a=$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a$a && # 4096
	a=${a}q &&
	echo $a >>expected

	test_cmp expected current
'

test_done
