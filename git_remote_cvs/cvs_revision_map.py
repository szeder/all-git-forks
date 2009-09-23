#!/usr/bin/env python

"""Functionality for mapping CVS revisions to associated metainfo.

This modules provides the following main classes:

CVSRevisionMap - provides a mapping from CVS revisions to the
                 the following associated metainfo:
                 - Mode/permission of the associated CVS path
                 - The Git blob holding the revision data
                 - The Git commits that correspond to the CVS
                   states in which this CVS revision is present

CVSStateMap - provides a mapping from CVS states to corresponding
              Git commits (i.e. Git commits, whose tree state is
              identical to a given CVS state)
"""

import os

from git_remote_cvs.util import debug, error, die, file_reader_method
from git_remote_cvs.cvs import CVSNum, CVSDate
from git_remote_cvs.git import GitFICommit


class _CVSPathInfo(object):

    """Information on a single CVS path."""

    __slots__ = ('revs', 'mode')

    def __init__ (self, mode = None):
        self.revs = {}
        self.mode = mode


class _CVSRevInfo(object):

    """Information on a single CVS revision."""

    __slots__ = ('blob', 'commits')

    def __init__ (self, blob):
        self.blob = blob
        self.commits = []


class CVSRevisionMap(object):

    """Encapsulate the mapping of CVS revisions to associated metainfo.

    This container maps CVS revisions (a combination of a CVS path and
    a CVS revision number) into Git blob names, Git commit names, and
    CVS path information.  Git object (blob/commit) names are either
    40-char SHA1 strings, or "git fast-import" mark numbers if the form
    ":<num>".

    The data structure is organized as follows:
    - A CVSRevisionMap instance knows about a set of CVS paths.
      For each CVS path, the following is known:
      - The mode (permission bits) of that CVS path (644 or 755)
      - The CVS revision numbers that exist for that CVS path.
        For each revision number, the following is known:
        - Exactly 1 blob name; the Git blob containing the contents of
          the revision (the contents of the CVS path at that CVS
          revision).
        - One or more commit names; the Git commits which encapsulate a
          CVS state in which this CVS revision

    To store this data structure persistently, this class uses a Git
    ref that points to a tree structure containing the above
    information.  When changes to the structure are made, this class
    will produce git-fast-import commands to update that tree structure
    accordingly.

    IMPORTANT: No changes to the CVSRevisionMap will be stored unless
    self.commit_map() is called with a valid GitFastImport instance.

    NOTES: Mark numbers are only transient references bound to the
    current "git fast-import" process (assumed to be running alongside
    this process).  Therefore, when the "git fast-import" process ends,
    it must write out a mark number -> SHA1 mapping (see the
    "--export-marks" argument to "git fast-import").  Subsequently,
    this mapping must be parsed, and the mark numbers in this
    CVSRevisionMap must be resolved into persistent SHA1 names.
    In order to quickly find all the unresolved mark number entries in
    the data structure, and index mapping mark numbers to revisions is
    kept in a separate file in the tree structure.
    See the loadMarksFile() method for more details.

    """

    MarkNumIndexFile = "CVS/marks"  # Invalid CVS path name

    def __init__ (self, git_ref, obj_fetcher):
        """Create a new CVS revision map bound to the given Git ref."""
        self.git_ref = git_ref
        self.obj_fetcher = obj_fetcher

        # The following data structure is a cache of the persistent
        # data structure found at self.git_ref.
        # It is structured as follows:
        # - self.d is a mapping from CVS paths to _CVSPathInfo objects.
        #   _CVSPathInfo object have two fields: revs, mode:
        #   - mode is either 644 (non-executable) or 755 (executable).
        #   - revs is a dict, mapping CVS revision numbers (CVSNum
        #     instances) to _CVSRevInfo objects.  _CVSRevInfo objects
        #     have two fields: blob, commits:
        #     - blob is the name of the Git blob object holding the
        #       contents of that revision.
        #     - commits is a collection of zero or more Git commit
        #       names where the commit contains this revision.
        self.d = {}  # dict: path -> _CVSPathInfo
        self.mods = set()  # (path, revnum) pairs for all modified revs
        self.marknum_idx = {}  # dict: mark num -> [(path, revnum), ...]
        self._load_marknum_idx()

    def __del__ (self):
        """Verify that self.commit_map() was called before destruction."""
        if self.mods:
            error("Missing call to self.commit_map().")
            error("%i revision changes are lost!", len(self.mods))

    def __nonzero__ (self):
        """Return True iff any information is currently stored here."""
        return bool(self.d)

    # Private methods:

    def _add_to_marknum_idx(self, marknum, path, revnum):
        """Add the given marknum -> (path, revnum) association."""
        entry = self.marknum_idx.setdefault(marknum, [])
        entry.append((path, revnum))

    def _load_marknum_idx(self):
        """Load contents of MarkNumIndexFile into self.marknum_idx."""
        blobspec = "%s:%s" % (self.git_ref, self.MarkNumIndexFile)
        try:
            f = self.obj_fetcher.open_obj(blobspec)
        except KeyError:
            return  # Missing object; nothing to do

        for line in f:
            # Format of line is "<marknum> <path>:<revnum>"
            mark, rest = line.strip().split(' ', 1)
            path, rev = rest.rsplit(':', 1)
            self._add_to_marknum_idx(int(mark), path, CVSNum(rev))
        f.close()

    def _save_marknum_idx(self):
        """Prepare data for storage into MarkNumIndexFile.

        The returned string contains the current contents of
        self.marknum_idx, formatted to be stored verbatim in
        self.MarkNumIndexFile.

        """
        lines = []
        for marknum, revs in sorted(self.marknum_idx.iteritems()):
            for path, revnum in revs:
                # Format of line is "<marknum> <path>:<revnum>"
                line = "%i %s:%s\n" % (marknum, path, revnum)
                lines.append(line)
        return "".join(lines)

    def _save_rev(self, path, revnum):
        """Return blob data for storing the given revision persistently.

        Generate the blob contents that will reconstitute the same
        revision entry when read back in with _fetch_path().

        """
        lines = []
        rev_info = self.d[path].revs[revnum]
        lines.append("blob %s\n" % (rev_info.blob))
        for commit in rev_info.commits:
            lines.append("commit %s\n" % (commit))
        return "".join(lines)

    @staticmethod
    def _valid_objname (objname):
        """Return the argument as a SHA1 (string) or mark num (int)."""
        # Blob names are either 40-char SHA1 strings, or mark numbers
        if isinstance(objname, int) or len(objname) != 40:  # Mark number
            return int(objname)
        return objname

    def _load (self, path, mode, data):
        """GitObjectFetcher.walk_tree() callback."""
        assert mode in (644, 755)
        cvs_path, revnum = os.path.split(path)
        revnum = CVSNum(revnum)
        if cvs_path in self.d:
            assert mode == self.d[cvs_path].mode
        else:
            self.d[cvs_path] = _CVSPathInfo(mode)
        assert revnum not in self.d[cvs_path].revs
        rev_info = None
        for line in data.split("\n"):
            if not line:
                continue
            t, objname = line.split()
            objname = self._valid_objname(objname)
            if t == "blob":
                assert rev_info is None
                rev_info = _CVSRevInfo(objname)
            elif t == "commit":
                assert rev_info is not None
                rev_info.commits.append(objname)
            else:
                assert False, "Unknown type '%s'" % (t)
        assert rev_info.commits  # Each rev is in at least one commit
        self.d[cvs_path].revs[revnum] = rev_info

    def _fetch_path (self, path):
        """If the given path exists, create a path entry in self.d."""
        tree_spec = "%s:%s" % (self.git_ref, path)
        self.obj_fetcher.walk_tree(tree_spec, self._load, path)
        # TODO: Don't load entire tree at once?

    # Public methods:

    def has_path (self, path):
        """Return True iff the given path is present."""
        if path not in self.d:
            self._fetch_path(path)
        return path in self.d

    def has_rev (self, path, revnum):
        """Return True iff the given path:revnum is present."""
        if path not in self.d:
            self._fetch_path(path)
        return path in self.d and revnum in self.d[path].revs

    def get_mode (self, path):
        """Return mode bits for the given path."""
        if path not in self.d:
            self._fetch_path(path)
        return self.d[path].mode

    def get_blob (self, path, revnum):
        """Return the blob name for the given revision."""
        if path not in self.d:
            self._fetch_path(path)
        return self.d[path].revs[revnum].blob

    def get_commits (self, path, revnum):
        """Return the commit names containing the given revision."""
        if path not in self.d:
            self._fetch_path(path)
        return self.d[path].revs[revnum].commits

    def has_unresolved_marks (self):
        """Return True iff there are mark numbers in the data structure."""
        return self.marknum_idx

    # Public non-const methods

    def add_path (self, path, mode):
        """Add the given path and associated mode bits to this map."""
        if path not in self.d:
            self._fetch_path(path)
        if path in self.d:
            if self.d[path].mode:
                assert mode == self.d[path].mode, \
                    "The mode of %s has changed from %s " \
                    "to %s since the last import.  This " \
                    "is not supported." % (
                        path, self.d[path].mode, mode)
            else:
                self.d[path].mode = mode
        else:
            self.d[path] = _CVSPathInfo(mode)
        # Do not add to self.mods yet, as we expect revisions to be
        # added before commit_map() is called

    def add_blob (self, path, revnum, blobname):
        """Add the given path:revnum -> blobname association."""
        assert blobname
        if path not in self.d:
            self._fetch_path(path)
        blobname = self._valid_objname(blobname)
        if isinstance(blobname, int):  # Mark number
            self._add_to_marknum_idx(blobname, path, revnum)
        entry = self.d.setdefault(path, _CVSPathInfo())
        assert revnum not in entry.revs
        entry.revs[revnum] = _CVSRevInfo(blobname)
        self.mods.add((path, revnum))

    def add_commit (self, path, revnum, commitname):
        """Add the given path:revnum -> commitname association."""
        if path not in self.d:
            self._fetch_path(path)
        commitname = self._valid_objname(commitname)
        if isinstance(commitname, int):  # Mark number
            self._add_to_marknum_idx(commitname, path, revnum)
        assert revnum in self.d[path].revs
        assert commitname not in self.d[path].revs[revnum].commits
        self.d[path].revs[revnum].commits.append(commitname)
        self.mods.add((path, revnum))

    @file_reader_method(missing_ok = True)
    def load_marks_file (self, f):
        """Load mark -> SHA1 mappings from git-fast-import marks file.

        Replace all mark numbers with proper SHA1 names in this data
        structure (using self.marknum_idx to find them quickly).

        """
        if not f:
            return 0
        marks = {}
        last_mark = 0
        for line in f:
            (mark, sha1) = line.strip().split()
            assert mark.startswith(":")
            mark = int(mark[1:])
            assert mark not in marks
            marks[mark] = sha1
            if mark > last_mark:
                last_mark = mark
        for marknum, revs in self.marknum_idx.iteritems():
            sha1 = marks[marknum]
            for path, revnum in revs:
                if path not in self.d:
                    self._fetch_path(path)
                rev_info = self.d[path].revs[revnum]
                if rev_info.blob == marknum:  # Replace blobname
                    rev_info.blob = sha1
                else:  # Replace commitname
                    assert marknum in rev_info.commits
                    i = rev_info.commits.index(marknum)
                    rev_info.commits[i] = sha1
                    assert marknum not in rev_info.commits
                self.mods.add((path, revnum))
        self.marknum_idx = {}  # Resolved all transient mark numbers
        return last_mark

    def sync_modeinfo_from_cvs (self, cvs):
        """Update with mode information from current CVS checkout.

        This method will retrieve mode information on all paths in the
        current CVS checkout, and update this data structure
        correspondingly.  In the case where mode information is already
        present for a given CVS path, this method will verify that such
        information is correct.

        """
        for path, mode in cvs.get_modeinfo().iteritems():
            self.add_path(path, mode)

    def commit_map (self, gfi, author, message):
        """Produce git-fast-import commands for storing changes.

        The given GitFastImport object is used to produce a commit
        making the changes done to this data structure persistent.

        """
        now = CVSDate("now")
        commitdata = GitFICommit(
            author[0], author[1], now.ts, now.tz_str(), message)

        # Add updated MarkNumIndexFile to commit
        mark = gfi.blob(self._save_marknum_idx())
        commitdata.modify(644, mark, self.MarkNumIndexFile)

        for path, revnum in self.mods:
            mark = gfi.blob(self._save_rev(path, revnum))
            mode = self.d[path].mode
            assert mode in (644, 755)
            commitdata.modify(mode, mark, "%s/%s" % (path, revnum))

        gfi.commit(self.git_ref, commitdata)
        self.mods = set()


class CVSStateMap(object):

    """Map CVSState object to the commit names which produces that state."""

    def __init__ (self, cvs_rev_map):
        """Create a CVSStateMap object, bound to the given CVS revision map."""
        self.cvs_rev_map = cvs_rev_map

    def get_commits (self, state):
        """Map the given CVSState to commits that contain this state.

        Return all commits where the given state is a subset of the
        state produced by that commit.

        Returns a set of commit names.  The set may be empty.

        """
        candidate_commits = None
        for path, revnum in state:
            commits = self.cvs_rev_map.get_commits(path, revnum)
            if candidate_commits is None:
                candidate_commits = set(commits)
            else:
                candidate_commits.intersection_update(commits)
        return candidate_commits

    def get_exact_commit (self, state, commit_map):
        """Map the given CVSState to the commit with this exact state.

        The given commit_map must be a CommitStates object.

        Return the only commit (if any) that produces the exact given
        CVSState.

        Returns a commit name, or None if no matching commit is found.

        """
        commits = self.get_commits(state)
        for c in commits:
            if state == commit_map.get(c):
                return c
        return None
