#!/bin/sh

make

rm -rf svn
mkdir svn
cd svn

#Setup the test database
svnadmin create db

cat > db/conf/passwd <<!
[users]
user = pass
!

cat > db/conf/svnserve.conf <<!
[general]
anon-access = none
auth-access = write
password-db = passwd
realm = Test Repository
!

# Setup the subversion repo
killall svnserve
killall lt-svnserve
svnserve --daemon --log-file svnlog --root db

svn co --username user --password pass svn://localhost co
cd co
svn mkdir trunk
cd trunk
cat > file.txt <<!
Some file contents
Some more

!
svn add file.txt
svn ci -m 'add file.txt'

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

svn mkdir svn://localhost/tags -m 'create tags folder'

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
#echo "some new text" >> trunk/b/foo2.txt
#echo "some new file" > trunk/b/foo.txt
#svn add trunk/b/foo.txt
#svn mkdir fake
#svn ci -m 'git commit add/mod'
#exit

svn mkdir branches
svn ci -m 'create branches folder'
svn up

svn cp svn://localhost/trunk svn://localhost/branches/foobranch -m 'create branch'
svn cp svn://localhost/trunk@4 svn://localhost/tags/footag -m 'create tag'

cd ..
git init

cat > .git/svn-authors <<!
# Some comment

user:pass = James M <james@example.com>
!

../git-svn-fetch -v -r 3 -t trunk -b branches -T tags --user user --pass pass svn://localhost
../git-svn-fetch -v -t trunk -b branches -T tags --user user --pass pass svn://localhost

git config user.name 'James M'
git config user.email 'james@example.com'


git checkout -b master svn/master

echo "some new text" >> b/foo2.txt
echo "some new file" > b/foo.txt
git add b/foo.txt b/foo2.txt
git commit -m 'git commit add/mod'

mkdir -p b/c
echo "some more text" >> b/c/foo.txt
git add b/c/foo.txt
git commit -m 'git commit 2'

git rm -rf b
git commit -m 'some removals'


git checkout -b foobranch svn/foobranch

echo "some branch file" >> b/foo2.txt
git add b/foo2.txt
git commit -m 'git branch commit'

../git svn-push -v -t trunk -b branches -T tags svn://localhost heads/master svn/master master
../git svn-push -v -t trunk -b branches -T tags svn://localhost heads/foobranch svn/foobranch foobranch

cat > .git/hooks/pre-receive <<!
#!/bin/sh
exec $PWD/../git-svn-push -v -t trunk -b branches -T tags --pre-receive svn://localhost
!

chmod +x .git/hooks/pre-receive
git config receive.denyCurrentBranch ignore

../git clone -- . gitco
cd gitco

git config user.name 'James M'
git config user.email 'james@example.com'

echo "some mod" >> b/foo2.txt
../../git add b/foo2.txt
../../git commit -m 'git clone commit'
../../git push

