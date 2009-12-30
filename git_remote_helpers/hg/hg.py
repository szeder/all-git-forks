from git_remote_helpers.hg.metadata_util import (get_git_author,
        get_git_parents, get_git_message)

class GitHg(object):
    """Class that handles various aspects of converting a hg commit to git.
    """

    def __init__(self, warn):
        """Initializes a new GitHg object with the specified warner.
        """

        self.warn = warn

    def format_timezone(self, offset):
        if offset % 60 != 0:
            raise ValueError("Unable to handle non-minute offset.")
        sign = (offset < 0) and '-' or '+'
        offset = abs(offset)
        return '%c%02d%02d' % (sign, offset / 3600, (offset / 60) % 60)

    def get_committer(self, ctx):
        extra = ctx.extra()

        if 'committer' in extra:
            # fixup timezone
            (name_timestamp, timezone) = extra['committer'].rsplit(' ', 1)
            try:
                timezone = self.format_timezone(-int(timezone))
                return '%s %s' % (name_timestamp, timezone)
            except ValueError:
                self.warn("Ignoring committer in extra, invalid timezone in r%s: '%s'.\n" % (ctx.rev(), timezone))

        return None

    def get_message(self, ctx):
        return get_git_message(ctx)

    def get_author(self, ctx):
        author = get_git_author(ctx)

        (time, timezone) = ctx.date()
        date = str(int(time)) + ' ' + self.format_timezone(-timezone)

        return author + ' ' + date

    def get_parents(self, ctx):
        return get_git_parents(ctx)
