OUTPUT_FILE = multi-lookup

REQUESTER_LOG_FILE = test1.txt
RESOLVER_LOG_FILE = test2.txt

all: 
	gcc -Wall -Wextra -g multi-lookup.c util.c -lrt -pthread -o $(OUTPUT_FILE) 

test1:
	echo "running test1" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 5 10 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/name3.txt input/names4.txt input/names5.txt input/names3.txt 2> errlog.txt

test510:
	echo "running test510" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 5 10 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt

test11:
	echo "running test11" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 1 1 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt

test13:
	echo "running test13" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 1 3 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt

test31:
	echo "running test31" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 3 1 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt

test35:
	echo "running test35" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 3 5 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt

test53:
	echo "running test53" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 5 3 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt

test55:
	echo "running test55" >> $(REQUESTER_LOG_FILE)
	./$(OUTPUT_FILE) 5 3 $(REQUESTER_LOG_FILE) $(RESOLVER_LOG_FILE) input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt input/names1.txt input/names2.txt input/names3.txt input/names4.txt input/names5.txt 2> errlog.txt
