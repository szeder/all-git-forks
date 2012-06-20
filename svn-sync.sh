#!/bin/sh

rm -rf svn
mkdir svn
cd svn

#Setup the test database
svnadmin create db

cat > db/conf/passwd <<!
[users]
user = pass
!

cat > db/conf/authz <<!
[/]
user = rw
!

cat > db/conf/svnserve.conf <<!
[general]
anon-access = read
auth-access = write
password-db = passwd
realm = Test Repository
authz-db = authz
!

# Setup the subversion repo
killall svnserve
svnserve --daemon --listen-host=localhost --log-file svnlog --root db

svn co --username user --password pass svn://localhost co
cd co
cat > file.txt <<!
Some file contents
Some more

!
svn add file.txt
svn ci -m 'add file.txt'
exit

svn mkdir --parents a/b/c/d/e/f
cat > a/b/c/d/e/f/deep.txt <<!
Some deep contents
.....
!
svn add a/b/c/d/e/f/deep.txt
svn ci -m 'add deep.txt'
svn up

svn rm file.txt
svn ci -m 'remove file.txt'
svn up

svn rm a/b
svn ci -m 'remove folder a/b'
svn up

cat > a/foo.txt <<!
Some other contents for foo.txt
!
svn add a/foo.txt
svn ci -m 'add foo.txt'
svn up

svn mv a b
svn ci -m 'move folder'
svn up

svn mv b/foo.txt b/foo2.txt
svn ci -m 'move file'
svn up

cd ..
rm -rf co
git init

f=/tmp/svnfifo
rm -f $f
mkfifo $f
cat $f | nc localhost 3690 | ../git-svn-sync -v --tmp $PWD/tmp --user user --pass pass svn://localhost master a  > $f
