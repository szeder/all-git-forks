#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "die.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "refs.h"

#ifdef __cplusplus
}
#endif

using namespace std;


static int execute(const string& s) {
	return system(s.c_str());
}

#define execute_or_die(cmd) {                              \
	const string& command = cmd;                           \
	if(execute(command) != 0) {                            \
		die("DIED in %d: %s", __LINE__, command.c_str());  \
	}                                                      \
}


struct EachRef {
	EachRef(const string& refname, const string& sha1_hex, int flags)
		: refname(refname), sha1_hex(sha1_hex), flags(flags)
		{}

	string refname;
	string sha1_hex;
	int flags;
};

typedef vector<EachRef> Refs;


struct ExpectedRef {
	string refname;
	bool dir_as_refname;

	ExpectedRef(const string& refname, bool dir_as_refname=false)
		: refname(refname), dir_as_refname(dir_as_refname)
	{}
};

typedef vector<ExpectedRef> ExpectedRefs;

const ExpectedRefs ALL_REFS = {
	ExpectedRef("refs", true),
	ExpectedRef("refs/heads", true),
	ExpectedRef("refs/heads/br", true),
	ExpectedRef("refs/heads/br/brX", true),
	ExpectedRef("refs/heads/br1"),
	ExpectedRef("refs/heads/br2/br2"),
	ExpectedRef("refs/ot", true),
	ExpectedRef("refs/ot/otX", true),
	ExpectedRef("refs/ot2/ot2"),
	ExpectedRef("refs/remotes", true),
	ExpectedRef("refs/remotes/rm", true),
	ExpectedRef("refs/remotes/rm/rmX", true),
	ExpectedRef("refs/remotes/rm1"),
	ExpectedRef("refs/remotes/rm2/rm2"),
	ExpectedRef("refs/tags", true),
	ExpectedRef("refs/tags/tg", true),
	ExpectedRef("refs/tags/tg/tgX", true),
	ExpectedRef("refs/tags/tg1"),
	ExpectedRef("refs/tags/tg2/tg2"),
};

static ExpectedRefs filter_by_prefix(const ExpectedRefs& refs, const string& prefix) {
	ExpectedRefs filtered;
	auto pred = [prefix](const ExpectedRef& ref) {
		return ref.refname.substr(0, prefix.size()) != prefix;
	};
	remove_copy_if(refs.begin(), refs.end(), back_inserter(filtered), pred);
	return filtered;
}

string head_sha1_hex;

static void compare_returned_and_expected_refs(const Refs& refs, const ExpectedRefs& expected_refs, const string& prefix = "") {
	assert(refs.size() == expected_refs.size());

	for(unsigned int i = 0; i < expected_refs.size(); i++) {
		const EachRef& ref = refs[i];
		const ExpectedRef& expected_ref = expected_refs[i];
		assert(prefix + ref.refname == expected_ref.refname);
		assert(ref.sha1_hex == head_sha1_hex);
		assert(ref.flags == 0);
	}
}

static string read_sha1_hex_from_file(const string& filename) {
	ifstream f(filename.c_str());
	string sha1_hex;
	getline(f, sha1_hex);
	assert(sha1_hex.size() == 40);
	return sha1_hex;
}


static void create_dir_as_refname(const string& ref) {
	execute_or_die("mkdir -p .git/" + ref);
	execute_or_die("cp .git/refs/heads/br1 .git/" + ref + "/~0");
}

static void prepare_repo() {
	const string HEAD_SHA1_HEX_FILENAME = "sha1_hex";
	const string REPO_DIR = "/tmp/bla";

	if(chdir("/tmp") != 0) {
		die("chdir in %d", __LINE__);
	}
	execute_or_die("rm -rf " + REPO_DIR);
	execute_or_die("mkdir -p " + REPO_DIR);
	if(chdir(REPO_DIR.c_str()) != 0) {
		die("chdir in %d", __LINE__);
	}
	execute_or_die("git init >> /dev/null");

	execute_or_die("> file");
	execute_or_die("git add file");
	execute_or_die("git commit -m M >> /dev/null");
	execute_or_die("git rev-parse HEAD > "+ HEAD_SHA1_HEX_FILENAME);
	head_sha1_hex = read_sha1_hex_from_file(HEAD_SHA1_HEX_FILENAME);

	// Local branches
	execute_or_die("git branch -m br1"); // Rename current branch
	execute_or_die("git branch br2/br2");

	// Tags
	execute_or_die("git tag tg1");
	execute_or_die("git tag tg2/tg2");

	// Remote branches
	execute_or_die("git push . HEAD:refs/remotes/rm1 2>> /dev/null");
	execute_or_die("git push . HEAD:refs/remotes/rm2/rm2 2>> /dev/null");

	// Other
	//execute_or_die("git push . HEAD:refs/ot1");   // "funny refname "
	execute_or_die("git push . HEAD:refs/ot2/ot2 2>> /dev/null");

	// Dirs-as-refnames
	create_dir_as_refname("refs");          // allowed?
	create_dir_as_refname("refs/ot");
	create_dir_as_refname("refs/ot/otX");
	create_dir_as_refname("refs/heads");    // allowed?
	create_dir_as_refname("refs/heads/br");
	create_dir_as_refname("refs/heads/br/brX");
	create_dir_as_refname("refs/remotes");  // allowed?
	create_dir_as_refname("refs/remotes/rm");
	create_dir_as_refname("refs/remotes/rm/rmX");
	create_dir_as_refname("refs/tags");     // allowed?
	create_dir_as_refname("refs/tags/tg");
	create_dir_as_refname("refs/tags/tg/tgX");
}

template <typename T>
static void show_reflist(const string& title, const T& reflist) {
	cout << title << endl;
	for(auto& ref : reflist) {
		cout << "\t" << ref.refname << endl;
	}
}


static string sha1_to_sha1_hex(const unsigned char *sha1) {
	stringstream ss;
	ss.fill('0');
	for(int i = 0; i < 20; i++) {
		ss.width(2);
		ss << std::hex << (unsigned int)sha1[i];
	}
	return ss.str();
}


static int test_each_ref_fn(const char *refname, const unsigned char *sha1, int flags, void *cb_data) {
	Refs& refs = *static_cast<Refs*>(cb_data);
	string sha1_hex = sha1_to_sha1_hex(sha1);
	refs.push_back(EachRef(refname, sha1_hex, flags));
	return 0;
}


static void test_head_ref() {
	Refs refs;
	head_ref(test_each_ref_fn, &refs);

	assert(refs.size() == 1);
	assert(refs[0].refname == "HEAD");
	assert(refs[0].sha1_hex == head_sha1_hex);
	assert(refs[0].flags == REF_ISSYMREF);
}


static void test_for_each_ref() {
	Refs refs;
	for_each_ref(test_each_ref_fn, &refs);
	compare_returned_and_expected_refs(refs, ALL_REFS);
}


static void test_for_each_ref_in() {
	Refs refs;
	const string prefix = "refs/hea";
	for_each_ref_in(prefix.c_str(), test_each_ref_fn, &refs);

	const ExpectedRefs expected_refs = filter_by_prefix(ALL_REFS, prefix);
	compare_returned_and_expected_refs(refs, expected_refs, prefix);
}


static void test_for_each_tag_ref() {
	Refs refs;
	for_each_tag_ref(test_each_ref_fn, &refs);

	const string prefix = "refs/tags/";
	const ExpectedRefs expected_refs = filter_by_prefix(ALL_REFS, prefix);
	compare_returned_and_expected_refs(refs, expected_refs, prefix);
}


static void test_for_each_branch_ref() {
	Refs refs;
	for_each_branch_ref(test_each_ref_fn, &refs);

	const string prefix = "refs/heads/";
	const ExpectedRefs expected_refs = filter_by_prefix(ALL_REFS, prefix);
	compare_returned_and_expected_refs(refs, expected_refs, prefix);
}


static void test_for_each_remote_ref() {
	Refs refs;
	for_each_remote_ref(test_each_ref_fn, &refs);

	const string prefix = "refs/remotes/";
	const ExpectedRefs expected_refs = filter_by_prefix(ALL_REFS, prefix);
	compare_returned_and_expected_refs(refs, expected_refs, prefix);
}


static void test_for_each_rawref() {
	Refs refs;
	for_each_rawref(test_each_ref_fn, &refs);
	compare_returned_and_expected_refs(refs, ALL_REFS);
}


static void test_ref_exists() {
	for(auto& ref : ALL_REFS) {
		string refname = ref.refname;
		assert(ref_exists(refname.c_str()));

		refname += "/~0";
		assert(!ref_exists(refname.c_str()));
	}
}


extern "C" int delete_ref(const char *refname, const unsigned char *sha1, int delopt);
static void test_delete_ref() {
	for(auto& ref : ALL_REFS) {
		prepare_repo();
		cout << '.' << flush;
		delete_ref(ref.refname.c_str(), NULL, 0);

		for(auto& not_deleted_ref : ALL_REFS) {
			if(not_deleted_ref.refname == ref.refname) {
				assert(!ref_exists(not_deleted_ref.refname.c_str()));
			} else {
				assert(ref_exists(not_deleted_ref.refname.c_str()));
			}
		}
	}
}


#define TEST(name) {                                         \
	cout << "Testing " #name "() " << flush;                 \
	pid_t p = fork();                                        \
	if(p) {                                                  \
		/* Parent */                                         \
		int status;                                          \
		waitpid(p, &status, 0);                              \
		if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {  \
			cout << " \e[32mOK\e[0m" << endl;                \
		} else {                                             \
			cout << " \e[31mFAIL\e[0m" << endl;              \
		}                                                    \
	} else {                                                 \
		/* Child */                                          \
		test_##name();                                       \
		exit(0);                                             \
	}                                                        \
}


int main() {
	prepare_repo();

	/*
	*/
	TEST(head_ref);
	TEST(for_each_ref);
	TEST(for_each_ref_in);
	TEST(for_each_tag_ref);
	TEST(for_each_branch_ref);
	TEST(for_each_remote_ref);
	TEST(for_each_rawref);
	TEST(ref_exists);
	TEST(delete_ref);

	cout << "THE END" << endl;
	return 0;
}
