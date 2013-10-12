TESTS := $(wildcard test-*.t)

export T := $(addprefix $(CURDIR)/,$(TESTS))
export MAKE := $(MAKE) -e
export PATH := $(CURDIR):$(PATH)
export TEST_LINT := test-lint-executable test-lint-shell-syntax
export TEST_DIRECTORY := $(CURDIR)/../../t

all:

test:
	$(MAKE) -C ../../t $@

$(TESTS): all
	$(MAKE) -C ../../t $(CURDIR)/$@

.PHONY: all test $(TESTS)
