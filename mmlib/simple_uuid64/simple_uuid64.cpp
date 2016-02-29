#include "simple_uuid64.h"

namespace mmlib
{

static uint64_t FromEpoch(const char *strtime)
{
  struct tm tm;
  strptime(strtime, "%Y-%m-%d %H:%M:%S", &tm);

  return mktime(&tm) * 1000;
}

struct FNVHash
{
  int32_t operator()(const char *start, size_t len) const
  {
    return Fnv1a(start, len) % 0x400; 
  }

  int32_t operator()(const char *start, size_t len, int32_t seed) const
  {
    return (Fnv1a(start, len) % 0x400) ^ (seed & 0x3FF);
  }

private:
  static const int32_t FNV_32_INIT = 0x811c9dc5;
  static const int32_t FNV_32_PRIME = 0x01000193;
  int32_t Fnv1a(const char *start, size_t len) const
  {
    int32_t h = FNV_32_INIT;
    for (size_t i = 0; i < len; i++)
    {
      h ^= (int32_t)start[i];
      h *= FNV_32_PRIME;
    }

    return h;
  }
};


uint64_t SimpleUUID64::UUID_EPOCH_MS = FromEpoch("2011-01-01 00:00:00");
int32_t SimpleUUID64::m_seq_number = 0;

SimpleUUID64::SimpleUUID64(int32_t worker_num) : m_worker_num(worker_num)
{
}

SimpleUUID64::SimpleUUID64(uint32_t worker_id, bool) : m_worker_num(
      FNVHash()((const char *)&worker_id, sizeof(worker_id), getpid()))
{
}

uint64_t SimpleUUID64::Generate()
{
  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t now_uuid_epoch_ms = now.tv_sec * 1000 + now.tv_usec / 1000;
  now_uuid_epoch_ms -= UUID_EPOCH_MS;

  uint64_t result = now_uuid_epoch_ms << 22;
  result += (m_worker_num & 0x3FF) << 12;
  result += m_seq_number & 0xFFF;

  m_seq_number++;
  if (m_seq_number >= 0x1000)
    m_seq_number = 0;

  return result;
}

}
