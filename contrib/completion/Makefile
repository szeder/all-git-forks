all	: git-completion.bash


git-completion.bash: git-completion.bash.in git-completion.bash.generate
	# Generate completions for binaries we have just built
	PATH="$(shell pwd)/../..:$$PATH" ./git-completion.bash.generate


clean:
	rm -f git-completion.bash
