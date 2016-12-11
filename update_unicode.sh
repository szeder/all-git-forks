#!/bin/sh
#See http://www.unicode.org/reports/tr44/
#
#Me Enclosing_Mark  an enclosing combining mark
#Mn Nonspacing_Mark a nonspacing combining mark (zero advance width)
#Cf Format          a format control character
#

dec_to_hex() {
	# convert any decimal numbers to 4-digit hex
	perl -pe 's/(\d+)/sprintf("0x%04X", $1)/ge'
}

UNICODEWIDTH_H=../unicode_width.h
if ! test -d unicode; then
	mkdir unicode
fi &&
( cd unicode &&
	wget -N http://www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt \
		http://www.unicode.org/Public/UCD/latest/ucd/EastAsianWidth.txt &&
	if ! test -d uniset; then
		git clone https://github.com/depp/uniset.git
	else
	(
		cd uniset &&
		git pull
	)
	fi &&
	(
		cd uniset &&
		if ! test -x uniset; then
			autoreconf -i &&
			./configure --enable-warnings=-Werror CFLAGS='-O0 -ggdb'
		fi &&
		make
	) &&
	UNICODE_DIR=. && export UNICODE_DIR &&
	dec_to_hex >$UNICODEWIDTH_H <<-EOF
	static const struct interval zero_width[] = {
		$(uniset/uniset --32 cat:Me,Mn,Cf + U+1160..U+11FF - U+00AD)
	};
	static const struct interval double_width[] = {
		$(uniset/uniset --32 eaw:F,W)
	};
	EOF
)
