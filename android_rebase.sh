git remote add gitorigin https://github.com/git/git.git 2>/dev/null
git fetch gitorigin
git rebase gitorigin/master || git rebase --abort

# We need to remove the "v" of the version...
# GIT_VERSION=`git describe gitorigin/master`
# echo "GIT_VERSION = $GIT_VERSION" > GIT-VERSION-FILE

