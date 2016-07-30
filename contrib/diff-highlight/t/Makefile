-include ../../../config.mak.autogen
-include ../../../config.mak

T = $(wildcard t[0-9][0-9][0-9][0-9]-*.sh)

all: test
test: $(T)

.PHONY: help clean all test $(T)

help:
	@echo 'Run "$(MAKE) test" to launch test scripts'
	@echo 'Run "$(MAKE) clean" to remove trash folders'

$(T):
	@echo "*** $@ ***"; sh $@ $(GIT_TEST_OPTS)

clean:
	$(RM) -r 'trash directory'.*
