#ifndef LOCKS_H
#define LOCKS_H

#include <boost/thread.hpp>
#include <boost/date_time.hpp>

typedef boost::shared_mutex shared_mutex;
typedef boost::unique_lock< shared_mutex > unique_lock;
typedef boost::shared_lock< shared_mutex > shared_lock;

using boost::posix_time::millisec;
using boost::get_system_time;


#endif