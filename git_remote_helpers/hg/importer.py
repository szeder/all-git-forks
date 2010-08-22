import os.path
import sys

from git_remote_helpers.hg import hgimport
from git_remote_helpers.fastimport import processor, parser


class GitImporter(object):
    def __init__(self, repo):
        self.repo = repo

    def handle_line(self):
        # returns '' on EOF, '\n' on empty line
        line = sys.stdin.readline()

        if not line:
            return False

        self.outfile.write(line)

        return True

    def old_do_import(self, base):
        self.outfile = open(os.path.join('.git', "export.log"), "wa")

        more = True

        while more:
            more = self.handle_line()

        self.outfile.close()

    def do_import(self, base):
        sources = ["-"]

        dirname = self.repo.get_base_path(base)

        if not os.path.exists(dirname):
            os.makedirs(dirname)

        marks_file = os.path.abspath(os.path.join(dirname, 'hg.marks'))

        procc = hgimport.HgImportProcessor(self.repo.ui, self.repo)

        if os.path.exists(marks_file):
            procc.load_marksfile(marks_file)

        processor.parseMany(sources, parser.ImportParser, procc)

        procc.write_marksfile(marks_file)
