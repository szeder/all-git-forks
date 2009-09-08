#!/usr/bin/env python

"""Implementation of a local CVS symbol cache.

A CVS symbol cache stores a list of CVS symbols and the CVS state
associated with each of those CVS symbols at some point in time.

Keeping a local cache of CVS symbols is often needed because the
design of CVS makes it potentially very expensive to query the CVS
server directly for CVS symbols and associated states.

In these cases, a local CVS symbol cache can provide equivalent
(although possibly out-of-date) information immediatele.

Synchronization with the current state on the CVS server can be
done on a symbol-by-symbol basis (by checking out a given symbol
and extracting the CVS state from the CVS work tree), or by
synchronizing _all_ CVS symbols in one operation (by executing
'cvs rlog' and parsing CVS states from its output).

"""

import sys
import os

from git_remote_cvs.util import debug, error, die, ProgressIndicator
from git_remote_cvs.cvs import CVSNum, CVSState, CVSLogParser


class CVSSymbolStateLister(CVSLogParser):

    """Extract current CVSStates for all CVS symbols from a CVS log."""

    def __init__ (self, cvs_repo, show_progress = False):
        """Create a new CVSSymbolStateLister.

        The cvs_repo argument must be a (cvs_root, cvs_module) tuple
        show_progress determines whether a progress indicator should
        be displayed.

        """
        super(CVSSymbolStateLister, self).__init__(cvs_repo)
        self.symbols = {}  # CVS symbol name -> CVSState object
        self.cur_file = None  # current CVS file being processed
        self.cur_file_numrevs = 0  # #revs in current CVS file
        self.cur_revnum = None  # current revision number
        self.rev2syms = {}  # CVSNum -> [CVS symbol names]
        self.cur_revs = {}  # CVSNum -> True/False (deleted)
        self.head_num = None  # CVSNum of the HEAD rev or branch

        # Possible states:
        # - BeforeSymbols - waiting for "symbolic names:"
        # - WithinSymbols - reading CVS symbol names
        # - BeforeRevs  - waiting for "total revisions:"
        # - BetweenRevs - waiting for "----------------------------"
        # - ReadingRev  - reading CVS revision details
        self.state = 'BeforeSymbols'

        self.progress = None
        if show_progress:
            self.progress = ProgressIndicator("\t", sys.stderr)

    def finalize_symbol_states (self):
        """Adjust CVSStates in self.symbols based on revision data.

        Based on the information found in self.rev2syms and
        self.cur_revs, remove deleted revisions and turn branch numbers
        into corresponding revisions in the CVSStates found in
        self.symbols.

        """
        # Create a mapping from branch numbers to the last existing
        # revision number on those branches
        branch2lastrev = {}  # branch number -> revision number
        for revnum in self.cur_revs.iterkeys():
            branchnum = revnum.branch()
            if (branchnum not in branch2lastrev) or \
               (revnum > branch2lastrev[branchnum]):
                branch2lastrev[branchnum] = revnum

        for cvsnum, symbols in self.rev2syms.iteritems():
            if cvsnum.is_branch():
                # Turn into corresponding revision number
                revnum = branch2lastrev.get(cvsnum, cvsnum.parent())
                for s in symbols:
                    state = self.symbols[s]
                    assert state[self.cur_file] == cvsnum
                    state.replace(self.cur_file, revnum)
                cvsnum = revnum
            assert cvsnum.is_rev()
            assert cvsnum in self.cur_revs
            if self.cur_revs[cvsnum]:  # cvsnum is a deleted rev
                # Remove from CVSStates
                for s in symbols:
                    state = self.symbols[s]
                    state.remove(self.cur_file, cvsnum)

        self.rev2syms = {}
        self.cur_revs = {}
        self.cur_file = None

    def __call__ (self, line):
        """Line parser; this method is invoked for each line in the log."""
        if self.state == 'BeforeSymbols':
            if line.startswith("RCS file: "):
                self.cur_file = self.cleanup_path(line[10:])
                if self.progress:
                    self.progress("%5i symbols found - Parsing CVS file #%i: "
                                  "%s " % (len(self.symbols), self.progress.n,
                                           self.cur_file,))
            if line.startswith("head: "):
                self.head_num = CVSNum(line[6:])
            if line.startswith("branch: "):
                self.head_num = CVSNum(line[8:])
            elif line == "symbolic names:":
                assert self.head_num
                s = self.symbols.setdefault("HEAD", CVSState())
                s.add(self.cur_file, self.head_num)
                r = self.rev2syms.setdefault(self.head_num, [])
                r.append("HEAD")
                self.head_num = None
                self.state = 'WithinSymbols'
        elif self.state == 'WithinSymbols':
            if line.startswith("\t"):
                symbol, cvsnum = line.split(":", 1)
                symbol = symbol.strip()
                cvsnum = CVSNum(cvsnum)
                s = self.symbols.setdefault(symbol, CVSState())
                s.add(self.cur_file, cvsnum)
                r = self.rev2syms.setdefault(cvsnum, [])
                r.append(symbol)
            else:
                self.state = 'BeforeRevs'
        elif self.state == 'BeforeRevs':
            if line.startswith("total revisions: "):
                assert self.cur_file
                totalrevs_unused, selectedrevs = line.split(";")
                self.cur_file_numrevs = int(selectedrevs.split(":")[1].strip())
                self.state = 'BetweenRevs'
        elif self.state == 'BetweenRevs':
            if (line == "----------------------------" or
                line == "======================================"
                        "======================================="):
                if self.cur_revnum:
                    assert self.cur_revnum in self.cur_revs
                    self.cur_revnum = None
                if line == "----------------------------":
                    self.state = 'ReadingRev'
                else:
                    # Finalize current CVS file
                    assert len(self.cur_revs) == self.cur_file_numrevs
                    self.finalize_symbol_states()
                    self.state = 'BeforeSymbols'
        elif self.state == 'ReadingRev':
            if line.startswith("revision "):
                self.cur_revnum = CVSNum(line.split()[1])
            else:
                date, author, state, dummy = line.split(";", 3)
                assert date.startswith("date: ")
                assert author.strip().startswith("author: ")
                assert state.strip().startswith("state: ")
                state = state.strip()[7:]
                assert self.cur_revnum not in self.cur_revs
                deleted = state == "dead"
                self.cur_revs[self.cur_revnum] = deleted
                self.state = 'BetweenRevs'

    def finish (self):
        """This method is invoked after the last line has been parsed."""
        assert self.state == 'BeforeSymbols'
        if self.progress:
            self.progress.finish("Parsed %i symbols in %i files" %
                                 (len(self.symbols), self.progress.n))


class CVSSymbolCache(object):

    """Local cache of the current CVSState of CVS symbols.

    Simulates a dictionary of CVS symbol -> CVSState mappings.

    """

    def __init__ (self, symbols_dir):
        """Create a new CVS symbol cache, located in the given directory."""
        self.symbols_dir = symbols_dir
        if not os.path.isdir(self.symbols_dir):
            os.makedirs(self.symbols_dir)

    def __len__ (self):
        """Return the number of CVS symbols stored in this cache."""
        return len(os.listdir(self.symbols_dir))

    def __iter__ (self):
        """Return an iterator traversing symbol names stored in this cache."""
        for filename in os.listdir(self.symbols_dir):
            yield filename

    def __contains__ (self, symbol):
        """Return True if the given symbol is present in this cache."""
        return os.access(os.path.join(self.symbols_dir, symbol),
                         os.F_OK | os.R_OK)

    def __getitem__ (self, symbol):
        """Return the cached CVSState of the given CVS symbol."""
        try:
            f = open(os.path.join(self.symbols_dir, symbol), 'r')
        except IOError:
            raise KeyError("'%s'" % (symbol))
        ret = CVSState()
        ret.load(f)
        f.close()
        return ret

    def __setitem__ (self, symbol, cvs_state):
        """Store the given CVS symbol and CVSState into the cache."""
        cvs_state.save(os.path.join(self.symbols_dir, symbol))

    def __delitem__ (self, symbol):
        """Remove the the given CVS symbol from the cache."""
        os.remove(os.path.join(self.symbols_dir, symbol))

    def get (self, symbol, default = None):
        """Return the cached CVSState of the given CVS symbol."""
        try:
            return self[symbol]
        except KeyError:
            return default

    def items (self):
        """Return list of (CVS symbol, CVSState) tuples saved in this cache."""
        for filename in self:
            yield (filename, self[filename])

    def clear (self):
        """Remove all entries from this CVS symbol cache."""
        for filename in os.listdir(self.symbols_dir):
            os.remove(os.path.join(self.symbols_dir, filename))

    def sync_symbol (self, symbol, cvs, progress):
        """Synchronize the given CVS symbol with the CVS server.

        The given CVS workdir is used for the synchronization.
        The retrieved CVSState is also returned

        """
        progress("Retrieving state of CVS symbol '%s'..." % (symbol))
        cvs.update(symbol)
        state = cvs.get_state()

        progress("Saving state of '%s' to symbol cache..." % (symbol))
        self[symbol] = state

    def sync_all_symbols (self, cvs_repo, progress, symbol_filter = None):
        """Synchronize this entire CVS symbol cache with the CVS server.

        This may be very expensive if the CVS repository is large, or
        has many symbols.  After this method returns, the symbol cache
        will be in sync with the current state on the server.

        This method returns a dict with the keys 'unchanged',
        'changed', 'added', and 'deleted', where each map to a list of
        CVS symbols.  Each CVS symbol appears in exactly one of these
        lists.

        If symbol_filter is given, it specifies functions that takes
        one parameter - a CVS symbol name - and returns True if that
        symbol should be synchronized, and False if that symbol should
        be skipped.  Otherwise all CVS symbols are synchronized.

        """
        if symbol_filter is None:
            symbol_filter = lambda symbol: True

        # Run cvs rlog to fetch current CVSState for all CVS symbols
        progress("Retrieving current state of all CVS symbols from CVS "
                 "server...", lf = True)
        parser = CVSSymbolStateLister(cvs_repo, True)
        retcode = parser.run()
        if retcode:
            raise EnvironmentError(retcode, "cvs rlog exit code %i" % retcode)

        # Update symbol cache with new states from the CVS server
        progress("Updating symbol cache with current CVS state...")
        results = {}
        result_keys = ("unchanged", "changed", "added", "deleted")
        for k in result_keys:
            results[k] = []
        # Classify existing symbols as unchanged, changed, or deleted
        for symbol in filter(symbol_filter, self):
            if symbol not in parser.symbols:  # Deleted
                results["deleted"].append(symbol)
                del self[symbol]
            elif self[symbol] != parser.symbols[symbol]:  # Changed
                results["changed"].append(symbol)
                self[symbol] = parser.symbols[symbol]
            else:  # Unchanged
                results["unchanged"].append(symbol)
            progress()
        # Add symbols that are not in self
        for symbol, state in parser.symbols.iteritems():
            if not symbol_filter(symbol):
                debug("Skipping CVS symbol '%s'...", symbol)
            elif symbol in self:
                assert state == self[symbol]
            else:  # Added
                results["added"].append(symbol)
                self[symbol] = state
            progress()
        progress("Synchronized local symbol cache (%s)" %
                 (", ".join(["%i %s" % (len(results[k]), k)
                             for k in result_keys])), True)
        return results
