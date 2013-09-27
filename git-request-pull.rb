#!git ruby

require 'date'

patch = nil

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
    return name.skip_prefix('refs/tags/')
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
# Otherwise find a random ref that matches $head_id.

def get_ref(transport, head_ref, head_id, tag_name)
  found = nil
  transport.get_remote_refs().each do |e|
    sha1 = e.old_sha1
    next unless sha1 == head_id
    ref, deref = e.name.scan(/^(\S+?)(\^\{\})?$/).first
    found = abbr(ref)
    break if (deref && ref == "refs/tags/#{tag_name}")
    break if ref == head_ref
  end
  return found
end

def parse_buffer(buffer)
  summary = []
  date = msg = nil
  header, body = buffer.split("\n\n", 2)
  header.each_line do |line|
    case line
    when /^committer ([^<>]+) <(\S+)> (.+)$/
      date = DateTime.strptime($3, '%s %z')
    end
  end
  body.each_line do |l|
    break if (l.strip.empty?)
    summary << l.chomp
  end
  summary = summary.join(' ')
  date = date.strftime('%F %T %z')
  return [summary, date]
end

def show_shortlog(base, head)
  rev = Git::RevInfo.setup(nil, ['^' + base, head], nil)
  shortlog(rev.to_a)
end

def show_diff(patch, base, head)
  rev = Git::RevInfo.setup(nil, ['^' + sha1_to_hex(base), sha1_to_hex(head)], nil)
  rev.diffopt.stat_width = -1
  rev.diffopt.stat_graph_width = -1
  rev.diffopt.output_format = patch ? DIFF_FORMAT_PATCH : DIFF_FORMAT_DIFFSTAT
  rev.diffopt.output_format |= DIFF_FORMAT_SUMMARY
  rev.diffopt.detect_rename = DIFF_DETECT_RENAME
  rev.diffopt.flags |= DIFF_OPT_RECURSIVE

  diff_tree_sha1(base, head, "", rev.diffopt)
  log_tree_diff_flush(rev)
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
branch_name = branch_desc = nil

usage unless base or url

_, _, head_ref = dwim_ref(head)

if head_ref.start_with?('refs/heads')
  branch_name = head_ref[11..-1]
  branch_desc = read_branch_desc(branch_name)
  branch_name = nil if not branch_desc
end

tag_name = describe(head)

base_id = get_sha1("#{base}^0")
die "Not a valid revision: #{base}" unless base_id

head_id = get_sha1("#{head}^0")
die "Not a valid revision: #{head}" unless head_id

base_commit = Git::Commit.get(base_id)
head_commit = Git::Commit.get(head_id)

merge_bases = get_merge_bases([base_commit, head_commit], 0);
die "No commits in common between #{base} and #{head}" unless merge_bases

merge_base_id = merge_bases.first.sha1
merge_base_commit = Git::Commit.get(merge_base_id)

remote = remote_get(url)
transport = transport_get(remote, nil)

ref = get_ref(transport, head_ref != "HEAD" ? head_ref : nil, head_id, tag_name)
url = remote.url.first

merge_base_summary, merge_base_date = parse_buffer(merge_base_commit.buffer)
head_summary, head_date = parse_buffer(head_commit.buffer)

puts "The following changes since commit %s:

  %s (%s)

are available in the git repository at:

" % [merge_base_commit, merge_base_summary, merge_base_date]
puts "  #{url}" + (ref ? " #{ref}" : "")
puts "
for you to fetch changes up to %s:

  %s (%s)

----------------------------------------------------------------
" % [head_commit, head_summary, head_date]

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
  buffer, _ = read_sha1_file(get_sha1(tag_name))
  puts buffer.scan(/(?:\n\n)(.+)(?:-----BEGIN PGP )?/m).first
  puts
end

if branch_name || tag_name
  puts "----------------------------------------------------------------"
end

show_shortlog(base, head)
show_diff(patch, merge_base_id, head_id)

if ! ref
  $stderr.puts "warn: No branch of #{url} is at:"
  $stderr.puts "warn:   %s: %s'" % [find_unique_abbrev(head_id, DEFAULT_ABBREV), head_summary]
  $stderr.puts "warn: Are you sure you pushed '#{abbr(head_ref)}' there?"
  exit 1
end
