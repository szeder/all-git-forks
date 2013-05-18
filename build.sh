set -e
cd "`dirname "$0"`"

#rm -f git-svn

export CC="ccache gcc"
export PYTHON_PATH="/usr/bin/python2"
make CC="$CC" all

#ln -s git-svn.perl git-svn
for pm in Git/SVN.pm Git/SVN/Fetcher.pm Git/SVN/Ra.pm; do
  rm -f "perl/blib/lib/$pm" && ln -s --relative "`pwd`/perl/$pm" "perl/blib/lib/$pm"
done

