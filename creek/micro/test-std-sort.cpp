#include <iostream>
#include <cstdio>
#include <vector>
#include <array>
#include <algorithm>
#include "../Record.h"
#include "../measure.h"

using namespace std;

#define NITEM 8 * 1024 * 1024

/*
   To compile
   /shared/hikey/toolchains/aarch64/bin/aarch64-linux-gnu-g++ -std=c++11 \
   -O3 test-std-sort.cpp ../measure.c -o test-std-sort -Wall

   ./test-std-sort input_x3.txt
 */
struct K2VCompLess {
    bool operator()(const Record<creek::kvpair> & lhs, const Record<creek::kvpair> & rhs) const {
        return std::get<0>(lhs.data) < std::get<0>(rhs.data);
    }
} K2VCompLess;

template <size_t N>
struct arraycmp {
	bool operator()(const array<uint32_t, N> & lhs, const array<uint32_t, N> & rhs) const {
		return lhs[0] < rhs[0];
	}
};

template <size_t N>
struct arraycmp2 {
	bool operator()(const array<uint32_t, N> & lhs, const array<uint32_t, N> & rhs) const {
		if (lhs[0] < rhs[0])
			return true;
		if (lhs[0] > rhs[0])
			return false;
		return (lhs[1] < rhs[1]);
	}
};

#define arraylen 4

int main(int argc, char **argv)
{
    FILE *fh;
    uint32_t count;
    uint32_t record[8];

//		using T = Record<creek::kvpair>;
		using T = array<uint32_t, arraylen>;

		T rec;
		pair<uint32_t, uint32_t> data;
		// vector<Record<creek::kvpair>> vec;
//		vector<uint32_t> vec;
		auto vec = new vector<T>();

		if (argc != 2) {
				cout << "Usage test-std-sort Filename" << endl;
				return -1;
		}

		fh = fopen(argv[1], "r+");
		if (!fh) {
				cout << "File " << argv[1] << "cannot be opened." << endl;
				return 1;
		}

		/* read in data from file ... */
		for (count = 0; count < NITEM; ++count) {
				if (fscanf(fh, "%d %d %d", &record[0], &record[1], &record[2]) == EOF) {
						cout << "Error!" << endl;
						return 1;
				}
				// data = pair<uint32_t, uint32_t>(record[1], record[2]);
//			 	rec = Record<creek::kvpair>(make_pair(record[1], record[2]), record[0]);
			for (unsigned i = 0; i < arraylen; i++)
				rec[i] = record[i];
			vec->push_back(rec);
//				vec.push_back(record[1]);
		}

		printf("total %lu items, each %lu bytes\n", vec->size(), sizeof(T));

		k2_measure("start");
//		std::sort(vec.begin(), vec.end());
//		std::sort(vec->begin(), vec->end(), K2VCompLess);
		std::sort(vec->begin(), vec->end(), arraycmp2<arraylen>());
//		delete vec;
    k2_measure("end");
//    k2_measure_flush();

     for (unsigned i = 0; i < 100; i++)
         printf("%u ", vec->at(i)[0]);
     cout << endl;

		k2_measure_flush();

	delete vec;

	fclose(fh);
    return 0;
}
