//
// Created by xzl on 12/11/17.
//

#ifndef CREEK_UTIL_H
#define CREEK_UTIL_H

#include <memory>
#include "boost/thread/thread.hpp" // uses macro V()
#include "log.h"

namespace creek {
	bool upgrade_locks(boost::shared_lock <boost::shared_mutex> *prlock,
										 boost::upgrade_lock <boost::shared_mutex> *pulock,
										 std::unique_ptr <boost::upgrade_to_unique_lock<boost::shared_mutex>> *ppwlock);

}

#endif //CREEK_UTIL_H
