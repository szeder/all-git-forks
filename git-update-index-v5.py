#!/usr/bin/env python

# Usage: python git-update-index-v5.py [-h] [-v] [--file=FILENAME] [--update=FILENAME]
# The -h command line option shows the header of the index file
# The -v command line option shows a more verbose file list
# The --file command takes an argument, which file should be read.
# The -h and -v options are mutually exclusive.
# The --update command line option lets you update a single index entry.
# This option can be used to test the re-reading of part of the index. (it will
# have a built-in delay, to make the test easier)

# If no argument is given, the output is a list of all files in th index file
# including the path, sorted lexically. (The same format as git ls-files)
# (including stat data)
#
# The index v2/v3 can be converted to v5 using git-convert-index.py

import struct
import binascii
import sys
import os.path
import python.lib.indexlib as indexlib
import time
from collections import deque


class CRC(object):
	def __init__(self):
		self.value = 0

	def add(self, data):
		"""Add data into the checksum."""

		self.value = indexlib.calculate_crc(data, self.value)

	def matches(self, f):
		"""Verify a checksum from file f.

		Read the next 4 bytes from f and return True iff they match
		the checksum stored in this object."""

		return f.read(indexlib.CRC_STRUCT.size) == \
				indexlib.CRC_STRUCT.pack(self.value)


def write_calc_crc(fw, data, partialcrc=0):
    fw.write(data)
    crc = indexlib.calculate_crc(data, partialcrc)
    return crc


def read_struct(f, s, crc=None):
	""" Read a struct from f and update the crc for the data (if applicable).

	Args:
		f: the file from which the data should be read
		s: a struct.Struct describing the data to be read
		crc: an optional CRC instance, updated with the data
	Returns:
		a tuple containing the results of unpacking the data read.
	"""
	data = f.read(s.size)
	if crc is not None:
		crc.add(data)
	return s.unpack(data)


def read_header(f):
	""" Read the header of a index-v5 file

	Args:
		f: the file from which the header should be read
	Returns:
		a dict with all the header values
	Raises:
		SignatureError: the signature of the index file is wrong
		VersionError: the version of the index file is wrong
		CrcError: the crc code doesn't match with the contents that have been
			read
	"""
	min_header_size = (indexlib.HEADER_V5_STRUCT.size
			+ indexlib.CRC_STRUCT.size)

	if os.path.getsize(f.name) < min_header_size:
		raise indexlib.FilesizeError("Index file smaller than expected")

	crc = CRC()
	(signature, vnr, ndir, nfile, fblockoffset, nextensions) = read_struct(f,
			indexlib.HEADER_V5_STRUCT, crc)

	if signature != "DIRC":
		raise indexlib.SignatureError("Signature is not DIRC. Signature: " +
				signature)

	if vnr != 5:
		raise indexlib.VersionError("The index is not Version 5. Version: " +
				str(vnr))

	extoffsets = list()
	for i in xrange(nextensions):
		readoffset = read_struct(f,
				indexlib.EXTENSION_OFFSET_STRUCT.size, crc)
		extoffsets.append(readoffset)

	if not crc.matches(f):
		raise indexlib.CrcError("Wrong header crc")

	return dict(signature=signature, vnr=vnr, ndir=ndir, nfile=nfile,
			fblockoffset = fblockoffset, nextensions=nextensions,
			extoffsets=extoffsets)


def read_name(f, crc=None):
	""" Read a nul terminated name from the file

	Args:
		f: the file from which the name should be read. The method will start
			reading from where the file pointer in that file is at the moment.
		crc: an optional crc instance that should be updated with the data
			read.
	Returns:
		name: The name that was read
	"""
	name = ""
	while True:
		byte = f.read(1)
		if byte == '\0':
			break
		name += byte

	if crc is not None:
		crc.add(name + '\0')

	return name


def read_index_entries(f, header):
	""" Read all index entries in the index file

	Args:
		f: the file from which the index entries should be read.
		header: the header of the index file
	Returns:
		files: A list of all index entries
		fblockoffset: The offset to the first index entry
	"""
	# Skip header and directory offsets
	# Header size = 24 bytes, each extension offset and dir offset is 4 bytes
	f.seek(indexlib.HEADER_V5_STRUCT.size
			+ header["nextensions"] * indexlib.EXTENSION_OFFSET_STRUCT.size
			+ header["ndir"] * indexlib.DIR_OFFSET_STRUCT.size + 4
			+ indexlib.CRC_STRUCT.size)

	directories = read_dirs(f, header["ndir"])

	# The foffset only needs to be considered for the first directory, since
	# we read the files continously and have the file pointer always in the
	# right place. Doing so saves 2 seeks per directory.
	f.seek(header["fblockoffset"] + directories[0]["foffset"])

	files = dict()
	dirnr, fblockoffset = read_files(f, directories, f.tell(), 0, files)
	return files, fblockoffset


def read_file(f, pathname, fblockoffset):
	""" Read a single file from the index

	Args:
		f: the index file from which the file data should be read.
		pathname: the pathname of the file, with which the filename is
			combined to get the full path
		fblockoffset: the offset to the file block, used for calculating the
			crc code.
	Returns:
		A dict with all filedata
	Raises:
		CrcError: The crc code in the index file doesn't match with the crc
			code of the read data.
	"""
	crc = CRC()
	namestart = f.tell()
	# A little cheating here in favor of simplicity and execution speed.
	# The fileoffset is only read when really needed, in the other cases
	# it's just calculated from the file position, to save on reads and
	# simplify the code.
	crc.add(indexlib.FILE_OFFSET_STRUCT.pack(f.tell() - fblockoffset))

	filename = read_name(f, crc)

	(flags, mode, mtimes, mtimens, statcrc, objhash) = \
			read_struct(f, indexlib.FILE_DATA_STRUCT, crc)

	if not crc.matches(f):
		raise indexlib.CrcError("Wrong CRC for file entry: " + filename + str(crc.value))

	return dict(name=pathname + filename, flags=flags, mode=mode,
			mtimes=mtimes, mtimens=mtimens, statcrc=statcrc,
			objhash=binascii.hexlify(objhash), start=namestart)


def read_files(f, directories, fblockoffset, dirnr, files_out):
	""" Read all files from the index and combine them with their respective
	pathname. Files are read lexically ordered. The function does this
	recursively

	Args:
		f: the index file from which the files should be read
		directories: all directories that are in the index file
		fblockoffset: the offset to the fileentries block. This is used for
			calculating the crc code, so that the fileoffsets don't need to
			be read.
		dirnr: The files of the dirnrth directory are read. This has to be 0
			for the initial call.
		files_out: The list into which is used to store the files. Pass a
			empty list at the initial call. The list will contain all the files
			when the function returns.
	Returns:
		dirnr: The number of the directory that was used last. Of no use in the
			function that calls read_files
		fblockoffset: The offset to the first file in the index.
	"""
	queue = deque()
	for i in xrange(directories[dirnr]["nfiles"]):
		queue.append(read_file(f, directories[dirnr]["pathname"],
			fblockoffset))

	while queue:
		if (len(directories) > dirnr + 1 and
				queue[0]["name"] > directories[dirnr + 1]["pathname"]):
			dirnr, fblockoffset = read_files(f, directories,
					fblockoffset, dirnr + 1, files_out)
		else:
			new_file = queue.popleft()
			files_out[new_file["name"]] = new_file

	return dirnr, fblockoffset


def read_dir(f):
	""" Read a single directory from the index file.

	Args:
		f: The index file from which the directory data should be read
	Returns:
		A dict with all file data
	Raises:
		CrcError: The crc code in the file doesn't match with the crc code
			of the data that was read
	"""
	crc = CRC()
	pathname = read_name(f, crc)

	(foffset, cr, ncr, nsubtrees, nfiles, nentries, objname, flags) = \
			read_struct(f, indexlib.DIRECTORY_DATA_STRUCT, crc)

	if not crc.matches(f):
		raise indexlib.CrcError("Wrong crc for directory entry: " + pathname)

	return dict(pathname=pathname, flags=flags, foffset=foffset,
		cr=cr, ncr=ncr, nsubtrees=nsubtrees, nfiles=nfiles,
		nentries=nentries, objname=binascii.hexlify(objname))


def read_dirs(f, ndir):
	""" Read all directories from the index file.

	Args:
		f: The index file from which the directories should be read
		ndir: Number of directory that should be read
	Returns:
		A list of all directories in the index file
	"""
	dirs = list()
	for i in xrange(ndir):
		dirs.append(read_dir(f))

	return dirs


def print_header(header):
	print(indexlib.HEADER_V5_FORMAT % header)


def print_directories(directories):
	for d in directories:
		print (indexlib.DIRECTORY_FORMAT % d)


def print_files(files, verbose=False):
	for fi in sorted(files.itervalues()):
		if verbose:
			print (indexlib.FILES_FORMAT % fi + hex(fi["statcrc"]))
		else:
			print fi["name"]


def update_index_entry(fw, filename, files, fblockoffset):
	""" Update a single index entry and set its mtime to 0.
	
	Since it's just for testing purposes anyway, we set it to 0 and thus
	emulate a racy entry. Before updating the crc sum too we wait for a
	little while to test the re-reading capabilities of the reader of
	index-v5.

	Args:
		filename: The file that should be updated
		files: The dict of all files, which gives us the offset
			to the file that shall be updated.
		fblockoffset: The offset to the first file in the index to
			calculate the crc sum.
	"""

	f = files[filename]
	fw.seek(f["start"])
	partialcrc = indexlib.calculate_crc(indexlib.OFFSET_STRUCT.pack(f["start"]
								- fblockoffset))
	partialcrc = write_calc_crc(fw, os.path.basename(filename) + "\0",
			partialcrc)

	data = indexlib.FILE_DATA_STRUCT.pack(f["flags"], f["mode"], 0, 0,
			f["statcrc"], binascii.unhexlify(f["objhash"]))
	partialcrc = write_calc_crc(fw, data, partialcrc)
	# Fsync is needed to really make the crc wrong, otherwise the file + crc
	# will only be written when the file descriptor is closed
	fw.flush()
	os.fsync(fw.fileno())
	time.sleep(1)
	fw.write(indexlib.CRC_STRUCT.pack(partialcrc))


def main(args):
	f = None
	pheader = False
	pverbose = False
	to_update = None
	for arg in args:
		if arg == "-h":
			pheader = True
		if arg == "-v":
			pverbose = True
		if arg[:7] == '--file=':
			f = open(arg[7:], "rb")
		if arg[:9] == '--update=':
			to_update = arg[9:]

	if not f:
		f = open(".git/index", "rb+")

	header = read_header(f)

	files, fblockoffset = read_index_entries(f, header)
	if (to_update):
		update_index_entry(f, to_update, files, fblockoffset)
	if pheader:
		print_header(header)
	# else:
	# 	print_files(files, pverbose)

if __name__ == "__main__":
	main(sys.argv[1:])
