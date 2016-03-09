#!/usr/bin/env python2.7

from __future__ import print_function

import argparse
import errno
import os
import re
import sys
import shlex
import shutil
import signal
import subprocess
import urlparse

from contextlib import contextmanager

# combine-pack uses SIGURG we should handle that here, ASAP
signal.signal(signal.SIGURG, signal.SIG_IGN)

PROGNAME = 'git-manage-config'

def say(msg):
  sys.stderr.write("{0}: {1}\n".format(PROGNAME, msg))

def debug(msg):
  if 'GIT_TRACE' in os.environ:
    say(msg)

class Git(object):
  class GitResult(object):
    """encapsulates the stdout and stderr of a git command

    acts like a container of stdout.splitlines() but provides access to the underlying strings
    using the out and err properties
    """

    def __init__(self, out, err):
      self._out = out
      self._err = err
      self._lines = None

    @property
    def out(self):
      return self._out

    @property
    def err(self):
      return self._err

    def __getitem__(self, key):
      return self.lines[key]

    def __iter__(self):
      return iter(self.lines)

    def __len__(self):
      return len(self.lines)

    def __nonzero__(self):
      # rather than split lines, see if there was any output from the command in a cheaper way
      return bool(self.out)

    @property
    def lines(self):
      """returns a list of the lines of output split by line"""
      if not self._lines:
        self._lines = self.out.splitlines()
      return self._lines[:]


  class GitCommandFailure(subprocess.CalledProcessError):
    def __init__(self, returncode, cmd, out='', err=''):
      super(Git.GitCommandFailure, self).__init__(returncode, cmd)
      self.out = out
      self.err = err

    def __str__(self):
      a = [super(Git.GitCommandFailure, self).__str__()]
      a.append("-- STDOUT --")
      a.append(self.out)
      a.append("-- STDERR --")
      a.append(self.err)
      return "\n".join(a)


  @classmethod
  def git(cls, cmd_arg, git_bin='git', repo_dir=None, stdin=None):
    if isinstance(cmd_arg, str):
      return cls.gits(cmd_arg, git_bin=git_bin, repo_dir=repo_dir, stdin=stdin)
    elif isinstance(cmd_arg, list):
      return cls.git_cmd(cmd_arg, git_bin=git_bin, repo_dir=repo_dir, stdin=stdin)
    else:
      raise TypeError("cmd_arg must be str or list, received: {0!r}".format(cmd_arg))

  @classmethod
  def gits(cls, cmd_str, git_bin='git', repo_dir=None, stdin=None):
    return cls.git_cmd(shlex.split(cmd_str), git_bin=git_bin, repo_dir=repo_dir, stdin=stdin)

  @classmethod
  def git_popen(cls, args, git_bin='git', repo_dir=None, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.PIPE):
    cmd = [git_bin] + args

    return subprocess.Popen(
        cmd,
        stdin=stdin,
        stdout=stdout,
        stderr=stderr,
        cwd=repo_dir,
        bufsize=-1)


  @classmethod
  def git_cmd(cls, args, git_bin='git', repo_dir=None, stdin=None):
    p = cls.git_popen(args, git_bin=git_bin, repo_dir=repo_dir, stdin=stdin)

    out_str, err_str = p.communicate()
    rcode = p.wait()

    cls._trace_git_output(args, out_str, err_str)

    if rcode == 0:
      return cls.GitResult(out_str, err_str)
    else:
      raise cls.GitCommandFailure(rcode, args, out=out_str, err=err_str)


  GIT_TRACE_FMT = """
--< GIT_TRACE >--
command: {cmd!r}

STDOUT:
{stdout}

STDERR:
{stderr}

"""

  @classmethod
  def _trace_git_output(cls, cmd, out_str, err_str):
    if os.environ.get('TW_GIT_REPO_TRACE'):
      sys.stderr.write(cls.GIT_TRACE_FMT.format(cmd=cmd, stdout=out_str, stderr=err_str))


class ManagedConfig(Git):
  class ConfigException(Exception): pass

  class ExplicitlyDisabledException(ConfigException): pass

  class RemoteBranchMissingError(ConfigException):
    def __init__(self, branch_name):
      self.branch_name = branch_name

    def __str__(self):
      return "RemoteBranchMissingError: The branch {0} did not exist".format(self.branch_name)

  class LockfileExistsError(ConfigException):
    MESSAGE = """\
LockfileExistsError: lockfile {s.path} already exists. Another process is attempting
to update the repo settings, or a lockfile was left behind.

Ensure no other process is attempting to update the repo settings and remove the lockfile.
"""

    def __init__(self, path):
      self.path = path

    def __str__(self):
      return self.MESSAGE.format(s=self)

  class RemoteOriginNotDefinedException(Exception): pass

  WHITELISTED_REPOS = frozenset(['source'])   # this may include other repos as needed

  WHITELISTED_HOST_NAMES = frozenset([
    'kgit.twitter.biz',
    'git.twitter.biz',
    'git-ro-source.twitter.biz',
    ])

  REMOTE_NAME = 'origin'                      # the only option for now
  REMOTE_BRANCH = 'repo.d/master'
  REPO_D_DIR_NAME = 'repo.d'
  UPDATE_SCRIPT = os.path.join(REPO_D_DIR_NAME, 'update.sh')
  LOCK_PATH = REPO_D_DIR_NAME + '.lock'
  SHA1_RE = re.compile('\A[0-9a-f]{40}\Z')

  DEFAULT_REPOD_REF_NAME = os.path.join('remotes', REMOTE_NAME, REMOTE_BRANCH)

  # --------------------

  @classmethod
  def cmd_update(cls, **kw):
    cls(**kw).update()

  def __init__(self, force=False, auto=False, repo_d_ref=None):
    """repo_d_ref should be a fully qualified ref to unpack as the repo.d directory or None for the default"""
    self._toplevel_path = None
    self._force = force
    self._auto = auto
    self._current_remote_sha = None
    self._git_dir = None
    self._enabled_state = None
    self._repo_d_ref = repo_d_ref

  @property
  def is_force(self):
    return self._force

  @property
  def is_auto(self):
    return self._auto

  @property
  def is_explicit_ref(self):
    return bool(self._repo_d_ref)

  @property
  def toplevel_path(self):
    if self._toplevel_path is None:
      self._toplevel_path = self.git('rev-parse --show-toplevel')[0]
    return self._toplevel_path

  @property
  def git_dir(self):
    if self._git_dir is None:
      self._git_dir = os.path.abspath(self.git('rev-parse --git-dir')[0])
    return self._git_dir

  @contextmanager
  def pushd_toplevel_path(self):
    cwd = os.getcwd()
    os.chdir(self.toplevel_path)
    try:
      yield self.toplevel_path
    finally:
      os.chdir(cwd)

  @property
  def repo_d_path(self):
    return os.path.join(self.git_dir, self.REPO_D_DIR_NAME)

  @property
  def repo_d_lock_path(self):
    return os.path.join(self.git_dir, self.LOCK_PATH)

  @property
  def origin_url(self):
    try:
      return self.git('config --local remote.origin.url')[0]
    except self.GitCommandFailure:
      raise self.RemoteOriginNotDefinedException()

  @property
  def is_whitelisted_repo(self):
    """checks the whitelist after normalizing the origin_url"""

    # (jsimms): this is really not clearer than having a well-tested regexp

    try:
      origin_url = self.origin_url
    except self.RemoteOriginNotDefinedException:
      return False

    url = urlparse.urlparse(origin_url)

    if url.scheme != 'https':
      debug("insecure origin url: {0}".format(origin_url))
      return False

    # maybe it's jsimms@git.twitter.biz
    if '@' in url.netloc:
      _, host = url.netloc.split('@', 2)
    else:
      host = url.netloc

    if host not in self.WHITELISTED_HOST_NAMES:
      debug("Host %s is not in the whitelist" % (host,))
      return False

    # ok, now normalize the path to remove /ro
    path_parts = url.path.split('/')
    n_parts = len(path_parts)

    # yes i realize 'return boolean' is kinda lame

    if n_parts < 2 or n_parts > 3:    # we only should have ['', 'ro', 'something'] or ['', 'something']
      return False

    elif path_parts[0] != '':         # idk how this could even be false, but if it is something is hosed
      return False

    elif n_parts == 3 and path_parts[1] != 'ro':
      return False

    else:
      repo_name = path_parts[-1]

      # strip off any extensions (eg. '.git')
      if '.' in repo_name:
        repo_name = repo_name.split('.', 2)[0]

      return repo_name in self.WHITELISTED_REPOS

  CONFIG_ENABLED  = 'true'
  CONFIG_DISABLED = 'false'
  CONFIG_UNSET    = 'unset'

  @property
  def config_enabled_state(self):
    if self._enabled_state is None:
      try:
        self._enabled_state = self.git('config --get --bool manageconfig.enable')[0]
      except self.GitCommandFailure as e:
        if e.returncode == 1:   # the option was unset
          self._enabled_state = self.CONFIG_UNSET
        else:
          raise e
    return self._enabled_state

  @property
  def is_config_enabled(self):
    """has the user explicitly enabled this feature"""
    return self.config_enabled_state == self.CONFIG_ENABLED

  @property
  def is_config_disabled(self):
    """has the user explicitly disabled this feature"""
    return self.config_enabled_state == self.CONFIG_DISABLED

  @property
  def is_config_unset(self):
    """has the user not specified a state for this feature"""
    return self.config_enabled_state == self.CONFIG_UNSET

  @property
  def is_twconfig_enabled(self):
    debug("config state: %s" % self.config_enabled_state)

    if self.is_force or self.is_config_enabled or self.is_explicit_ref:
      debug('config was forced or enabled or explicit ref given')
      return True
    elif self.is_config_disabled:
      say("management has been explicitly disabled in git config: manageconfig.enabled was false")
      return False
    else:
      debug('is_whitelisted_repo: %r' % self.is_whitelisted_repo)
      return self.is_whitelisted_repo

  @property
  def repo_d_ref_name(self):
    if self._repo_d_ref:
      return self._repo_d_ref
    else:
      return self.DEFAULT_REPOD_REF_NAME

  @property
  def current_remote_head(self):
    """gets the SHA1 of the current repo.d/master on the remote branch or the explicitly given --use-ref argument"""
    if self._current_remote_sha:
      return self._current_remote_sha

    git_result = self.git(['rev-parse', '--revs-only', self.repo_d_ref_name])
    if git_result:
      sha = git_result[0]
      assert self.SHA1_RE.match(sha)
      self._current_remote_sha = sha
      debug("current_remote_head sha is: %s" % (sha,))
      return sha
    else:
      debug("remote config branch did not exist: %s" % self.repo_d_ref_name)
      return None

  def _safe_delete(self, path):
    try:
      os.unlink(path)
    except OSError as e:
      if e.errno != errno.ENOENT:
        raise e

  BUFSIZE = -1

  @contextmanager
  def lock(self, path):
    try:
      fd = os.open(path, os.O_EXCL|os.O_RDWR|os.O_CREAT, 0644)
    except OSError as e:
      if e.errno == errno.EEXIST:
        raise self.LockfileExistsError(path), None, sys.exc_info()[2]
      else:
        raise e

    fobj = os.fdopen(fd, 'w+b', self.BUFSIZE)

    try:
      yield fobj
    finally:
      fobj.close()
      self._safe_delete(path)

  @property
  def _extracted_repo_d_sha_path(self):
    return os.path.join(self.repo_d_path, 'SHA')

  @property
  def _extracted_repo_d_sha(self):
    """if it exists, reads the SHA we extracted the current .repo.d directory from

    returns None if the file doesn't exist
    """
    try:
      with open(self._extracted_repo_d_sha_path, 'rb') as fp:
        return fp.read().strip()
    except IOError as e:
      if e.errno == errno.ENOENT:
        return None
      else:
        raise e

  def _write_extracted_repo_d_sha(self):
    """after extraction, write the SHA file so we know to not extract unless the tip moved"""
    debug("writing %s" % (self._extracted_repo_d_sha_path,))
    with open(self._extracted_repo_d_sha_path, 'wb') as fp:
      print(self.current_remote_head, file=fp)

  @contextmanager
  def repo_d_lock(self):
    with self.lock(self.repo_d_lock_path):
      yield

  @contextmanager
  def repo_d_moved(self):
    """moves .repo.d to .repo.d.OLD and yields.

    if no exception is raised, removes *.OLD version
    """
    if os.path.exists(self.repo_d_path):
      old_path = self.repo_d_path + '.OLD'

      if os.path.islink(old_path) or os.path.isfile(old_path):  # cover our bases here
        os.unlink(old_path)
      elif os.path.isdir(old_path):
        shutil.rmtree(old_path, True)
      # it could be a fifo or something, but the chances of that are *really* slim

      shutil.move(self.repo_d_path, old_path)
      yield
      shutil.rmtree(old_path, ignore_errors=True)
    else:
      yield

  @property
  def _repo_d_up_to_date(self):
    return self._extracted_repo_d_sha == self.current_remote_head

  def expand_repo_d_from_remote(self):
    """expands the remote branch into .repo.d

    this method assumes we have chdir'd into the toplevel dir before running
    and that the appropriate locks are held
    """
    assert(os.path.realpath(os.getcwd()) == os.path.realpath(self.toplevel_path))

    archive_cmd = ['git', 'archive', '--format=tar', '--prefix=%s/' % (self.repo_d_path,), self.repo_d_ref_name]
    tar_cmd = ['tar', '-Pxf', '-']

    p1 = subprocess.Popen(archive_cmd, stdout=subprocess.PIPE, bufsize=-1)
    p2 = subprocess.Popen(tar_cmd, stdin=p1.stdout, bufsize=-1)
    p1.stdout.close()

    p2.communicate()
    p1.wait()
    p2.wait()

    if p1.returncode:
      raise subprocess.CalledProcessError(p1.returncode, archive_cmd)
    if p2.returncode:
      raise subprocess.CalledProcessError(p2.returncode, tar_cmd)

    self._write_extracted_repo_d_sha()

  def cleanup_toplevel_repo_d_path(self):
    """we moved repo.d under the git dir, clean up the old path if it exists"""
    shutil.rmtree(os.path.join(self.toplevel_path, '.repo.d'), ignore_errors=True)

  def run_update_sh(self):
    env = dict(os.environ)
    env['GIT_DIR'] = self.git_dir
    env['REPO_DOT_D'] = self.repo_d_path

    return subprocess.check_call(
        [os.path.join(self.repo_d_path, 'update.sh')],
        cwd=self.toplevel_path,
        env=env)

  def update(self):
    # if there's no remote head, we can't run
    if self.is_twconfig_enabled and self.current_remote_head:
      debug('hook enabled')
      with self.repo_d_lock(), self.pushd_toplevel_path():
        if not self._repo_d_up_to_date:
          with self.repo_d_moved():
            self.expand_repo_d_from_remote()
            self.run_update_sh()
        else:
          self.run_update_sh()
      self.cleanup_toplevel_repo_d_path()


USAGE = """
Allows centralized management of client-side git repo settings from a remote branch.

Currently this will only run without intervention if the remote origin matches the whitelist of:

  https://(\w+[@])?[k]?git\.twitter\.biz/(ro/)?source

Workflow is:
* client fetches changes
* git-fetch invokes `git-manage-config update --auto` before returning

* if one of the following conditions is met:
  - origin URL is in the whitelist
  - the user passes the --force option
  - the user explicitly enables the command using config
  and there is a remote branch 'refs/remotes/origin/repo.d/master'

* the following (equivalent) shell commands are invoked:

$ set -o noclobber; :> .repo.d.lock || exit 1; set +o noclobber
$ mv .repo.d .repo.d.OLD
$ git-archive --format=tar --prefix=.repo.d/ refs/remotes/origin/repo.d/master|tar -xf -
$ ./repo.d/update.sh
$ rm -rf .repo.d.OLD .repo.d.lock

There's a check to see if the SHA of the remote branch has changed since the last invocation.
You can override this check with the --force command.

"""

def main(*argv):
  @contextmanager
  def error_handler():
    try:
      yield
    except ManagedConfig.ConfigException as e:
      sys.stderr.write(str(e) + "\n")
      sys.exit(1)
    except KeyboardInterrupt:
      # don't show a stacktrace on ctrl-c
      sys.exit(1)

  parser = argparse.ArgumentParser(
      prog=PROGNAME,
      description=USAGE,
      formatter_class=argparse.RawDescriptionHelpFormatter)

  def update_cb(a):
    with error_handler():
      use_ref = a.use_ref[0] if a.use_ref else None
      ManagedConfig.cmd_update(force=a.force, auto=a.auto, repo_d_ref=use_ref)

  def help_cb(*a):
    parser.print_help()

  sub = parser.add_subparsers()

  help = sub.add_parser('help')
  help.set_defaults(func=help_cb)

  update = sub.add_parser('update')
  update.set_defaults(func=update_cb)

  group = update.add_mutually_exclusive_group()
  group.add_argument('--auto', action='store_true', default=False, help='use heuristics to see if we need to run')
  group.add_argument('--force', action='store_true', default=False, help='always run')

  group.add_argument('--use-ref',
      nargs=1,
      default=None,
      help='use this explicit ref as the repo.d/master branch')

  args = list(argv)

  if len(args) == 1:
    args.append('help')

  x = parser.parse_args(args[1:])
  x.func(x)

if __name__ == '__main__':
  main(*sys.argv)
