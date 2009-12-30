import hashlib
import os

from git_remote_helpers.util import die, warn

class NonLocalHg(object):
    def __init__(self, repo, alias):
        self.repo = repo
        self.alias = alias
        self.hg = repo.hg

    def repo_path(self, base):
        hasher = hashlib.sha1()
        hasher.update(self.repo.url())
        hash = hasher.hexdigest()

        return os.path.join(base, 'info', 'fast-import', 'non-local', hash)

    def clone(self, base):
        path = self.repo_path(base)

        # already cloned
        if os.path.exists(path):
            return path

        os.makedirs(path)
        self.repo.ui.setconfig('ui', 'quiet', "true")
        self.hg.clone(self.repo.ui, self.repo.url(), path, update=False, pull=True)

        return path

    def update(self, base):
        path = self.repo_path(base)

        if not os.path.exists(path):
            die("could not find repo at %s", path)

        repo = self.hg.repository(self.repo.ui, path)

        repo.ui.setconfig('ui', 'quiet', "true")
        repo.pull(self.repo, heads=self.repo.heads(), force=True)
