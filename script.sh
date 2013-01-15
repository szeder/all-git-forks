#!/bin/sh

PATH=~/code/git/contrib/remote-helpers/:~/code/git/bin-wrappers/:$PATH

rm -rf mercurial
rm -rf git

(hg init mercurial &&
cd mercurial &&
>txt &&
hg add txt &&
hg commit -m. -uAuthor &&
echo " " >txt &&
hg commit -m. -uAuthor) &&
git clone hg::$PWD/mercurial git &&
(cd mercurial &&
hg update 0 &&
hg --config "extensions.mq=" strip 1 &&
echo " " >>txt &&
hg commit -m. -uCommitter &&
echo " " >>txt &&
hg commit -m. -uAuthor &&
hg bookmark test) &&
(cd git &&
git fetch &&
git log --author Committer || echo "no such committer") &&
(cd mercurial &&
hg update 0 &&
hg bookmark -f test) &&
(cd git &&
git fetch)
