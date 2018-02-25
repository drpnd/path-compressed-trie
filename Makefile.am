#
# Copyright (c) 2018 Hirochika Asai.
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

EXTRA_DIST = tests/linx-rib.20141217.0000-p46.txt
bin_PROGRAMS = path_compressed_trie_test_basic
path_compressed_trie_test_basic_SOURCES = tests/basic.c tests/radix.c tests/radix.h \
	pctrie.c pctrie.h

CLEANFILES = *~

test: all
	@echo "Testing all..."
	$(top_builddir)/path_compressed_trie_test_basic
