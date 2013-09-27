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

def read_branch_desc(name)
  git_config() do |key, value|
    return value if key == "branch.#{name}.description"
  end
  return nil
end

def describe(rev)
  for_each_ref() do |name, sha1, flags|
    next unless name.start_with?('refs/tags/')
    next unless peel_ref(name) == get_sha1(rev)
    return name[10..-1]
  end
  return nil
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
branch_name = branch_desc = nil

usage unless base or url

_, _, headref = dwim_ref(head)

if headref.start_with?('refs/heads')
  branch_name = headref[11..-1]
  branch_desc = read_branch_desc(branch_name)
  branch_name = nil if not branch_desc
end

tag_name = describe(head)

baserev = get_sha1("#{base}^0")
die "Not a valid revision: #{base}" unless baserev

headrev = get_sha1("#{head}^0")
die "Not a valid revision: #{head}" unless headrev

baserev = Git::Commit.get(baserev)
headrev = Git::Commit.get(headrev)

merge_bases = get_merge_bases([baserev, headrev], 0);
die "No commits in common between #{base} and #{head}" unless merge_bases

merge_base = sha1_to_hex(merge_bases.first.sha1)

ref = get_ref(url, headref != "HEAD" ? headref : nil, headrev.to_s, tag_name)
url = `git ls-remote --get-url "#{url}"`.chomp

begin
  run(%[git show -s --format='The following changes since commit %H:

  %s (%ci)

are available in the git repository at:
' #{merge_base}])
  puts "  #{url}" + (ref ? " #{ref}" : "")
  run(%[git show -s --format='
for you to fetch changes up to %H:

  %s (%ci)

----------------------------------------------------------------' #{headrev}])

  if branch_name
    puts "(from the branch description for #{branch_name} local branch)"
    puts
    puts branch_desc
  end

  if tag_name
    if ref != "tags/#{tag_name}"
      $stderr.puts "warn: You locally have #{tag_name} but it does not (yet)"
      $stderr.puts "warn: appear to be at #{url}"
      $stderr.puts "warn: Do you want to push it there, perhaps?"
    end
    run(%[git cat-file tag "#{tag_name}" | sed -n -e '1,/^$/d' -e '/^-----BEGIN PGP /q' -e p])
    puts
  end

  if branch_name || tag_name
    puts "----------------------------------------------------------------"
  end

  run(%[git shortlog ^#{base} #{head}])
  run(%[git diff -M --stat --summary #{patch} ^#{merge_base} #{head}])

  if ! ref
    $stderr.puts "warn: No branch of #{url} is at:"
    run("git show -s --format='warn:   %h: %s' #{head} >&2")
    $stderr.puts "warn: Are you sure you pushed '#{abbr(headref)}' there?"
    status = 1
  end
rescue CommandError
  status = 1
end

exit status
