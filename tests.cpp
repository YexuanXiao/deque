

#include <cassert>
// max_block_cap
#define BIZWEN_DEQUE_BASE_BLOCK_SIZE 256uz
#include "deque.hpp"
#include <ranges>
#include <vector>

template <std::size_t size>
class vsn
{
    unsigned char data[size]{};

  public:
    vsn() = default;
    vsn(std::size_t num)
    {
        data[0] = num;
    }
    vsn(vsn const &other) = default;
    vsn &operator=(vsn const &other) = default;
    ~vsn()
    {
        // non-trival
    }
    bool operator==(std::size_t num) const
    {
        return data[0] == static_cast<unsigned char>(num);
    }
};

template <typename Type>
void test()
{
    using namespace std::ranges;
    {
        // constructor (3)
        for (auto i = 0uz; i < 1000; ++i)
        {
            bizwen::deque<Type> d(i + 1uz);
            for (auto [idx, v] : views::enumerate(d))
            {
                assert(d[idx] == 0uz);
                assert(v == 0uz);
            }
        }
        // constructor (4)
        for (auto i = 0uz; i < 1000; ++i)
        {
            bizwen::deque<Type> d(i + 1uz, 0x7Euz);
            for (auto [idx, v] : views::enumerate(d))
            {
                assert(d[idx] == 0x7Euz);
                assert(v == 0x7Euz);
            }
        }
        // constructor (5)
        for (auto i = 0uz; i < 1000; ++i)
        {
            std::vector<Type> v(i + 1uz, 0x7Euz);
            bizwen::deque<Type> d(v.begin(), v.end());
            bizwen::deque<Type> d1(d.begin(), d.end());
            for (auto [idx, v] : views::enumerate(d))
            {
                assert(d[idx] == 0x7Euz);
                assert(v == 0x7Euz);
            }
            for (auto [idx, v] : views::enumerate(d1))
            {
                assert(d1[idx] == 0x7Euz);
                assert(v == 0x7Euz);
            }
            // emplace_back version tests after
        }
        // all other variants use four variants of the above
    }
}

int main()
{
    test<vsn<1uz>>();
    test<vsn<2uz>>();
    test<vsn<3uz>>();
    test<vsn<4uz>>();
    test<vsn<5uz>>();
    test<vsn<6uz>>();
    test<vsn<7uz>>();
    test<vsn<8uz>>();
    test<vsn<9uz>>();
}