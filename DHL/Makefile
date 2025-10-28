CC = g++ -std=c++2a -O3 -Wall -Wextra -pthread -o
TCC = g++ -std=c++2a -ggdb -Wall -Wextra -o
INC = src/road_network.cpp src/util.cpp

all: index query update test_dhl test_qc

index:
	$(CC) index src/index.cpp $(INC)
query:
	$(CC) query src/query.cpp $(INC)
update:
	$(CC) update src/update.cpp $(INC)
test_dhl:
	$(CC) test_dhl test_dhl.cpp $(INC)
test_qc:
	$(CC) test_qc_dhl test_qc_dhl.cpp $(INC)

clean:
	rm -f index query update test_dhl test_qc_dhl test_graph.txt test_queries.txt test_updates.txt csv_test_graph.txt
