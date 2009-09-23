#!/usr/bin/env python

"""Code for collecting individual CVS revisions into "changesets"

A changeset is a collection of CVSRev objects that belong together in
the same "commit".  This is a somewhat artificial construct on top of
CVS, which only stores changes at the per-file level.  Normally, CVS
users create several CVS revisions simultaneously by applying the
"cvs commit" command to several files with related changes.  This
module tries to reconstruct this notion of related revisions.

"""

from git_remote_cvs.util import debug, error, die


class Changeset(object):

    """Encapsulate a single changeset/commit."""

    __slots__ = ('revs', 'date', 'author', 'message')

    # The maximum time between the changeset's date, and the date of a
    # rev to included in that changeset.
    MaxSecondsBetweenRevs = 8 * 60 * 60  # 8 hours

    @classmethod
    def from_rev (cls, rev):
        """Return a Changeset based on the given CVSRev object."""
        c = cls(rev.date, rev.author, rev.message)
        result = c.add(rev)
        assert result
        return c

    def __init__ (self, date, author, message):
        """Create a new Changeset with the given metadata."""
        self.revs = {}  # dict: path -> CVSRev object
        self.date = date  # CVSDate object
        self.author = author
        self.message = message  # Lines of commit message

    def __str__ (self):
        """Stringify this Changeset object."""
        msg = self.message[0]  # First line only
        # Limit message to 25 chars
        if len(msg) > 25:
            msg = msg[:22] + "..."
        return ("<Changeset @(%s) by %s (%s) updating %i files>" %
                (self.date, self.author, msg, len(self.revs)))

    def __iter__ (self):
        """Return iterator traversing the CVSRevs in this Changeset."""
        return self.revs.itervalues()

    def __getitem__ (self, path):
        """Look up a specific CVSRev in this Changeset."""
        return self.revs[path]

    def within_time_window (self, rev):
        """Return True iff the rev is within the time window of self."""
        return abs(rev.date.diff(self.date)) <= self.MaxSecondsBetweenRevs

    def add (self, rev):
        """Add the given CVSRev to this Changeset.

        The addition will only succeed if the following holds:
          - rev.author == self.author
          - rev.message == self.message
          - rev.path is not in self.revs
          - rev.date is within MaxSecondsBetweenRevs of self.date
        If the addition succeeds, True is returned; otherwise False.

        """
        if rev.author != self.author or \
           rev.message != self.message or \
           rev.path in self.revs or \
           not self.within_time_window(rev):
            return False

        self.revs[rev.path] = rev
        return True


def build_changesets_from_revs (cvs_revs):
    """Organize CVSRev objects into a chronological list of Changesets."""
    # Construct chronological list of CVSRev objects
    chron_revs = []
    for path, d in cvs_revs.iteritems():
        i = 0  # Current index into chronRevs
        for revnum, cvsrev in sorted(d.iteritems()):
            assert path == cvsrev.path
            assert revnum == cvsrev.num
            while i < len(chron_revs) and cvsrev.date > chron_revs[i].date:
                i += 1
            # Insert cvsRev at position i in chronRevs
            chron_revs.insert(i, cvsrev)
            i += 1

    changesets = []  # Chronological list of Changeset objects
    while len(chron_revs):
        # There are still more revs to be added to Changesets
        # Create Changeset based on the first rev in chronRevs
        changeset = Changeset.from_rev(chron_revs.pop(0))
        # Keep adding revs chronologically until MaxSecondsBetweenRevs
        rejects = []  # Revs that cannot be added to this changeset
        while len(chron_revs):
            rev = chron_revs.pop(0)
            reject = False
            # First, if we have one of rev's parents in rejects, we
            # must also reject rev
            for r in rejects:
                if r.path == rev.path:
                    reject = True
                    break
            # Next, add rev to changeset, reject if add fails
            if not reject:
                reject = not changeset.add(rev)
            if reject:
                rejects.append(rev)
                # stop trying when rev is too far in the future
                if not changeset.within_time_window(rev):
                    break
        chron_revs = rejects + chron_revs  # Reconstruct remaining revs
        changesets.append(changeset)

    return changesets
