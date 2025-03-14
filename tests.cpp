

#include <cassert>
#include <deque>
#include <initializer_list>
#include <numeric>
#include <ranges>
#include <vector>

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
            // forward/bidirectional iterator variant are equivalent to emplace_back
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
    {
        deque d(100uz);
        deque d1{};
        d1 = d;
        assert(d1.size() == 100uz);
        d = d1;
        assert(d.size() == 100uz);
    }
    // (3) equivalent to (1) and constructor (5.1)
    {
        deque d(100uz);
        d = {0uz, 1uz, 2uz, 3uz};
        assert(d.size() == 4uz);
    }
}

// assign
template <typename deque>
void test_assign(std::size_t count = 1000uz)
{
    using namespace std::ranges;
    // (1) equivalent to constructor (4)
    {
        deque d(100uz);
        d.assign(100uz,1uz);
        assert(d.size() == 100uz);
    }
    // (2) equivalent to constructor (5)
    {
        deque d(100uz);
        deque d1(100uz);
        d1.assign(d.begin(), d.end());
        assert(d1.size() == 100uz);
    }
    // (3) equivalent to constructor (11)
    {
        deque d(100uz);
        deque d1(100uz);
        d1.assign({0uz, 1uz, 2uz, 3uz});
        assert(d1.size() == 4uz);
    }
}

// assign_range
// equivalent to assign

template <typename deque>
void test_assign_range(std::size_t count = 1000uz)
{
    deque d{};
    auto ilist = {0uz, 1uz, 2uz, 3uz};
    d.assign_range(ilist);
    assert(d.size() == 4uz);
}

// operator at tests in other tests above
// at equivalent to operator at

// front/back never fail
template <typename deque>
void test_front_back(std::size_t count = 1000uz)
{
    deque d(1uz);
    d.front();
    d.back();
    deque const &d1 = d;
    d1.front();
    d1.back();
}

// size/empty tests in constructor's above and others

// shrink_to_fit never fail
template <typename deque>
void test_shrink(std::size_t count = 1000uz)
{
    deque d{};
    d.shrink_to_fit();
}

// clear tests in constructor (3) and others
template <typename deque>
void test_clear(std::size_t count = 1000uz)
{
    deque d{100uz};
    d.clear();
    assert(d.empty());
}

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

// pop_back/pop_front tests in emplace_back/emplace_front

template <typename Type>
void test_all(std::size_t count = 1000uz)
{
    {
        test_constructor<std::deque<Type>>(count);
        test_operator_assign<std::deque<Type>>(count);
        test_assign<std::deque<Type>>(count);
        test_assign_range<std::deque<Type>>(count);
        test_front_back<std::deque<Type>>(count);
        test_shrink<std::deque<Type>>(count);
        test_clear<std::deque<Type>>(count);
        test_emplace_back<std::deque<Type>>(count);
        test_emplace_front<std::deque<Type>>(count);
    }
    {
        test_constructor<bizwen::deque<Type>>(count);
        test_operator_assign<bizwen::deque<Type>>(count);
        test_assign<bizwen::deque<Type>>(count);
        test_assign_range<bizwen::deque<Type>>(count);
        test_front_back<bizwen::deque<Type>>(count);
        test_shrink<bizwen::deque<Type>>(count);
        test_clear<bizwen::deque<Type>>(count);
        test_emplace_back<bizwen::deque<Type>>(count);
        test_emplace_front<bizwen::deque<Type>>(count);
    }
}

int main()
{
    test_all<vsn<1uz>>();
    test_all<vsn<2uz>>();
    test_all<vsn<3uz>>();
    test_all<vsn<4uz>>();
    test_all<vsn<5uz>>();
    test_all<vsn<6uz>>();
    test_all<vsn<7uz>>();
    test_all<vsn<8uz>>();
    test_all<vsn<9uz>>();
}