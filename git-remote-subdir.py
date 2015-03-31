#!/usr/bin/env python

import logging
import os
from subprocess import (
    check_call, check_output, CalledProcessError, Popen, PIPE)
import sys
from tempfile import TemporaryDirectory


def check_output_lines(*popenargs, **kwargs):
    """Like subprocess.check_output(), but yield output lines, lazily."""
    p = Popen(*popenargs, stdout=PIPE, **kwargs)
    for line in p.stdout:
        yield line
    retcode = p.wait()
    if retcode:
        raise CalledProcessError(retcode, p.args)


class Git(object):
    """Wrap various Git commands."""
    def __init__(self, git_dir):
        self.git_dir = git_dir
        self.log = logging.getLogger(self.__class__.__name__)

    def args(self, *args):
        return ['git', '--git-dir=' + str(self.git_dir)] + list(args)

    def config_get_one(self, key):
        self.log.debug('config_get_one({})'.format(key))
        try:
            return check_output(self.args(
                'config', '--get', key)).decode('utf-8').rstrip()
        except CalledProcessError:
            return None

    def config_get_all(self, key):
        self.log.debug('config_get_all({})'.format(key))
        try:
            for line in check_output_lines(self.args('config', '--get', key)):
                yield line.decode('utf-8').rstrip()
        except CalledProcessError:
            pass

    def ls_remote(self, remote_or_url):
        self.log.debug('ls_remote({})'.format(remote_or_url))
        for line in check_output_lines(self.args('ls-remote', remote_or_url)):
            self.log.debug('Line: {!r}'.format(line))
            value, refname = line.decode('utf-8').rstrip().split('\t', 1)
            yield (value, refname)

    def fetch(self, remote_or_url, *refspecs, prune=True):
        """Perform a fetch from the given 'remote_or_url'."""
        self.log.debug('fetch({}, {})'.format(
            remote_or_url, ', '.join(refspecs)))
        args = []
        if prune:
            args.append('--prune')
        args.append(remote_or_url)
        args.extend(refspecs)
        check_call(self.args('fetch', *args))

    def refs(self, pattern):
        """Yield (refname, value) pairs for each refname matching 'pattern'."""
        self.log.debug('refs({})'.format(pattern))
        args = self.args(
            'for-each-ref', '--format=%(objectname)%00%(refname)', pattern)
        for line in check_output_lines(args):
            self.log.debug('Line: {!r}'.format(line))
            value, refname = line.decode('utf-8').rstrip().split('\0')
            yield (refname, value)

    def rev_parse(self, spec):
        args = self.args('rev-parse', '--verify', spec)
        return check_output(args).decode('ascii').rstrip()

    def object_type(self, sha1):
        self.log.debug('object_type({})'.format(sha1))
        args = self.args('cat-file', '-t', sha1)
        try:
            return check_output(args).decode('ascii').rstrip()
        except CalledProcessError:
            return None

    def commit_sha1s(self, commit, max_count=None):
        """Yield (commit, tree, parents...) tuples from a history walk.

        Start walking the commit history at the given 'commit', and for each
        commit enountered, yield its commit SHA1, its tree SHA1, and its (zero
        or more) parent commit SHA1s.
        """
        self.log.debug('commit_sha1s({}, {})'.format(commit, max_count))
        args = self.args('log', '--format=%H %T %P', commit)
        if max_count is not None:
            args.append('--max-count={}'.format(max_count))
        for line in check_output_lines(args):
            self.log.debug('Line: {!r}'.format(line))
            yield line.decode('ascii').rstrip().split(' ')

    def rewrite_commit(self, commit, replace_tree):
        self.log.debug('rewrite_commit({}, {})'.format(commit, replace_tree))
        raw_object = check_output(self.args('cat-file', 'commit', commit))
        tree, tail = raw_object.split(b'\n', 1)
        assert tree.startswith(b'tree ')
        assert tail.startswith(b'parent ') or tail.startswith(b'author ')
        args = self.args(
            'hash-object', '-t', 'commit', '-w', '--no-filters', '--stdin')
        proc = Popen(args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        proc.stdin.write(b'tree ' + replace_tree.encode('ascii') + b'\n')
        output, errors = proc.communicate(tail)
        if proc.returncode:
            raise CalledProcessError(
                'Failed to rewrite commit {}: {}'.format(commit, errors))
        new_commit = output.decode('ascii').rstrip()
        assert len(new_commit) == 40
        return new_commit

    def read_tree(self, tree, index=None, prefix=None):
        args = self.args('read-tree')
        env = os.environ.copy()
        if index is not None:
            env['GIT_INDEX_FILE'] = index
        if prefix is not None:
            args.append('--prefix={}'.format(prefix))
        args.append(tree)
        check_call(args, env=env)

    def write_tree(self, index=None):
        env = os.environ.copy()
        if index is not None:
            env['GIT_INDEX_FILE'] = index
        args = self.args('write-tree')
        return check_output(args, env=env).decode('ascii').rstrip()


class RefSpec(object):
    @classmethod
    def parse(cls, refspec):
        """Parse a refspec string.

        Parse a string on the form '+refs/heads/*:refs/remotes/foo/*' into the
        equivalent RefSpec instance.
        """
        force = refspec.startswith('+')
        if force:
            refspec = refspec[1:]
        l, r = refspec.split(':')
        if not (l.endswith('*') and r.endswith('*')):
            raise ValueError("Invalid refspec: {}".format(refspec))
        return cls(l[:-1], r[:-1], force)

    def __init__(self, left_prefix, right_prefix, force):
        self.left_prefix = left_prefix
        self.right_prefix = right_prefix
        self.force = force

    def __str__(self):
        return '{}{}*:{}*'.format(
            '+' if self.force else '',
            self.left_prefix,
            self.right_prefix)

    def ltr(self, left_ref):
        """Map the given ref from the left to the right side of this refspec.

        Return None iff left_ref does not match the left side.
        """
        if left_ref.startswith(self.left_prefix):
            return self.right_prefix + left_ref[len(self.left_prefix):]
        return None

    def rtl(self, right_ref):
        """Map the given ref from the right to the left side of this refspec.

        Return None iff right_ref does not match the right side.
        """
        if right_ref.startswith(self.right_prefix):
            return self.left_prefix + right_ref[len(self.right_prefix):]
        return None

    def with_left(self, left_prefix, force=None):
        """Return a new RefSpec with the given left_prefix."""
        if force is None:
            force = self.force
        return self.__class__(left_prefix, self.right_prefix, force)

    def with_right(self, right_prefix, force=None):
        """Return a new RefSpec with the given right_prefix."""
        if force is None:
            force = self.force
        return self.__class__(self.left_prefix, right_prefix, force)


class DirSpec(object):
    @classmethod
    def parse(cls, git, dirspec):
        """Parse a dirspec string.

        Parse a string on the form 'foo/bar:blarg' into the equivalent DirSpec
        instance.
        """
        l, r = dirspec.split(':')
        if l.startswith('/') or r.startswith('/'):
            raise ValueError("Invalid dirspec: {}".format(dirspec))
        return cls(git, l.rstrip('/'), r.rstrip('/'))

    def __init__(self, git, left_dir, right_dir):
        self.log = logging.getLogger(self.__class__.__name__)
        self.log.debug(
            'Constructing with {!r} -> {!r}'.format(left_dir, right_dir))

        self.git = git
        self.left_dir = left_dir
        self.right_dir = right_dir

    def __str__(self):
        return '{}:{}'.format(self.left_dir, self.right_dir)

    def _ltr_tree(self, tree_sha1):
        """Map the given tree from the left to the right side of this dirspec.

        Return the sha1 of result tree.
        """
        assert self.git.object_type(tree_sha1) == 'tree'
        old_tree_sha1 = tree_sha1

        if self.left_dir:  # extract left_dir from within tree_sha1
            tree_sha1 = self.git.rev_parse(tree_sha1 + ':' + self.left_dir)
        if self.right_dir:  # create tree with tree_sha1 located at right_dir
            with TemporaryDirectory() as tmp_dir:
                tmp_index = os.path.join(tmp_dir, 'index')
                self.git.read_tree(
                    tree_sha1, index=tmp_index, prefix=self.right_dir)
                tree_sha1 = self.git.write_tree(index=tmp_index)

        self.log.debug('Mapped tree {} -> {}'.format(old_tree_sha1, tree_sha1))
        return tree_sha1

    def _ltr_commit(self, commit_sha1):
        """Map the given commit from the left to the right side.

        Return the sha1 of the result commit.
        """
        # Need to rewrite the tree and parents of the given commit object
        commit, tree, *parents = list(
            self.git.commit_sha1s(commit_sha1, max_count=1))[0]
        assert commit == commit_sha1, '{!r} != {!r}!'.format(commit, commit_sha1)
        new_tree = self._ltr_tree(tree)
        if parents:
            raise NotImplementedError("Don't know how to map parent objects")
        new_commit = self.git.rewrite_commit(commit, new_tree)
        self.log.debug('Mapped commit {} -> {}'.format(commit, new_commit))
        return new_commit

    def _ltr_tag(self, tag_sha1):
        """Map the given tag object from the left to the right side.

        Return the sha1 of the result tag.
        """
        raise NotImplementedError("Don't know how to map tag objects")

    def ltr(self, sha1):
        """Map the given object from left to right side of this dirspec.

        Return the sha1 of the resulting object.
        """
        handlers = {
            'blob': lambda blob_sha1: blob_sha1,
            'tree': self._ltr_tree,
            'commit': self._ltr_commit,
            'tag': self._ltr_tag,
        }
        new_sha1 = handlers[self.git.object_type(sha1)](sha1)
        self.log.debug('Mapped object {} -> {}'.format(sha1, new_sha1))
        return new_sha1


class RemoteSubdirHelper(object):

    # Where to store remote-tracking refs before mapping through dirspec.
    UnmappedRefPrefix = 'refs/remote-subdir/{0}/'

    # Where to store remote-tracking refs after mapping through dirspec
    MappedRefPrefix = 'refs/remotes/{0}/'

    def __init__(self, git_dir, remote_name):
        self.log = logging.getLogger(self.__class__.__name__)
        self.log.debug(
            'Constructing with (git_dir={!r}, remote_name={!r})'.format(
                git_dir, remote_name))

        self.git = Git(git_dir)
        self.remote = remote_name
        self.url = self.git.config_get_one('remote.{}.url'.format(self.remote))
        self.dirspec = DirSpec.parse(self.git, self.git.config_get_one(
            'remote.{}.dirspec'.format(self.remote)))  # TODO: More dirspecs?
        self.refspecs = [RefSpec.parse(s) for s in self.git.config_get_all(
            'remote.{}.fetch'.format(self.remote))]

        if not (self.url and self.dirspec and self.refspecs):
            raise ValueError(
                'Missing one or more of remote.{}.url/dirspec/fetch'.format(
                    self.remote))
        if self.url.startswith('subdir:'):  # TODO: Nested subdir-remotes?
            raise ValueError(
                'remote.{0}.url cannot start with "subdir:". Instead '
                'configure remote.{0}.vcs = subdir'.format(self.remote))

        self.unmapped_ref_prefix = self.UnmappedRefPrefix.format(self.remote)
        self.mapped_ref_prefix = self.MappedRefPrefix.format(self.remote)

        # Split each refspec remote_ref_prefix*:mapped_ref_prefix* in two:
        #  - remote_ref_prefix*:unmapped_ref_prefix*
        #  - unmapped_ref_prefix*:mapped_ref_prefix*
        self.fetchspecs, self.mapspecs = zip(*[
            self.split_refspec(refspec) for refspec in self.refspecs])

        self.log.debug("Will fetch from {}".format(self.url))
        for phase1, phase2 in zip(self.fetchspecs, self.mapspecs):
            self.log.debug("Will fetch refs {} -> {}".format(phase1, phase2))
        self.log.debug("Will map dirs {}".format(self.dirspec))

    def split_refspec(self, refspec):
        remove = self.mapped_ref_prefix
        insert = self.unmapped_ref_prefix
        if not refspec.right_prefix.startswith(remove):
            raise ValueError((
                'Cannot work with refspec {}. The right side does not start '
                'with {}!').format(refspec, remove))
        updated = insert + refspec.right_prefix[len(remove):]
        return (
            refspec.with_right(updated, force=True),
            refspec.with_left(updated))

    def do_capabilities(self, f):
        f.write('*fetch\n')
#        f.write('check-connectivity\n')
#        f.write('option\n')
        f.write('\n')

#    def do_option(self, f, name, value):
#        supported = {('followtags', 'true')}
#        f.write('ok\n' if (name, value) in supported else 'unsupported\n')

    def do_list(self, f):
        # Here, we'd like run git ls-remote against remote repo, and for each
        # (unmapped-value, remote-ref) returned, we'd like to do the following:
        #  - If we've already seen the unmapped-value, then map it through
        #    our dirspec to get the mapped-value to return to our caller as
        #    the first argument.
        #  - Otherwise, we don't (yet) have a mapping for unmapped-value, so
        #    we instead return a '?' as the first argument.
        #  - The remote-ref is returned to our caller as the second
        #    argument.
        #  - If we have seen remote-ref before, and its unmapped-value is equal
        #    to refs/remote-subdir/remote-name/remote-ref, then we should
        #    append 'unchanged' as a last argument.
        #  - Return the arguments (separated by a space) to our caller.
        #
        # HOWEVER, it seems that transport-helper.c currently does not handle
        # unknown ref values correctly: When returning '?' as the first
        # argument, transport-helper will follow up with a fetch request like:
        #   fetch 0000000000000000000000000000000000000000 refs/heads/foo
        # This doesn't really cause any problems for us (we can happily ignore
        # the null sha1 and simply fetch the the real value of the ref along
        # with the required objects from the remote. But we have no way of
        # communicating the _real_ value of the ref back to transport-helper.c.
        # Instead, the fetch machinery move on to validating the fetched data,
        # which fails hard when it tries to look up the incorrect null sha1.
        #
        # SO INSTEAD, we're forced to never return '?' from a list command, but
        # must instead perform the entire fetch _now_, so that the correct sha1
        # can be returned for each ref.

        # Here is how the fetch is performed:
        #  1. Record the current values of all refs in the mapped ref space
        #  2. Fetch remote refs into the unmapped ref space.
        #  3. For each fetched ref (i.e. each ref in the unmapped ref space):
        #     - Perform the dirspec mapping on the ref value (and recursively
        #       on all relevant objects reachable from the ref value), to
        #       determine the corresponding mapped ref value for this entry.
        #     - Perform the reverse fetch refspec mapping to determine the
        #       remote ref name that should be provided in the returned entry.
        #     - Compare the new mapped ref value against its previous value
        #       recorded in #1 to see if we should append 'unchanged' to the
        #       returned entry.
        #  4. That's it. The surrounding fetch machinery will automatically
        #     take care of updating the refs in the mapped ref space.

        old_refs = dict(self.git.refs(self.mapped_ref_prefix))

        self.git.fetch(
            self.url, *[str(r) for r in self.fetchspecs], prune=True)

        for refname, value in self.git.refs(self.unmapped_ref_prefix):
            mapped_value = self.dirspec.ltr(value)
            remote_refname = self.fetchspecs[0].rtl(refname)
            mapped_refname = self.mapspecs[0].ltr(refname)
            unchanged = old_refs.get(mapped_refname) == mapped_value
            suffix = ' unchanged' if unchanged else ''
            f.write('{} {}{}\n'.format(mapped_value, remote_refname, suffix))
        f.write('\n')

    def do_fetch(self, f, sha1, name):
        raise RuntimeError(
            "Should not get here, as fetch was already done in do_list()...")
#        out_f.write('\n')

    def process_commands(self, in_f, out_f):
        while True:
            cmdline = in_f.readline()
            self.log.debug('---')
            self.log.debug('Command line: {}'.format(cmdline.rstrip()))
            if cmdline in ['', '\n']:  # EOF or end of command stream
                break
            args = cmdline.rstrip().split()
            getattr(self, 'do_' + args[0])(out_f, *args[1:])
            out_f.flush()


def main(remote_or_url, url=None):
    logging.basicConfig(stream=sys.stderr, level='DEBUG')
    logging.debug('{} run from {}'.format(sys.argv[0], os.getcwd()))

    # We only accept a proper git remote name as the first argument, as we
    # need to look up some configured settings that cannot be passed on the
    # command line. RemoteSubdirHelper will raise ValueError when
    # remote_or_url does not name a configure git remote.
    helper = RemoteSubdirHelper(os.environ['GIT_DIR'], remote_or_url)
    helper.process_commands(sys.stdin, sys.stdout)


if __name__ == '__main__':
    main(*sys.argv[1:])
