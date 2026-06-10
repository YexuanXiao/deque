module;
#include <cassert>

export module bizwen.deque;

import std;

#define BIZWEN_MODULE

export "C++"
{
#include "./deque.hpp"
}
