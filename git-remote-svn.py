#!/usr/bin/env python

import sys, os, re, time, subprocess
import svn.core, svn.repos, svn.fs

ct_short = ['M', 'A', 'D', 'R', 'X']


############################################################################
# Class which encapsulates the fast import helper. Provides methods to
# read/write data to it.
class FastImportHelper:

	def __init__(self):
		PIPE = subprocess.PIPE
		args = ['git', 'fast-import-helper']
		self.helper = subprocess.Popen(args, stdin=PIPE, stdout=PIPE)

	def write(self, data):
		self.helper.stdin.write(data)
		self.helper.stdin.flush()

	def readline(self):
		return self.helper.stdout.readline()

	def close(self):
		self.helper.stdin.close()
		self.helper.wait()


############################################################################
# Base class for python remote helpers. It handles the main command loop.
class RemoteHelper(object):

	def __init__(self, kind):
		self.kind = kind

	# The commands we understand
	COMMANDS = ( 'capabilities', 'list', 'fetch', )

	# Read next command. Raise an exception if the command is invalid.
	# Return a tuple (command, args,)
	def read_next_command(self):
		line = sys.stdin.readline()
		if not line:
			return ( None, None, )
	
		cmdline = line.strip().split()
		if not cmdline:
			return ( None, None, )

		cmd = cmdline.pop(0)
		if cmd not in self.COMMANDS:
			raise Exception("Invalid command '%s'" % cmd)
		
		return ( cmd, cmdline, )

	# Run the remote helper, process commands until the end of the world. Or
	# until we're told to finish.
	def run(self):
		while (True):
			( cmd, args, ) = self.read_next_command()
			if cmd is None:
				return

			func = getattr(self, cmd, None)
			if func is None or not callable(func):
				raise Exception("Command '%s' not implemented" % cmd)

			result = func(args)
			sys.stdout.flush()


	# Convenience method for writing data back to git
	def reply(self, data):
		sys.stdout.write(data)

	# Return all refs and the contents of the note attached to each.
	# This can be used by the remote helper to find out what the latest
	# version is that we fetched into this repo.
	# Returns list of tuples of (sha1, typename, refname, note,)
	def refs(self):
		refs = []
		
		PIPE = subprocess.PIPE
		args = [ 'git', 'for-each-ref' ]
		gfer = subprocess.Popen(args, stdin=PIPE, stdout=PIPE)
		
		# Regular expression for matching the output from g-f-e-r
		pattern = re.compile(r"(.{40}) (\w+)	(.*)")
		while (True):
			line = gfer.stdout.readline()
			if not line:
				break 

			match = pattern.match(line)
			
			# The sha1 and name of the ref
			sha1 = match.group(1)
			typename = match.group(2)
			refname = match.group(3)
			
			# Extract the note using `git notes show <sha>`
			git_notes_show = [ 'git', 'notes', 'show', sha1 ]
			
			# Set GIT_NOTES_REF to point to the notes of our kind
			env = { "GIT_NOTES_REF": "refs/notes/%s" % self.kind }
			
			note = subprocess.Popen(git_notes_show, env=env, stdout=PIPE, stderr=PIPE)
			refs.append(( sha1, typename, refname, note.stdout.readline() ))

		gfer.wait()

		return refs


############################################################################
# Remote helper for Subversion
class RemoteHelperSubversion(RemoteHelper):

	def __init__(self, url):
		super(RemoteHelperSubversion, self).__init__("svn")

		url = svn.core.svn_path_canonicalize(url)
		self.repo = svn.repos.svn_repos_open(url)
		self.fs = svn.repos.svn_repos_fs(self.repo)
		self.uuid = svn.fs.svn_fs_get_uuid(self.fs)


	# Here follow the commands this helper implements
	
	def capabilities(self, args):
		self.reply("list\nfetch\n\n")

	def list(self, args):
		rev = svn.fs.svn_fs_youngest_rev(self.fs)
		self.reply(":r%s trunk\n\n" % ( rev ))

	def fetch(self, args):
		# Create the fast-import helper
		fih = FastImportHelper()
		msha1 = None

		# Fetches are done in batches. Process fetch lines until we see a
		# blank newline
		while args:
			# The revision to fetch, strip the leading 'r' from 'r42'
			new = int(args[0][1:])
			
			# Trailing slash to ensure that it's a directory
			prefix = "/%s/" % args[1]
			
			( sha1, old, ) = self.parent(args[1])
			sys.stderr.write("Best parent: %s\n" % old)
			
			if old != new:
				( sha1, msha1, ) = self.fi(prefix, old, new, sha1, fih)
			self.reply("map r%s %s\n" % ( new, sha1 ))

			# Read next line, break if it's a newline (ending this fetch batch)
			( cmd, args, ) = self.read_next_command()
			if not cmd:
				break

		fih.close()
		if msha1:
			self.reply("silent refs/notes/svn %s\n" % ( msha1 ))
		self.reply("\n")
		

	# Find the git commit we can use as parent when importing from the
	# repo with the given prefix. All commits imported from svn
	# have a note attached which contains this information. But to make our
	# job easier, we only scan ref heads and not the whole history.
	# Go through all refs, see which one has a note that matches the given
	# prefix and extract the svn revision number from the note.
	# Return a tuple (sha1, rev,) which identifies the git commit and svn
	# revision.
	def parent(self, prefix):
		pattern = re.compile(r"([0-9a-h-]+)/(\w+)@(\d+)")
		res = []
		for ( sha1, typename, name, note, ) in self.refs():
			if typename != "commit":
				continue

			match = pattern.match(note)
			if not match:
				continue

			if match.group(2) == prefix and match.group(1) == self.uuid:
				rev = int(match.group(3))
				res.append(( sha1, rev ))

		if len(res) == 0:
			return ( None, 1, )

		res.sort(lambda a,b: a[1] < b[1])
		return res[0]

	# Run fast import of revision `old` up to `new`, only considering files
	# under the given prefix. Use `sha1` as the parent of the first commit.
	# Return the git commit name that corresponds to the last revision so
	# we can report it back to git.
	def fi(self, prefix, old, new, sha1, fih):
		# The range we want to import
		revs = xrange(old or 1, new + 1)
		marks = { }
		msha1 = None
		
		for r in revs:
			sha1 = feed(self.repo, self.fs, r, prefix, sha1, fih)
			if sha1:
				marks[r] = sha1

			if len(marks) >= 10:
				msha1 = update_marks(self.repo, self.fs, r, msha1, marks, prefix, fih)
				marks = { }

		if len(marks) > 0:
			msha1 = update_marks(self.repo, self.fs, new, msha1, marks, prefix, fih)

		return (sha1, msha1,)	



def dump_file_blob(root, path, fih):
	length = int(svn.fs.svn_fs_file_length(root, path))
	fih.write("data %s\n" % length)
	
	# Oh boy, directly streaming from an svn stream to fih.stdin is so
	# incredibely complicated
	stream = svn.fs.svn_fs_file_contents(root, path)
	while length > 0:
		avail = min(length, 4096)
		data = svn.core.svn_stream_read(stream, avail)
		err = fih.write(data)
		length -= avail
		#sys.stderr.write("Wrote %d bytes, %d to go %s\n" % (avail, length, err))

	fih.write("\n")

# Feed the fast-import helper with the given revision
def feed(repo, fs, rev, prefix, sha1, fih):
	# Open the root at that revision
	root = svn.fs.svn_fs_revision_root(fs, rev)

	# And the list of what changed in this revision.
	changes = svn.fs.svn_fs_paths_changed(root)

	i = 1
	marks = {}
	file_changes = []

	for path, change_type in changes.iteritems():
		c_t = ct_short[change_type.change_kind]
		if svn.fs.svn_fs_is_dir(root, path):
			continue

		if not path.startswith(prefix):
			continue
		
		realpath = path.replace(prefix, '')
		if c_t == 'D':
			file_changes.append("D %s" % realpath)
		else:
			marks[i] = realpath
			file_changes.append("M 644 :%s %s" % (i, marks[i]))
			fih.write("blob\nmark :%s\n" % i)
			#sys.stderr.write("File change: %s\n" % marks[i])
			
			dump_file_blob(root, path, fih)
			i += 1
			
			# Read the mark
			line = fih.readline()

	if len(file_changes) == 0:
		return

	# Get the commit author and message
	props = svn.fs.svn_fs_revision_proplist(fs, rev)
	if props.has_key('svn:author'):
		author = "%s <%s@localhost>" % (props['svn:author'], props['svn:author'])
	else:
		author = 'nobody <nobody@localhost>'

	svndate = props['svn:date'][0:-8]
	commit_time = time.mktime(time.strptime(svndate, '%Y-%m-%dT%H:%M:%S'))
	fih.write("commit refs/heads/master\n")
	fih.write("mark :r%s\n" % (rev))
	fih.write("committer %s %s -0000\n" % (author, int(commit_time)))
	fih.write("data %s\n" % len(props['svn:log']))
	fih.write(props['svn:log'])
	if sha1:
		fih.write("from %s\n" % sha1)
	fih.write('\n'.join(file_changes))
	fih.write("\n\n")
	
	# Read the mark
	line = fih.readline().strip().split(' ')
	return line[2]


def add_mark(repo, fs, mark, rev, prefix, fih):
	note = "%s%s@%s" % (svn.fs.svn_fs_get_uuid(fs), prefix[:-1], rev)
	fih.write("N inline :r%s\n" % (rev))
	fih.write("data %s\n" % len(note))
	fih.write(note)
	fih.write("\n")

def update_marks(repo, fs, rev, msha1, marks, prefix, fih):
	fih.write("commit refs/notes/svn\n")
	fih.write("mark :svn-notes\n")

	author = 'nobody <nobody@localhost>'
	commit_time = time.time()
	fih.write("committer %s %s -0000\n" % (author, int(commit_time)))

	log = "Update svn notes to r%s" % rev
	fih.write("data %s\n" % len(log))
	fih.write(log)
	if msha1:
		fih.write("from %s\n" % msha1)
	else:
		fih.write("from refs/notes/svn^0\n")
	
	for rev, mark in marks.iteritems():
		add_mark(repo, fs, mark, rev, prefix, fih)

	fih.write("\n")

	# Read the mark
	line = fih.readline().strip().split(' ')
	return line[2]



if __name__ == '__main__':
	helper = RemoteHelperSubversion(sys.argv[2])
	helper.run()
