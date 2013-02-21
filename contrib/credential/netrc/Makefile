test_netrc:
	@(echo "bad data" | ./git-credential-netrc -f A -d -v) || echo "Bad invocation test, ignoring failure"
	@echo "=> Silent invocation... nothing should show up here with a missing file"
	@echo "bad data" | ./git-credential-netrc -f A get
	@echo "=> Back to noisy: -v and -d used below, missing file"
	echo "bad data" | ./git-credential-netrc -f A -d -v get
	@echo "=> Look for any entry in the default file set"
	echo "" | ./git-credential-netrc -d -v get
	@echo "=> Look for github.com in the default file set"
	echo "host=google.com" | ./git-credential-netrc -d -v get
	@echo "=> Look for a nonexistent machine in the default file set"
	echo "host=korovamilkbar" | ./git-credential-netrc -d -v get
