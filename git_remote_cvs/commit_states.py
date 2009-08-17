#!/usr/bin/env python

"""Code for relating Git commits to corresponding CVSState objects."""

from git_remote_cvs.util import debug, error, die
from git_remote_cvs.cvs import CVSState


class CommitStates(object):

    """Provide a mapping from Git commits to CVSState objects.

    Behaves like a dictionary of Git commit names -> CVSState mappings.

    Every Git commit converted from CVS has a corresponding CVSState,
    which describes exactly which CVS revisions are present in a
    checkout of that commit.

    This class provides the interface to map from a Git commit to its
    corresponding CVSState.  The mapping uses GitNotes as a persistent
    storage backend.

    """

    def __init__ (self, notes):
        """Create a new Git commit -> CVSState map."""
        self.notes = notes
        self._cache = {}  # commitname -> CVSState

    def _load (self, commit):
        """Retrieve the CVSState associated with the given Git commit."""
        if commit is None:
            return None
        if commit in self._cache:
            return self._cache[commit]
        notedata = self.notes.get(commit)
        if notedata is None:  # Given commit has no associated note
            return None
        state = CVSState()
        state.load_data(notedata)
        self._cache[commit] = state
        return state

    def add (self, commit, state, gfi):
        """Add the given Git commit -> CVSState mapping."""
        assert commit not in self._cache
        self._cache[commit] = state
        self.notes.import_note(commit, str(state), gfi)

    def __getitem__ (self, commit):
        """Return the CVSState associated with the given commit."""
        state = self._load(commit)
        if state is None:
            raise KeyError("Unknown commit '%s'" % (commit))
        return state

    def get (self, commit, default = None):
        """Return the CVSState associated with the given commit."""
        state = self._load(commit)
        if state is None:
            return default
        return state
