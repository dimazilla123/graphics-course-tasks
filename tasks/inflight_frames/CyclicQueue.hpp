#pragma once

#include <vector>
template <class T>
class CyclicQueue
{
public:
  explicit CyclicQueue(std::size_t n = 2)
    : buf(n)
    , it(0)
  {
  }

  void resize(std::size_t n)
  {
    buf.assign(n, T());
    it = 0;
  }

  void move() { it = (it + 1) % buf.size(); }

  constexpr T& get() { return buf[it]; }

private:
  std::vector<T> buf;
  std::size_t it;
};
