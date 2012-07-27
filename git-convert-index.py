#!/usr/bin/env python

<<<<<<< HEAD
# Usage: python git-convert-index.py
# Command line options (They all work on the v2/v3 index file)
# The -h option shows the header of the index file
# The -i options shows all index entries in the file. (git ls-files --debug
#   format)
# The -c options shows the cache-tree data (test-dump-cache-tree format
# The -u options shows all data that was in the REUC Extension

# Read the index format with git-read-index-v5.py
# read-index-v5 outputs the same format as git ls-files


=======
# Outputs: Calculated and read sha1 checksum in hex format
# Usage: python git-convert-index.py
# Read the index format with git-read-index-v5.py
# read-index-v5 outputs the same format as git ls-files

>>>>>>> origin/ce_namelen
import hashlib
import binascii
import struct
import os.path
<<<<<<< HEAD
import sys
import python.lib.indexlib as indexlib
from collections import defaultdict


class Reader():
    def __init__(self):
        self._sha1 = hashlib.sha1()
        self._f = open(".git/index", "rb")

    def read(self, n):
        data = self._f.read(n)
        self._sha1.update(data)
        return data

    def read_without_updating_sha1(self, n):
        return self._f.read(n)

    def tell(self):
        return self._f.tell()

    def updateSha1(self, data):
        self._sha1.update(data)

    def getSha1(self):
        return self._sha1

class Header:
    def __init__(self, signature, version, nrofentries):
        self.signature = signature
        self.version = version
        self.nrofentries = nrofentries


class IndexEntry:
    def __init__(self, ctimesec, ctimensec, mtimesec, mtimensec, dev, ino,
            mode, uid, gid, filesize, sha1, flags, pathname, filename,
            xtflags = None):
        self.ctimesec  = ctimesec
        self.ctimensec = ctimensec
        self.mtimesec  = mtimesec
        self.mtimensec = mtimensec
        self.dev       = dev
        self.ino       = ino
        self.mode      = mode
        self.uid       = uid
        self.gid       = gid
        self.filesize  = filesize
        self.sha1      = sha1
        self.flags     = flags
        self.pathname  = pathname
        self.filename  = filename
        self.xtflags   = xtflags


class TreeExtensionData:
    def __init__(self, path, entry_count, subtrees, sha1 = "invalid"):
        self.path        = path
        self.entry_count = entry_count
        self.subtrees    = subtrees
        self.sha1        = sha1


class ReucExtensionData:
    def __init__(self, path, entry_mode0, entry_mode1, entry_mode2, obj_name0,
            obj_name1, obj_name2):
        self.path        = path
        self.entry_mode0 = entry_mode0
        self.entry_mode1 = entry_mode1
        self.entry_mode2 = entry_mode2
        self.obj_name0  = obj_name0
        self.obj_name1  = obj_name1
        self.obj_name2  = obj_name2

    def nconflicts(self):
        if self.entry_mode2 != 0:
            return 3
        if self.entry_mode1 != 0:
            return 2

        return 1


class DirEntry:
    def __init__(self, nfiles = 0, flags = 0, cr = 0, ncr = 0, nsubtrees = 0,
            nentries = 0, objname = 20 * '\0'):
        self.nfiles = nfiles
        self.flags = flags
        self.cr = cr
        self.ncr = ncr
        self.nsubtrees = nsubtrees
        self.nentries = nentries
        self.objname = objname


def write_calc_crc(fw, data, partialcrc=0):
    fw.write(data)
    crc = indexlib.calculate_crc(data, partialcrc)
    return crc


def read_name(r, delimiter):
    string = ""
    byte = r.read(1)
    readbytes = 1
    while byte != delimiter:
        string = string + byte
        byte = r.read(1)
        readbytes += 1
    return string, readbytes


def read_header(r):
    (signature, version, nrofentries) = indexlib.HEADER_STRUCT.unpack(
            r.read(indexlib.HEADER_STRUCT.size))
    return Header(signature, version, nrofentries)


def read_entry(r, header):
    (ctimesec, ctimensec, mtimesec, mtimensec, dev, ino, mode, uid, gid,
            filesize, sha1, flags) = indexlib.STAT_DATA_STRUCT.unpack(
                    r.read(indexlib.STAT_DATA_STRUCT.size))

    if header.version == 3:
        xtflags = indexlib.XTFLAGS_STRUCT.unpack(r.read(
            indelib.XTFLAGS_STRUCT.size))

    (name, readbytes) = read_name(r, '\0')

    pathname = os.path.dirname(name)
    filename = os.path.basename(name)

    entry = IndexEntry(ctimesec, ctimensec, mtimesec, mtimensec, dev, ino,
            mode, uid, gid, filesize, sha1, flags, pathname, filename)

    if (header.version == 3):
        entry.xtflags = xtflags

    if header.version == 2:
        j = 8 - (readbytes + 5) % 8
    else:
        j = 8 - (readbytes + 1) % 8

    # Just throw the padding away.
    r.read(j - 1)

    return entry


def read_index_entries(r, header):
=======
from collections import defaultdict

#  fread {{{
f = open(".git/index", "rb")
filedata = list()


def fread(n):
    global filedata
    data = f.read(n)
    filedata.append(data)
    return data
# }}}


# fwrite {{{
fw = open(".git/index-v5", "wb")
writtenbytes = 0
writtendata = list()


def fwrite(data):
    global writtenbytes
    global writtendata
    writtendata.append(data)
    writtenbytes += len(data)
    fw.write(data)
# }}}


# convert {{{
def convert(n):
    return str(struct.unpack('!I', n)[0])
# }}}


# readheader {{{
def readheader(f):
    # Signature
    signature = fread(4)
    header = struct.unpack('!II', fread(8))
    return dict({"signature": signature, "version": header[0], "nrofentries": header[1]})
# }}}


# readindexentries {{{
def readindexentries(f):
>>>>>>> origin/ce_namelen
    indexentries = []
    conflictedentries = defaultdict(list)
    paths = set()
    files = list()
<<<<<<< HEAD
    # Read index entries
    for i in xrange(header.nrofentries):
        entry = read_entry(r, header)

        for p in indexlib.get_sub_paths(entry.pathname):
            paths.add(p)
        files.append(entry.filename)

        stage = (entry.flags & 0b0011000000000000) / 0b001000000000000

        if stage == 0:      # Not conflicted
            indexentries.append(entry)
        else:                   # Conflicted
            if stage == 1:
                # Write the stage 1 entry to the main index, to avoid
                # rewriting the whole index once the conflict is resolved
                indexentries.append(entry)
            else:
                conflictedentries[entry.pathname].append(entry)

    return indexentries, conflictedentries, paths, files


def read_tree_extensiondata(r):
    extensionsize = r.read(4)
=======
    filedirs = defaultdict(list)
    byte = fread(1)
    i = 0
    # Read index entries
    while i < header["nrofentries"]:
        entry = struct.unpack('!IIIIIIIIII', byte + fread(39))  # stat data
        entry = entry + (str(binascii.hexlify(fread(20))),)     # SHA-1

        if (header["version"] == 3):
            entry = entry + struct.unpack('!hh', fread(4))      # Flags + extended flags
        else:
            entry = entry + struct.unpack('!h', fread(2))       # Flags

        string = ""
        byte = fread(1)
        readbytes = 1
        while byte != '\0':
            string = string + byte
            byte = fread(1)
            readbytes += 1

        pathname = os.path.dirname(string)
        filename = os.path.basename(string)
        paths.add(pathname)
        files.append(filename)
        filedirs[pathname].append(filename)

        entry = entry + (pathname, filename)           # Filename

        if (header["version"] == 3):
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec',
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'xtflags', 'pathname', 'filename'), entry))
        else:
            dictentry = dict(zip(('ctimesec', 'ctimensec', 'mtimesec', 'mtimensec',
                'dev', 'ino', 'mode', 'uid', 'gid', 'filesize', 'sha1', 'flags',
                'pathname', 'filename'), entry))

        if header["version"] == 2:
            j = 8 - (readbytes + 5) % 8
        else:
            j = 8 - (readbytes + 1) % 8

        while byte == '\0' and j > 0:
            byte = fread(1)
            j -= 1

        stage = (entry[11] & 0b0011000000000000) / 0b001000000000000

        if stage == 0:      # Not conflicted
            indexentries.append(dictentry)
        else:                   # Conflicted
            if stage == 1:  # Write the stage 1 entry to the main index, to avoid rewriting the whole index once the conflict is resolved
                indexentries.append(dictentry)
            conflictedentries[pathname].append(dictentry)

        i = i + 1

    return indexentries, conflictedentries, byte, paths, files, filedirs
# }}}


# readextensiondata {{{
def readextensiondata(f):
    extensionsize = fread(4)

>>>>>>> origin/ce_namelen
    read = 0
    subtreenr = [0]
    subtree = [""]
    listsize = 0
    extensiondata = dict()
<<<<<<< HEAD
    while read < int(indexlib.SIZE_STRUCT.unpack(extensionsize)[0]):
        (path, readbytes) = read_name(r, '\0')
        read += readbytes
=======
    while read < int(convert(extensionsize)):
        path = ""
        byte = fread(1)
        read += 1
        while byte != '\0':
            path += byte
            byte = fread(1)
            read += 1
>>>>>>> origin/ce_namelen

        while listsize >= 0 and subtreenr[listsize] == 0:
            subtreenr.pop()
            subtree.pop()
            listsize -= 1

        fpath = ""
        if listsize > 0:
            for p in subtree:
                if p != "":
                    fpath += p + "/"
            subtreenr[listsize] = subtreenr[listsize] - 1
        fpath += path + "/"

<<<<<<< HEAD
        (entry_count, readbytes) = read_name(r, " ")
        read += readbytes

        (subtrees, readbytes) = read_name(r, "\n")
        read += readbytes
=======
        entry_count = ""
        byte = fread(1)
        read += 1
        while byte != " ":
            entry_count += byte
            byte = fread(1)
            read += 1

        subtrees = ""
        byte = fread(1)
        read += 1
        while byte != "\n":
            subtrees += byte
            byte = fread(1)
            read += 1
>>>>>>> origin/ce_namelen

        subtreenr.append(int(subtrees))
        subtree.append(path)
        listsize += 1

        if entry_count != "-1":
<<<<<<< HEAD
            sha1 = r.read(20)
=======
            sha1 = binascii.hexlify(fread(20))
>>>>>>> origin/ce_namelen
            read += 20
        else:
            sha1 = "invalid"

<<<<<<< HEAD
        extensiondata[fpath] = TreeExtensionData(fpath, entry_count, subtrees,
                sha1)

    return extensiondata


def read_reuc_extension_entry(r):
    (path, readbytes) = read_name(r, '\0')
    read = readbytes

    entry_mode = list()
    i = 0
    while i < 3:
        (mode, readbytes) = read_name(r, '\0')
        read += readbytes
        i += 1

        entry_mode.append(int(mode, 8))

    obj_names = list()
    for i in xrange(3):
        if entry_mode[i] != 0:
            obj_names.append(r.read(20))
            read += 20
        else:
            obj_names.append("")

    return ReucExtensionData(path, entry_mode[0], entry_mode[1],
            entry_mode[2], obj_names[0], obj_names[1], obj_names[2]), read


def read_reuc_extensiondata(r):
    extensionsize = r.read(4)

    read = 0
    extensiondata = defaultdict(list)
    while read < int(indexlib.SIZE_STRUCT.unpack(extensionsize)[0]):
        (entry, readbytes) = read_reuc_extension_entry(r)
        read += readbytes
        extensiondata["/".join(entry.path.split("/")[:-1])].append(entry)

    return extensiondata


def print_header(header):
    print indexlib.HEADER_FORMAT % {"signature": header.signature,
            "version": header.version, "nrofentries": header.nrofentries}


def print_indexentries(indexentries):
    for entry in indexentries:
        if entry.pathname != "":
            print entry.pathname + "/" + entry.filename
        else:
            print entry.filename
        print indexlib.ENTRIES_FORMAT % {"ctimesec": entry.ctimesec,
                "ctimensec": entry.ctimensec, "mtimesec": entry.mtimesec,
                "mtimensec": entry.mtimensec, "dev": entry.dev,
                "ino": entry.ino, "uid": entry.uid, "gid": entry.gid,
                "filesize": entry.filesize} + "%x" % entry.flags


def print_extensiondata(extensiondata):
    for entry in sorted(extensiondata.itervalues()):
        dictentry = {"sha1": entry.sha1, "path": entry.path,
                "entry_count": entry.entry_count, "subtrees": entry.subtrees}
        try:
            print indexlib.EXTENSION_FORMAT % dictentry
        except KeyError:
            print indexlib.EXTENSION_FORMAT_WITHOUT_SHA % dictentry


def print_reucextensiondata(extensiondata):
    if extensiondata:
        for (path, data) in extensiondata.iteritems():
            for e in data:
                print indexlib.REUCEXTENSION_FORMAT % {"path": e.path,
                        "entry_mode0": e.entry_mode0, "entry_mode1":
                        e.entry_mode1, "entry_mode2": e.entry_mode2}
                print ("Objectnames 1: " + binascii.hexlify(e.obj_names0) +
                        " Objectnames 2: " + binascii.hexlify(e.obj_names1) +
                        " Objectnames 3: " + binascii.hexlify(e.obj_names2))


def write_header(fw, header, paths, files, fblockoffset):
    # The header is at the beginning ;-)
    fw.seek(0)
    # Offset to the file block has to be filled in later
    crc = write_calc_crc(fw, indexlib.HEADER_V5_STRUCT.pack(header.signature, 5,
        len(paths), len(files), fblockoffset, 0))
    fw.write(indexlib.CRC_STRUCT.pack(crc))


def skip_header_dir_offsets(fw, paths):
    # Just seek over the unneeded stuff
    fw.seek(indexlib.HEADER_V5_STRUCT.size
            + indexlib.CRC_STRUCT.size
            + len(paths) * indexlib.DIR_OFFSET_STRUCT.size)


def write_directories(fw, paths):
    diroffsets = list()
    dirwritedataoffsets = dict()
    # Directory offsets relative to the start of the directory block
    beginning = fw.tell()
    for p in sorted(paths, key=lambda k: k + "/"):
        diroffsets.append(fw.tell() - beginning)

        # pathname
        if p == "":
            fw.write("\0")
        else:
            fw.write(p + "/\0")

        dirwritedataoffsets[p] = fw.tell()
=======
        if sha1 == "invalid":
            extensiondata[fpath] = dict({"path": fpath, "entry_count": entry_count,
            "subtrees": subtrees})
        else:
            extensiondata[fpath] = dict({"path": fpath, "entry_count": entry_count,
            "subtrees": subtrees, "sha1": sha1})

    return extensiondata
# }}}


# readreucextensiondata {{{
def readreucextensiondata(f):
    extensionsize = fread(4)

    read = 0
    extensiondata = defaultdict(list)
    while read < int(convert(extensionsize)):
        path = ""
        byte = fread(1)
        read += 1
        while byte != '\0':
            path += byte
            byte = fread(1)
            read += 1

        entry_mode = list()
        i = 0
        while i < 3:
            byte = fread(1)
            read += 1
            mode = ""
            while byte != '\0':
                mode += byte
                byte = fread(1)
                read += 1
            i += 1

            entry_mode.append(int(mode, 8))

        i = 0
        obj_names = list()
        while i < 3:
            if entry_mode[i] != 0:
                obj_names.append(fread(20))
                read += 20
            else:
                obj_names.append("")
            i += 1

        extensiondata["/".join(path.split("/"))[:-1]].append(dict({"path": path, "entry_mode0": entry_mode[0], "entry_mode1": entry_mode[1], "entry_mode2": entry_mode[2], "obj_names0": obj_names[0], "obj_names1": obj_names[1], "obj_names2": obj_names[2]}))

    return extensiondata

# }}}


# printheader {{{
def printheader(header):
    print "Signature: " + header["signature"]
    print "Version: " + str(header["version"])
    print "Number of entries: " + str(header["nrofentries"])
# }}}


# printindexentries {{{
def printindexentries(indexentries):
    for entry in indexentries:
        if entry["pathname"] != "":
            print entry["pathname"] + "/" + entry["filename"]
        else:
            print entry["filename"]
        print "  ctime: " + str(entry["ctimesec"]) + ":" + str(entry["ctimensec"])
        print "  mtime: " + str(entry["mtimesec"]) + ":" + str(entry["mtimensec"])
        print "  dev: " + str(entry["dev"]) + "\tino: " + str(entry["ino"])
        print "  uid: " + str(entry["uid"]) + "\tgid: " + str(entry["gid"])
        print "  size: " + str(entry["filesize"]) + "\tflags: " + "%x" % entry["flags"]
# }}}


# {{{ printextensiondata
def printextensiondata(extensiondata):
    for entry in extensiondata.viewvalues():
        print entry["sha1"] + " " + entry["path"] + " (" + entry["entry_count"] + " entries, " + entry["subtrees"] + " subtrees)"
# }}}


# printreucextensiondata {{{
def printreucextensiondata(extensiondata):
    for e in extensiondata:
        print "Path: " + e["path"]
        print "Entrymode 1: " + str(e["entry_mode0"]) + " Entrymode 2: " + str(e["entry_mode1"]) + " Entrymode 3: " + str(e["entry_mode2"])
        print "Objectnames 1: " + binascii.hexlify(e["obj_names0"]) + " Objectnames 2: " + binascii.hexlify(e["obj_names1"]) + " Objectnames 3: " + binascii.hexlify(e["obj_names2"])
# }}}


# Write stuff for index-v5 draft0 {{{
# writev5_0header {{{
def writev5_0header(header, paths, files):
    fwrite(header["signature"])
    fwrite(struct.pack("!IIIIQ", header["version"], len(paths), len(files), header["nrofentries"], 0))
# }}}


# writev5_0directories {{{
def writev5_0directories(paths, treeextensiondata):
    offsets = dict()
    subtreenr = dict()
    # Calculate subtree numbers
    for p in sorted(paths, reverse=True):
        splited = p.split("/")
        if p not in subtreenr:
            subtreenr[p] = 0
        if len(splited) > 1:
            i = 0
            path = ""
            while i < len(splited) - 1:
                path += "/" + splited[i]
                i += 1
            if path[1:] not in subtreenr:
                subtreenr[path[1:]] = 1
            else:
                subtreenr[path[1:]] += 1

    for p in paths:
        offsets[p] = writtenbytes
        fwrite(struct.pack("!Q", 0))
        fwrite(p.split("/")[-1] + "\0")
        p += "/"
        if p in treeextensiondata:
            fwrite(struct.pack("!ll", int(treeextensiondata[p]["entry_count"]), int(treeextensiondata[p]["subtrees"])))
            if (treeextensiondata[p]["entry_count"] != "-1"):
                fwrite(binascii.unhexlify(treeextensiondata[p]["sha1"]))

        else:  # If there is no cache-tree data we assume the entry is invalid
            fwrite(struct.pack("!ii", -1, subtreenr[p.strip("/")]))
    return offsets
# }}}


# writev5_0files {{{
def writev5_0files(files):
    offsets = dict()
    for f in files:
        offsets[f] = writtenbytes
        fwrite(f)
        fwrite("\0")
    return offsets
# }}}


# writev5_0fileentries {{{
def writev5_0fileentries(entries, fileoffsets):
    offsets = dict()
    for e in sorted(entries, key=lambda k: k['pathname']):
        if e["pathname"] not in offsets:
            offsets[e["pathname"]] = writtenbytes
        fwrite(struct.pack("!IIIIIIIIII", e["ctimesec"], e["ctimensec"],
            e["mtimesec"], e["mtimensec"], e["dev"], e["ino"], e["mode"],
            e["uid"], e["gid"], e["filesize"]))
        fwrite(binascii.unhexlify(e["sha1"]))

        try:
            fwrite(struct.pack("!III", e["flags"], e["xtflags"],
                fileoffsets[e["filename"]]))
        except KeyError:
            fwrite(struct.pack("!III", e["flags"], 0,
                fileoffsets[e["filename"]]))

        writecrc32()
    return offsets
# }}}


# writev5_0fileoffsets {{{
def writev5_0fileoffsets(diroffsets, fileoffsets, dircrcoffset):
    for d in sorted(diroffsets):
        fw.seek(diroffsets[d])
        fw.write(struct.pack("!Q", fileoffsets[d]))

    # Calculate crc32
    #f.seek(0)
    #data = f.read(dircrcoffset)
    fw.seek(dircrcoffset)
    fw.write(struct.pack("!I", binascii.crc32(writtendata) & 0xffffffff))

# }}}


# writev5_0reucextensiondata {{{
def writev5_0reucextensiondata(data):
    global writtenbytes
    offset = writtenbytes
    for d in data:
        fwrite(d["path"])
        fwrite("\0")
        stages = set()
        fwrite(struct.pack("!b", 0))
        for i in xrange(0, 2):
            fwrite(struct.pack("!i", d["entry_mode" + str(i)]))
            if d["entry_mode" + str(i)] != 0:
                stages.add(i)

        for i in sorted(stages):
            fwrite(d["obj_names" + str(i)])
    writecrc32()
    fw.seek(20)
    fw.write(struct.pack("!Q", offset))
# }}}


# writev5_0conflicteddata {{{
def writev5_0conflicteddata(conflicteddata):
    print "Not implemented yet"
# }}}

# }}}


# Write stuff for index-v5 draft1 {{{
# Write header {{{
def writev5_1header(header, paths, files):
    fwrite(header["signature"])
    fwrite(struct.pack("!IIII", 5, len(paths), len(files), 0))
# }}}


# Write fake directory offsets which can only be filled in later {{{
def writev5_1fakediroffsets(paths):
    for p in paths:
        fwrite(struct.pack("!I", 0))
# }}}


# Write directories {{{
def writev5_1directories(paths):
    diroffsets = list()
    dirwritedataoffsets = dict()
    dirdata = defaultdict(dict)
    for p in sorted(paths):
        diroffsets.append(writtenbytes)

        # pathname
        if p == "":
            fwrite("\0")
        else:
            fwrite(p + "/\0")

        dirwritedataoffsets[p] = writtenbytes
>>>>>>> origin/ce_namelen

        # flags, foffset, cr, ncr, nsubtrees, nfiles, nentries, objname, dircrc
        # All this fields will be filled out when the rest of the index
        # is written
<<<<<<< HEAD
        # CRC will be calculated when data is filled in
        fw.write(indexlib.DIRECTORY_DATA_STRUCT.pack(0, 0, 0, 0, 0, 0,
            20 * '\0', 0))
        fw.write(indexlib.CRC_STRUCT.pack(0))

    return diroffsets, dirwritedataoffsets


def write_fake_file_offsets(fw, indexentries):
    beginning = fw.tell()
    fw.seek(beginning + len(indexentries) * 4)
    return beginning


def write_dir_offsets(fw, offsets):
    # Skip the header, which currently doesn't include any extensions
    fw.seek(indexlib.HEADER_V5_STRUCT.size
            + indexlib.CRC_STRUCT.size)
    for o in offsets:
        fw.write(indexlib.DIR_OFFSET_STRUCT.pack(o))


def write_file_entry(fw, entry, offset):
    partialcrc = indexlib.calculate_crc(indexlib.OFFSET_STRUCT.pack(offset))
    partialcrc = write_calc_crc(fw, entry.filename + "\0", partialcrc)

    # Prepare flags
    flags = entry.flags & 0b1000000000000000
    flags += (entry.flags & 0b0011000000000000)

    # calculate crc for stat data
    stat_crc = indexlib.calculate_crc(indexlib.STAT_DATA_CRC_STRUCT.pack(entry.ctimesec,
        entry.ctimensec, entry.ino, entry.filesize, entry.dev, entry.uid,
        entry.gid))

    stat_data = indexlib.FILE_DATA_STRUCT.pack(flags, entry.mode,
            entry.mtimesec, entry.mtimensec, stat_crc, entry.sha1)
    partialcrc = write_calc_crc(fw, stat_data, partialcrc)

    fw.write(indexlib.CRC_STRUCT.pack(partialcrc))


def write_file_data(fw, indexentries):
    dirdata = dict()
    fileoffsets = list()
    beginning = fw.tell()
    for entry in sorted(indexentries, key=lambda k: k.pathname + "/"):
        offset = fw.tell() - beginning
        fileoffsets.append(offset)
        write_file_entry(fw, entry, offset)
        if entry.pathname not in dirdata:
            # Add empty directories to the index too
            for p in indexlib.get_sub_paths(entry.pathname):
                if p not in dirdata:
                    dirdata[p] = DirEntry()

        dirdata[entry.pathname].nfiles += 1

    return fileoffsets, dirdata, beginning


def write_file_offsets(fw, foffsets, fileoffsetbeginning):
    fw.seek(fileoffsetbeginning)
    for f in foffsets:
        fw.write(indexlib.OFFSET_STRUCT.pack(f))


def write_directory_data(fw, dirdata, dirwritedataoffsets,
        fileoffsetbeginning):
    foffset = 0
    for (pathname, entry) in sorted(dirdata.iteritems()):
        try:
            fw.seek(dirwritedataoffsets[pathname])
        except KeyError:
            continue

        if pathname == "":
            partialcrc = indexlib.calculate_crc(pathname + "\0")
        else:
            partialcrc = indexlib.calculate_crc(pathname + "/\0")


        partialcrc = write_calc_crc(fw,
                indexlib.DIRECTORY_DATA_STRUCT.pack(foffset,
                entry.cr, entry.ncr, entry.nsubtrees, entry.nfiles,
                entry.nentries, entry.objname, entry.flags), partialcrc)

        foffset += entry.nfiles * 4

        fw.write(indexlib.CRC_STRUCT.pack(partialcrc))


def write_conflicted_data(fw, conflictedentries, reucdata, dirdata):
    todo = set()
    for d in dirdata:
        for c in conflictedentries:
            if c in d:
                todo.add(c)
                break
        for r in reucdata:
            if r in d:
                todo.add(r)
                break

    for t in todo:
        while conflictedentries[t]:
            entries = list()
            entry = conflictedentries[t].pop()
            entries.append(entry)

            if dirdata[t].cr == 0:
                dirdata[t].cr = fw.tell()

            dirdata[t].ncr += 1

            for i in xrange(len(conflictedentries[t])):
                e = conflictedentries[t].pop()
                if entry.pathname in e.filename:
                    entries.append(e)
                else:
                    conflictedentries[t].append(e)
            
            if entry.pathname == "":
                pathname = entry.filename + "\0"
            else:
                pathname = entry.pathname + "/" + entry.filename + "\0"

            crc = write_calc_crc(fw, pathname)
            crc = write_calc_crc(fw,
                    indexlib.NR_CONFLICT_STRUCT.pack(len(entries)), crc)
            conflicted = 0b1000000000000000
            for e in sorted(entries, key=lambda k: k.flags):
                flags = (e.flags & 0b0011000000000000) << 1
                flags |= conflicted
                crc = write_calc_crc(fw, indexlib.CONFLICT_STRUCT.pack(flags,
                    e.mode, e.sha1), crc)

            fw.write(indexlib.CRC_STRUCT.pack(crc))

        for r in reucdata[t]:
            crc = write_calc_crc(fw, r.path + "\0")
            crc = write_calc_crc(fw,
                    indexlib.NR_CONFLICT_STRUCT.pack(r.nconflicts()), crc)
            flags = 0
            if r.entry_mode0 != 0:
                crc = write_calc_crc(fw, indexlib.CONFLICT_STRUCT.pack(flags,
                    r.entry_mode0, r.obj_name0))

            if r.entry_mode1 != 0:
                crc = write_calc_crc(fw, indexlib.CONFLICT_STRUCT.pack(flags,
                    r.entry_mode1, r.obj_name1))

            if r.entry_mode2 != 0:
                crc = write_calc_crc(fw, indexlib.CONFLICT_STRUCT.pack(flags,
                    r.entry_mode2, r.obj_name2))
            fw.write(indexlib.CRC_STRUCT.pack(crc))

    return dirdata


def write_fblockoffset(fw, fblockoffset):
    fw.seek(16)
    fw.write(indexlib.FBLOCK_OFFSET_STRUCT.pack(fblockoffset))


def compile_cache_tree_data(dirdata, extensiondata):
    for (path, entry) in extensiondata.iteritems():
        dirdata[path.strip("/")].nentries = \
                int(entry.entry_count)

        dirdata[path.strip("/")].nsubtrees = \
                int(entry.subtrees)

        if entry.sha1 != "invalid":
            dirdata[path.strip("/")].objname = entry.sha1

    return dirdata


def read_index():
    r = Reader()
    header = read_header(r)

    (indexentries, conflictedentries, paths, files) = read_index_entries(r,
            header)

    treeextensiondata = dict()
    reucextensiondata = defaultdict(list)
    ext = r.read_without_updating_sha1(4)

    if ext == "TREE" or ext == "REUC":
        r.updateSha1(ext)
        if ext == "TREE":
            treeextensiondata = read_tree_extensiondata(r)
        else:
            reucextensiondata = read_reuc_extensiondata(r)
        ext = r.read_without_updating_sha1(4)

        if ext == "REUC":
            r.updateSha1(ext)
            reucextensiondata = read_reuc_extensiondata(r)

    sha1 = r.getSha1()

    if ext == "TREE" or ext == "REUC":
        sha1read = r.read_without_updating_sha1(20)
    else:
        sha1read = ext + r.read_without_updating_sha1(16)

    if sha1.hexdigest() != binascii.hexlify(sha1read):
        raise indexlib.SHAError("SHA-1 code of the file doesn't match")

    return (header, indexentries, conflictedentries, paths, files,
            treeextensiondata, reucextensiondata)


def write_index_v5(header, indexentries, conflictedentries, paths, files,
        treeextensiondata, reucextensiondata):
    fw = open(".git/index-v5", "wb")

    skip_header_dir_offsets(fw, paths)
    (diroffsets, dirwritedataoffsets) = write_directories(fw,
            paths)

    fileoffsetbeginning = write_fake_file_offsets(fw, indexentries)
    (fileoffsets, dirdata, fblockoffset) = write_file_data(fw, indexentries)

    dirdata = write_conflicted_data(fw, conflictedentries,
            reucextensiondata, dirdata)

    write_header(fw, header, paths, files, fblockoffset)
    write_dir_offsets(fw, diroffsets)
    write_file_offsets(fw, fileoffsets, fileoffsetbeginning)

    dirdata = compile_cache_tree_data(dirdata, treeextensiondata)
    write_directory_data(fw, dirdata, dirwritedataoffsets,
            fileoffsetbeginning)


def main(args):
    (header, indexentries, conflictedentries, paths, files, treeextensiondata,
            reucextensiondata) = read_index()

    for a in args:
        if a == "-h":
            print_header(header)
        if a == "-i":
            print_indexentries(indexentries)
        if a == "-c":
            print_extensiondata(treeextensiondata)
        if a == "-u":
            print_reucextensiondata(reucextensiondata)

    write_index_v5(header, indexentries, conflictedentries, paths, files,
            treeextensiondata, reucextensiondata)

if __name__ == "__main__":
    main(sys.argv[1:])
=======
        fwrite(struct.pack("!HIIIIIIIIIIII", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))

        # Subtreenr for later usage
        if p != "":
            path = p.split("/")
            try:
                dirdata["/".join(path[:-1])]["nsubtrees"] += 1
            except KeyError:
                dirdata["/".join(path[:-1])]["nsubtrees"] = 1

    return diroffsets, dirwritedataoffsets, dirdata
# }}}


# Write fake file offsets {{{
def writev5_1fakefileoffsets(indexentries):
    beginning = writtenbytes
    for f in indexentries:
        fwrite(struct.pack("!I", 0))
    return beginning
# }}}


# Write directory offsets for real {{{
def writev5_1diroffsets(offsets):
    fw.seek(24)
    for o in offsets:
        fw.write(struct.pack("!I", o))
# }}}


# Write file data {{{
def writev5_1filedata(indexentries, dirdata):
    global writtenbytes
    global writtendata
    fileoffsets = list()
    writtendata = list()
    for entry in sorted(indexentries, key=lambda k: k['pathname']):
        offset = writtenbytes
        fileoffsets.append(offset)
        writtendata.append(struct.pack("!I", fw.tell()))
        fwrite(entry["filename"] + "\0")

        # Prepare flags
        # TODO: Consider extended flags
        flags = entry["flags"] & 0b1000000000000000
        flags += (entry["flags"] & 0b0011000000000000) * 2
        fwrite(struct.pack("!H", flags))

        # mode
        fwrite(struct.pack("!H", entry["mode"]))

        # mtime
        fwrite(struct.pack("!I", entry["mtimesec"]))
        fwrite(struct.pack("!I", entry["mtimensec"]))

        # calculate crc for stat data
        crc = binascii.crc32(struct.pack("!IIIIIIII", offset, entry["ctimesec"], entry["ctimensec"], entry["ino"], entry["filesize"], entry["dev"], entry["uid"], entry["gid"]))
        fwrite(struct.pack("!i", crc))

        fwrite(binascii.unhexlify(entry["sha1"]))

        writecrc32()
        try:
            dirdata[entry["pathname"]]["nfiles"] += 1
        except KeyError:
            dirdata[entry["pathname"]]["nfiles"] = 1

    return fileoffsets, dirdata

# }}}


# Write file offsets for read {{{
def writev5_1fileoffsets(foffsets, fileoffsetbeginning):
    fw.seek(fileoffsetbeginning)
    for f in foffsets:
        fw.write(struct.pack("!I", f))
# }}}


# Write correct directory data {{{
def writev5_1directorydata(dirdata, dirwritedataoffsets, fileoffsetbeginning):
    global writtendata
    foffset = fileoffsetbeginning
    for d in sorted(dirdata.iteritems()):
        try:
            fw.seek(dirwritedataoffsets[d[0]])
        except KeyError:
            continue
        if d[0] == "":
            writtendata = [d[0] + "\0"]
        else:
            writtendata = [d[0] + "/\0"]
        try:
            nsubtrees = d[1]["nsubtrees"]
        except KeyError:
            nsubtrees = 0

        try:
            nfiles = d[1]["nfiles"]
        except KeyError:
            nfiles = 0

        try:
            fwrite(struct.pack("!H", d[1]["flags"]))
        except KeyError:
            fwrite(struct.pack("!H", 0))

        if nfiles == -1 or nfiles == 0:
            fwrite(struct.pack("!I", 0))
        else:
            fwrite(struct.pack("!I", foffset))
            foffset += (nfiles) * 4

        try:
            fwrite(struct.pack("!I", d[1]["cr"]))
        except KeyError:
            fwrite(struct.pack("!I", 0))

        try:
            fwrite(struct.pack("!I", d[1]["ncr"]))
        except KeyError:
            fwrite(struct.pack("!I", 0))

        fwrite(struct.pack("!I", nsubtrees))
        fwrite(struct.pack("!I", nfiles))

        try:
            fwrite(struct.pack("!i", d[1]["nentries"]))
        except KeyError:
            fwrite(struct.pack("!I", 0))

        try:
            fwrite(binascii.unhexlify(d[1]["objname"]))
        except KeyError:
            fwrite(struct.pack("!IIIII", 0, 0, 0, 0, 0))

        writecrc32()
# }}}


# Write conflicted data {{{
def writev5_1conflicteddata(conflictedentries, reucdata, dirdata):
    global writtenbytes
    for d in sorted(conflictedentries):
        for f in d:
            if d["pathname"] == "":
                filename = d["filename"]
            else:
                filename = d["pathname"] + "/" + d["filename"]

            dirdata[filename]["cr"] = fw.tell()
            try:
                dirdata[filename]["ncr"] += 1
            except KeyError:
                dirdata[filename]["ncr"] = 1

            fwrite(d["pathname"] + d["filename"])
            fwrite("\0")
            stages = set()
            fwrite(struct.pack("!b", 0))
            for i in xrange(0, 2):
                fwrite(struct.pack("!i", d["mode"]))
                if d["mode"] != 0:
                    stages.add(i)

            for i in sorted(stages):
                print i
                fwrite(binascii.unhexlify(d["sha1"]))

            writecrc32()

        for f in reucdata[d]:
            print f

    return dirdata
# }}}


# Compile cachetreedata and factor it into the dirdata
def compilev5_1cachetreedata(dirdata, extensiondata):
    for entry in extensiondata.iteritems():
        dirdata[entry[1]["path"].strip("/")]["nentries"] = int(entry[1]["entry_count"])
        try:
            dirdata[entry[1]["path"].strip("/")]["objname"] = entry[1]["sha1"]
        except:
            pass  # Cache tree entry invalid

        try:
            if dirdata[entry[1]["path"].strip("/")]["nsubtrees"] != entry[1]["subtreenr"]:
                print entry[0]
                print dirdata[entry[1]["path"].strip("/")]["nsubtrees"]
                print entry[1]["subtreenr"]
        except KeyError:
            pass

    return dirdata
# }}}

# }}}


# writecrc32 {{{
def writecrc32():
    global writtendata
    crc = binascii.crc32("".join(writtendata))
    fwrite(struct.pack("!i", crc))
    writtendata = list()  # Reset writtendata for next crc32
# }}}


header = readheader(f)

indexentries, conflictedentries, byte, paths, files, filedirs = readindexentries(f)

filedata = filedata[:-1]
ext = byte + f.read(3)
extensiondata = []

ext2 = ""
if ext == "TREE":
    filedata += ext
    treeextensiondata = readextensiondata(f)
    ext2 = f.read(4)
else:
    treeextensiondata = dict()

if ext == "REUC" or ext2 == "REUC":
    if ext == "REUC":
        filedata += ext
    else:
        filedata += ext2
    reucextensiondata = readreucextensiondata(f)
else:
    reucextensiondata = list()

# printheader(header)
# printindexentries(indexentries)
# printextensiondata(extensiondata)
# printreucextensiondata(reucextensiondata)


if ext != "TREE" and ext != "REUC" and ext2 != "REUC":
    sha1read = ext + f.read(16)
elif ext2 != "REUC" and ext == "TREE":
    sha1read = ext2 + f.read(16)
else:
    sha1read = f.read(20)

print "SHA1 over the whole file: " + str(binascii.hexlify(sha1read))

sha1 = hashlib.sha1()
sha1.update("".join(filedata))
print "SHA1 over filedata: " + str(sha1.hexdigest())

if sha1.hexdigest() == binascii.hexlify(sha1read):
    # Write v5_0 {{{
    # writev5_0header(header, paths, files)
    # diroffsets = writev5_0directories(sorted(paths), treeextensiondata)
    # fileoffsets = writev5_0files(files)
    # dircrcoffset = writtenbytes

    # # TODO: Replace with something faster. Doesn't make sense to calculate crc32 here
    # writecrc32()
    # fileoffsets = writev5_0fileentries(indexentries, fileoffsets)
    # writev5_0reucextensiondata(reucextensiondata)
    # writev5_0conflicteddata(conflictedentries)
    # writev5_0fileoffsets(diroffsets, fileoffsets, dircrcoffset)
    # }}}

    # Write v5_1 {{{
    writev5_1header(header, paths, files)
    writecrc32()
    writev5_1fakediroffsets(paths)
    # writecrc32() # TODO Check if needed
    diroffsets, dirwritedataoffsets, dirdata = writev5_1directories(paths)
    fileoffsetbeginning = writev5_1fakefileoffsets(indexentries)
    # writecrc32() # TODO Check if needed
    fileoffsets, dirdata = writev5_1filedata(indexentries, dirdata)
    dirdata = writev5_1conflicteddata(conflictedentries, reucextensiondata, dirdata)
    writev5_1diroffsets(diroffsets)
    writev5_1fileoffsets(fileoffsets, fileoffsetbeginning)
    dirdata = compilev5_1cachetreedata(dirdata, treeextensiondata)
    writev5_1directorydata(dirdata, dirwritedataoffsets, fileoffsetbeginning)
    # }}}
else:
    print "File is corrupted"
>>>>>>> origin/ce_namelen
