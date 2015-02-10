# Git on z/OS

This document outlines a procedure to build Git on z/OS v1r13.  The time needed
to complete the entire document will vary depending on your mainframe hardware.
For the worst case, plan on taking two hours from start to finish.


## Adding Storage

This section briefly notes how to add additional disk space to your z/OS UNIX
environment.  Compiling the required software can use about 300 megabytes, and
the installed programs may use about 100 megabytes.

### Allocation

Here is a sample JCL step to allocate a data set that can be mounted under USS
to add additional disk space.  Replace the four variables with values that are
applicable to your site (`&DATASET`, `&VOLSER`, `&PRIMARY`, and `&SECONDA`),
then add a job card and submit it.

    //ALLOC    EXEC PGM=IEFBR14
    //HFSDD    DD DSN=&DATASET,VOL=SER=&VOLSER,
    //            DISP=(NEW,CATLG),
    //            DSNTYPE=HFS,
    //            SPACE=(CYL,(&PRIMARY,&SECONDA,1)),
    //            UNIT=3390

Note that a directory blocks value (`1` here) is required for HFS, but it does
not affect allocation.

### Mounting

The data set can be mounted with these TSO commands.  First, create the mount
point if it doesn't already exist.  (Substitute an appropriate path.)

    MKDIR '/path/to/point'

Now mount the data set.  (Substitute the path and fully qualified DSN.)

    MOUNT FILESYSTEM('OMVS.HFS.DSN') MOUNTPOINT('/path/to/point') TYPE(HFS) MODE(RDWR)

See IBM's documentation on the `BPXPRMxx` parmlib member's `MOUNT` statement in
_z/OS UNIX System Services Planning_ for mounting this data set on IPL.


## Preparation

You will need to either set up your account or configure site-wide settings as
follows to be able to run Git (as built by this document).  You can start OMVS
and paste these commands directly into the prompt.

### Automatic Character Conversion

Since Git will use Enhanced ASCII to convert between character sets, define a
few environment variables to enable this functionality automatically for your
account.  You should exit OMVS after this change and logon again to verify the
variables appear in your environment by running `env`.

    echo '_BPXK_AUTOCVT=ON ; export _BPXK_AUTOCVT' >> ~/.profile
    echo "_CEE_RUNOPTS='FILETAG(AUTOCVT,AUTOTAG) POSIX(ON)' ; export _CEE_RUNOPTS" >> ~/.profile

To enable site-wide automatic conversion, see documentation on the `BPXPRMxx`
parmlib member's `AUTOCVT` statement in _z/OS UNIX System Services Planning_.

### Git Configuration

Write a Git configuration file containing your name and e-mail address for
later.  Also tag the file as IBM-1047 encoded text for automatic conversion.

    echo "[user]\n\tname = $(id -nru)\n\temail = $(id -nru)@$(hostname)" > ~/.gitconfig
    chtag -tc IBM-1047 ~/.gitconfig

These settings can be made site-wide instead of just for your account by
replacing `~/.gitconfig` with `/etc/gitconfig` in each of the above lines.


## Installation

All commands in this section can be pasted into the OMVS prompt.  The commands
will rely on the `prefix` variable being defined to the path where you want to
install everything.  For example, you can run `prefix=/usr/local` to install
programs to `/usr/local/bin`, etc.  Everything will be compiled in your current
working directory.  If you want to install to your current working directory as
well, run the following.

    prefix=$PWD

If necessary, add the relevant paths to your environment.  For example, set
`PATH=$prefix/bin:$PATH` and `MANPATH=$prefix/man/%L:$MANPATH`.  Note that some
of the documentation being installed will need to be rendered with a `roff`
program for the z/OS `man` command to properly display the pages.

Also make sure that you will be able to run the XL C compiler.  IBM recommends
a minimum region size of `148M`.

### Install gzip

Some source archives are compressed using gzip, so install the z/OS port hosted
by IBM if you don't already have it.

Fetch the files via FTP.

    echo 'anonymous\n-\nbinary\nget /s390/zos/tools/gzip/gzip.pax.Z.bin gzip.pax.Z' | ftp public.dhe.ibm.com

Install the files to your chosen `prefix`.

    uncompress -c gzip.pax.Z | pax -rs"#/*#$prefix/#" && rm -f gzip.pax.Z
    ln -s gzip $prefix/bin/gunzip

### Install gmake

While USS provides a `make` implementation, these projects use GNU extensions.
Again, install the z/OS port hosted by IBM if you don't already have it.

Fetch the files via FTP.

    echo 'anonymous\n-\nbinary\nget /s390/zos/tools/gmake/gmake.pax.Z.bin gmake.pax.Z' | ftp public.dhe.ibm.com

Install the files to your chosen `prefix`.

    uncompress -c gmake.pax.Z | pax -rs"#/*#$prefix/#" && rm -f gmake.pax.Z
    rmdir $prefix/man/man1

Note that this will extract the file `bin/make`, which has the same name as the
z/OS `/bin/make` program.  Take care with ordering your `PATH` variable so you
call your preferred version in the future.

### Install HTTP(S) download support

If you have the `curl` program from IBM's Ported Tools product, you can run the
following command and skip the rest of this section.

    alias download='/usr/lpp/ported/bin/curl --location'

If you don't have cURL and would like an easier time downloading files from the
web, download https://raw.githubusercontent.com/dm0-/git/zos/contrib/download.c
and transfer it into USS.  This will allow you to download files directly from
web servers to USS without having to transfer them from your workstation first.

Compile and install the program.  Note this uses IBM's Global Security Kit for
HTTPS URLs.  If GSKit isn't available to you, remove the `-DENABLE_GSKSSL` and
`/usr/lpp/gskssl/lib/GSKSSL.x` arguments from this command to use HTTP only.

    xlc -qLANGLVL=STDC99 -qXPLINK -DENABLE_GSKSSL -o $prefix/bin/download download.c /usr/lpp/gskssl/lib/GSKSSL.x &&
    rm -f download.o

The above program looks for the file `key.kdb` in the current directory with
password `password` for its trusted CA certificates.  Build a keyring database
for it to validate HTTPS requests to GitHub.

    echo '1\nkey.kdb\npassword\npassword\n\n\n0\n\n0' | gskkyman
    download http://cacerts.digicert.com/DigiCertHighAssuranceEVRootCA.crt > digicert.crt
    echo '2\nkey.kdb\npassword\n7\ndigicert.crt\nDigiCert High Assurance EV Root CA\n\n0' | gskkyman
    rm -f digicert.crt

### Install zlib

Git requires zlib for compression.

Download the latest version of zlib over HTTP, and extract it.

    download http://zlib.net/zlib-1.2.8.tar.gz | gunzip | pax -roto=IBM-1047

The version string in `zlib.h` will be converted to ASCII when using `-qASCII`
to compile something that will link against zlib, which breaks version checks.
Run the following to change the version string to the binary EBCDIC character
values so it is not subject to conversion.

    sed '/#define.ZLIB_VERSION/{s/[0-9]/\\xF&/g;s/\./\\x4B/g;}' zlib-1.2.8/zlib.h > zlib.h+ &&
    touch -r zlib-1.2.8/zlib.h zlib.h+ && mv -f zlib.h+ zlib-1.2.8/zlib.h

Compile and test zlib.

    (cd zlib-1.2.8 && CC='xlc -qLANGLVL=STDC99 -qXPLINK' mandir='${prefix}/man/C' ./configure --prefix=$prefix --static)
    gmake -C zlib-1.2.8 test TEST_LDFLAGS=libz.a

Install the zlib headers, library, and manual page to the configured `prefix`.

    gmake -C zlib-1.2.8 install man3dir='${mandir}/cat3'

Optionally, build a simple program for debugging compression later.

    xlc -qLANGLVL=STDC99 -qXPLINK -I$prefix/include -L$prefix/lib -o $prefix/bin/zpipe zlib-1.2.8/examples/zpipe.c -lz &&
    rm -f zpipe.o

### Install install

The Git installation procedure wants to use the `install` command, so fetch the
shell version over HTTP and make it executable.

    download http://git.savannah.gnu.org/cgit/automake.git/plain/lib/install-sh | iconv -f UTF-8 -t IBM-1047 >$prefix/bin/install
    chmod 755 $prefix/bin/install

### Install git

Download and extract a source archive of this Git branch.

    download https://codeload.github.com/dm0-/git/tar.gz/zos | gunzip | pax -roto=IBM-1047

*Possible bug:*  The `pax` command seems to fail to convert these tar files
from GitHub.  Check if you get readable text output from running the command
`head -c 50 git-zos/Makefile`, and if not, just tag everything as ASCII.  *Do
not run this if the conversion did succeed and you got readable text output.*

    chtag -Rtc ISO8859-1 git-zos

Build Git.

    gmake -C git-zos CPPFLAGS=-I$prefix/include LDFLAGS=-L$prefix/lib V=1 \
        prefix=$prefix mandir='$(prefix)/man/C' sysconfdir=/etc

Tag the configuration files so they get installed with encoding information.

    chtag -Rtc IBM-1047 git-zos/templates/blt

Install Git.

    gmake -C git-zos CPPFLAGS=-I$prefix/include LDFLAGS=-L$prefix/lib V=1 \
        prefix=$prefix mandir='$(prefix)/man/C' sysconfdir=/etc install

### Install documentation

*This section is optional.*

Git uses man pages by default to display command usage information.  Since the
z/OS `man` program only supports plain text, the man pages need to be rendered
beforehand with a `roff` program.

Install the z/OS port of `groff` that is hosted by IBM.

    echo 'anonymous\n-\nbinary\nget /s390/zos/tools/groff/groff.pax.Z.bin groff.pax.Z' | ftp public.dhe.ibm.com
    uncompress -c groff.pax.Z | pax -rs"#/*#$prefix/#" && rm -f groff.pax.Z
    echo '.\" Local modifications' > $prefix/share/groff/1.17/tmac/man.local

Add a CA certificate to validate kernel.org HTTPS requests if not using cURL.

    download http://www.startssl.com/certs/ca.crt > startcom.crt
    echo '2\nkey.kdb\npassword\n7\nstartcom.crt\nStartCom Certification Authority\n\n0' | gskkyman
    rm -f startcom.crt

Download and extract the Git man pages.

    download https://www.kernel.org/pub/software/scm/git/git-manpages-2.3.0.tar.gz | gunzip | pax -roto=IBM-1047

Convert and install all of the man pages.

    for c in 1 5 7 ; do mkdir -p $prefix/man/C/cat$c ; for x in man$c/*.$c ; do
    groff -F$prefix/share/groff/1.17/font -M$prefix/share/groff/1.17/tmac -mandoc -Tcp1047 $x > $prefix/man/C/cat$c/${x##*/}
    done ; done

Verify that commands like `git commit --help` display readable man pages.  If
your `MANPATH` is set properly, you should also be able to read documentation
with `man githooks`, for example.


## Usage

The most troublesome aspect of using Git as a DVCS on z/OS is (of course) text
encoding.  When you clone a repository from a remote server, there is virtually
a 100% chance that the project uses an EBCDIC-incompatible character set.  Due
to the way Git works--everything is identified by its SHA-1 sum--you cannot
re-encode the source files in place without Git thinking that you've replaced
every file.

Two example post-checkout hooks are provided to work around this.  The first
hook, `post-checkout.zos-tag`, is the safer of the two.  It tags every file as
ASCII so you can work on them with native (EBCDIC) tools transparently.  The
second hook, `post-checkout.zos-convert`, actually converts untagged files from
ASCII to EBCDIC on disk, then tags them as EBCDIC so Git converts them back on
the fly.  This is for cases where programs that can't automatically convert
character sets will need to access Git-tracked files.

To enable either of these hooks, remove its file extension in each repository
that you want to be affected, then call `git checkout` to run it.  For example:

    ln -fs post-checkout.zos-tag .git/hooks/post-checkout
    git checkout HEAD

Remember to manually tag and/or convert any files that you've created before
`git add`ing them.

Do *not* use these hooks unless you are sure all files in the repository are
plain ASCII text, specifically ISO8859-1 compatible.  Binary files or other
encodings (e.g. UTF-8) can have their data corrupted by conversion operations.
You'll need to come up with more specific solutions for such cases.

### Client Examples

Since this was a fairly minimal build of Git (without libcurl), you should only
plan to work with repositories over `git://` and `ssh://` URLs.

#### Cloning a Project

This will clone the mainline Git repository's upcoming updates branch.

    git clone --branch next git://github.com/git/git.git ~/git-next

Remember to enable a hook to keep all the project files properly tagged.  This
chooses the one to just tag everything as ASCII.  Also, run it the first time.

    ln -fs post-checkout.zos-tag ~/git-next/.git/hooks/post-checkout
    git -C ~/git-next checkout HEAD

From this point, you should be able to work with the tracked files normally, as
long as you stick with tools that automatically convert character sets (which
most programs under z/OS UNIX will support).

#### Creating a Project

Call `git init` to create an empty repository.  Choose a character conversion
and/or tagging hook, but you won't need to run it since there are no files yet.

    mkdir ~/repo-init
    git -C ~/repo-init init
    ln -fs post-checkout.zos-convert ~/repo-init/.git/hooks/post-checkout

Create a file for Git to track.  Remember to tag any new files as text with the
proper encoding so Git can do diff processing etc.

    echo 'Example data' > ~/repo-init/test-file
    chtag -tc IBM-1047 ~/repo-init/test-file

Next, tell git to track the new file, then commit its contents.

    git -C ~/repo-init add test-file
    git -C ~/repo-init commit -am 'Initial commit'

Try running some commands to view information from Git's object database.

    git -C ~/repo-init log -p
    git -C ~/repo-init count-objects -Hv

You now have a z/OS-made Git repository.  We'll use this repository's contents
to be served to other systems below.

### Server Examples

Before setting up a server on z/OS, note that this will open up another service
on the network, increase the system workload, and grant foreign users access to
certain parts of the file system.  These are normally things to avoid on a
mainframe.  You might prefer to have your Git repositories served from another
system, with a z/OS batch job periodically pulling updates from it as a regular
Git client.  This would allow you to control when the potentially CPU-intensive
Git operations occur without opening access to the mainframe over the network.
If you *still* want to host a Git server on z/OS, keep reading...

Hosting a Git server on z/OS should be no different than on any other OS, since
bare repositories don't need to deal with character conversion.  For example,
this will start a bare repository that can be cloned.

    mkdir -p /srv/git/test.git
    git -C /srv/git/test.git init --bare

Using the example repository built in the previous section, push its `master`
branch into the bare repository via the local file system so it has some data.

    git -C ~/repo-init remote add server /srv/git/test.git
    git -C ~/repo-init push server master

#### Using the Git Protocol

See Git documentation for detailed information, but the `git://` protocol has
no built-in security or authentication, so you should only use it when you want
to give anonymous read-only access to your repositories.  (There are probably
better hosting solutions for that than your mainframe, but this is an example.)
You will need to make sure there are no firewalls blocking access to the Git
server's TCP port, which is 9418 by default.

Start the Git daemon, and restrict it to a subdirectory (`/srv/git` in this
case) to prevent it from accessing anything other than the Git repositories.
Moreover, whitelist access only to directories with a `.git` extension.  Note
that the `--export-all` just allows you to omit creating `git-daemon-export-ok`
files; it still only exports the explicitly whitelisted paths.

    git daemon --base-path=/srv/git --export-all --strict-paths /srv/git/*.git

This command will run the daemon in the foreground to print error messages to
the console.  You will need to open another shell to kill the process when you
are finished testing.  In OMVS, press `PF2` and run the `op` subcommand to open
another shell, then use `PF9` to swap between open shells and `PF3` to exit the
subcommand prompt.

The `test.git` repository can now be cloned over the `git://` protocol by other
systems.  (You can replace `$zos_ip_addr` with `127.0.0.1` to test on the z/OS
system itself, in case incoming TCP port 9418 connections are being blocked.)

    git clone git://$zos_ip_addr/test.git test-git

Even though `test-file` in this repository was created with EBCDIC encoding, it
should be in ASCII when cloned.  No one should be able to see a difference when
z/OS is involved in Git workflows.

#### Using the SSH Protocol

Since SSH provides a secure connection and uses the system's authentication, it
should be used where authorized users need read/write access to the z/OS Git
repositories.  Fine-grained per-repository permissions are handled with regular
UNIX permissions on the repository directories.  You will need to make sure
there are no firewalls blocking the SSH TCP port, which is 22 by default.

This setup will use the OpenSSH server from IBM's Ported Tools product.  If you
haven't used it before, create a basic configuration from sample files.

    cp -p /samples/ssh_prng_cmds /samples/sshd_config /samples/zos_sshd_config /etc/ssh/
    chmod 600 /etc/ssh/*sshd_config
    ssh-keygen -t dsa -f /etc/ssh/ssh_host_dsa_key -N ''
    ssh-keygen -t rsa -f /etc/ssh/ssh_host_rsa_key -N ''

*This is an ugly hack that will hopefully get a better solution some day.*  The
z/OS SSH server automatically converts input and output, which confuses the Git
programs that are called over SSH.  Wrap these programs with a shell script to
undo the extra conversion.

    mkdir -p $prefix/ssh-hack
    echo '#!/bin/sh -e\nPATH=${PATH#*:} # Drop the hack path.' > $prefix/ssh-hack/git-upload-pack
    echo 'iconv -f IBM-1047 -t ISO8859-1 |\n${0##*/} "$@" |\niconv -f ISO8859-1 -t IBM-1047' >> $prefix/ssh-hack/git-upload-pack
    chmod 755 $prefix/ssh-hack/git-upload-pack
    ln $prefix/ssh-hack/git-upload-pack $prefix/ssh-hack/git-receive-pack

Edit the sample configuration to allow custom user environments, and set your
account's `PATH` to include the program installation directory (`$prefix/bin`).
*The weird `ssh-hack` path must come first for the above programs to work.*

    echo '/PermitUserEnv/\ns/#//\ns/no/yes/\nw' | ed /etc/ssh/sshd_config
    mkdir -p ~/.ssh && chmod 700 ~/.ssh
    echo "PATH=$prefix/ssh-hack:$prefix/bin:/usr/bin:/usr/sbin:/bin:/sbin" > ~/.ssh/environment

If you haven't enabled system-wide automatic character conversion, add these
settings to your environment as well so Git can decode the EBCDIC configuration
files when called over SSH.

    echo _BPXK_AUTOCVT=ON >> ~/.ssh/environment
    echo '_CEE_RUNOPTS=FILETAG(AUTOCVT,AUTOTAG) POSIX(ON)' >> ~/.ssh/environment

Start the server.  This runs the service in the foreground to send output to
the console.  See the notes in the previous section to kill it.

    /usr/sbin/sshd -De

The `test.git` repository can now be cloned over SSH, but note that the full
path on the file system must be specified.

    git clone ssh://$zos_user@$zos_ip_addr/srv/git/test.git test-ssh

If you could write to the repository on z/OS, you should be able to use Git to
push your changes back over SSH.  Make some commits on your cloned repository's
`master` branch, then run this to send the changes back to z/OS.

    git push origin master


## Relocatable Installation

This section describes building Git with an alternate configuration that can be
packaged into a binary archive.  It allows installation on other z/OS systems
just by extracting a pax file, and it can be extracted to any location on the
file system (with a helper script).

### Package the Binaries

*This part is to be run on the build system.*  It assumes this document was
followed through installing dependencies and Git's source and man page archives
were extracted.

Build Git with the `RUNTIME_PREFIX` option enabled, and remove the debug flag
from `CFLAGS` since the source won't be included with these binaries for `dbx`.

    gmake -C git-zos CFLAGS=-O3 CPPFLAGS=-I$prefix/include LDFLAGS=-L$prefix/lib RUNTIME_PREFIX=1 V=1 \
        prefix=/usr/local mandir='$(prefix)/man/C' sysconfdir=/etc

Tag the configuration files so they get installed with encoding information.

    chtag -Rtc IBM-1047 git-zos/templates/blt

Install Git into a staging directory given by `DESTDIR`.

    gmake -C git-zos CFLAGS=-O3 CPPFLAGS=-I$prefix/include LDFLAGS=-L$prefix/lib RUNTIME_PREFIX=1 V=1 \
        prefix=/usr/local mandir='$(prefix)/man/C' sysconfdir=/etc DESTDIR=$PWD/gitroot install

Include the documentation.

    for c in 1 5 7 ; do mkdir -p gitroot/usr/local/man/C/cat$c ; for x in man$c/*.$c ; do
    groff -F$prefix/share/groff/1.17/font -M$prefix/share/groff/1.17/tmac \
        -mandoc -Tcp1047 $x > gitroot/usr/local/man/C/cat$c/${x##*/}
    done ; done

Bundle everything into a pax file with a format that supports file tags.

    pax -ws,gitroot/usr/local/,, -xpax gitroot/usr/local/* | compress -c > git.pax.Z

The file `git.pax.Z` can now be transferred to other z/OS systems, and the next
section describes how to extract it.  Note if you distribute these binaries,
then you will need to be prepared to give the source to anyone that requests it
since Git is distributed under GPLv2 terms.

### Binary Installation

*This part is to be run on the run-time system.*  The system shouldn't need any
additional configuration, neither user-specific nor site-wide.

Set the variable `instdir` to the location where you want to install Git.  For
example, this will install the `git` program to `/usr/git/bin/git`.

    instdir=/usr/git

Extract the pax file to its desired installation directory.

    uncompress -c git.pax.Z | pax -r"s,/*,$instdir/,"

Write a wrapper script in the default `PATH` that sets up automatic Enhanced
ASCII conversion and calls `git` with a full path.

    echo '#!/bin/sh -e' > /bin/git
    echo 'export _BPXK_AUTOCVT=ON' >> /bin/git
    echo "export _CEE_RUNOPTS='FILETAG(AUTOCVT,AUTOTAG) POSIX(ON)'" >> /bin/git
    echo "exec $instdir/bin/\${0##*/} \"\$@\"" >> /bin/git
    chmod 755 /bin/git

If the system will be accessed by Git over SSH, write similar wrapper scripts
that will work around the extra conversion from the OpenSSH server.

    echo '#!/bin/sh -e' > /bin/git-upload-pack
    echo 'export _BPXK_AUTOCVT=ON' >> /bin/git-upload-pack
    echo "export _CEE_RUNOPTS='FILETAG(AUTOCVT,AUTOTAG) POSIX(ON)'" >> /bin/git-upload-pack
    echo 'iconv -f IBM-1047 -t ISO8859-1 |' >> /bin/git-upload-pack
    echo "$instdir/bin/\${0##*/} \"\$@\" |" >> /bin/git-upload-pack
    echo 'iconv -f ISO8859-1 -t IBM-1047' >> /bin/git-upload-pack
    chmod 755 /bin/git-upload-pack
    ln /bin/git-upload-pack /bin/git-receive-pack

If global configuration will be used, tag the files' encoding.

    touch /etc/gitconfig /etc/gitattributes
    chtag -tc IBM-1047 /etc/gitconfig /etc/gitattributes

If you want to read the Git man pages by running the `man` command, add its
directory to the `MANPATH` variable.  (The man pages will be used by Git's help
commands automatically without modifying the `MANPATH`.)

    MANPATH=$instdir/man/%L:$MANPATH

Git commands should now be ready for normal usage.
