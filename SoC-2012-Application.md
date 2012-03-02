This is a draft of git's application to Google's Summer of Code 2012.

Organization Admin
------------------

Jeff King &lt;peff@peff.net&gt;


Description
-----------

At almost seven years old, Git is probably the most widely-used
distributed revision control system in Open Source.  Many large and
successful projects use git, including the Linux Kernel, Perl, Eclipse,
Gnome, KDE, Qt, Ruby on Rails, Android, PostgreSQL, Debian, and X.org.

This achievement is the product of the lively Git development community,
a loose-knit team of developers, technical writers, and end users with a
passion for high quality open-source development. Because we use git to
track the development of git itself, we keep the barrier to contributing
low and encourage patches from a "long tail" of developers.


Home Page
---------

http://git-scm.com


Main Organization License
-------------------------

GPLv2


Backup Admin
------------

TBD


What is the URL for your ideas page?
------------------------------------

http://github.com/peff/git/wiki/SoC-2012-Ideas


What is the main IRC channel for your organization?
---------------------------------------------------

 #git and #git-devel (on freenode)


What is the main development mailing list for your organization?
----------------------------------------------------------------

git@vger.kernel.org


Why is your organization applying to participate in GSoC 2012? What do you hope to gain by participating?
---------------------------------------------------------------------------------------------------------

Git has participated in GSoC since 2007. We have appreciated not only
the code contributions, but also the increased project visibility and
the addition of new long-term contributors.

By participating in GSoC 2012 the Git community hopes to attract more
new talent to our community, and convert that talent into long-term
contributors and high-quality enhancements to Git.


Did your organization participate in past GSoCs? If so, please summarize your involvement and the successes and challenges of your participation.
-------------------------------------------------------------------------------------------------------------------------------------------------

Yes; git has participated in GSoC every year since 2007, typically
mentoring 2-5 students each year. Our mentors have always been active
contributors within the community. The students typically did not have a
prior relationship with the community, though in one case we took on a
student who had previously contributed patches.

Of the 19 projects we have mentored, 16 have resulted in success. In
many cases, the code has been merged and is in daily use in git. In some
cases, the code was spun off into its own project (e.g., the
git-statistics project in 2008 ended up as a separate project). In other
cases, the implementations, while never merged into mainline git, served
(or continue to serve) as the basis for discussion and advancement of
certain features (e.g., 2008's GitTorrent project and 2009's svn
interaction improvements). The libgit2 project, which has been the
subject of multiple GSoC projects, has gone from being an unusable
skeleton to a thriving ecosystem, with bindings in Objective-C, Python,
Ruby, C#, Lua, and more. While most of those projects were not done
through GSoC, the GSoC projects were instrumental in getting the library
to a point that attracted outside interest.

But most important has been the development of students into open source
contributors. In almost every project, even those whose ultimate goals
did not end up merged into mainline git, students ended up contributing
related commits to git. Furthermore, we have had success with retaining
members in the community. At least 5 of our 16 successful students
continued contributing in the year after their GSoC involvement, and the
student from our 2010 libgit2 project has essentially become the project
maintainer.

One of the biggest challenges has been integrating students into the
public development process, and especially convincing them to produce
and publish work continually throughout the period. While we have had
several students turn into long-term members, just as many disappear
after GSoC. And while many projects have been successful, we often have
difficulty integrating them into mainline git when the results are
dumped on the community at the end. In 2010 and 2011, we made a
conscious effort to have mentors encourage their students to have more
contact with the community at large, and that seems to have helped some.


If your organization has not previously participated in GSoC, have you applied in the past? If so, for what year(s)?
--------------------------------------------------------------------------------------------------------------------

N/A


Does your organization have an application template you would like to see students use? If so, please provide it now.
---------------------------------------------------------------------------------------------------------------------

http://github.com/peff/git/wiki/Soc-2012-Template


What criteria did you use to select individuals as mentors? Please be as specific as possible.
----------------------------------------------------------------------------------------------

All mentors volunteered for the specific project(s) that they could
contribute the most to.  All mentors are active contributors within
the Git development community.

Active contributors are defined to be those who have submitted and
have had accepted into a shipped release a substantial amount of
code. Substantial amount of code is defined to be equal in size (or
larger) to what might be reasonably expected of a student working
on a Google Summer of Code project.

All mentors are well known within the Git development community
for their accomplishments and contributions.


What is your plan for dealing with disappearing students?
---------------------------------------------------------

Every reasonable effort will be made to maintain contact with
students and ensure they are making progress throughout the summer.

In the unfortunate event that a student abandons and does not
complete his/her GSoC project, the Git community will try to pick up
and continue the work without the student.  This is one reason why we
will require frequent publishing of project materials.

Students will also be strongly encouraged by our mentors to work
through their project by completing several small milestones
throughout the summer.  This strategy of project organization will
help students to feel like they have accomplished something useful
sooner, as well as making it easier for the Git community to pick
up an abandoned project.


What is your plan for dealing with disappearing mentors?
--------------------------------------------------------

Most of our suggested projects have more than one mentor available.
In the unlikely event that a mentor disappears during the summer
another mentor will be arranged to step in.  The Git community
has plenty of good people within that would be more than happy to
help a student finish their project.  It is very probable that the
replacement mentor would already be familiar with the student and
the project, as many community members routinely review code and
discussions on the mailing list.


What steps will you take to encourage students to interact with your project's community before, during and after the program?
------------------------------------------------------------------------------------------------------------------------------

Students will be required to join the main development mailing
list, and post their patches for discussion to same.  All current
contributors already do this, so students will see the experienced
hands performing the same tasks, and will therefore be able to learn
by example.  We feel that the list based discussions will help the
students to become, and stay, a member of the community.

Mailing list traffic is currently around 80-100 messages per day,
and is focused on Git feature development.  Keeping current by
at least skimming list messages is an important part of the Git
development process.

Students will be required to post their work as a Git repository on a
publicly available server so that their works-in-progress will be
available for everyone to review. However, as patch review typically
happens on the mailing list, we expect that to be the main venue for
review of the students' work.

Mentors will also exchange email with students on at least a
weekly basis, if not more frequently.  Students will be required
to provide weekly progress reports back to their mentors, so that
the mentors are aware of the tasks that a student might be stuck on
or are having difficulty with.  The intent of the progress reports
is to give the mentors a chance to provide suggestions for problem
resolution back to the student.

Frequent email and IRC interaction with mentors and other developers
will be strongly encouraged by suggesting students post their questions
and ideas to the mailing list, and to discuss them on #git.  Many
developers either already hold "office-hours" on IRC, or have agreed to
do so during the GSoC period.


Are you a new organization who has a Googler or other organization to vouch for you? If so, please list their name(s) here.
---------------------------------------------------------------------------------------------------------------------------

N/A


Are you an established or larger organization who would like to vouch for a new organization applying this year? If so, please list their name(s) here.
-------------------------------------------------------------------------------------------------------------------------------------------------------

N/A
