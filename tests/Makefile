THIS_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
export PROJ_ROOT := $(THIS_DIR)/..

INCLUDE_DIRS := -I$(THIS_DIR) -I$(PROJ_ROOT)
export CXXFLAGS := $(INCLUDE_DIRS) -DUNIT_TEST -g3 -Wall -Wextra -Wno-unused-function -fsanitize=address

DIRS := filesystem/path filesystem/vfs

test-all:
	@for dir in $(DIRS); do \
		$(MAKE) -C $$dir test; \
	done
