

#include <cassert>
#include <initializer_list>
#include <numeric>
#include <ranges>
#include <vector>
#include <deque>
#if !defined(__cpp_size_t_suffix) || __cpp_size_t_suffix < 202011L
// make IntelliSence happy
inline constexpr std::size_t operator"" uz(unsigned long long const value) noexcept
{
    return value;
}
#endif
// max_block_cap
#define BIZWEN_DEQUE_BASE_BLOCK_SIZE 256uz
#include "deque.hpp"

template <std::size_t Size>
class vsn
{
    unsigned char data[Size]{};

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

template <typename deque>
void test_constructor(std::size_t count = 1000uz)
{
    using namespace std::ranges;
    {
        // (1) zero construct
        {
            deque d{};
            assert(d.size() == 0uz);
            assert(d.empty());
        }
        // (2) not support
        // (3)
        for (auto i = 0uz; i != count; ++i)
        {
            deque d(i + 1uz);
            assert(d.size() == (i + 1uz));
            assert(!d.empty());
            for (auto [idx, v] : views::enumerate(d))
            {
                assert(d[idx] == 0uz);
                assert(v == 0uz);
            }
            d.clear();
            assert(d.empty());
        }
        // (4)
        for (auto i = 0uz; i != count; ++i)
        {
            deque d(i + 1uz, 0x7Euz);
            assert(d.size() == (i + 1uz));
            for (auto [idx, v] : views::enumerate(d))
            {
                assert(d[idx] == 0x7Euz);
                assert(v == 0x7Euz);
            }
        }
        // (5)
        for (auto i = 0uz; i != count; ++i)
        {
            std::vector<typename deque::value_type> v(i + 1uz, 0x7Euz);
            deque d(v.begin(), v.end());
            assert(d.size() == (i + 1uz));
            deque d1(d.begin(), d.end());
            assert(d1.size() == (i + 1uz));
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
            // forward iterator variant are equivalent to emplace_back
        }
        // (6) equivalent to (4), (5) and operator= (1)
        {
            std::vector<typename deque::value_type> v{};
            deque d(std::from_range, v);
        }
        // (7) equivalent to (5)
        {
            deque d{};
            auto d1{d};
        }
        // (8) equivalent to swap
        {
            deque d{};
            auto d1{std::move(d)};
        }
        // (9, 10) not support
        // (11) equivalent to (5.1)
        {
            deque d{1uz, 2uz, 3uz, 4uz};
        }
    }
}

template <typename deque>
void test_operator_assign(std::size_t count = 1000uz)
{
    using namespace std::ranges;
    // (1) equivalent to copy constructor (7)
    {
        for (auto i = 0uz; i != count; ++i)
        {
            deque d(std::from_range, std::ranges::iota_view(0uz, i + 1uz));
            deque d1(100uz);
            d1 = d;
            assert(d.size() == (i + 1uz));
            for (auto [idx, v] : views::enumerate(d1))
            {
                assert(d[idx] == idx);
                assert(v == idx);
            }
        }
    }
    // (2) equivalent to swap
    // (3) equivalent to (1) and constructor (5.1)
}

// assign
// (1) equivalent to constructor (4)
// (2) equivalent to constructor (5)
// (3) equivalent to constructor (11)

// assign_range
// equivalent to assign

// operator at tests in other tests above
// at equivalent to operator at

// front/back never fail

// size/empty tests in constructor's above and others

// shrink_to_fit never fail

// clear tests in constructor (3) and others

// todo: emplace、insert、insert_range、emplace、erase、

template <typename deque>
void test_emplace_back(std::size_t count = 1000uz)
{
    using namespace std::ranges;
    {
        for (auto i = 0uz; i != count; ++i)
        {
            deque d{};
            for (auto j = 0uz; j != i; ++j)
            {
                d.emplace_back(j);
                assert(d[j] == j);
                assert(d.size() == (j + 1uz));
            }
            for (auto j = 0uz; j != i; ++j)
            {
                auto const size = i - j;
                assert(d.size() == size);
                assert(d[size - 1uz] == size - 1uz);
                if (i)
                {
                    d.pop_back();
                }
            }
        }
        for (auto i = 0uz; i != count; ++i)
        {
            auto const half_head = (i + 1uz) / 2uz;
            deque d{std::from_range, std::ranges::iota_view(0uz, half_head)};
            for (auto j = half_head; j != i; ++j)
            {
                d.emplace_back(j);
                assert(d.size() == (j + 1uz));
                assert(d[j] == j);
            }
            for (auto j = 0uz; j != i; ++j)
            {
                auto const size = i - j;
                auto si = d.size();
                assert(d.size() == size);
                assert(d[0] == j);
                if (i)
                {
                    d.pop_front();
                }
            }
        }
    }
}

template <typename deque>
void test_emplace_front(std::size_t count = 1000uz)
{
    using namespace std::ranges;
    {
        for (auto i = 0uz; i != count; ++i)
        {
            deque d{};
            for (auto j = 0uz; j != i; ++j)
            {
                d.emplace_front(i - j - 1uz);
                assert(d[0uz] == i - j - 1uz);
                assert(d.size() == (j + 1uz));
            }
            for (auto [idx, v] : views::enumerate(d))
            {
                assert(d[idx] == idx);
                assert(v == idx);
            }
            for (auto j = 0uz; j != i; ++j)
            {
                auto const size = i - j;
                assert(d.size() == size);
                assert(d[size - 1uz] == size - 1uz);
                if (i)
                {
                    d.pop_back();
                }
            }
        }
        for (auto i = 0uz; i != count; ++i)
        {
            auto const half_head = (i + 1uz) / 2uz;
            deque d{std::from_range, std::ranges::iota_view(half_head, i)};
            for (auto j = 0; j != half_head; ++j)
            {
                d.emplace_front(half_head - j - 1uz);
                assert(d.size() == ((i - half_head) + j + 1uz));
                assert(d[0uz] == half_head - j - 1uz);
            }
            for (auto [idx, v] : views::enumerate(d))
            {
                assert(d[idx] == idx);
                assert(v == idx);
            }
            for (auto j = 0uz; j != i; ++j)
            {
                auto const size = i - j;
                auto si = d.size();
                assert(d.size() == size);
                assert(d[0] == j);
                if (i)
                {
                    d.pop_front();
                }
            }
        }
    }
}

template <typename deque>
void test_all(std::size_t count = 1000uz){
    test_constructor<deque>(count);
    test_operator_assign<deque>(count);
    test_emplace_back<deque>(count);
    test_emplace_front<deque>(count);
}

int main()
{
    test_all<bizwen::deque<vsn<1uz>>>();
    test_all<bizwen::deque<vsn<2uz>>>();
    test_all<bizwen::deque<vsn<3uz>>>();
    test_all<bizwen::deque<vsn<4uz>>>();
    test_all<bizwen::deque<vsn<5uz>>>();
    test_all<bizwen::deque<vsn<6uz>>>();
    test_all<bizwen::deque<vsn<7uz>>>();
    test_all<bizwen::deque<vsn<8uz>>>();
    test_all<bizwen::deque<vsn<9uz>>>();

    test_all<std::deque<vsn<1uz>>>();
    test_all<std::deque<vsn<2uz>>>();
    test_all<std::deque<vsn<3uz>>>();
    test_all<std::deque<vsn<4uz>>>();
    test_all<std::deque<vsn<5uz>>>();
    test_all<std::deque<vsn<6uz>>>();
    test_all<std::deque<vsn<7uz>>>();
    test_all<std::deque<vsn<8uz>>>();
    test_all<std::deque<vsn<9uz>>>();
}