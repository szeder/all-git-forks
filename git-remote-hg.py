#!/usr/bin/env python

import hashlib
import sys
import os
sys.path.insert(0, os.getenv("GITPYTHONLIB","."))

from git_remote_helpers.util import die, debug, warn
from git_remote_helpers.hg import util
from git_remote_helpers.hg.hg import GitHg
from git_remote_helpers.hg.exporter import GitExporter
from git_remote_helpers.hg.importer import GitImporter
from git_remote_helpers.hg.non_local import NonLocalHg


def get_repo(alias, url):
    """Returns a hg.repository object initialized for usage.
    """

    try:
        from mercurial import hg, ui
    except ImportError:
        die("Mercurial python libraries not installed")

    remote = False

    if url.startswith("remote://"):
        remote = True
        url = "file://%s" % url[9:]

    ui = ui.ui()
    source, revs, checkout = util.parseurl(ui.expandpath(url), ['default'])
    repo = hg.repository(ui, source)
    if repo.capable('branchmap'):
        revs += repo.branchmap().keys()
        revs = set(revs)

    hasher = hashlib.sha1()
    hasher.update(repo.path)
    repo.hash = hasher.hexdigest()

    repo.get_base_path = lambda base: os.path.join(
        base, 'info', 'fast-import', repo.hash)

    prefix = 'refs/hg/%s/' % alias
    debug("prefix: '%s'", prefix)

    repo.hg = hg
    repo.gitdir = ""
    repo.alias = alias
    repo.prefix = prefix
    repo.revs = revs

    repo.git_hg = GitHg(warn)
    repo.exporter = GitExporter(repo)
    repo.importer = GitImporter(repo)
    repo.non_local = NonLocalHg(repo)

    repo.is_local = not remote and repo.local()

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
    local.hash = repo.hash
    local.get_base_path = repo.get_base_path
    local.exporter = GitExporter(local)
    local.importer = GitImporter(local)
    local.is_local = repo.is_local

    return local


def do_capabilities(repo, args):
    """Prints the supported capabilities.
    """

    print "import"
    print "export"
    print "*gitdir"

    sys.stdout.flush()
    if not read_one_line(repo):
        die("Expected gitdir, got empty line")

    print "*refspec refs/heads/*:%s*" % repo.prefix

    dirname = repo.get_base_path(repo.gitdir)

    if not os.path.exists(dirname):
        os.makedirs(dirname)

    path = os.path.join(dirname, 'git.marks')

    print "*export-marks %s" % path
    if os.path.exists(path):
        print "*import-marks %s" % path

    print # end capabilities


def do_list(repo, args):
    """Lists all known references.
    """

    for ref in repo.revs:
        debug("? refs/heads/%s", ref)
        print "? refs/heads/%s" % ref

    debug("@refs/heads/default HEAD")
    print "@refs/heads/default HEAD"

    print # end list


def update_local_repo(repo):
    """Updates (or clones) a local repo.
    """

    if repo.is_local:
        return repo

    path = repo.non_local.clone(repo.gitdir)
    repo.non_local.update(repo.gitdir)
    repo = local_repo(repo, path)
    return repo


def do_import(repo, args):
    """Exports a fast-import stream from hg for git to import.
    """

    if args:
        die("Import expects its ref seperately")

    if not repo.gitdir:
        die("Need gitdir to import")

    refs = []

    while True:
        line = sys.stdin.readline()
        if line == '\n':
            break
        refs.append(line.strip())

    repo = update_local_repo(repo)
    repo.exporter.setup(True, repo.gitdir, True, True)

    repo.exporter.export_repo()
    repo.exporter.export_branch('default', 'default')

    repo.exporter.write_marks(repo.gitdir)

    print "done"


def do_export(repo, args):
    """Imports a fast-import stream from git to hg.
    """

    if not repo.gitdir:
        die("Need gitdir to export")

    local_repo = update_local_repo(repo)
    local_repo.importer.do_import(local_repo.gitdir)

    if not repo.is_local:
        repo.non_local.push(repo.gitdir)

    print "ok refs/heads/default"
    print


def do_gitdir(repo, args):
    """Stores the location of the gitdir.
    """

    if not args:
        die("gitdir needs an argument")

    repo.gitdir = ' '.join(args)


COMMANDS = {
    'capabilities': do_capabilities,
    'list': do_list,
    'import': do_import,
    'export': do_export,
    'gitdir': do_gitdir,
}


def sanitize(value):
    """Cleans up the url.
    """

    if value.startswith('hg::'):
        value = value[4:]

    return value


def read_one_line(repo):
    """Reads and processes one command.
    """

    line = sys.stdin.readline()

    cmdline = line

    if not cmdline:
        warn("Unexpected EOF")
        return False

    cmdline = cmdline.strip().split()
    if not cmdline:
        debug("Got empty line, quitting")
        # Blank line means we're about to quit
        return False

    cmd = cmdline.pop(0)
    debug("Got command '%s' with args '%s'", cmd, ' '.join(cmdline))

    if cmd not in COMMANDS:
        die("Unknown command, %s", cmd)

    func = COMMANDS[cmd]
    func(repo, cmdline)

    try:
        sys.stdout.flush()
    except IOError, e:
        warn("while flushing '%s' with args '%s'", str(cmd), str(cmdline))
        die(str(e))

    return True


def main(args):
    """Starts a new remote helper for the specified repository.
    """

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
