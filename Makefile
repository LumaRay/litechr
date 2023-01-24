MODULE_NAME = litechrdrv
TEST_NAME = test
obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-objs := litechr.o context.o
KVER = `uname -r`
$(MODULE_NAME):
	make -C /lib/modules/$(KVER)/build M=$(PWD) modules
	strip --strip-debug $(MODULE_NAME).ko
debug: clean
	make -C /lib/modules/$(KVER)/build M=$(PWD) modules
all: $(MODULE_NAME) test-make
clean:
	make -C /lib/modules/$(KVER)/build M=$(PWD) clean
	rm -f ./$(TEST_NAME)
	rm -f ./*.mod
install:
	insmod $(MODULE_NAME).ko
uninstall:
	rmmod $(MODULE_NAME)
log:
	dmesg | grep litechr | tail
log-cont:
	dmesg -wH
test-make: $(TEST_NAME).c
	cc $(TEST_NAME).c -g -lpthread -Wall -o $(TEST_NAME)
test: test-make
	./$(TEST_NAME)
test-mem-install:
	apt install valgrind
test-mem: test-make
	valgrind --leak-check=full -v ./$(TEST_NAME)
