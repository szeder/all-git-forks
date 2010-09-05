#!/bin/sh
#
# Copyright (c) 2007 Shawn Pearce
#

test_description='test git fast-import utility'
. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh ;# test-lib chdir's into trash

test_expect_success 'setup: verify_packs helper' '
	verify_packs () {
		for p in .git/objects/pack/*.pack
		do
			git verify-pack $p ||
			return
		done
	}
'

test_expect_success 'setup' '
	zeroes=0000000000000000000000000000000000000000 &&

	# Use command substitution to strip off final newlines.
	file2_data=$(cat <<-\END_FILE2_DATA
		file2
		second line of EOF
		END_FILE2_DATA
	) &&
	file2_id=$(echo "$file2_data" | git hash-object --stdin) &&
	file3_data=$(cat <<-\END_FILE3_DATA
		EOF
		in 3rd file
		 END
		END_FILE3_DATA
	) &&
	file3_id=$(echo "$file3_data" | git hash-object --stdin) &&
	file4_data=abcd &&
	file4_len=4 &&
	file4_id=$(printf "%s" "$file4_data" | git hash-object --stdin) &&
	file5_data=$(cat <<-\END_FILE5_DATA
		an inline file.
		  we should see it later.
		END_FILE5_DATA
	) &&
	file5_id=$(echo "$file5_data" | git hash-object --stdin) &&
	file6_data=$(cat <<-\END_FILE6_DATA
		#!/bin/sh
		echo "$@"
		END_FILE6_DATA
	) &&
	file6_id=$(echo "$file6_data" | git hash-object --stdin)
'

test_expect_success 'setup: series A' '
	test_tick &&

	cat >input <<-INPUT_END &&
	blob
	mark :2
	data <<EOF
	$file2_data
	EOF

	blob
	mark :3
	data <<END
	$file3_data
	END

	blob
	mark :4
	data $file4_len
	$file4_data
	commit refs/heads/master
	mark :5
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	initial
	COMMIT

	M 644 :2 file2
	M 644 :3 file3
	M 755 :4 file4

	tag series-A
	from :5
	data <<EOF
	An annotated tag without a tagger
	EOF

	INPUT_END

	git fast-import --export-marks=marks.out <input &&
	git whatchanged master
'

test_expect_success 'fast-import writes valid packs' '
	verify_packs
'

test_expect_success 'A: verify commit' '
	cat >expect <<-EOF &&
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	initial
	EOF
	git cat-file commit master >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'A: verify tree' '
	cat >expect <<-\EOF &&
	100644 blob file2
	100644 blob file3
	100755 blob file4
	EOF
	git cat-file -p master^{tree} >tree &&
	sed "s/ [0-9a-f]*	/ /" <tree >actual &&
	test_cmp expect actual
'

test_expect_success 'A: verify file2' '
	echo "$file2_data" >expect &&
	git cat-file blob master:file2 >actual &&
	test_cmp expect actual
'

test_expect_success 'A: verify file3' '
	echo "$file3_data" >expect &&
	git cat-file blob master:file3 >actual &&
	test_cmp expect actual
'

test_expect_success 'A: verify file4' '
	printf "$file4_data" >expect &&
	git cat-file blob master:file4 >actual &&
	test_cmp expect actual
'

test_expect_success 'A: verify tag/series-A' '
	master=$(git rev-parse refs/heads/master) &&
	cat >expect <<-EOF &&
	object $master
	type commit
	tag series-A

	An annotated tag without a tagger
	EOF
	git cat-file tag tags/series-A >actual &&
	test_cmp expect actual
'

test_expect_success 'A: verify marks output' '
	cat >expect <<-EOF &&
	:2 $(git rev-parse --verify master:file2)
	:3 $(git rev-parse --verify master:file3)
	:4 $(git rev-parse --verify master:file4)
	:5 $(git rev-parse --verify master^0)
	EOF
	test_cmp expect marks.out
'

test_expect_success 'A: import marks' '
	cat >expect <<-EOF &&
	:2 $(git rev-parse --verify master:file2)
	:3 $(git rev-parse --verify master:file3)
	:4 $(git rev-parse --verify master:file4)
	:5 $(git rev-parse --verify master^0)
	EOF
	git fast-import \
		--import-marks=marks.out \
		--export-marks=marks.new \
		</dev/null &&
	test_cmp expect marks.new
'

test_expect_success 'setup: A: verify marks import' '
	test_tick &&

	cat >input <<-INPUT_END &&
	commit refs/heads/verify--import-marks
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	recreate from :5
	COMMIT

	from :5
	M 755 :2 copy-of-file2

	INPUT_END

	git fast-import --import-marks=marks.out <input &&
	verify_packs &&
	git whatchanged verify--import-marks
'

test_expect_success 'A: verify diff' '
	echo ":000000 100755 $zeroes $file2_id A	copy-of-file2" >expect &&
	echo $file2_id >expect.copy &&
	git diff-tree -M -r master verify--import-marks >actual &&
	git rev-parse --verify verify--import-marks:copy-of-file2 >actual.copy &&
	compare_diff_raw expect actual &&
	test_cmp expect.copy actual.copy
'

test_expect_success 'A: export marks with large values' '
	test_tick &&
	mt=$(git hash-object --stdin </dev/null) &&
	>input.blob &&
	>marks.exp &&
	>tree.exp &&

	cat >input.commit <<-EOF &&
	commit refs/heads/verify--dump-marks
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	test the sparse array dumping routines with exponentially growing marks
	COMMIT
	EOF

	(
		i=0 &&
		l=4 &&
		m=6 &&
		n=7 &&
		while test "$i" -lt 27
		do
			cat >>input.blob <<-EOF &&
			blob
			mark :$l
			data 0
			blob
			mark :$m
			data 0
			blob
			mark :$n
			data 0
			EOF
			{
				echo "M 100644 :$l l$i" &&
				echo "M 100644 :$m m$i" &&
				echo "M 100644 :$n n$i"
			} >>input.commit &&
			{
				echo ":$l $mt" &&
				echo ":$m $mt" &&
				echo ":$n $mt"
			} >>marks.exp &&
			{
				printf "100644 blob $mt\tl$i\n" &&
				printf "100644 blob $mt\tm$i\n" &&
				printf "100644 blob $mt\tn$i\n"
			} >>tree.exp &&

			l=$(($l + $l)) &&
			m=$(($m + $m)) &&
			n=$(($l + $n)) &&
			i=$((1 + $i)) ||
			exit
		done
	) &&
	sort tree.exp >tree.exp_s &&
	cat input.blob input.commit |
	git fast-import --export-marks=marks.large &&
	git ls-tree refs/heads/verify--dump-marks >tree.out &&
	test_cmp tree.exp_s tree.out &&
	test_cmp marks.exp marks.large
'

test_expect_success 'B: fail on invalid blob sha1' '
	test_tick &&

	cat >input <<-INPUT_END &&
	commit refs/heads/branch
	mark :1
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	corrupt
	COMMIT

	from refs/heads/master
	M 755 0000000000000000000000000000000000000001 zero1

	INPUT_END

	test_must_fail git fast-import <input
'

test_expect_success 'B: fail on invalid branch name ".badbranchname"' '
	rm -f .git/objects/pack_* .git/objects/index_* &&

	cat >input <<-INPUT_END &&
	commit .badbranchname
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	corrupt
	COMMIT

	from refs/heads/master

	INPUT_END

	test_must_fail git fast-import <input
'

test_expect_success 'B: fail on invalid branch name "bad[branch]name"' '
	rm -f .git/objects/pack_* .git/objects/index_* &&

	cat >input <<-INPUT_END &&
	commit bad[branch]name
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	corrupt
	COMMIT

	from refs/heads/master

	INPUT_END

	test_must_fail git fast-import <input
'

test_expect_success 'B: accept branch name "TEMP_TAG"' '
	git rev-parse master >expect &&
	rm -f .git/objects/pack_* .git/objects/index_* &&

	cat >input <<-INPUT_END &&
	commit TEMP_TAG
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	tag base
	COMMIT

	from refs/heads/master

	INPUT_END

	test_when_finished "rm -f .git/TEMP_TAG" &&
	git fast-import <input &&
	test -f .git/TEMP_TAG &&
	git rev-parse TEMP_TAG^ >actual &&
	test_cmp expect actual
'

test_expect_success 'setup: series C' '
	newf=$(echo hi newf | git hash-object -w --stdin) &&
	oldf=$(git rev-parse --verify master:file2) &&
	echo $newf >expect.new &&
	echo $oldf >expect.old &&
	test_tick &&

	cat >input <<-INPUT_END &&
	commit refs/heads/branch
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	second
	COMMIT

	from refs/heads/master
	M 644 $oldf file2/oldf
	M 755 $newf file2/newf
	D file3

	INPUT_END

	git fast-import <input &&
	verify_packs &&
	git whatchanged branch
'

test_expect_success 'C: reuse of existing blob' '
	git rev-parse --verify branch:file2/newf >actual.new &&
	git rev-parse --verify branch:file2/oldf >actual.old &&
	test_cmp expect.new actual.new &&
	test_cmp expect.old actual.old
'

test_expect_success 'C: verify commit' '
	cat >expect <<-EOF &&
	parent `git rev-parse --verify master^0`
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	second
	EOF
	git cat-file commit branch >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'C: validate rename result' '
	cat >expect <<-EOF &&
	:000000 100755 $zeroes $newf A	file2/newf
	:100644 100644 $file2_id $file2_id R100	file2	file2/oldf
	:100644 000000 $file3_id 0000000000000000000000000000000000000000 D	file3
	EOF
	git diff-tree -M -r master branch >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'setup: D: inline data in commit' '
	test_tick &&

	cat >input <<-INPUT_END &&
	commit refs/heads/branch
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	third
	COMMIT

	from refs/heads/branch^0
	M 644 inline newdir/interesting
	data <<EOF
	$file5_data
	EOF

	M 755 inline newdir/exec.sh
	data <<EOF
	$file6_data
	EOF

	INPUT_END

	git fast-import <input &&
	verify_packs &&
	git whatchanged branch
'

test_expect_success 'D: validate new files added' '
	cat >expect <<-EOF &&
	:000000 100755 $zeroes $file6_id A	newdir/exec.sh
	:000000 100644 $zeroes $file5_id A	newdir/interesting
	EOF
	git diff-tree -M -r branch^ branch >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'D: verify file5' '
	echo "$file5_data" >expect &&
	git cat-file blob branch:newdir/interesting >actual &&
	test_cmp expect actual
'

test_expect_success 'D: verify file6' '
	echo "$file6_data" >expect &&
	git cat-file blob branch:newdir/exec.sh >actual &&
	test_cmp expect actual
'

test_expect_success 'E: rfc2822 date, --date-format=raw' '
	cat >input <<-INPUT_END &&
	commit refs/heads/branch
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> Tue Feb 6 11:22:18 2007 -0500
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> Tue Feb 6 12:35:02 2007 -0500
	data <<COMMIT
	RFC 2822 type date
	COMMIT

	from refs/heads/branch^0

	INPUT_END
	test_must_fail git fast-import --date-format=raw <input
'

test_expect_success 'E: rfc2822 date, --date-format=rfc2822' '
	cat >expect <<-EOF &&
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> 1170778938 -0500
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> 1170783302 -0500

	RFC 2822 type date
	EOF
	git fast-import --date-format=rfc2822 <input &&
	verify_packs &&
	git cat-file commit branch >commit &&
	sed 1,2d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'setup: F: non-fast-forward update' '
	test_tick &&

	cat >input <<-INPUT_END
	commit refs/heads/branch
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	losing things already?
	COMMIT

	from refs/heads/branch~1

	reset refs/heads/other
	from refs/heads/branch

	INPUT_END

	old_branch=$(git rev-parse --verify branch^0) &&
	test_must_fail git fast-import <input &&
	verify_packs
'

test_expect_success 'F: non-fast-forward update skips' '
	echo $old_branch >expect &&
	git rev-parse --verify branch^0 >actual &&
	test_cmp expect actual
'

test_expect_success 'F: verify other commit' '
	cat >expect <<-EOF &&
	tree $(git rev-parse branch~1^{tree})
	parent $(git rev-parse branch~1)
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	losing things already?
	EOF
	git cat-file commit other >actual &&
	test_cmp expect actual
'

test_expect_success 'setup: G: forced non-fast-forward update' '
	test_tick &&
	cat >input <<-INPUT_END &&
	commit refs/heads/branch
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	losing things already?
	COMMIT

	from refs/heads/branch~1

	INPUT_END

	git rev-parse --verify branch^0 >old_branch &&
	git fast-import --force <input &&
	verify_packs
'

test_expect_success 'G: branch changed, but logged' '
	git rev-parse --verify branch^0 >branch &&
	git rev-parse --verify branch@{1} >prev &&
	! test_cmp old_branch branch &&
	test_cmp old_branch prev
'

test_expect_success 'setup: H: deleteall, add one' '
	test_tick &&

	cat >input <<-INPUT_END &&
	commit refs/heads/H
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	third
	COMMIT

	from refs/heads/branch^0
	M 644 inline i-will-die
	data <<EOF
	this file will never exist.
	EOF

	deleteall
	M 644 inline h/e/l/lo
	data <<EOF
	$file5_data
	EOF

	INPUT_END

	git fast-import <input &&
	verify_packs &&
	git whatchanged H
'

test_expect_success 'H: validate old files removed, new files added' '
	cat >expect <<-EOF &&
	:100755 000000 $newf $zeroes D	file2/newf
	:100644 000000 $oldf $zeroes D	file2/oldf
	:100755 000000 $file4_id $zeroes D	file4
	:100644 100644 $file5_id $file5_id R100	newdir/interesting	h/e/l/lo
	:100755 000000 $file6_id $zeroes D	newdir/exec.sh
	EOF
	git diff-tree -M -r H^ H >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'H: verify file' '
	echo "$file5_data" >expect &&
	git cat-file blob H:h/e/l/lo >actual &&
	test_cmp expect actual
'

test_expect_success 'I: --export-pack-edges' '
	echo ".git/objects/pack/pack-.pack: EDGE" >expect &&

	cat >input <<-INPUT_END &&
	commit refs/heads/export-boundary
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	we have a border.  its only 40 characters wide.
	COMMIT

	from refs/heads/branch

	INPUT_END

	git fast-import --export-pack-edges=edges.list <input &&
	tip=$(git rev-parse --verify export-boundary) &&
	sed -e "
			s/pack-.*pack/pack-.pack/
			s/$tip/EDGE/
		" edges.list >actual &&
	test_cmp expect actual
'

test_expect_success 'J: reset existing branch creates empty commit' '
	echo SHA >one &&
	>empty &&

	cat >input <<-INPUT_END &&
	commit refs/heads/J
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	create J
	COMMIT

	from refs/heads/branch

	reset refs/heads/J

	commit refs/heads/J
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	initialize J
	COMMIT

	INPUT_END

	git fast-import <input &&
	git rev-list J >commitlist &&
	git ls-tree J >listing &&

	sed -e "s/$_x40/SHA/" commitlist >num-commits &&
	test_cmp one num-commits &&
	test_cmp empty listing
'

test_expect_success 'K: reinit branch with from' '
	git rev-parse --verify branch^1 >expect &&

	cat >input <<-INPUT_END &&
	commit refs/heads/K
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	create K
	COMMIT

	from refs/heads/branch

	commit refs/heads/K
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	redo K
	COMMIT

	from refs/heads/branch^1

	INPUT_END

	git fast-import <input &&
	git rev-parse --verify K^1 >actual &&
	test_cmp expect actual
'

test_expect_success 'L: trees sort correctly' '
	some_data=$(echo some data | git hash-object -w --stdin) &&
	other_data=$(echo other data | git hash-object -w --stdin) &&
	some_tree=$(
		rm -f tmp_index &&
		echo "100644 blob $some_data	other" |
		GIT_INDEX_FILE=tmp_index git update-index --index-info &&
		GIT_INDEX_FILE=tmp_index git write-tree
	) &&
	other_tree=$(
		rm -f tmp_index &&
		echo "100644 blob $other_data	other" |
		GIT_INDEX_FILE=tmp_index git update-index --index-info &&
		GIT_INDEX_FILE=tmp_index git write-tree
	) &&
	cat >expect <<-EXPECT_END &&
	:100644 100644 $some_data $other_data M	b.
	:040000 040000 $some_tree $other_tree M	b
	:100644 100644 $some_data $other_data M	ba
	EXPECT_END

	cat >input <<-INPUT_END &&
	blob
	mark :1
	data <<EOF
	some data
	EOF

	blob
	mark :2
	data <<EOF
	other data
	EOF

	commit refs/heads/L
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	create L
	COMMIT

	M 644 :1 b.
	M 644 :1 b/other
	M 644 :1 ba

	commit refs/heads/L
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	update L
	COMMIT

	M 644 :2 b.
	M 644 :2 b/other
	M 644 :2 ba
	INPUT_END
	git fast-import <input &&
	git diff-tree L^ L >output &&
	test_cmp expect output
'

test_expect_success 'M: rename file within subdirectory' '
	test_tick &&
	cat >expect <<-EOF &&
	:100755 100755 $newf $newf R100	file2/newf	file2/n.e.w.f
	EOF

	cat >input <<-INPUT_END &&
	commit refs/heads/M1
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	file rename
	COMMIT

	from refs/heads/branch^0
	R file2/newf file2/n.e.w.f

	INPUT_END

	git fast-import <input &&
	git diff-tree -M -r M1^ M1 >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'M: rename file to new subdirectory (and set up M2)' '
	cat >expect <<-EOF &&
	:100755 100755 $newf $newf R100	file2/newf	i/am/new/to/you
	EOF

	cat >input <<-INPUT_END &&
	commit refs/heads/M2
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	file rename
	COMMIT

	from refs/heads/branch^0
	R file2/newf i/am/new/to/you

	INPUT_END

	git fast-import <input &&
	git diff-tree -M -r M2^ M2 >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'M: rename subdirectory to new subdirectory' '
	cat >expect <<-EOF &&
	:100755 100755 $newf $newf R100	i/am/new/to/you	other/sub/am/new/to/you
	EOF

	cat >input <<-INPUT_END &&
	commit refs/heads/M3
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	file rename
	COMMIT

	from refs/heads/M2^0
	R i other/sub

	INPUT_END

	git fast-import <input &&
	git diff-tree -M -r M3^ M3 >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'N: copy file in same subdirectory' '
	test_tick &&
	cat >expect <<-EOF &&
	:100755 100755 $newf $newf C100	file2/newf	file2/n.e.w.f
	EOF

	cat >input <<-INPUT_END &&
	commit refs/heads/N1
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	file copy
	COMMIT

	from refs/heads/branch^0
	C file2/newf file2/n.e.w.f

	INPUT_END

	git fast-import <input &&
	git diff-tree -C --find-copies-harder -r N1^ N1 >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'N: copy then modify subdirectory (and set up N2)' '
	cat >expect <<-EOF &&
	:100644 100644 $file5_id $file5_id C100	newdir/interesting	file3/file5
	:100755 100755 $newf $newf C100	file2/newf	file3/newf
	:100644 100644 $oldf $oldf C100	file2/oldf	file3/oldf
	EOF

	cat >input <<-INPUT_END &&
	commit refs/heads/N2
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	clean directory copy
	COMMIT

	from refs/heads/branch^0
	C file2 file3

	commit refs/heads/N2
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	modify directory copy
	COMMIT

	M 644 inline file3/file5
	data <<EOF
	$file5_data
	EOF

	INPUT_END

	git fast-import <input &&
	git diff-tree -C --find-copies-harder -r N2^^ N2 >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'N: copy dirty subdirectory (and set up N3)' '
	git rev-parse N2^{tree} >expect &&

	cat >input <<-INPUT_END &&
	commit refs/heads/N3
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	dirty directory copy
	COMMIT

	from refs/heads/branch^0
	M 644 inline file2/file5
	data <<EOF
	$file5_data
	EOF

	C file2 file3
	D file2/file5

	INPUT_END

	git fast-import <input &&
	git rev-parse N3^{tree} >actual &&
	test_cmp expect actual
'

test_expect_success 'N: copy directory by id' '
	subdir=$(git rev-parse --verify refs/heads/branch^0:file2) &&
	cat >expect <<-EOF &&
	:100755 100755 $newf $newf C100	file2/newf	file3/newf
	:100644 100644 $oldf $oldf C100	file2/oldf	file3/oldf
	EOF

	cat >input <<-INPUT_END &&
	commit refs/heads/N4
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	copy by tree hash
	COMMIT

	from refs/heads/branch^0
	M 040000 $subdir file3
	INPUT_END

	git fast-import <input &&
	git diff-tree -C --find-copies-harder -r N4^ N4 >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'N: modify copied tree' '
	subdir=$(git rev-parse refs/heads/branch^0:file2) &&
	cat >expect <<-EOF &&
	:100644 100644 $file5_id $file5_id C100	newdir/interesting	file3/file5
	:100755 100755 $newf $newf C100	file2/newf	file3/newf
	:100644 100644 $oldf $oldf C100	file2/oldf	file3/oldf
	EOF

	cat >input <<-INPUT_END &&
	commit refs/heads/N5
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	copy by tree hash
	COMMIT

	from refs/heads/branch^0
	M 040000 $subdir file3

	commit refs/heads/N5
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	modify directory copy
	COMMIT

	M 644 inline file3/file5
	data <<EOF
	$file5_data
	EOF
	INPUT_END

	git fast-import <input &&
	git diff-tree -C --find-copies-harder -r N5^^ N5 >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'O: comments are all skipped' '
	git rev-parse N3 >expect &&

	cat >input <<-INPUT_END &&
	#we will
	commit refs/heads/O1
	# -- ignore all of this text
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	# $GIT_COMMITTER_NAME has inserted here for his benefit.
	data <<COMMIT
	dirty directory copy
	COMMIT

	# don'\''t forget the import blank line!
	#
	# yes, we started from our usual base of branch^0.
	# i like branch^0.
	from refs/heads/branch^0
	# and we need to reuse file2/file5 from N3 above.
	M 644 inline file2/file5
	# otherwise the tree will be different
	data <<EOF
	$file5_data
	EOF

	# don'\''t forget to copy file2 to file3
	C file2 file3
	#
	# or to delete file5 from file2.
	D file2/file5
	# are we done yet?

	INPUT_END

	git fast-import <input &&
	git rev-parse O1 >actual &&
	test_cmp expect actual
'

test_expect_success 'O: blank lines not necessary after data commands' '
	git rev-parse N3 >expect &&

	cat >input <<-INPUT_END &&
	commit refs/heads/O2
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	dirty directory copy
	COMMIT
	from refs/heads/branch^0
	M 644 inline file2/file5
	data <<EOF
	$file5_data
	EOF
	C file2 file3
	D file2/file5

	INPUT_END

	git fast-import <input &&
	git rev-parse O2 >actual &&
	test_cmp expect actual
'

test_expect_success 'O: blank lines not necessary after other commands' '
	cat >expect <<-\INPUT_END &&
	string
	of
	empty
	commits
	INPUT_END

	git repack -a -d &&

	cat >input <<-INPUT_END &&
	commit refs/heads/O3
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zstring
	COMMIT
	commit refs/heads/O3
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zof
	COMMIT
	checkpoint
	commit refs/heads/O3
	mark :5
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zempty
	COMMIT
	checkpoint
	commit refs/heads/O3
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zcommits
	COMMIT
	reset refs/tags/O3-2nd
	from :5
	reset refs/tags/O3-3rd
	from :5
	INPUT_END

	git fast-import <input &&
	git log --reverse --pretty=oneline O3 >log &&
	git rev-parse O3^ >o3-parent &&
	git rev-parse refs/tags/O3-2nd >o3-2nd &&

	sed s/^.*z// log >actual &&
	test_cmp expect actual &&
	test 8 = `find .git/objects/pack -type f | wc -l` &&
	test_cmp o3-2nd o3-parent
'

test_expect_success 'O: progress outputs as requested by input' '
	cat >input <<-INPUT_END &&
	commit refs/heads/O4
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zstring
	COMMIT
	commit refs/heads/O4
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zof
	COMMIT
	progress Two commits down, 2 to go!
	commit refs/heads/O4
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zempty
	COMMIT
	progress Three commits down, 1 to go!
	commit refs/heads/O4
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	zcommits
	COMMIT
	progress I'\''m done!
	INPUT_END
	grep "progress " <input >expect &&

	git fast-import <input >actual &&
	test_cmp expect actual
'

test_expect_success 'setup: P: supermodule & submodule mix' '
	cat >input <<-INPUT_END &&
	blob
	mark :1
	data 10
	test file

	reset refs/heads/sub
	commit refs/heads/sub
	mark :2
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 12
	sub_initial
	M 100644 :1 file

	blob
	mark :3
	data <<DATAEND
	[submodule "sub"]
		path = sub
		url = "$(pwd)/sub"
	DATAEND

	commit refs/heads/subuse1
	mark :4
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 8
	initial
	from refs/heads/master
	M 100644 :3 .gitmodules
	M 160000 :2 sub

	blob
	mark :5
	data 20
	test file
	more data

	commit refs/heads/sub
	mark :6
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 11
	sub_second
	from :2
	M 100644 :5 file

	commit refs/heads/subuse1
	mark :7
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	second
	from :4
	M 160000 :6 sub

	INPUT_END

	git fast-import <input &&
	git checkout subuse1 &&
	rm -rf sub &&
	mkdir sub &&
	(
		cd sub &&
		git init &&
		git fetch --update-head-ok .. refs/heads/sub:refs/heads/master &&
		git checkout master
	) &&
	git submodule init &&
	git submodule update
'

test_expect_success 'P: verbatim SHA gitlinks (and set up subuse2)' '
	SUBLAST=$(git rev-parse --verify sub) &&
	SUBPREV=$(git rev-parse --verify sub^) &&
	git rev-parse --verify subuse1 >expect &&

	cat >input <<-INPUT_END &&
	blob
	mark :1
	data <<DATAEND
	[submodule "sub"]
		path = sub
		url = "$(pwd)/sub"
	DATAEND

	commit refs/heads/subuse2
	mark :2
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 8
	initial
	from refs/heads/master
	M 100644 :1 .gitmodules
	M 160000 $SUBPREV sub

	commit refs/heads/subuse2
	mark :3
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	second
	from :2
	M 160000 $SUBLAST sub

	INPUT_END

	git branch -D sub &&
	git gc && git prune &&
	git fast-import <input &&
	git rev-parse --verify subuse2 >actual &&
	test_cmp expect actual
'

test_expect_success 'P: fail on inline gitlink' '
	test_tick &&

	cat >input <<-INPUT_END &&
	commit refs/heads/subuse3
	mark :1
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	corrupt
	COMMIT

	from refs/heads/subuse2
	M 160000 inline sub
	data <<DATA
	$SUBPREV
	DATA

	INPUT_END

	test_must_fail git fast-import <input
'

test_expect_success 'P: fail on blob mark in gitlink' '
	test_tick &&

	cat >input <<-INPUT_END &&
	blob
	mark :1
	data <<DATA
	$SUBPREV
	DATA

	commit refs/heads/subuse3
	mark :2
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	corrupt
	COMMIT

	from refs/heads/subuse2
	M 160000 :1 sub

	INPUT_END

	test_must_fail git fast-import <input
'

test_expect_success 'setup: series Q (notes)' '
	test_tick &&

	note1_data="The first note for the first commit" &&
	note2_data="The first note for the second commit" &&
	note3_data="The first note for the third commit" &&
	note1b_data="The second note for the first commit" &&
	note1c_data="The third note for the first commit" &&
	note2b_data="The second note for the second commit" &&

	cat >input <<-INPUT_END &&
	blob
	mark :2
	data <<EOF
	$file2_data
	EOF

	commit refs/heads/notes-test
	mark :3
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	first (:3)
	COMMIT

	M 644 :2 file2

	blob
	mark :4
	data $file4_len
	$file4_data
	commit refs/heads/notes-test
	mark :5
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	second (:5)
	COMMIT

	M 644 :4 file4

	commit refs/heads/notes-test
	mark :6
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	third (:6)
	COMMIT

	M 644 inline file5
	data <<EOF
	$file5_data
	EOF

	M 755 inline file6
	data <<EOF
	$file6_data
	EOF

	blob
	mark :7
	data <<EOF
	$note1_data
	EOF

	blob
	mark :8
	data <<EOF
	$note2_data
	EOF

	commit refs/notes/foobar
	mark :9
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	notes (:9)
	COMMIT

	N :7 :3
	N :8 :5
	N inline :6
	data <<EOF
	$note3_data
	EOF

	commit refs/notes/foobar
	mark :10
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	notes (:10)
	COMMIT

	N inline :3
	data <<EOF
	$note1b_data
	EOF

	commit refs/notes/foobar2
	mark :11
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	notes (:11)
	COMMIT

	N inline :3
	data <<EOF
	$note1c_data
	EOF

	commit refs/notes/foobar
	mark :12
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data <<COMMIT
	notes (:12)
	COMMIT

	deleteall
	N inline :5
	data <<EOF
	$note2b_data
	EOF

	INPUT_END

	git fast-import <input &&
	verify_packs &&
	git whatchanged notes-test
'

test_expect_success 'Q: verify first commit' '
	cat >expect <<-EOF &&
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	first (:3)
	EOF

	git cat-file commit notes-test~2 >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify second commit' '
	commit1=$(git rev-parse notes-test~2) &&
	cat >expect <<-EOF
	parent $commit1
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	second (:5)
	EOF
	git cat-file commit notes-test^ >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify third commit' '
	commit2=$(git rev-parse notes-test^) &&
	cat >expect <<-EOF &&
	parent $commit2
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	third (:6)
	EOF
	git cat-file commit notes-test >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify first notes commit' '
	cat >expect <<-EOF &&
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	notes (:9)
	EOF
	git cat-file commit refs/notes/foobar~2 >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify first notes tree' '
	commit1=$(git rev-parse notes-test~2) &&
	commit2=$(git rev-parse notes-test^) &&
	commit3=$(git rev-parse notes-test) &&
	sort >expect <<-EOF &&
	100644 blob $commit1
	100644 blob $commit2
	100644 blob $commit3
	EOF

	git cat-file -p refs/notes/foobar~2^{tree} >tree &&
	sed "s/ [0-9a-f]*	/ /" <tree >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify first note for first commit' '
	echo "$note1_data" >expect &&
	git cat-file blob refs/notes/foobar~2:$commit1 >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify first note for second commit' '
	echo "$note2_data" >expect &&
	git cat-file blob refs/notes/foobar~2:$commit2 >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify first note for third commit' '
	echo "$note3_data" >expect &&
	git cat-file blob refs/notes/foobar~2:$commit3 >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify second notes commit' '
	cat >expect <<-EOF &&
	parent `git rev-parse --verify refs/notes/foobar~2`
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	notes (:10)
	EOF
	git cat-file commit refs/notes/foobar^ >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify second notes tree' '
	sort >expect <<-EOF
	100644 blob $commit1
	100644 blob $commit2
	100644 blob $commit3
	EOF
	git cat-file -p refs/notes/foobar^^{tree} >tree &&
	sed "s/ [0-9a-f]*	/ /" <tree >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify second note for first commit' '
	echo "$note1b_data" >expect &&
	git cat-file blob refs/notes/foobar^:$commit1 >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify first note for second commit' '
	echo "$note2_data" >expect &&
	git cat-file blob refs/notes/foobar^:$commit2 >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify first note for third commit' '
	echo "$note3_data" >expect &&
	git cat-file blob refs/notes/foobar^:$commit3 >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify third notes commit' '
	cat >expect <<-EOF &&
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	notes (:11)
	EOF
	git cat-file commit refs/notes/foobar2 >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify third notes tree' '
	echo "100644 blob $commit1" >expect &&
	git cat-file -p refs/notes/foobar2^{tree} >tree &&
	sed "s/ [0-9a-f]*	/ /" <tree >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify third note for first commit' '
	echo "$note1c_data" >expect
	git cat-file blob refs/notes/foobar2:$commit1 >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify fourth notes commit' '
	parent=$(git rev-parse --verify refs/notes/foobar^) &&
	cat >expect <<-EOF &&
	parent $parent
	author $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE

	notes (:12)
	EOF
	git cat-file commit refs/notes/foobar >commit &&
	sed 1d <commit >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify fourth notes tree' '
	echo "100644 blob $commit2" >expect &&
	git cat-file -p refs/notes/foobar^{tree} >tree &&
	sed "s/ [0-9a-f]*	/ /" <tree >actual &&
	test_cmp expect actual
'

test_expect_success 'Q: verify second note for second commit' '
	echo "$note2b_data" >expect
	git cat-file blob refs/notes/foobar:$commit2 >actual &&
	test_cmp expect actual
'

test_expect_success 'R: abort on unsupported feature' '
	cat >input <<-EOF &&
	feature no-such-feature-exists
	EOF

	test_must_fail git fast-import <input
'

test_expect_success 'R: supported feature is accepted' '
	cat >input <<-EOF &&
	feature date-format=now
	EOF

	git fast-import <input
'

test_expect_success 'R: abort on receiving feature after data command' '
	cat >input <<-EOF &&
	blob
	data 3
	hi
	feature date-format=now
	EOF

	test_must_fail git fast-import <input
'

test_expect_success 'R: only one import-marks feature allowed per stream' '
	cat >input <<-\EOF &&
	feature import-marks=git.marks
	feature import-marks=git2.marks
	EOF

	test_must_fail git fast-import <input
'

test_expect_success 'setup: R: stream using export-marks feature' '
	cat >input <<-\EOF
	feature export-marks=git.marks
	blob
	mark :1
	data 3
	hi

	EOF
'

test_expect_success 'R: export-marks feature results in a marks file being created' '
	git fast-import <input &&
	grep :1 git.marks
'

test_expect_success 'R: export-marks options can be overriden by commandline options' '
	git fast-import --export-marks=other.marks <input &&
	grep :1 other.marks
'

test_expect_success 'R: import to output marks works without any content' '
	cat >input <<-\EOF &&
	feature import-marks=marks.out
	feature export-marks=marks.new
	EOF

	git fast-import <input &&
	test_cmp marks.out marks.new
'

test_expect_success 'R: import marks prefers commandline marks file over the stream' '
	cat >input <<-EOF &&
	feature import-marks=nonexistant.marks
	feature export-marks=marks.new
	EOF
	git fast-import --import-marks=marks.out <input &&
	test_cmp marks.out marks.new
'

test_expect_success 'R: multiple --import-marks= are honoured' '
	cat >input <<-EOF &&
	feature import-marks=nonexistant.marks
	feature export-marks=combined.marks
	EOF

	head -n2 marks.out >one.marks &&
	tail -n +3 marks.out >two.marks &&
	git fast-import --import-marks=one.marks --import-marks=two.marks <input &&
	test_cmp marks.out combined.marks
'

test_expect_success 'R: feature relative-marks is honoured' '
	cat >input <<-\EOF &&
	feature relative-marks
	feature import-marks=relative.in
	feature export-marks=relative.out
	EOF

	mkdir -p .git/info/fast-import/ &&
	cp marks.out .git/info/fast-import/relative.in &&
	git fast-import <input &&
	test_cmp marks.out .git/info/fast-import/relative.out
'

test_expect_success 'R: feature no-relative-marks is honoured' '
	cat >input <<-\EOF &&
	feature relative-marks
	feature import-marks=relative.in
	feature no-relative-marks
	feature export-marks=non-relative.out
	EOF

	git fast-import <input &&
	test_cmp marks.out non-relative.out
'

test_expect_success 'have pipes?' '
	test_when_finished "rm -f frob" &&
	if mkfifo frob
	then
		test_set_prereq PIPE
	fi
'

test_expect_success PIPE 'R: feature report-fd is honoured' '
	mkfifo commits &&
	test_when_finished "rm -f commits" &&
	cat >frontend <<-\FRONTEND_END &&
		#!/bin/sh
		cat <<EOF &&
		feature report-fd=3
		commit refs/heads/printed
		committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
		data <<COMMIT
		to be printed
		COMMIT

		from refs/heads/master
		D file3

		EOF

		read cid <&3 &&
		echo "$cid" >received
		EOF
	FRONTEND_END
	chmod +x frontend &&
	(
		{
			sh frontend 3<commits ||
			exit
		} |
		git fast-import 3>commits
	) &&
	git rev-parse --verify printed >real &&
	test_cmp real received
'

test_expect_success PIPE 'R: report-fd: can feed back printed tree' '
	cat >frontend <<-\FRONTEND_END &&
		#!/bin/sh
		cat <<EOF &&
		feature report-fd=3
		commit refs/heads/printed
		committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
		data <<COMMIT
		to be printed
		COMMIT

		from refs/heads/master
		D file3

		EOF

		read commit_id <&3 &&
		echo "$commit_id" >printed &&
		echo "$commit_id commit" >expect.response &&
		echo "cat $commit_id" &&
		read cid2 type size <&3 &&
		echo "$cid2 $type" >response &&
		dd if=/dev/stdin of=commit bs=1 count=$size <&3 &&
		read newline <&3 &&
		read tree tree_id <commit &&

		cat <<EOF &&
		commit refs/heads/printed
		committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
		data <<COMMIT
		to be printed
		COMMIT

		from refs/heads/printed^0
		M 040000 $tree_id old

		EOF
		read cid <&3
	FRONTEND_END
	chmod +x frontend &&

	mkfifo commits &&
	test_when_finished "rm -f commits" &&
	(
		{
			sh frontend 3<commits ||
			exit
		} |
		git fast-import 3>commits
	) &&
	git rev-parse printed^ >expect.printed &&
	git cat-file commit printed^ >expect.commit &&

	test_cmp expect.printed printed &&
	test_cmp expect.response response &&
	test_cmp expect.commit commit
'

test_expect_success PIPE 'R: report-fd: can feed back printed blob' '
	cat >expect <<-EOF &&
	:100755 100644 $file6_id $file6_id C100	newdir/exec.sh	file6
	EOF

	cat >frontend <<-\FRONTEND_END &&
		#!/bin/sh

		branch=$(git rev-parse --verify refs/heads/branch) &&
		cat <<EOF &&
		feature report-fd=3
		cat $branch
		EOF

		read commit_id type size <&3 &&
		dd if=/dev/stdin of=commit bs=1 count=$size <&3 &&
		read newline <&3 &&
		read tree tree_id <commit &&

		echo "cat $tree_id \"newdir/exec.sh\"" &&
		read blob_id type size <&3 &&
		dd if=/dev/stdin of=blob bs=1 count=$size <&3 &&
		read newline <&3 &&

		cat <<EOF &&
		commit refs/heads/copyblob
		committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
		data <<COMMIT
		copy file6 to top level
		COMMIT

		from refs/heads/branch^0
		M 644 inline "file6"
		data $size
		EOF
		cat blob &&
		echo &&
		echo &&

		read cid <&3
	FRONTEND_END
	chmod +x frontend &&

	mkfifo commits &&
	test_when_finished "rm -f commits" &&
	(
		{
			sh frontend 3<commits ||
			exit
		} |
		git fast-import 3>commits
	) &&
	git diff-tree -C --find-copies-harder -r copyblob^ copyblob >actual &&
	compare_diff_raw expect actual
'

test_expect_success 'R: quiet option results in no stats being output' '
	>empty &&
	cat >input <<-\EOF &&
	option git quiet
	blob
	data 3
	hi

	EOF

	git fast-import <input 2>output &&
	test_cmp empty output
'

test_expect_success 'R: die on unknown option' '
	cat >input <<-\EOF &&
	option git non-existing-option
	EOF

	test_must_fail git fast-import <input
'

test_expect_success 'R: unknown commandline options are rejected' '\
	test_must_fail git fast-import --non-existing-option </dev/null
'

test_expect_success 'R: ignore non-git options' '
	cat >input <<-\EOF &&
	option non-existing-vcs non-existing-option
	EOF

	git fast-import <input
'

test_expect_success 'R: blob bigger than threshold' '
	blobsize=$((2*1024*1024 + 53)) &&
	test-genrandom bar $blobsize >expect &&
	echo ONE | wc -l >expect.count &&

	{
		cat <<-INPUT_END &&
		commit refs/heads/big-file
		committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
		data <<COMMIT
		R - big file
		COMMIT

		M 644 inline big1
		data $blobsize
		INPUT_END

		cat expect &&
		cat <<-INPUT_END &&
		M 644 inline big2
		data $blobsize
		INPUT_END

		cat expect &&
		echo
	} >input &&

	test_create_repo R &&
	git --git-dir=R/.git fast-import --big-file-threshold=1 <input &&
	(
		for p in R/.git/objects/pack/*.pack
		do
			git verify-pack -v $p ||
			exit
		done
	) >verify &&
	git --git-dir=R/.git cat-file blob big-file:big1 >actual &&
	git --git-dir=R/.git rev-parse big-file:big1 >a &&
	git --git-dir=R/.git rev-parse big-file:big2 >b &&
	grep $(cat a) verify | wc -l >count &&

	test_cmp expect actual &&
	test_cmp a b &&
	# blob only appears once
	test_cmp expect.count count
'

test_done
