#!/usr/bin/python
# gitweb snapshot.cgi - generate snapshots of git repositories
# Copyright (C) 2005-2006 Anders Gustafsson, Sham Chukoury
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import sys
import os
import cgi
import time
import bz2

# where the git tools are located
git_bin_dir = "/usr/bin"

# where the git repositories are located
git_base_dir = "/xmms2"

# where to put snapshots
snapshot_dir = "/tmp/snapshots"

# where to send users if an invalid snapshot is requested
snapshots_url = "http://git.xmms.se/"

# Note: in the context of this script, a 'tree' is a Git repository,
# not a Git tree.

def redirect(url):
	print "Location: %s\n" % url

def send_text(msg):
	print "Content-Type: text/plain\n"
	print msg

class Snapshot:
	def __init__(self, git_dir, treename, commitID):
		self.git_dir = git_dir
		self.treename = treename
		self.dir = os.path.join(snapshot_dir, treename)

		if (commitID == "HEAD"):
			self.commitID = file("%s/HEAD" % git_dir).read().rstrip()
			if not valid_hash(self.commitID) and self.commitID.startswith("ref:"):
				ref = self.commitID.split()[1]
				self.commitID = file("%s/%s" % (git_dir, ref)).read().rstrip()
			redirect("%s/%s-snapshot-%s.tar.bz2" % (
			os.environ["SCRIPT_NAME"], treename, self.commitID))
			sys.exit()
		else:
			self.commitID = commitID

		# read commit data
		os.putenv("GIT_DIR", git_dir)
		f = os.popen("%s commit %s" % (
		os.path.join(git_bin_dir, "git-cat-file"), self.commitID))
		self.tree = f.readline().rstrip().split(" ")[-1]
		f.readline() # parent
		f.readline() # author
		commit = f.readline().rstrip()

		if commit == "":
			# commit is empty, must be invalid hash
			send_text("Invalid hash %s for tree %s" % (self.commitID, self.treename))
			sys.exit()
		# parse committer field, generate snapshot name
		#committime = int(commit.split(" ")[-2])
		self.name = "%s-snapshot-%s" % (treename, self.commitID)
		self.filepath = "%s/%s.tar.bz2" % (self.dir, self.name)
		self.tarpath = self.filepath[:-4]

	# check whether snapshot dir exists, or create it
	def check_dir(self):
		if not os.access("%s/" % self.dir, os.F_OK):
			os.mkdir("%s/" % self.dir)

	def make_commithash(self):
		# check whether commithash file exists
		ch = ("%s/commithash-%s" % (self.dir, self.commitID))
		if not os.access(ch, os.F_OK):
			# make commithash file
			chfile = open(ch, "w+")
			chfile.write("%s\n\n" % self.commitID)
			lstree = os.popen("git-ls-tree -r %s" % self.commitID)
			for line in lstree:
				chfile.write(line)
			chfile.close()
			lstree.close()
		return ch

	# check whether snapshot file exists, or build it
	def build(self):
		self.check_dir()
		if not os.access(self.filepath, os.F_OK):
			import tarfile
			# todo: trap possible errors here
			# create tarball
			os.system("%s %s %s > %s" % (
			os.path.join(git_bin_dir, "git-tar-tree"),
			self.tree, self.name, self.tarpath))

			# add commithash file to tarball
			chFilename = self.make_commithash()
			tfile = tarfile.TarFile(self.tarpath, "a")
			tfile.add(chFilename, "%s/commithash" % self.name)
			tfile.close()
			# compress tarball
			os.system("bzip2 %s" % self.tarpath)

			# add to .htaccess
			file("%s/.htaccess" % self.dir,"a").write('AddDescription "%s git snapshot (%s)" %s.tar.bz2\n' % (self.treename, self.commitID, self.name)) 

	# open snapshot file
	def get_file(self):
		try:
			retFile = file(self.filepath, "r")
		except IOError:
			retFile = None
		return retFile

	def send_bheaders(self, filename = None, size = None, lastmod = None):
		if filename is None:
			filename = self.name + ".tar.bz2"
		sizestr = ""
		if size is not None:
			sizestr = "; size=%i" % size
		print "Content-Type: application/x-bzip2"
		print "Content-Encoding: x-bzip2"
		print "Content-Disposition: inline; filename=%s%s" % (
		filename, sizestr)
		print "Accept-Ranges: none"
		if size is not None:
			print "Content-Length: %i" % size
		if lastmod is not None:
			print "Last-Modified: %s" % (
			time.strftime("%a, %d %b %Y %H:%M:%S GMT", time.gmtime(lastmod)))
		print ""

	# send pre-made tarball (self.build, or otherwise)
	def send_binary(self):
		bfile = self.get_file()
		if bfile is None:
			send_text("Sorry, could not provide snapshot for tree %s, commit %s" % (self.treename, self.commitID))
		else:
			self.send_bheaders(size=os.stat(self.filepath)[6],
			lastmod = os.stat(self.filepath)[8])
			for line in bfile:
				sys.stdout.write(line)
			bfile.close()

	# make snapshot tarball and send, on the fly
	def on_the_fly(self):
		def cache_chunk_send(chunk, cfile):
			cfile.write(chunk)
			sys.stdout.write(chunk)

		# try to get file from disk if it exists, first
		if os.access(self.filepath, os.F_OK):
			self.send_binary()
		else:
			self.check_dir()
			cachefile = file(self.filepath, "w")
			tar = os.popen("%s %s %s" % (
			os.path.join(git_bin_dir, "git-tar-tree"),
			self.tree, self.name))

			kompressor = bz2.BZ2Compressor()
			self.send_bheaders()
			for line in tar:
				cache_chunk_send(kompressor.compress(line),
				cachefile)
			cache_chunk_send(kompressor.flush(), cachefile)
			cachefile.close()
			tar.close()

if not os.access(snapshot_dir, os.F_OK):
	try:
		os.mkdir(snapshot_dir)
	except OSError:
		send_text("Could not create snapshot dir '%s'" % snapshot_dir)
		sys.exit()

def valid_hash(ahash):
	retVal = True

	if ahash != "HEAD":
		if len(ahash) != 40:
			retVal = False
		for char in ahash:
			if char not in "0123456789abcdef":
				retVal = False
	return retVal

def valid_pathinfo():
	import re
	retVal = ()
	try:
		path = os.environ["PATH_INFO"]
	except KeyError:
		return retVal
	tree = None
	commit = None
	# path must be '/treename-snapshot-hash.tar.bz2'
	match = re.compile(r"^/[\w\d\.-]+-snapshot").search(path)
	if match is not None:
		tree = match.group()[1:-len("-snapshot")]
	match = re.compile(r"snapshot-(([\da-f]{40})|(HEAD))\.tar\.bz2$").search(path)
	if match is not None:
		commit = match.group()[len("snapshot-"):-len(".tar.bz2")]
	if tree and commit:
		retVal = (tree, commit)
	return retVal

def send_snapshot(gitdir, tree, commit):
	# validate tree name
	if not os.access(gitdir, os.F_OK):
		send_text("No such tree: %s" % tree)
		sys.exit()

	snap = Snapshot(gitdir, tree, commit)
	snap.build()
	snap.send_binary()

fs = cgi.FieldStorage()
pathArgs = valid_pathinfo()
if (fs.has_key("tree") and fs.has_key("commit")):
	tree = fs["tree"].value
	commit = fs["commit"].value

	# validate commit hash
	if not valid_hash(commit):
		send_text("Invalid hash: %s" % commit)
		sys.exit()

	send_snapshot(os.path.join(git_base_dir, tree), tree, commit)

#elif fs.has_key("tree"):
#	redirect("%s%s" % (snapshots_url, fs["tree"].value))
#	#if os.access(os.path.join(snapshot_dir, fs["tree"].value), os.F_OK):
#	#	redirect("%s%s" % (snapshots_url, fs["tree"].value))
#	#else:
#	#	send_text("No such tree: %s\n" % fs["tree"].value)

elif pathArgs:
	tree = pathArgs[0]
	commit = pathArgs[1]

	send_snapshot(os.path.join(git_base_dir, tree), tree, commit)
else:
	# user requested url directly, without a commit hash
	# redirect to snapshots dir
	redirect(snapshots_url)
