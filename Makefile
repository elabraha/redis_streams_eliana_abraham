test: 
	g++ -std=c++17 -Wall -Wextra -pthread -o regular_tests regular_tests.cpp
concurrency_test: 
	g++ -std=c++17 -Wall -Wextra -pthread -o concurrency_test concurrency_test.cpp
# idk if I need this yet probably not necessary but can't hurt
thread_sanitizer: 
	g++ -std=c++17 -Wall -Wextra -pthread -fsanitize=thread -o concurrency_test concurrency_test.cpp
# regular make with just the interface
interface: 
	g++ -std=c++17 -Wall -Wextra -pthread -o redis_stream interface.cpp stream.cpp
run: clean interface
	./redis_stream
run_all_tests: clean test concurrency_test
	./regular_tests
	./concurrency_test
clean:
	rm -f regular_tests concurrency_test redis_stream
.PHONY: test concurrency_test interface run clean thread_sanitizer run_all_tests
