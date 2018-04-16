#ifndef _C4_TPL_COMMON_HPP_
#define _C4_TPL_COMMON_HPP_

#include <c4/substr.hpp>

namespace c4 {
namespace tpl {

using csubstr = c4::csubstr;
using substr  = c4::substr;
  //using Allocator = c4::Allocator;

enum : size_t { NONE = size_t(-1) };
enum : size_t { npos = size_t(-1) };


} // namespace tpl
} // namespace c4

#endif /* _C4_TPL_COMMON_HPP_ */
