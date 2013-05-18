set -e
export CC="ccache gcc"
export PYTHON_PATH="/usr/bin/python2"
make CC="$CC" all

rm -f git-svn
ln -s git-svn.perl git-svn
for pm in Git/SVN.pm Git/SVN/Fetcher.pm Git/SVN/Ra.pm; do
  rm -f "perl/blib/lib/$pm" && ln -s --relative "`pwd`/perl/$pm" "perl/blib/lib/$pm"
done

