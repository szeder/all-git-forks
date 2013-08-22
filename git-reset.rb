$quiet = false
$patch_mode = false

def die(arg)
  $stderr.puts "fatal: " + arg
  exit 128
end

def warning(arg)
  $stderr.puts "warning: " + arg
end

def error(arg)
  $stderr.puts "fatal: " + arg
  return -1
end

def sha1_to_hex(sha1)
  sha1.unpack('H*').first
end

def git_path(*path)
  File.join(get_git_dir, *path)
end

class SimpleParser
  attr_writer :usage

  class Option
    attr_reader :short, :long, :values, :help

    def initialize(short, long, values, help, &block)
      @block = block
      @short = short
      @long = long
      @values = values
      @help = help
    end

    def call(v)
      @block.call(v)
    end
  end

  def initialize
    @list = {}
  end

  def on(*args, &block)
    short = args.shift if args.first.is_a?(String) or args.first == nil
    long = args.shift if args.first.is_a?(String)
    values = args.shift if args.first.is_a?(Array)
    help = args.shift if args.first.is_a?(String)
    opt = Option.new(short, long, values, help, &block)
    @list[short] = opt if short
    @list[long] = opt if long
  end

  def parse(args = ARGV)
    i = 0
    if args.member?('-h') or args.member?('--help')
      usage
      exit! 1
    end
    while cur = args[i] do
      if cur =~ /^(-.+?)(?:=(.*))?$/
        opt = @list[$1]
        if opt
          v = $2
          if not v
            if not opt.values
              extra = true
            else
              extra = !!opt.values.map(&:to_s).member?(args[i + 1])
            end
            extra = false
            v = extra ? args.delete_at(i + 1) : true
          end
          opt.call(v)
          args.delete_at(i)
          next
        end
      end
      i += 1
    end
  end

  def usage
    puts 'usage: %s' % @usage
    @list.values.uniq.each do |opt|
      s = '    '
      s << [opt.short, opt.long].compact.join(', ')
      s << '%*s%s' % [26 - s.size, '', opt.help] if opt.help
      puts s
    end
  end

end

def reflog_message(action, rev=nil)
  rla = ENV['GIT_REFLOG_ACTION']
  if rla
    '%s: %s' % [rla, action]
  elsif rev
    'reset: moving to %s' % rev
  else
    'reset: %s' % action
  end
end

def update_refs(rev, sha1)
  sha1_old_orig = get_sha1('ORIG_HEAD')
  old_orig = sha1_old_orig if sha1_old_orig
  sha1_orig = get_sha1('HEAD')
  if sha1_orig
    orig = sha1_orig
    msg = reflog_message('updating ORIG_HEAD')
    update_ref(msg, 'ORIG_HEAD', orig, old_orig, 0, MSG_ON_ERR)
  elsif old_orig
    delete_ref('ORIG_HEAD', old_orig, 0)
  end
  msg = reflog_message('updating HEAD', rev)
  return update_ref(msg, 'HEAD', sha1, orig, 0, MSG_ON_ERR)
end

def is_merge
  return test('e', git_path('MERGE_HEAD'))
end

def parse_args(args)
  rev = 'HEAD'
  if args[0]
    if args[0] == '--'
      args.shift
    elsif args[1] == '--'
      rev = args.shift
      args.shift
    elsif (!args[1] && get_sha1_committish(args[0])) || (args[1] && get_sha1_treeish(args[0]))
      verify_non_filename($prefix, args[0])
      rev = args.shift
    else
      verify_filename($prefix, args[0], 1)
    end
  end
  pathspec = args[0] ? get_pathspec($prefix, args) : []
  return [rev, pathspec]
end

def reset_index(sha1, reset_type)
  read_cache_unmerged
  opts = {}

  case reset_type
  when :merge, :keep
    opts[:update] = true
  when :hard
    opts[:update] = true
    opts[:reset] = true
  else
    opts[:reset] = true
  end

  opts[:verobse] = true unless $quiet

  if reset_type == :keep
    head_sha1 = get_sha1('HEAD')
    return error('You do not have a valid HEAD.') unless head_sha1
    opts[:head] = head_sha1
  end

  if unpack_trees(sha1, opts) != 0
    return -1
  end

  if reset_type == :hard || reset_type == :mixed
    tree = parse_tree_indirect(sha1);
    prime_cache_tree(nil, tree);
  end

  return 0
end

def print_new_head_line(commit)
  hex = find_unique_abbrev(commit.sha1, DEFAULT_ABBREV)
  print('HEAD is now at %s' % hex)
  _, body = commit.buffer.split("\n\n", 2)
  if body
    puts [' ', body.lines.first].join
  else
    puts
  end
end

def cmd(*args)
  args.shift

  reset_type = :none

  opts = SimpleParser.new
  opts.usage = 'git reset [--mixed | --soft | --hard | --merge | --keep] [-q] [<commit>]'

  opts.on('-q', '--quiet', '') do |v|
    $quiet = true
  end

  opts.on(nil, '--hard', 'reset HEAD, index and working tree') do |v|
    reset_type = :hard
  end

  opts.on(nil, '--soft', 'reset only HEAD') do |v|
    reset_type = :soft
  end

  opts.on(nil, '--mixed', 'reset HEAD and index') do |v|
    reset_type = :mixed
  end

  opts.on(nil, '--merge', '') do |v|
    reset_type = :merge
  end

  opts.on(nil, '--keep', '') do |v|
    reset_type = :keep
  end

  opts.on('-p', '--patch', '') do |v|
    $patch_mode = true
  end

  $prefix = setup_git_directory
  git_config

  opts.parse(args)
  rev, pathspec = parse_args(args)

  unborn = rev == 'HEAD' && !get_sha1('HEAD')
  if unborn
    sha1 = EMPTY_TREE_SHA1_BIN
  elsif pathspec.empty?
    sha1 = get_sha1_committish(rev)
    die("Failed to resolve '%s' as a valid revision." % rev) unless sha1
    commit = lookup_commit_reference(sha1)
    die("Could not parse object '%s'." % rev) unless commit
    sha1 = commit.sha1
  else
    sha1 = get_sha1_treeish(rev)
    die("Failed to resolve '%s' as a valid tree." % rev) unless sha1
    tree = parse_tree_indirect(sha1)
    die("Could not parse object '%s'.", rev) unless tree
    sha1 = tree.sha1
  end

  if $patch_mode
    args = []
    args << 'add--interactive'
    args << '--patch=reset'
    args << sha1_to_hex(sha1)
    args << '--'
    args += pathspec
    return run_command(args, RUN_GIT_CMD);
  end

  if not pathspec.empty?
    if reset_type == :mixed
      warning("--mixed with paths is deprecated; use 'git reset -- <paths>' instead.")
    elsif reset_type != :none
      die('Cannot do %s reset with paths.' % reset_type.to_s)
    end
  end

  reset_type = :mixed if reset_type == :none

  if reset_type != :soft && reset_type != :mixed
    setup_work_tree
  end

  if reset_type == :mixed && is_bare_repository
    die('%s reset is not allowed in a bare repository' % reset_type.to_s);
  end

  if reset_type == :soft || reset_type == :keep
    if is_merge || read_cache < 0 || unmerged_cache != 0
      die('Cannot do a %s reset in the middle of a merge.' % reset_type.to_s)
    end
  end

  if reset_type != :soft
    do_locked_index(1) do |f|
      if reset_type == :mixed
        r = read_from_tree(pathspec, sha1)
        return 1 if not r
        flags = $quiet ? REFRESH_QUIET : REFRESH_IN_PORCELAIN
        refresh_index(flags, nil, 'Unstaged changes after reset:')
      else
        err = reset_index(sha1, reset_type)
        err = reset_index(sha1, :mixed) if reset_type == :keep && err == 0
        die("Could not reset index file to revision '%s'." % rev) if err != 0
      end
      write_cache(f)
    end || die('Could not write new index file.')
  end

  status = 0
  if pathspec.empty? && !unborn
    status = update_refs(rev, sha1)
    if reset_type == :hard && status == 0 && !$quiet
      print_new_head_line(lookup_commit_reference(sha1))
    end
  end
  remove_branch_state if pathspec.empty?
  return status
end

exit cmd(*ARGV)
