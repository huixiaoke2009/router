#ifndef SNSLIB_SIMPLE_UUID64_H_
#define SNSLIB_SIMPLE_UUID64_H_

#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

namespace mmlib
{
// Generate the roughly-sorted 64 bit ids in an uncoordinated manner
// refer http://engineering.twitter.com/2010/06/announcing-snowflake.html#?oauth_error_reason=not_authed
class SimpleUUID64
{
public:
  static uint64_t UUID_EPOCH_MS;

  // Set worker_num to 0-1024 (from configuration file)
  // (that means, your machine number can not be greater than 1024)
  SimpleUUID64(int32_t worker_num);

  // Set ip/bus_id here, hash it to 0-1024 (collision might exists)
  // and add pid as a sault
  SimpleUUID64(uint32_t worker_id, bool);

  // Generate a 64 bits id
  // composed by:
  // 1. time - 41 bits (can use 69 years)
  // 2. configured worker number - 10 bits
  // 3. sequence number - 12 bits (0-4096, rolls over)
  uint64_t Generate();

  int32_t WorkerNum() { return m_worker_num; }

private:
  int32_t m_worker_num;
  static int32_t m_seq_number;
};
}

#endif
