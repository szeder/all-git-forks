#!/bin/sh

test_description='PHP hunk header test'

. ./test-lib.sh


commit () {
	test_tick &&
	cat <<EOF > foo.php &&
<?php
class Foo {

    /**
	 * Long header comment
	 * Long header comment
	 * Long header comment
	 */
	protected function Bar(\$args)
	{
		// long method
		// long method
		// long method
		// long method
		// long method
		// long method
		print("hello");
		// long method
		// long method
		// long method
		// long method
		// long method
		// long method
	}

}
EOF
	git add foo.php &&
	git commit -m "Initialized file"

	cat <<EOF > foo.php &&
<?php
class Foo {

    /**
	 * Long header comment
	 * Long header comment
	 * Long header comment
	 */
	protected function Bar(\$args)
	{
		// long method
		// long method
		// long method
		// long method
		// long method
		// long method
		print("changed");
		// long method
		// long method
		// long method
		// long method
		// long method
		// long method
	}

}
EOF
	git add foo.php &&
	git commit -m "Changed file"
}

test_expect_success 'setup' '

	commit

'

test_expect_success 'PHP hunk header includes function name' '

	git diff HEAD~1 | grep -q "protected function Bar(\\$args)"

'

test_done
