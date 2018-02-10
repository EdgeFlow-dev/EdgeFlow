#include <iostream>
#include <cstdio>
#include <map>
#include <vector>
#include <algorithm>
#include "../Record.h"
#include "../measure.h"

using namespace std;

#define NITEM 2 * 1024 * 1024
#define USE_VECTOR
/*
   To compile
   /shared/hikey/toolchains/aarch64/bin/aarch64-linux-gnu-g++ -std=c++11 \
   -O3 test-std-groupby.cpp ../measure.c -o test-std-groupby

   ./test-std-sort input_x3.txt
 */

int main(int argc, char **argv)
{
    FILE *fh;
    uint32_t count;
    uint32_t record[3];
    {
        Record<creek::kvpair> rec;
        pair<uint32_t, uint32_t> data;
        multimap<uint32_t, uint32_t> map;
        vector<Record<creek::kvpair>> vec;
        uint32_t *record = (uint32_t*)malloc(sizeof(uint32_t) * 3 * NITEM);

        if (argc != 2) {
            cout << "Usage test-std-sort Filename" << endl;
            return -1;
        }

        fh = fopen(argv[1], "r+");
        if (!fh) {
            cout << "File " << argv[1] << "cannot be opened." << endl;
            return 1;
        }

        // 
        for (count = 0; count < NITEM; ++count) {
            if (fscanf(fh, "%d %d %d", &record[3*count], &record[3*count + 1], &record[3*count + 2]) == EOF) {
                cout << "Error!" << endl;
                return 1;
            }
            data = pair<uint32_t, uint32_t>(record[1], record[2]);
            rec = Record<creek::kvpair>(data, record[0]);
            vec.push_back(rec);
        }
        k2_measure("start");   
        for (count = 0; count < 3*NITEM; count+=3) {
            map.insert(std::pair<uint32_t, uint32_t>(record[count + 1], record[count + 2]));
        }
        // for (auto i: map)
        // cout << std::get<0>(i) << ' ' << std::get<1>(i) << endl;
    }
    k2_measure("end");
    k2_measure_flush();
    fclose(fh);
    return 0;
}
