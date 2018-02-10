#ifndef RANDOM_SOURCE_H
#define RANDOM_SOURCE_H

#define TOTAL_RECS 		100000
#define NUM_CAMPAIGN	100
#define ADS_PER_CAMP	10

/* ad-types 	: banner, modal, sponsored-search, mail, mobile"
   event-types	: view, click, purchase */


namespace {

}

template<typename T>
class RandomSource : public SourceBase<T> {
public:
	RandomSource(const string & addr, uint8_t rec_type,
			uint64_t ts_start, uint64_t ts_delta, uint64_t rpb, uint64_t interval)
		: SourceBase<T>(addr, rec_type, ts_start, ts_delta, rpb, interval) {

		this->kv = (T *) malloc (sizeof(T) * TOTAL_RECS);
	}

	~FileSource() {
		this->bundle_pool.clear();
		free(this->kv);
	}

private:
	uint64_t GenerateRandomKV(void) {
		uint64_t i;
		uint64_t start = 0;
		uint64_t cnt = 0 ;

		while (cnt < TOTAL_RECS) {
			kv[cnt] = std::make_tuple();
			/* TS_POS = 1 */
			for (i = TS_POS + 1; i < rec_type; i++) {
				/* generate random values */
				/* specify range later */
				std::get<i>(kv[cnt]) = rand();
			}
			cnt++;

			if (cnt % nrecs == 0) {
				bundle_pool.push_back(&kb[start]);
				start = cnt;
			}
		}
		return cnt;
	}

	void MakeBundlePool(void) {
		GenerateRandomKV();
	}
};

#endif
