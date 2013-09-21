#!git ruby

ENV['GIT_PAGER'] =

patch = ''

def usage
  puts <<EOF
usage: git request-pull [options] start url [end]

    -p                    show patch text as well

EOF
  exit 1
end

def abbr(ref)
  if (ref =~ %r{^refs/heads/(.*)} || ref =~ %r{^refs/(tags/.*)})
    return $1
  end
  return ref
end

# $head is the token given from the command line, and $tag_name, if
# exists, is the tag we are going to show the commit information for.
# If that tag exists at the remote and it points at the commit, use it.
# Otherwise, if a branch with the same name as $head exists at the remote
# and their values match, use that instead.
#
# Otherwise find a random ref that matches $headrev.

def get_ref(url, headref, headrev, tag_name)
  found = nil
  IO.popen(%[git ls-remote "#{url}"]) do |out|
    out.each do |l|
      sha1, ref, deref = l.scan(/^(\S+)\s+(\S+?)(\^\{\})?$/).first
      next unless sha1 == headrev
      found = abbr(ref)
      break if (deref && ref == "refs/tags/#{tag_name}")
      break if ref == headref
    end
  end
  return found
end

until ARGV.empty?
  case ARGV.first
  when '-p'
    patch = '-p'
  when '--'
    ARGV.shift
    break
  when /^-/
    usage
  else
    break
  end
  ARGV.shift
end

base = ARGV[0]
url = ARGV[1]
head = ARGV[2] || 'HEAD'
status = 0
branch_name = nil

headref = `git rev-parse -q --verify --symbolic-full-name "#{head}"`.chomp
headref = "" if headref == "HEAD"

branch_name = headref.gsub(%r{^refs/heads/}, '')
if branch_name == headref ||
  ! system(%[git config "branch.#{branch_name}.description" >/dev/null])
  branch_name = nil
end

tag_name = `git describe --exact "#{head}^0" 2>/dev/null`.chomp

usage unless base or url

baserev = `git rev-parse --verify --quiet "#{base}"^0`.chomp
die "Not a valid revision: #{base}" if baserev.empty?

headrev = `git rev-parse --verify --quiet "#{head}"^0`.chomp
die "Not a valid revision: #{head}" if headrev.empty?

merge_base = `git merge-base #{baserev} #{headrev}`.chomp
die "No commits in common between #{base} and #{head}" unless $?.success?

ref = get_ref(url, headref, headrev, tag_name) || ''
url = `git ls-remote --get-url "#{url}"`.chomp

begin
  run(%[git show -s --format='The following changes since commit %H:

  %s (%ci)

are available in the git repository at:
' #{merge_base}])
  puts "  #{url}" + (ref.empty? ? "" : " #{ref}")
  run(%[git show -s --format='
for you to fetch changes up to %H:

  %s (%ci)

----------------------------------------------------------------' #{headrev}])

  if branch_name
    puts "(from the branch description for #{branch_name} local branch)"
    puts
    run(%[git config "branch.#{branch_name}.description"])
  end

  if not tag_name.empty?
    if ref.empty? || ref != "tags/#{tag_name}"
      $stderr.puts "warn: You locally have #{tag_name} but it does not (yet)"
      $stderr.puts "warn: appear to be at #{url}"
      $stderr.puts "warn: Do you want to push it there, perhaps?"
    end
    run(%[git cat-file tag "#{tag_name}" | sed -n -e '1,/^$/d' -e '/^-----BEGIN PGP /q' -e p])
    puts
  end

  if branch_name || ! tag_name.empty?
    puts "----------------------------------------------------------------"
  end

  run(%[git shortlog ^#{baserev} #{headrev}])
  run(%[git diff -M --stat --summary #{patch} #{merge_base}..#{headrev}])

  if ref.empty?
    short_headref = `git rev-parse -q --verify --symbolic-full-name --abbrev-ref "#{head}"`.chomp
    $stderr.puts "warn: No branch of #{url} is at:"
    run("git show -s --format='warn:   %h: %s' #{headrev} >&2")
    $stderr.puts "warn: Are you sure you pushed '#{short_headref}' there?"
    status = 1
  end
rescue CommandError
  status = 1
end

exit status
