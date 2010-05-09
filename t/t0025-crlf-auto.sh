#!/bin/sh

test_description='CRLF conversion'

. ./test-lib.sh

has_cr() {
	tr '\015' Q <"$1" | grep Q >/dev/null
}

test_expect_success setup '

	git config core.autocrlf false &&

	for w in Hello world how are you; do echo $w; done >one &&
	for w in I am very very fine thank you; do echo ${w}Q; done | q_to_cr >two &&
	git add . &&

	git commit -m initial &&

	one=`git rev-parse HEAD:one` &&
	two=`git rev-parse HEAD:two` &&

	for w in Some extra lines here; do echo $w; done >>one &&
	git diff >patch.file &&
	patched=`git hash-object --stdin <one` &&
	git read-tree --reset -u HEAD &&

	echo happy.
'

test_expect_success 'default settings cause no changes' '

	rm -f .gitattributes tmp one two &&
	git read-tree --reset -u HEAD &&

	if has_cr one || ! has_cr two
	then
		echo "Eh? $f"
		false
	fi &&
	onediff=`git diff one` &&
	twodiff=`git diff two` &&
	test -z "$onediff" -a -z "$twodiff"
'

test_expect_success 'no crlf=auto, explicit eolstyle=native causes no changes' '

	rm -f .gitattributes tmp one two &&
	git config core.eolstyle native &&
	git read-tree --reset -u HEAD &&

	if has_cr one || ! has_cr two
	then
		echo "Eh? $f"
		false
	fi &&
	onediff=`git diff one` &&
	twodiff=`git diff two` &&
	test -z "$onediff" -a -z "$twodiff"
'

test_expect_success 'crlf=auto, eolStyle=crlf <=> autocrlf=true' '

	rm -f .gitattributes tmp one two &&
	git config core.autocrlf false &&
	git config core.eolstyle crlf &&
	echo "* crlf=auto" > .gitattributes &&
	git read-tree --reset -u HEAD &&
	unset missing_cr &&

	for f in one two
	do
		if ! has_cr "$f"
		then
			echo "Eh? $f"
			missing_cr=1
			break
		fi
	done &&
	test -z "$missing_cr"
'

test_expect_success 'crlf=auto, eolStyle=lf <=> autocrlf=input' '

	rm -f .gitattributes tmp one two &&
	git config core.autocrlf false &&
	git config core.eolstyle lf &&
	echo "* crlf=auto" > .gitattributes &&
	git read-tree --reset -u HEAD &&

	if has_cr one || ! has_cr two
	then
		echo "Eh? $f"
		false
	fi &&
	onediff=`git diff one` &&
	twodiff=`git diff two` &&
	test -z "$onediff" -a -n "$twodiff"
'

test_expect_success 'crlf=auto, eolStyle=false <=> autocrlf=false' '

	rm -f .gitattributes tmp one two &&
	git config core.autocrlf false &&
	git config core.eolstyle false &&
	echo "* crlf=auto" > .gitattributes &&
	git read-tree --reset -u HEAD &&

	if has_cr one || ! has_cr two
	then
		echo "Eh? $f"
		false
	fi
	onediff=`git diff one` &&
	twodiff=`git diff two` &&
	test -z "$onediff" -a -z "$twodiff"
'

test_expect_success 'autocrlf=true overrides crlf=auto, eolStyle=lf' '

	rm -f .gitattributes tmp one two &&
	git config core.autocrlf true &&
	git config core.eolstyle lf &&
	echo "* crlf=auto" > .gitattributes &&
	git read-tree --reset -u HEAD &&
	unset missing_cr &&

	for f in one two
	do
		if ! has_cr "$f"
		then
			echo "Eh? $f"
			missing_cr=1
			break
		fi
	done &&
	test -z "$missing_cr"
'

test_expect_success 'autocrlf=input overrides crlf=auto, eolStyle=crlf' '

	rm -f .gitattributes tmp one two &&
	git config core.autocrlf input &&
	git config core.eolstyle crlf &&
	echo "* crlf=auto" > .gitattributes &&
	git read-tree --reset -u HEAD &&

	if has_cr one || ! has_cr two
	then
		echo "Eh? $f"
		false
	fi &&
	onediff=`git diff one` &&
	twodiff=`git diff two` &&
	test -z "$onediff" -a -n "$twodiff"
'

test_expect_success 'autocrlf=true overrides crlf=auto, eolStyle=false' '

	rm -f .gitattributes tmp one two &&
	git config core.autocrlf true &&
	git config core.eolstyle false &&
	echo "* crlf=auto" > .gitattributes &&
	git read-tree --reset -u HEAD &&
	unset missing_cr &&

	for f in one two
	do
		if ! has_cr "$f"
		then
			echo "Eh? $f"
			missing_cr=1
			break
		fi
	done &&
	test -z "$missing_cr"
'

test_done
