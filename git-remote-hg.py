#!/usr/bin/env python

import sys
import os
sys.path.insert(0, os.getenv("GITPYTHONLIB","."))

from git_remote_helpers.util import die, debug, warn
from git_remote_helpers.hg.hg import GitHg
from git_remote_helpers.hg.export import GitExporter
from git_remote_helpers.hg.non_local import NonLocalHg

def get_repo(alias, url):
    """Returns a hg.repository object initialized for usage.
    """

    try:
        from mercurial import hg, ui
    except ImportError:
        die("Mercurial python libraries not installed")

    ui = ui.ui()
    source, revs = hg.parseurl(ui.expandpath(url), ['tip'])
    repo = hg.repository(ui, source)

    prefix = 'refs/hg/%s/' % alias
    debug("prefix: '%s'", prefix)

    repo.hg = hg
    repo.gitdir = ""
    repo.alias = alias
    repo.prefix = prefix
    repo.revs = revs

    repo.git_hg = GitHg(warn)
    exporter = GitExporter(repo.git_hg, repo, 'hg.marks', 'git.marks', prefix)
    non_local = NonLocalHg(repo, alias)

    repo.exporter = exporter
    repo.non_local = non_local

    return repo


def local_repo(repo, path):
    """Returns a hg.repository object initalized for usage.
    """

    local = repo.hg.repository(repo.ui, path)

    local.git_hg = repo.git_hg
    local.non_local = None
    local.hg = repo.hg
    local.gitdir = repo.gitdir
    local.alias = repo.alias
    local.prefix = repo.prefix
    local.revs = repo.revs
    local.exporter = GitExporter(repo.git_hg, local, 'hg.marks', 'git.marks', repo.prefix)

    return local


def do_capabilities(repo, args):
    """Prints the supported capabilities.
    """

    print "import"
    print "gitdir"
    print "refspec refs/heads/*:%s*" % repo.prefix

    print # end capabilities


def do_list(repo, args):
    """Lists all known references.
    """

    for ref in repo.revs:
        debug("? refs/heads/%s", ref)
        print "? refs/heads/%s" % ref

    debug("@refs/heads/tip HEAD")
    print "@refs/heads/tip HEAD"

    print # end list


def get_branch(ref):
    if not ref.startswith("refs/heads/"):
        die("Ref should start with 'refs/heads/'")

    splitref = ref.split('/')

    if len(splitref) != 3:
        die("Cannot parse ref, need 3 slashes")

    branch = splitref[2]

    return branch


def update_local_repo(repo):
    """
    """

    if repo.local():
        return repo

    path = repo.non_local.clone(repo.gitdir)
    repo.non_local.update(repo.gitdir)
    repo = local_repo(repo, path)
    return repo


def do_import(repo, args):
    """
    """

    if len(args) != 1:
        die("Import needs exactly one ref")

    if not repo.gitdir:
        die("Need gitdir to import")

    if args[0] == 'HEAD':
        branch = 'tip'
    else:
        branch = get_branch(args[0])

    repo = update_local_repo(repo)
    repo.exporter.setup(True, repo.gitdir, True, True)

    repo.exporter.export_repo()
    repo.exporter.export_branch('tip', 'tip')

    repo.exporter.write_marks(repo.gitdir)

def do_gitdir(repo, args):
    """
    """

    if not args:
        die("gitdir needs an argument")

    repo.gitdir = ' '.join(args)


COMMANDS = {
    'capabilities': do_capabilities,
    'list': do_list,
    'import': do_import,
    'gitdir': do_gitdir,
}


def sanitize(value):
    if value.startswith('hg::'):
        value = value[4:]

    return value


def read_one_line(repo):
    line = sys.stdin.readline()

    cmdline = line.strip().split()

    if not cmdline:
        return False # Blank line means we're about to quit

    cmd = cmdline.pop(0)
    debug("Got command '%s' with args '%s'", cmd, ' '.join(cmdline))

    if cmd not in COMMANDS:
        die("Unknown command, %s", cmd)

    func = COMMANDS[cmd]
    func(repo, cmdline)
    sys.stdout.flush()

    return True


def main(args):
    if len(args) != 3:
        die("Expecting exactly three arguments.")
        sys.exit(1)

    if os.getenv("GIT_DEBUG_HG"):
        import git_remote_helpers.util
        git_remote_helpers.util.DEBUG = True

    alias = sanitize(args[1])
    url = sanitize(args[2])

    if not alias.isalnum():
        warn("non-alnum alias '%s'", alias)
        alias = "tmp"

    args[1] = alias
    args[2] = url

    repo = get_repo(alias, url)

    debug("Got arguments %s", args[1:])

    more = True

    while (more):
        more = read_one_line(repo)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
