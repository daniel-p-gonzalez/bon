/*----------------------------------------------------------------------------*\
|*
|* Simple unobtrusive mechanism for handling automatic cleanup,
|*  useful when a function or loop has multi exit points
|*
L*----------------------------------------------------------------------------*/

#pragma once
#include <functional>

namespace bon
{

struct AutoScope
{
  AutoScope( std::function<void(void)> func )
    : func_(func), aborted_(false)
  {
  }

  ~AutoScope()
  {
    if(!aborted_)
    {
      func_();
    }
  }

  void trigger()
  {
    if(!aborted_)
    {
      func_();
    }
    aborted_ = true;
  }

  void abort()
  {
    aborted_ = true;
  }

private:
  void operator=(const AutoScope&);
  AutoScope();
  std::function<void(void)> func_;
  bool aborted_;
};

} // namespace bon
