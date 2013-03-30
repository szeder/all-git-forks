desc "build git with no optimisations on osx"
task :build do
  sh "NO_GETTEXT=1 CFLAGS='-g -Wall -O0' make"
end

task default: :build
