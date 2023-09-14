all: generate-header-file

#CC = clang
CC = gcc

CC_FLAGS=-g -rdynamic -O3

armyknife-scheme: generate-header-file.c
	${CC} generate-header-file.c -o generate-header-file

format:
	clang-format -i generate-header-file.c

CLEAN_BINARIES = \
	a.out generate-header-file

clean:
	rm -rf *~ docs/*~ test/*~ tests/*~ scheme/*~ ${CLEAN_BINARIES} TAGS doxygen-docs 

diff: clean
	git difftool HEAD

## TESTS = ./tests/nop-test.sh \
## 	./tests/numbers-test.sh \
## 	./tests/alignment.sh \
## 	./tests/count-down-loop.sh
## 
## #
## # I'm trying to find a sensible test strategy. Tests should look
## # pretty simple and run fast.
## #	./tests/integer-binary-operators.sh
## #
## 
## test: armyknife-scheme
## 	./run-tests.sh ${TESTS}

