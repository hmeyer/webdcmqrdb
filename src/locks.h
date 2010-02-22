#ifndef LOCKS_H
#define LOCKS_H

#include <boost/thread.hpp>
#include <boost/date_time.hpp>
#include <string>

typedef boost::shared_mutex shared_mutex;

using boost::posix_time::millisec;
using boost::get_system_time;

template< class internal_lock_type, class ex = std::runtime_error >
class verbose_throw_lock {
  internal_lock_type internal_lock_;
  public:
  verbose_throw_lock( shared_mutex &m, boost::posix_time::time_duration td, const std::string &what);
  void unlock();
};

template< class internal_lock_type, class ex >
verbose_throw_lock< internal_lock_type, ex >::verbose_throw_lock( 
  shared_mutex &m, boost::posix_time::time_duration td, const std::string &what)
    :internal_lock_( m , boost::try_to_lock ) {
  if ( !internal_lock_.owns_lock() ) {
    try {
      internal_lock_.timed_lock( boost::get_system_time() + td );
      if (! internal_lock_.owns_lock() ) throw ex( what + ": cannot acquire lock");
    } catch ( boost::lock_error &e ) {
      throw ex( what + " caught boost::lock_error:" + e.what() );
    } catch ( std::exception &e ) {
      throw ex( what + " caught std::exception:" + e.what() );
    }
  }
}

template< class internal_lock_type, class ex >
void verbose_throw_lock< internal_lock_type, ex >::unlock() {
  internal_lock_.unlock();
}


typedef verbose_throw_lock< boost::unique_lock< shared_mutex > > unique_lock;
typedef verbose_throw_lock< boost::shared_lock< shared_mutex > > shared_lock;


#endif
