Name: 		git-minimal
Version: 	1.8.2
Release: 	1
Summary:  	Core git tools
License: 	GPL
Group: 		Development/Tools
URL: 		http://kernel.org/pub/software/scm/git/
Source: 	http://kernel.org/pub/software/scm/git/%{name}-%{version}.tar.bz2

BuildRequires:	zlib-devel >= 1.2, openssl-devel, curl-devel, expat-devel, gettext
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

Requires:	zlib >= 1.2, rsync, less, openssh-clients, expat
Provides:	git-core = %{version}-%{release}
Obsoletes:	git-core <= 1.5.4.2
Provides:  git = %{version}-%{release}
Conflicts: git-all
Provides:  git-p4 = %{version}-%{release}
Obsoletes:	git-p4

%description
Git is a fast, scalable, distributed revision control system with an
unusually rich command set that provides both high-level operations
and full access to internals.

The git-tiny rpm installs the cut down version of core tools with
minimal dependencies.

%define path_settings ETC_GITCONFIG=/etc/gitconfig prefix=%{_prefix} mandir=%{_mandir} htmldir=%{_docdir}/%{name}-%{version} gitexecdir=%{_bindir}


%define extra_make_flags NO_PERL=1 NO_TCLTK=1 NO_PYTHON=1

%prep
%setup -q

%build
make %{extra_make_flags} %{_smp_mflags} CFLAGS="$RPM_OPT_FLAGS" \
     %{path_settings} \
     all

%install
rm -rf %{buildroot}
make  %{extra_make_flags} %{_smp_mflags} CFLAGS="$RPM_OPT_FLAGS" DESTDIR=%{buildroot} \
     %{path_settings} \
     INSTALLDIRS=vendor install

find %{buildroot} -type f -name .packlist -exec rm -f {} ';'
find %{buildroot} -type f -name '*.bs' -empty -exec rm -f {} ';'
find %{buildroot} -type f -name perllocal.pod -exec rm -f {} ';'

(find %{buildroot}%{_bindir} -type f | grep -vE "archimport|svn|cvs|email|gitk|git-gui|git-citool" | sed -e s@^%{buildroot}@@)               > bin-files
rm -rf %{buildroot}%{_mandir}
rm -rf %{buildroot}%{_datadir}/locale

mkdir -p %{buildroot}%{_sysconfdir}/bash_completion.d
install -m 644 -T contrib/completion/git-completion.bash %{buildroot}%{_sysconfdir}/bash_completion.d/git

%clean
rm -rf %{buildroot}

%files -f bin-files
%defattr(-,root,root)
%{_datadir}/git-core/
%doc README COPYING
%{_sysconfdir}/bash_completion.d
