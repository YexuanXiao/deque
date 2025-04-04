#pragma once

#if !defined(__cpp_size_t_suffix)
#error "requires __cpp_size_t_suffix"
#endif

// assert
#include <cassert>
// ptrdiff_t/size_t
#include <cstddef>
// ranges::copy/copy_back_ward/rotate/move/move_backward/remove/remove_if
#include <algorithm>
// strong_ordering/lexicographical_compare/lexicographical_compare_three_way
#include <compare>
// iterator concepts/reverse_iterator/sentinel/iterator tag
#include <iterator>
// construct_at/destroy_at/uninitialized algorithms
#include <memory>
// add_pointer/remove_pointer/remove_const/is_const/is_object
#include <type_traits>
// ranges::view_interface/subrange/sized_range/from_range_t/begin/end/swap/size/empty/views::all
#include <ranges>
// out_of_range
#include <stdexcept>
// span
#include <span>
// move/forward
#include <utility>
// initializer_list
#include <initializer_list>
// __cpp_lib_containers_ranges
#include <version>

#if not defined(__cpp_pack_indexing)
// tuple/get
#include <tuple>
#endif

// 代码规范：
// 使用等号初始化
// 内部函数可以使用auto返回值
// 函数参数和常量加const
// 非API不使用size_t之外的整数
// const写右侧
// 不分配内存/构造/移动对象一律noexcept

namespace bizwen
{
template <typename T>
class deque;

namespace detail
{

// 用于从参数包中获得前两个对象（只有两个）的引用的辅助函数
#if not defined(__cpp_pack_indexing)
template <typename Tuple>
inline constexpr auto get(Tuple args) noexcept
{
    auto &first = std::get<0uz>(args);
    auto &second = std::get<1uz>(args);
    struct iter_ref_pair
    {
        decltype(first) &begin;
        decltype(second) &end;
    };
    return iter_ref_pair{first, second};
}
#else
#if defined(__clang__) // make clang happy
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++26-extensions"
#endif
template <typename... Args>
inline constexpr auto get(Args &&...args) noexcept
{
    auto &first = args...[0uz];
    auto &second = args...[1uz];
    struct iter_ref_pair
    {
        decltype(first) &begin;
        decltype(second) &end;
    };
    return iter_ref_pair{first, second};
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

// 测试时使用固定块大小可用于测试每个块有不同元素数量时的情况
// 不要定义它，影响ABI
#if defined(BIZWEN_DEQUE_BASE_BLOCK_SIZE)
consteval std::size_t calc_block(std::size_t const pv) noexcept
{
    return BIZWEN_DEQUE_BASE_BLOCK_SIZE;
}
#else
// 根据对象的大小计算block的大小，一定是4096的整数倍
consteval std::size_t calc_block(std::size_t const pv) noexcept
{
    // 块的基本大小
    auto const base = 4096uz;
    // 基本大小下，元素最大大小，至少保证16个
    auto const sieve = base / 16uz;
    auto result = 0uz;
    if (pv < sieve)
    {
        // 在基本大小的1-8倍间找到利用率最高的
        auto size_block = 0uz;
        auto rmd_pre = std::size_t(-1);
        for (auto i = 0uz; i != 8uz; ++i)
        {
            size_block = base * (i + 1uz);
            auto const rmd_cur = size_block % pv;
            // 越小利用率越高
            if (rmd_cur < rmd_pre)
            {
                rmd_pre = rmd_cur;
                result = size_block;
            }
        }
        return result;
    }
    else
    {
        // 寻找y使得y大于16*元素大小，且y为4096整数倍
        return (pv * 16uz + base - 1uz) / base * base;
    }
}

#if !defined(NDEBUG)
static_assert(calc_block(1uz) == 4096uz);
static_assert(calc_block(2uz) == 4096uz);
static_assert(calc_block(3uz) == 3uz * 4096uz);
static_assert(calc_block(4uz) == 4096uz);
static_assert(calc_block(5uz) == 5uz * 4096uz);
static_assert(calc_block(6uz) == 3uz * 4096uz);
static_assert(calc_block(7uz) == 7uz * 4096uz);
#endif

#endif

// 根据T的大小计算每个block有多少元素
template <typename T>
constexpr std::size_t block_elements_v = calc_block(sizeof(T)) / sizeof(T);

// 构造函数和赋值用，计算如何分配和构造
template <typename T>
inline constexpr auto calc_cap(std::size_t const size) noexcept
{
    auto const block_elems = block_elements_v<T>;
    struct cap_t
    {
        std::size_t block_size;  // 需要分配多少block
        std::size_t full_blocks; // 分配了几个完整block
        std::size_t rem_elems;   // 剩下的不完整block有多少元素
    };
    return cap_t{(size + block_elems - 1uz) / block_elems, size / block_elems, size % block_elems};
}

// 当头block的元素数量小于pos时，根据pos计算步数
template <typename T>
inline constexpr auto calc_pos(std::size_t const head_or_tail_size, std::size_t const pos) noexcept
{
    assert(pos < std::size_t(-1) / 2uz);
    auto const block_elems = block_elements_v<T>;
    auto const new_pos = pos - head_or_tail_size;
    struct pos_t
    {
        std::size_t block_step;
        std::size_t elem_step;
    };
    return pos_t{new_pos / block_elems + 1uz, new_pos % block_elems};
}

template <typename T>
class basic_bucket_type;

template <typename T>
class basic_bucket_iterator
{
    using RConstT = std::remove_const_t<T>;

    friend basic_bucket_type<RConstT>;
    friend basic_bucket_type<T>;
    friend basic_bucket_iterator<T const>;

    using Block = RConstT *;

    Block *block_elem_begin{};
    Block *block_elem_end{};
    Block *block_elem_curr{};
    RConstT *elem_begin_begin{};
    RConstT *elem_begin_end{};
    RConstT *elem_end_begin{};
    RConstT *elem_end_end{};
    RConstT *elem_curr_begin{};
    RConstT *elem_curr_end{};

    constexpr basic_bucket_iterator(Block *const block_elem_begin_, Block *const block_elem_end_,
                                    Block *const block_elem_curr_, RConstT *const elem_begin_begin_,
                                    RConstT *const elem_begin_end_, RConstT *const elem_end_begin_,
                                    RConstT *const elem_end_end_, RConstT *const elem_curr_begin_,
                                    RConstT *const elem_curr_end_) noexcept
        : block_elem_begin(block_elem_begin_), block_elem_end(block_elem_end_), block_elem_curr(block_elem_curr_),
          elem_begin_begin(elem_begin_begin_), elem_begin_end(elem_begin_end_), elem_end_begin(elem_end_begin_),
          elem_end_end(elem_end_end_), elem_curr_begin(elem_curr_begin_), elem_curr_end(elem_curr_end_)
    {
    }

    constexpr basic_bucket_iterator<RConstT> remove_const() const
        requires(std::is_const_v<T>)
    {
        return {block_elem_begin, block_elem_end, block_elem_curr, elem_begin_begin, elem_begin_end,
                elem_end_begin,   elem_end_end,   elem_curr_begin, elem_curr_end};
    }

    constexpr basic_bucket_iterator &plus_and_assign(std::ptrdiff_t const pos) noexcept
    {
        block_elem_curr += pos;
        if (block_elem_curr + 1uz == block_elem_end)
        {
            elem_curr_begin = elem_end_begin;
            elem_curr_end = elem_end_end;
        }
        else if (block_elem_curr == block_elem_begin)
        {
            elem_curr_begin = elem_begin_begin;
            elem_curr_end = elem_begin_end;
        }
        else
        {
            elem_curr_begin = *block_elem_begin;
            elem_curr_end = elem_begin_begin + block_elements_v<T>;
        }
        assert(block_elem_curr < block_elem_end && block_elem_curr >= block_elem_begin);
        return *this;
    }

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::span<T>;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::random_access_iterator_tag;

    constexpr basic_bucket_iterator() noexcept = default;

    constexpr basic_bucket_iterator(basic_bucket_iterator const &other) noexcept = default;

    constexpr basic_bucket_iterator &operator=(basic_bucket_iterator const &other) noexcept = default;

    constexpr ~basic_bucket_iterator() = default;

    constexpr basic_bucket_iterator &operator++() noexcept
    {
        ++block_elem_curr;
        if (block_elem_curr + 1uz == block_elem_end)
        {
            elem_curr_begin = elem_end_begin;
            elem_curr_end = elem_end_end;
        }
        else
        {
            elem_curr_begin = *block_elem_begin;
            elem_curr_end = elem_begin_begin + block_elements_v<T>;
        }
        assert(block_elem_curr < block_elem_end);
        return *this;
    }

    constexpr basic_bucket_iterator operator++(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return ++auto{*this};
#else
        auto temp = *this;
        ++temp;
        return temp;
#endif
    }

    constexpr basic_bucket_iterator &operator--() noexcept
    {
        --block_elem_curr;
        if (block_elem_curr == block_elem_begin)
        {
            elem_curr_begin = elem_begin_begin;
            elem_curr_end = elem_begin_end;
        }
        else
        {
            elem_begin_begin = *(block_elem_begin - 1uz);
            elem_begin_end = elem_begin_begin + block_elements_v<T>;
        }
        assert(block_elem_curr >= block_elem_begin);
        return *this;
    }

    constexpr basic_bucket_iterator operator--(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return --auto{*this};
#else
        auto temp = *this;
        --temp;
        return temp;
#endif
    }

    constexpr bool operator==(basic_bucket_iterator const &other) const noexcept
    {
        return block_elem_curr == other.block_elem_curr;
    }

    constexpr std::strong_ordering operator<=>(basic_bucket_iterator const &other) const noexcept
    {
        return block_elem_curr <=> other.block_elem_curr;
    }

    constexpr std::ptrdiff_t operator-(basic_bucket_iterator const &other) const noexcept
    {
        return block_elem_curr - other.block_elem_curr;
    }

    constexpr std::span<T> operator[](std::ptrdiff_t const pos)
    {
#if defined(__cpp_auto_cast)
        return *(auto{*this} += pos);
#else
        auto temp = *this;
        temp += pos;
        return *temp;
#endif
    }

    constexpr std::span<T> operator[](std::ptrdiff_t const pos) const noexcept
    {
#if defined(__cpp_auto_cast)
        return *(auto{*this} += pos);
#else
        auto temp = *this;
        temp += pos;
        return *temp;
#endif
    }

    constexpr value_type operator*() noexcept
    {
        return {elem_curr_begin, elem_curr_end};
    }

    constexpr value_type operator*() const noexcept
    {
        return {elem_curr_begin, elem_curr_end};
    }

    constexpr basic_bucket_iterator &operator+=(std::ptrdiff_t const pos) noexcept
    {
        return plus_and_assign(pos);
    }

    constexpr basic_bucket_iterator &operator-=(std::ptrdiff_t const pos) noexcept
    {
        return plus_and_assign(-pos);
    }

    friend constexpr basic_bucket_iterator operator+(basic_bucket_iterator const &it, std::ptrdiff_t const pos) noexcept
    {
#if defined(__cpp_auto_cast)
        return auto{it}.plus_and_assign(pos);
#else
        auto temp = it;
        temp.plus_and_assign(pos);
        return temp;
#endif
    }

    friend constexpr basic_bucket_iterator operator+(std::ptrdiff_t const pos, basic_bucket_iterator const &it) noexcept
    {
        return it + pos;
    }

    friend constexpr basic_bucket_iterator operator-(std::ptrdiff_t const pos, basic_bucket_iterator const &it) noexcept
    {
        return it + (-pos);
    }

    friend constexpr basic_bucket_iterator operator-(basic_bucket_iterator const &it, std::ptrdiff_t pos) noexcept
    {
        return it + (-pos);
    }

    constexpr operator basic_bucket_iterator<T const>() const
        requires(not std::is_const_v<T>)
    {
        return {block_elem_begin, block_elem_end, block_elem_curr, elem_begin_begin, elem_begin_end,
                elem_end_begin,   elem_end_end,   elem_curr_begin, elem_curr_end};
    }
};

#if !defined(NDEBUG)
static_assert(std::random_access_iterator<basic_bucket_iterator<int>>);
static_assert(std::random_access_iterator<basic_bucket_iterator<const int>>);
#endif

template <typename T>
class basic_bucket_type : public std::ranges::view_interface<basic_bucket_type<T>>
{
    using RConstT = std::remove_const_t<T>;

    friend deque<RConstT>;
    friend basic_bucket_type<std::remove_const_t<T>>;

    using Block = RConstT *;

    Block *block_elem_begin{};
    Block *block_elem_end{};
    RConstT *elem_begin_begin{};
    RConstT *elem_begin_end{};
    RConstT *elem_end_begin{};
    RConstT *elem_end_end{};

    constexpr basic_bucket_type(Block *const block_elem_begin_, Block *const block_elem_end_,
                                RConstT *const elem_begin_begin_, RConstT *const elem_begin_end_,
                                RConstT *const elem_end_begin_, RConstT *const elem_end_end_) noexcept
        : block_elem_begin(block_elem_begin_), block_elem_end(block_elem_end_), elem_begin_begin(elem_begin_begin_),
          elem_begin_end(elem_begin_end_), elem_end_begin(elem_end_begin_), elem_end_end(elem_end_end_)
    {
    }

    constexpr std::span<T> at_impl(std::size_t const pos) const noexcept
    {
        assert(block_elem_begin + pos <= block_elem_end);
        if (pos == 0uz)
        {
            return {elem_begin_begin, elem_begin_end};
        }
        else if (block_elem_begin + pos + 1uz == block_elem_end)
        {
            return {elem_end_begin, elem_end_end};
        }
        else
        {
            auto const begin = *(block_elem_begin + pos);
            return {begin, begin + block_elements_v<T>};
        }
    }

  public:
    using value_type = std::span<T>;
    using pointer = value_type *;
    using reference = value_type &;
    using const_pointer = value_type const *;
    using const_reference = value_type const &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = basic_bucket_iterator<T>;
    using const_iterator = basic_bucket_iterator<T const>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr basic_bucket_type() = default;

    constexpr ~basic_bucket_type() = default;

    constexpr basic_bucket_type(basic_bucket_type const &) = default;

    constexpr basic_bucket_type &operator=(basic_bucket_type const &) = default;

    constexpr std::size_t size() const noexcept
    {
        return block_elem_end - block_elem_begin;
    }

    // empty and operator bool provided by view_interface

    // Since bucket_iterator is not a continuous iterator,
    // view_interface does not provide the member function data
    // constexpr void data() const noexcept = delete;

    constexpr std::span<T> front() const noexcept
    {
        return {elem_begin_begin, elem_begin_end};
    }

    constexpr std::span<T const> front() noexcept
        requires(not std::is_const_v<T>)
    {
        return {elem_begin_begin, elem_begin_end};
    }

    constexpr std::span<T> back() const noexcept
    {
        return {elem_end_begin, elem_end_end};
    }

    constexpr std::span<T const> back() noexcept
        requires(not std::is_const_v<T>)
    {
        return {elem_end_begin, elem_end_end};
    }

    constexpr std::span<T> at(std::size_t const pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr std::span<const T> at(std::size_t const pos) const noexcept
        requires(not std::is_const_v<T>)
    {
        auto const s = at_impl(pos);
        return {s.data(), s.size()};
    }

    constexpr const_iterator begin() const noexcept
    {
        return {block_elem_begin, block_elem_end, block_elem_begin, elem_begin_begin, elem_begin_end,
                elem_end_begin,   elem_end_end,   elem_begin_begin, elem_begin_end};
    }

    constexpr const_iterator end() const noexcept
    {
        if (block_elem_begin == block_elem_end)
        {
            return {block_elem_begin, block_elem_end, block_elem_end, elem_begin_begin, elem_begin_end,
                    elem_end_begin,   elem_end_end,   elem_end_begin, elem_end_end};
        }
        else
        {
            return {block_elem_begin, block_elem_end, block_elem_end - 1uz, elem_begin_begin, elem_begin_end,
                    elem_end_begin,   elem_end_end,   elem_end_begin,       elem_end_end};
        }
    }

    constexpr iterator begin() noexcept
    {
        return static_cast<basic_bucket_type const &>(*this).begin().remove_const();
    }

    constexpr iterator end() noexcept
    {
        static_cast<basic_bucket_type const &>(*this).end().remove_const();
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

    constexpr auto rbegin() noexcept
    {
        return reverse_iterator{end()};
    }

    constexpr auto rend() noexcept
    {
        return reverse_iterator{begin()};
    }

    constexpr auto rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    constexpr auto rcbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto rcend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    constexpr operator basic_bucket_type<T const>() const
        requires(not std::is_const_v<T>)
    {
        return {block_elem_begin, block_elem_end, elem_begin_begin, elem_begin_end, elem_end_begin, elem_end_end};
    }
};

template <typename T>
class basic_deque_iterator
{
    using RConstT = std::remove_const_t<T>;

    friend deque<RConstT>;
    friend basic_deque_iterator<RConstT>;
    friend basic_deque_iterator<T const>;

    using Block = RConstT *;

    Block *block_elem_begin{};
    RConstT *elem_begin{};
    RConstT *elem_curr{};
    RConstT *elem_end{};

    constexpr basic_deque_iterator(Block *const elem_begin, RConstT *const curr, RConstT *const begin,
                                   RConstT *const end) noexcept
        : block_elem_begin(elem_begin), elem_curr(curr), elem_begin(begin), elem_end(end)
    {
    }

    constexpr basic_deque_iterator<RConstT> remove_const() const noexcept
        requires(std::is_const_v<T>)
    {
        return {block_elem_begin, elem_curr, elem_begin, elem_end};
    }

    constexpr T &at_impl(std::ptrdiff_t const pos) const noexcept
    {
        if (pos >= 0uz)
        {
            // 几乎等于deque的at，但缺少断言
            auto const head_size = elem_end - elem_curr;
            if (head_size > pos)
            {
                return *(elem_curr + pos);
            }
            else
            {
                auto const [block_step, elem_step] = detail::calc_pos<T>(head_size, pos);
                auto const target_block = block_elem_begin + block_step;
                return *(*target_block + elem_step);
            }
        }
        else
        {
            auto const tail_size = elem_curr - elem_begin;
            if (tail_size > (-pos))
            {
                return *(elem_curr + pos);
            }
            else
            {
                auto const [block_step, elem_step] = detail::calc_pos<T>(tail_size, -pos);
                auto const target_block = block_elem_begin - block_step;
                return *((*target_block) + detail::block_elements_v<T> - elem_step);
            }
        }
    }

    constexpr basic_deque_iterator &plus_and_assign(std::ptrdiff_t const pos) noexcept
    {
        if (pos >= 0uz)
        {
            // 几乎等于at_impl
            auto const head_size = elem_end - elem_curr;
            if (head_size > pos)
            {
                elem_curr += pos;
            }
            else
            {
                auto const [block_step, elem_step] = detail::calc_pos<T>(head_size, pos);
                auto const target_block = block_elem_begin + block_step;
                block_elem_begin = target_block;
                elem_begin = *target_block;
                elem_curr = elem_begin + elem_step;
                elem_end = elem_begin + detail::block_elements_v<T>;
            }
        }
        else
        {
            auto const tail_size = elem_curr - elem_begin;
            if (tail_size > (-pos))
            {
                elem_curr += pos;
            }
            else
            {
                auto const [block_step, elem_step] = detail::calc_pos<T>(tail_size, -pos);
                auto const target_block = block_elem_begin - block_step;
                block_elem_begin = target_block;
                elem_begin = *target_block;
                elem_end = elem_begin + detail::block_elements_v<T>;
                elem_curr = elem_end - elem_step;
            }
        }
        return *this;
    }

  public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T *;
    using reference = T &;
    using iterator_category = std::random_access_iterator_tag;

    constexpr basic_deque_iterator() noexcept = default;

    constexpr basic_deque_iterator(basic_deque_iterator const &other) noexcept = default;

    constexpr basic_deque_iterator &operator=(basic_deque_iterator const &other) noexcept = default;

    constexpr ~basic_deque_iterator() = default;

    constexpr bool operator==(basic_deque_iterator const &other) const noexcept
    {
        return elem_curr == other.elem_curr;
    }

    constexpr std::strong_ordering operator<=>(basic_deque_iterator const &other) const noexcept
    {
        if (block_elem_begin < other.block_elem_begin)
            return std::strong_ordering::less;
        if (block_elem_begin > other.block_elem_begin)
            return std::strong_ordering::greater;
        if (elem_curr < other.elem_curr)
            return std::strong_ordering::less;
        if (elem_curr > other.elem_curr)
            return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

    T &operator*() noexcept
    {
        return *elem_curr;
    }

    T &operator*() const noexcept
    {
        return *elem_curr;
    }

    constexpr basic_deque_iterator &operator++() noexcept
    {
        // 空deque的迭代器不能自增，不需要考虑
        ++elem_curr;
        if (elem_curr == elem_end)
        {
            ++block_elem_begin;
            elem_begin = *block_elem_begin;
            elem_curr = elem_begin;
            elem_end = elem_begin + detail::block_elements_v<T>;
        }
        return *this;
    }

    constexpr basic_deque_iterator operator++(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return ++auto{*this};
#else
        auto temp(*this);
        ++temp;
        return temp;
#endif
    }

    constexpr basic_deque_iterator &operator--() noexcept
    {
        if (elem_curr != elem_begin)
        {
            --elem_curr;
        }
        else
        {
            --block_elem_begin;
            elem_begin = *block_elem_begin;
            elem_end = elem_begin + detail::block_elements_v<T>;
            elem_curr = elem_end - 1uz;
        }
        return *this;
    }

    constexpr basic_deque_iterator operator--(int) noexcept
    {
#if defined(__cpp_auto_cast)
        return --auto{*this};
#else
        auto temp(*this);
        --temp;
        return temp;
#endif
    }

    constexpr T &operator[](std::ptrdiff_t const pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr T &operator[](std::ptrdiff_t const pos) const noexcept
    {
        return at_impl(pos);
    }

    friend constexpr std::ptrdiff_t operator-(basic_deque_iterator const &lhs, basic_deque_iterator const &rhs) noexcept
    {
        return (lhs.block_elem_begin - rhs.block_elem_begin) *
                   static_cast<std::ptrdiff_t>(detail::block_elements_v<T>) +
               (lhs.elem_curr - lhs.elem_begin) - (rhs.elem_curr - rhs.elem_begin);
    }

    constexpr basic_deque_iterator &operator+=(std::ptrdiff_t const pos) noexcept
    {
        return plus_and_assign(pos);
    }

    friend constexpr basic_deque_iterator operator+(basic_deque_iterator const &it, std::ptrdiff_t const pos) noexcept
    {
#if defined(__cpp_auto_cast)
        return auto{it}.plus_and_assign(pos);
#else
        auto temp = it;
        temp.plus_and_assign(pos);
        return temp;
#endif
    }

    friend constexpr basic_deque_iterator operator+(std::ptrdiff_t const pos, basic_deque_iterator const &it) noexcept
    {
        return it + pos;
    }

    constexpr basic_deque_iterator &operator-=(std::ptrdiff_t const pos) noexcept
    {
        return plus_and_assign(-pos);
    }

    friend constexpr basic_deque_iterator operator-(basic_deque_iterator const &it, std::ptrdiff_t const pos) noexcept
    {
        return it + (-pos);
    }

    friend constexpr basic_deque_iterator operator-(std::ptrdiff_t const pos, basic_deque_iterator const &it) noexcept
    {
        return it + (-pos);
    }

    constexpr operator basic_deque_iterator<T const>() const
        requires(not std::is_const_v<T>)
    {
        return {block_elem_begin, elem_curr, elem_begin, elem_end};
    }
};

// 通用操作，合并不需要分配器的代码
template <typename T>
struct deque_proxy
{
    using RConstT = std::remove_const_t<T>;
    using Block = RConstT *;
    using CBlockP = std::conditional_t<std::is_const_v<T>, Block *const, Block *>;
    using CConstTP = std::conditional_t<std::is_const_v<T>, RConstT *const, RConstT *>;

    CBlockP &block_ctrl_begin{};
    CBlockP &block_ctrl_end{};
    CBlockP &block_alloc_begin{};
    CBlockP &block_alloc_end{};
    CBlockP &block_elem_begin{};
    CBlockP &block_elem_end{};
    CConstTP &elem_begin_first{};
    CConstTP &elem_begin_begin{};
    CConstTP &elem_begin_end{};
    CConstTP &elem_end_begin{};
    CConstTP &elem_end_end{};
    CConstTP &elem_end_last{};

    constexpr std::size_t block_elem_size() const noexcept
    {
        return block_elem_end - block_elem_begin;
    }

    constexpr std::size_t block_ctrl_size() const noexcept
    {
        return block_ctrl_end - block_ctrl_begin;
    }

    constexpr std::size_t block_alloc_size() const noexcept
    {
        return block_alloc_end - block_alloc_begin;
    }

    constexpr std::size_t size() const noexcept
    {
        auto const block_size = block_elem_end - block_elem_begin + 0uz;
        auto result = 0uz;
        if (block_size)
        {
            result += elem_begin_end - elem_begin_begin;
        }
        if (block_size > 2uz)
        {
            result += (block_size - 2uz) * detail::block_elements_v<T>;
        }
        if (block_size > 1uz)
        {
            result += elem_end_end - elem_end_begin;
        }
        return result;
    }

    constexpr void elem_begin(T *const begin, T *const end, T *const first) noexcept
    {
        elem_begin_begin = begin;
        elem_begin_end = end;
        elem_begin_first = first;
    }

    constexpr void elem_end(T *const begin, T *const end, T *const last) noexcept
    {
        elem_end_begin = begin;
        elem_end_end = end;
        elem_end_last = last;
    }

    constexpr void fill_block_alloc_end() noexcept
    {
        *block_alloc_end = nullptr;
    }

    constexpr void align_alloc_as_ctrl_back() noexcept
    {
        std::ranges::copy(block_alloc_begin, block_alloc_end, block_ctrl_begin);
        auto const block_size = block_alloc_size();
        block_alloc_begin = block_ctrl_begin;
        block_alloc_end = block_ctrl_begin + block_size;
        fill_block_alloc_end();
    }

    constexpr void align_alloc_as_ctrl_front() noexcept
    {
        std::ranges::copy_backward(block_alloc_begin, block_alloc_end, block_ctrl_end);
        auto const block_size = block_alloc_size();
        block_alloc_end = block_ctrl_end;
        block_alloc_begin = block_ctrl_end - block_size;
        fill_block_alloc_end();
    }

    constexpr void align_elem_as_alloc_back() noexcept
    {
        std::ranges::rotate(block_alloc_begin, block_elem_begin, block_elem_end);
        auto const block_size = block_elem_size();
        block_elem_begin = block_alloc_begin;
        block_elem_end = block_alloc_begin + block_size;
    }

    constexpr void align_elem_as_alloc_front() noexcept
    {
        std::ranges::rotate(block_elem_begin, block_elem_end, block_alloc_end);
        auto const block_size = block_elem_size();
        block_elem_end = block_alloc_end;
        block_elem_begin = block_alloc_end - block_size;
    }

    constexpr void align_elem_alloc_as_ctrl_back(CBlockP const ctrl_begin) noexcept
    {
        auto const alloc_block_size = block_alloc_size();
        auto const elem_block_size = block_elem_size();
        auto const alloc_elem_offset_front = block_elem_begin - block_alloc_begin;
        // 将elem_begin和alloc_begin都对齐到ctrl_begin
        std::ranges::copy(block_elem_begin, block_elem_end, ctrl_begin);
        std::ranges::copy(block_alloc_begin, block_elem_begin, ctrl_begin + elem_block_size);
        std::ranges::copy(block_elem_end, block_alloc_end, ctrl_begin + elem_block_size + alloc_elem_offset_front);
        block_alloc_begin = ctrl_begin;
        block_alloc_end = ctrl_begin + alloc_block_size;
        block_elem_begin = ctrl_begin;
        block_elem_end = ctrl_begin + elem_block_size;
        fill_block_alloc_end();
    }

    constexpr void align_elem_alloc_as_ctrl_front(CBlockP const ctrl_end) noexcept
    {
        auto const alloc_block_size = block_alloc_size();
        auto const elem_block_size = block_elem_size();
        auto const alloc_elem_offset_front = block_elem_begin - block_alloc_begin;
        std::ranges::copy_backward(block_elem_begin, block_elem_end, ctrl_end);
        std::ranges::copy_backward(block_alloc_begin, block_elem_begin, ctrl_end - elem_block_size);
        std::ranges::copy_backward(block_elem_end, block_alloc_end,
                                   ctrl_end - elem_block_size - alloc_elem_offset_front);
        block_alloc_end = ctrl_end;
        block_alloc_begin = ctrl_end - alloc_block_size;
        block_elem_end = ctrl_end;
        block_elem_begin = ctrl_end - elem_block_size;
        fill_block_alloc_end();
    }

    template <bool throw_exception = false>
    constexpr RConstT &at_impl(std::size_t const pos) const noexcept
    {
        auto const head_size = elem_begin_end - elem_begin_begin + 0uz;
        if (head_size > pos)
        {
            return *(elem_begin_begin + pos);
        }
        else
        {
            auto const [block_step, elem_step] = detail::calc_pos<T>(head_size, pos);
            auto const target_block = block_elem_begin + block_step;
            auto const check_block = target_block < block_elem_end;
            auto const check_elem =
                (target_block + 1uz == block_elem_end) ? (elem_end_begin + elem_step < elem_end_end) : true;
            if constexpr (throw_exception)
            {
                if (not(check_block && check_elem))
                    throw std::out_of_range{"bizwen::deque::at"};
            }
            else
            {
                assert(check_block && check_elem);
            }
            return *(*target_block + elem_step);
        }
    }

    // 该函数不释放对象
    constexpr void pop_back_post() noexcept
    {
        /*
        --elem_end_end;
        std::destroy_at(elem_end_end);
        */
        if (elem_end_end == elem_end_begin)
        {
            --block_elem_end;
            auto const block_size = block_elem_size();
            if (block_size == 1uz)
            {
                elem_end(elem_begin_begin, elem_begin_end, elem_begin_end);
            }
            else if (block_size)
            {
                auto const begin = *(block_elem_end - 1uz);
                auto const last = begin + detail::block_elements_v<T>;
                elem_end(begin, last, last);
            }
            else
            {
                elem_begin(nullptr, nullptr, nullptr);
                elem_end(nullptr, nullptr, nullptr);
            }
        }
        else if (block_elem_size() == 1uz)
        {
            --elem_begin_end;
        }
    }

    constexpr void pop_front_post() noexcept
    {
        /*
        std::destroy_at(elem_begin_begin);
        ++elem_begin_begin;
        */
        if (elem_begin_end == elem_begin_begin)
        {
            ++block_elem_begin;
            auto const block_size = block_elem_size();
            // 注意，如果就剩最后一个block，那么应该采用end的位置而不是计算得到
            if (block_size == 1uz)
            {
                elem_begin(elem_end_begin, elem_end_end, elem_end_begin);
            }
            else if (block_size)
            {
                auto const begin = *block_elem_begin;
                auto const last = begin + detail::block_elements_v<T>;
                elem_begin(begin, last, begin);
            }
            else
            {
                elem_begin(nullptr, nullptr, nullptr);
                elem_end(nullptr, nullptr, nullptr);
            }
        }
        else if (block_elem_size() == 1uz)
        {
            ++elem_end_begin;
        }
    }
};
} // namespace detail

template <typename T>
class deque
{
#if !defined(NDEBUG)
    static_assert(std::is_object_v<T>);
    static_assert(not std::is_const_v<T>);
#endif

    using Block = T *;

    // 块数组的起始地址
    Block *block_ctrl_begin{};
    // 块数组的结束地址
    Block *block_ctrl_end{};
    // 已分配块的起始地址
    Block *block_alloc_begin{};
    // 已分配块结束地址
    Block *block_alloc_end{};
    // 已用块的首地址
    Block *block_elem_begin{};
    // 已用块的结束地址
    Block *block_elem_end{};
    // 首个有效块的起始分配地址
    T *elem_begin_first{};
    // 首个有效块的首元素地址
    T *elem_begin_begin{};
    // 首个有效块的结束分配以及尾后元素地址
    T *elem_begin_end{};
    // 有效末尾块的起始分配以及起始元素地址
    T *elem_end_begin{};
    // 有效末尾块的尾后元素地址
    T *elem_end_end{};
    // 有效末尾块的结束分配地址
    T *elem_end_last{};
    /*
ctrl_begin→ □
             □
alloc_begin→■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
             ■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
elem_begin →■ → □□□□□□□□□■■■■■■■■■■■■■■■■■■
                 ↑          ↑                   ↑
            first      begin                end
             ■ → ■■■■■■■■■■■■■■■■■■■■■■■■■■■■
             ■ → ■■■■■■■■■■■■■■■■■■■■■■■■■■■■
elem_end   →■ → ■■■■■■■■■■■■■□□□□□□□□□□□□□□
                 ↑              ↑               ↑
             begin           end            last
             ■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
             ■ → □□□□□□□□□□□□□□□□□□□□□□□□□□
alloc_end  →□
             □
ctrl_end   →
    */
    /*
    迭代器的++在走到last时会主动走到下一个块，由于block_alloc_end的保证，使得最终走到*block_elem_end得到0
    */

    constexpr detail::deque_proxy<T> to_proxy() noexcept
    {
        return {block_ctrl_begin, block_ctrl_end, block_alloc_begin, block_alloc_end,
                block_elem_begin, block_elem_end, elem_begin_first,  elem_begin_begin,
                elem_begin_end,   elem_end_begin, elem_end_end,      elem_end_last};
    }

    constexpr detail::deque_proxy<T const> to_proxy() const noexcept
    {
        return {block_ctrl_begin, block_ctrl_end, block_alloc_begin, block_alloc_end,
                block_elem_begin, block_elem_end, elem_begin_first,  elem_begin_begin,
                elem_begin_end,   elem_end_begin, elem_end_end,      elem_end_last};
    }

    constexpr void dealloc_block(Block b) noexcept
    {
        delete[] b;
    }

    constexpr Block alloc_block()
    {
        return new T[detail::block_elements_v<T>];
    }

    constexpr Block *alloc_ctrl(std::size_t const size)
    {
        return new Block[size];
    }

    constexpr void dealloc_ctrl() noexcept
    {
        delete[] block_ctrl_begin;
    }

    constexpr void destroy_elems() noexcept
        requires std::is_trivially_destructible_v<T>
    {
        /* */
    }

    // 空deque安全，但执行后必须手动维护状态合法
    constexpr void destroy_elems() noexcept
    {
        // 4种情况，0，1，2，3+个块有元素
        auto const block_size = block_elem_size();
        if (block_size)
        {
            for (auto const &i : std::ranges::subrange{elem_begin_begin, elem_begin_end})
            {
                std::destroy_at(&i);
            }
        }
        // 清理中间的块
        if (block_size > 2uz)
        {
            for (auto const block_begin : std::ranges::subrange{block_elem_begin + 1uz, block_elem_end - 1uz})
            {
                for (auto const &i : std::ranges::subrange{block_begin, block_begin + detail::block_elements_v<T>})
                {
                    std::destroy_at(&i);
                }
            }
        }
        if (block_size > 1uz)
        {
            for (auto const &i : std::ranges::subrange{elem_end_begin, elem_end_end})
            {
                std::destroy_at(&i);
            }
        }
    }

    // 完全等于析构函数
    constexpr void destroy() noexcept
    {
        destroy_elems();
        // 清理块数组
        for (auto const i : std::ranges::subrange{block_alloc_begin, block_alloc_end})
        {
            dealloc_block(i);
        }
        dealloc_ctrl();
    }

    constexpr void elem_begin(T *const begin, T *const end, T *const first) noexcept
    {
        elem_begin_begin = begin;
        elem_begin_end = end;
        elem_begin_first = first;
    }

    constexpr void elem_end(T *const begin, T *const end, T *const last) noexcept
    {
        elem_end_begin = begin;
        elem_end_end = end;
        elem_end_last = last;
    }

    constexpr std::size_t block_elem_size() const noexcept
    {
        return block_elem_end - block_elem_begin;
    }

    constexpr std::size_t block_ctrl_size() const noexcept
    {
        return block_ctrl_end - block_ctrl_begin;
    }

    constexpr std::size_t block_alloc_size() const noexcept
    {
        return block_alloc_end - block_alloc_begin;
    }

  public:
    using value_type = T;
    using pointer = value_type *;
    using reference = value_type &;
    using const_pointer = value_type const *;
    using const_reference = value_type const &;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = detail::basic_deque_iterator<T>;
    using reverse_iterator = std::reverse_iterator<detail::basic_deque_iterator<T>>;
    using const_iterator = detail::basic_deque_iterator<T const>;
    using const_reverse_iterator = std::reverse_iterator<detail::basic_deque_iterator<T const>>;
    using bucket_type = detail::basic_bucket_type<T>;
    using const_bucket_type = detail::basic_bucket_type<T const>;

    constexpr bucket_type buckets() noexcept
    {
        return {block_elem_begin, block_elem_end, elem_begin_begin, elem_begin_end, elem_end_begin, elem_end_end};
    }

    constexpr const_bucket_type buckets() const noexcept
    {
        return {block_elem_begin, block_elem_end, elem_begin_begin, elem_begin_end, elem_end_begin, elem_end_end};
    }

    constexpr ~deque()
    {
        destroy();
    }

    constexpr bool empty() const noexcept
    {
        return elem_begin_begin == elem_begin_end;
    }

    constexpr void clear() noexcept
    {
        destroy_elems();
        block_elem_begin = block_alloc_begin;
        block_elem_end = block_alloc_begin;
        elem_begin(nullptr, nullptr, nullptr);
        elem_end(nullptr, nullptr, nullptr);
    }

    constexpr void swap(deque &other) noexcept
    {
        using std::ranges::swap;
        swap(block_ctrl_begin, other.block_ctrl_begin);
        swap(block_ctrl_end, other.block_ctrl_end);
        swap(block_alloc_begin, other.block_alloc_begin);
        swap(block_alloc_end, other.block_alloc_end);
        swap(block_elem_begin, other.block_elem_begin);
        swap(block_elem_end, other.block_elem_end);
        swap(elem_begin_first, other.elem_begin_first);
        swap(elem_begin_begin, other.elem_begin_begin);
        swap(elem_begin_end, other.elem_begin_end);
        swap(elem_end_begin, other.elem_end_begin);
        swap(elem_end_end, other.elem_end_end);
        swap(elem_end_last, other.elem_end_last);
    }

    friend constexpr void swap(deque &lhs, deque &rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // 空deque安全
    constexpr std::size_t size() const noexcept
    {
        return to_proxy().size();
    }

    constexpr std::size_t max_size() const noexcept
    {
        return std::size_t(-1) / 2uz;
    }

#if !defined(NDEBUG)
    static_assert(std::random_access_iterator<iterator>);
    static_assert(std::sentinel_for<iterator, iterator>);
#endif

    constexpr iterator begin() noexcept
    {
        return iterator{block_elem_begin, elem_begin_begin, elem_begin_begin, elem_begin_end};
    }

    constexpr iterator end() noexcept
    {
        // 空deque不能迭代所以比较相同
        // 非空deque应该在尾block是满的情况下，end永远在*block_elem_end的位置
        if (block_elem_begin == block_elem_end)
        {
            return iterator{block_elem_end, elem_end_end, elem_end_begin, elem_end_end};
        }
        else
        {
            return iterator{block_elem_end - 1uz, elem_end_end, elem_end_begin, elem_end_end};
        }
    }

    constexpr const_iterator begin() const noexcept
    {
        return const_iterator{block_elem_begin, elem_begin_begin, elem_begin_begin, elem_begin_end};
    }

    constexpr const_iterator end() const noexcept
    {
        if (block_elem_begin == block_elem_end)
        {
            return const_iterator{block_elem_end, elem_end_end, elem_end_begin, elem_end_end};
        }
        else
        {
            return const_iterator{block_elem_end - 1uz, elem_end_end, elem_end_begin, elem_end_end};
        }
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return begin();
    }

    constexpr const_iterator cend() const noexcept
    {
        return end();
    }

    constexpr auto rbegin() noexcept
    {
        return reverse_iterator{end()};
    }

    constexpr auto rend() noexcept
    {
        return reverse_iterator{begin()};
    }

    constexpr auto rbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto rend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

    constexpr auto rcbegin() const noexcept
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto rcend() const noexcept
    {
        return const_reverse_iterator{begin()};
    }

  private:
    // 参见ctrl_alloc注释
    // 在每次调整block_alloc_end时都需要调用
    constexpr void fill_block_alloc_end() noexcept
    {
        *block_alloc_end = nullptr;
    }

    // case 1           case 2            case 3           case 4
    // A1 B1 → A2 B2   A1       A2       A1 B1   A2       A1       A2 B2
    // |        |       |        |        |    ↘ |        |     ↗ |
    // C1    → C2      B1    → B2       C1      B2       B1       C2
    // |        |       |        |        |    ↘ |        |     ↗ |
    // D1       D2      D1 C1 → C2 D2    D1      C1 D1    C1 D1   D2
    // case 1 4: back
    // case 2 3: front

    // 负责分配块数组
    // 构造和扩容时都可以使用
    struct ctrl_alloc
    {
        deque &d;
        Block *block_ctrl_begin{}; // A
        Block *block_ctrl_end{};   // D

        // 替换块数组到deque
        // 构造函数专用
        // 对空deque安全
        constexpr void replace_ctrl() const noexcept
        {
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
            d.block_alloc_begin = block_ctrl_begin;
            d.block_alloc_end = block_ctrl_begin;
            d.block_elem_begin = block_ctrl_begin;
            d.block_elem_end = block_ctrl_begin;
            d.fill_block_alloc_end();
        }

        // 扩容时，back为插入元素的方向
        // 对空deque安全
        constexpr void replace_ctrl_back() const noexcept
        {

            d.align_elem_alloc_as_ctrl_back(block_ctrl_begin);
            d.dealloc_ctrl();
            // 注意顺序
            // 从alloc替换回deque
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
        }

        constexpr void replace_ctrl_front() const noexcept
        {
            d.align_elem_alloc_as_ctrl_front(block_ctrl_end);
            d.dealloc_ctrl();
            // 注意顺序
            // 从alloc替换回deque
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
        }

        // 参数是新大小
        constexpr ctrl_alloc(deque &dq, std::size_t const ctrl_size) : d(dq)
        {
            // 永远多分配一个，使得block_ctrl_end可以解引用以及*block_elem_end/*block_alloc_end始终合法
            // 通过保证*block_alloc_end为0
            // 实现迭代器可以走到block_alloc_end并且不访问野值避免UB
            auto const size = ((ctrl_size + 1uz) + (4uz - 1uz)) / 4uz * 4uz;
            block_ctrl_begin = d.alloc_ctrl(size);
            // 多分配的这个不可见
            block_ctrl_end = block_ctrl_begin + size - 1uz;
        }
    };

    // 对齐控制块
    // 对齐alloc和ctrl的begin
    constexpr void align_alloc_as_ctrl_back() noexcept
    {
        to_proxy().align_alloc_as_ctrl_back();
    }

    // 对齐控制块
    // 对齐alloc和ctrl的end
    constexpr void align_alloc_as_ctrl_front() noexcept
    {
        to_proxy().align_alloc_as_ctrl_front();
    }

    // 对齐控制块
    // 对齐elem和alloc的begin
    constexpr void align_elem_as_alloc_back() noexcept
    {
        to_proxy().align_elem_as_alloc_back();
    }

    // 对齐控制块
    // 对齐elem和alloc的end
    constexpr void align_elem_as_alloc_front() noexcept
    {
        to_proxy().align_elem_as_alloc_front();
    }

    // ctrl_begin可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_back(Block *const ctrl_begin) noexcept
    {
        to_proxy().align_elem_alloc_as_ctrl_back(ctrl_begin);
    }

    // ctrl_end可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_front(Block *const ctrl_end) noexcept
    {
        to_proxy().align_elem_alloc_as_ctrl_front(ctrl_end);
    }

    // 向前分配新block，需要block_size小于等于(block_alloc_begin - block_ctrl_begin)
    // 且不block_alloc_X不是空指针
    constexpr void extent_block_front_uncond(std::size_t const block_size)
    {
        for (auto i = 0uz; i != block_size; ++i)
        {
            --block_alloc_begin;
            *block_alloc_begin = alloc_block();
        }
    }

    // 向后分配新block，需要block_size小于等于(block_ctrl_end - block_alloc_end)
    // 且不block_alloc_X不是空指针
    constexpr void extent_block_back_uncond(std::size_t const block_size)
    {
        for (auto i = 0uz; i != block_size; ++i)
        {
            *block_alloc_end = alloc_block();
            ++block_alloc_end;
        }
        fill_block_alloc_end();
    }

    // 向back扩展
    // 对空deque安全
    constexpr void reserve_back(std::size_t const add_elem_size)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_cap = (block_elem_begin - block_alloc_begin) * detail::block_elements_v<T>;
        // 尾部块的cap
        auto const tail_block_cap = (block_alloc_end - block_elem_end) * detail::block_elements_v<T>;
        // 尾块的已使用大小
        auto const tail_cap = elem_end_last - elem_end_end;
        // non_move_cap为尾部-尾部已用，不移动块时cap
        auto const non_move_cap = tail_block_cap + tail_cap;
        // 首先如果cap足够，则不需要分配新block
        if (non_move_cap >= add_elem_size)
        {
            return;
        }
        // move_cap为头部+尾部-尾部已用，移动已分配块的cap
        auto const move_cap = head_block_cap + non_move_cap;
        // 如果move_cap够则移动
        if (move_cap >= add_elem_size)
        {
            align_elem_as_alloc_back();
            return;
        }
        // 计算需要分配多少块数组，无论接下来是什么逻辑都直接使用它
        auto const add_block_size =
            (add_elem_size - move_cap + detail::block_elements_v<T> - 1uz) / detail::block_elements_v<T>;
        // 获得目前控制块容许容量
        auto const ctrl_cap = ((block_alloc_begin - block_ctrl_begin) + (block_ctrl_end - block_alloc_end)) *
                                  detail::block_elements_v<T> +
                              move_cap;
        // 如果容许容量足够，那么移动alloc
        if (ctrl_cap >= add_elem_size)
        {
            align_elem_alloc_as_ctrl_back(block_ctrl_begin);
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc const ctrl{*this, block_alloc_size() + add_block_size}; // may throw
            ctrl.replace_ctrl_back();
        }
        extent_block_back_uncond(add_block_size);
    }

    // 向back扩展
    // 对空deque安全
    constexpr void reserve_one_back()
    {
        if (block_alloc_end != block_elem_end)
        {
            return;
        }
        if (block_elem_begin != block_alloc_begin)
        {
            align_elem_as_alloc_back();
            return;
        }
        if ((block_alloc_begin - block_ctrl_begin) + (block_ctrl_end - block_alloc_end) != 0uz)
        {
            align_elem_alloc_as_ctrl_back(block_ctrl_begin);
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc const ctrl{*this, block_alloc_size() + 1uz}; // may throw
            ctrl.replace_ctrl_back();
        }
        extent_block_back_uncond(1uz);
    }

    // 从front扩展block，空deque安全
    constexpr void reserve_front(std::size_t const add_elem_size)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_alloc_cap = (block_elem_begin - block_alloc_begin) * detail::block_elements_v<T>;
        // 尾部块的cap
        auto const tail_block_alloc_cap = (block_alloc_end - block_elem_end) * detail::block_elements_v<T>;
        // 头块的已使用大小
        auto const head_cap = elem_begin_begin - elem_begin_first;
        // non_move_cap为头部-头部已用，不移动块时cap
        auto const non_move_cap = head_block_alloc_cap + head_cap;
        // 首先如果cap足够，则不需要分配新block
        if (non_move_cap >= add_elem_size)
        {
            return;
        }
        // move_cap为头部-头部已用+尾部，移动已分配块的cap
        auto const move_cap = non_move_cap + tail_block_alloc_cap;
        // 如果move_cap够则移动
        if (move_cap >= add_elem_size)
        {
            align_elem_as_alloc_front();
            return;
        }
        // 计算需要分配多少块数组，无论接下来是什么逻辑都直接使用它
        auto const add_block_size =
            (add_elem_size - move_cap + detail::block_elements_v<T> - 1uz) / detail::block_elements_v<T>;
        // 获得目前控制块容许容量
        auto const ctrl_cap = ((block_alloc_begin - block_ctrl_begin) + (block_ctrl_end - block_alloc_end)) *
                                  detail::block_elements_v<T> +
                              move_cap;
        if (ctrl_cap >= add_elem_size)
        {
            align_elem_alloc_as_ctrl_front(block_ctrl_end);
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc const ctrl{*this, block_alloc_size() + add_block_size}; // may throw
            ctrl.replace_ctrl_front();
        }
        // 必须最后执行
        extent_block_front_uncond(add_block_size);
    }

    // 向back扩展
    // 对空deque安全
    constexpr void reserve_one_front()
    {
        if (block_elem_begin != block_alloc_begin)
        {
            return;
        }
        if (block_alloc_end != block_elem_end)
        {
            align_elem_as_alloc_front();
            return;
        }
        if ((block_alloc_begin - block_ctrl_begin) + (block_ctrl_end - block_alloc_end) != 0uz)
        {
            align_elem_alloc_as_ctrl_front(block_ctrl_end);
        }
        else
        {
            // 否则扩展控制块
            ctrl_alloc const ctrl{*this, block_alloc_size() + 1uz}; // may throw
            ctrl.replace_ctrl_front();
        }
        extent_block_front_uncond(1uz);
    }

    struct construct_guard
    {
      private:
        deque *d;

      public:
        constexpr construct_guard(deque *dp) noexcept : d(dp)
        {
        }

        constexpr void release() noexcept
        {
            d = nullptr;
        }

        constexpr ~construct_guard()
        {
            if (d)
            {
                d->destroy();
            }
        }
    };

    // 构造函数和赋值的辅助函数
    // 调用后可直接填充元素
    constexpr void extent_block(std::size_t const new_block_size)
    {
        auto const ctrl_block_size = block_ctrl_size();
        auto const alloc_block_size = block_alloc_size();
        if (ctrl_block_size == 0uz)
        {
            ctrl_alloc const ctrl(*this, new_block_size); // may throw
            ctrl.replace_ctrl();
            extent_block_back_uncond(new_block_size); // may throw
            return;
        }
        if (alloc_block_size > new_block_size)
        {
            return;
        }
        if (ctrl_block_size < new_block_size)
        {
            ctrl_alloc const ctrl(*this, new_block_size); // may throw
            ctrl.replace_ctrl_back();
        }
        else
        {
            align_alloc_as_ctrl_back();
        }
        extent_block_back_uncond(new_block_size - alloc_block_size); // may throw
    }

    // 构造函数和复制赋值的辅助函数，调用前必须分配内存，以及用于构造时使用guard
    constexpr void copy(const_bucket_type const other, std::size_t const block_size)
    {
        if (block_size)
        {
            // 此时最为特殊，因为只有一个有效快时，可以从头部生长也可以从尾部生长
            // 这里选择按头部生长简化代码
            auto const elem_size = other.elem_begin_end - other.elem_begin_begin;
            auto const first = *block_elem_end;
            auto const last = first + detail::block_elements_v<T>;
            auto const begin = last - elem_size;
            std::ranges::uninitialized_copy(other.elem_begin_begin, other.elem_begin_end, begin,
                                            std::unreachable_sentinel);
            elem_begin(begin, last, first);
            elem_end(begin, last, last);
            ++block_elem_end;
        }
        if (block_size > 2uz)
        {
            for (auto const block_begin :
                 std::ranges::subrange{other.block_elem_begin + 1uz, other.block_elem_end - 1uz})
            {
                auto const begin = *block_elem_end;
                auto const src_begin = block_begin;
                std::ranges::uninitialized_copy(src_begin, src_begin + detail::block_elements_v<T>, begin,
                                                std::unreachable_sentinel);
                elem_end_begin = begin;
                elem_end_end = begin + detail::block_elements_v<T>;
                ++block_elem_end;
            }
            elem_end_last = elem_end_end;
        }
        if (block_size > 1uz)
        {
            auto const begin = *block_elem_end;
            std::ranges::uninitialized_copy(other.elem_end_begin, other.elem_end_end, begin, std::unreachable_sentinel);
            elem_end(begin, begin + (other.elem_end_end - other.elem_end_begin), begin + detail::block_elements_v<T>);
            ++block_elem_end;
        }
    }

    static consteval void is_iterator(iterator const &) noexcept
    {
        /* */
    }

  public:
    constexpr deque() noexcept = default;

  private:
#if defined(__clang__) // make clang happy
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++26-extensions"
#endif
    // 万能构造
    // 使用count、count和T、或者随机访问迭代器进行构造
    // 注意异常安全，需要调用者使用guard，并且分配好足够多内存
    template <typename... Ts>
    constexpr void construct(std::size_t const block_size, std::size_t const full_blocks, std::size_t const rem_elems,
                             Ts &&...ts)
    {
        // 由于析构优先考虑elem_begin，因此必须独立构造elem_begin
        if (full_blocks)
        {
            auto const begin = *block_elem_end;
            auto const end = begin + detail::block_elements_v<T>;
            if constexpr (sizeof...(Ts) == 0uz)
            {
                std::ranges::uninitialized_value_construct(begin, end);
            }
            else if constexpr (sizeof...(Ts) == 1uz)
            {
                std::ranges::uninitialized_fill(begin, end, ts...);
            }
            else if constexpr (sizeof...(Ts) == 2uz)
            {
#if defined(__cpp_pack_indexing)
                auto [src_begin, src_end] = detail::get(ts...);
#else
                auto [src_begin, src_end] = detail::get(std::forward_as_tuple(ts...));
#endif
                std::ranges::uninitialized_copy(src_begin, std::unreachable_sentinel, begin,
                                                begin + detail::block_elements_v<T>);
                src_begin += detail::block_elements_v<T>;
            }
            else
            {
                static_assert(false);
            }
            elem_begin(begin, end, begin);
            elem_end(begin, end, end);
            ++block_elem_end;
        }
        if (full_blocks > 1uz)
        {
            for (auto i = 0uz; i != full_blocks - 1uz; ++i)
            {
                auto const begin = *block_elem_end;
                auto const end = begin + detail::block_elements_v<T>;
                if constexpr (sizeof...(Ts) == 0uz)
                {
                    std::ranges::uninitialized_value_construct(begin, end);
                }
                else if constexpr (sizeof...(Ts) == 1uz)
                {
                    std::ranges::uninitialized_fill(begin, end, ts...);
                }
                else if constexpr (sizeof...(Ts) == 2uz)
                {
#if defined(__cpp_pack_indexing)
                    auto [src_begin, src_end] = detail::get(ts...);
#else
                    auto [src_begin, src_end] = detail::get(std::forward_as_tuple(ts...));
#endif
                    std::ranges::uninitialized_copy(src_begin, std::unreachable_sentinel, begin,
                                                    begin + detail::block_elements_v<T>);
                    src_begin += detail::block_elements_v<T>;
                }
                else
                {
                    static_assert(false);
                }
                elem_end_begin = begin;
                elem_end_end = end;
                ++block_elem_end;
            }
            elem_end_last = elem_end_end;
        }
        if (rem_elems)
        {
            auto const begin = *block_elem_end;
            auto const end = begin + rem_elems;
            if constexpr (sizeof...(Ts) == 0uz)
            {
                std::ranges::uninitialized_value_construct(begin, end);
            }
            else if constexpr (sizeof...(Ts) == 1uz)
            {
                std::ranges::uninitialized_fill(begin, end, ts...);
            }
            else if constexpr (sizeof...(Ts) == 2uz)
            {
#if defined(__cpp_pack_indexing)
                auto [src_begin, src_end] = detail::get(ts...);
#else
                auto [src_begin, src_end] = detail::get(std::forward_as_tuple(ts...));
#endif
                std::ranges::uninitialized_copy(src_begin, src_end, begin, std::unreachable_sentinel);
            }
            else
            {
                static_assert(false);
            }
            if (full_blocks == 0uz) // 注意
            {
                elem_begin(begin, end, begin);
            }
            elem_end(begin, end, begin + detail::block_elements_v<T>);
            ++block_elem_end;
        }
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    // 参考emplace_front
    template <typename... V>
    constexpr T &emplace_back_pre(std::size_t const block_size, V &&...v)
    {
        auto const end = elem_end_end;
        std::construct_at(end, std::forward<V>(v)...); // may throw
        elem_end_end = end + 1uz;
        // 修正elem_begin
        if (block_size == 1uz)
        {
            elem_begin_end = end + 1uz;
        }
        return *end;
    }

    // 参考emplace_front
    template <typename... V>
    constexpr T &emplace_back_post(std::size_t const block_size, V &&...v)
    {
        auto const begin = *block_elem_end;
        std::construct_at(begin, std::forward<V>(v)...); // may throw
        elem_end(begin, begin + 1uz, begin + detail::block_elements_v<T>);
        ++block_elem_end;
        // 修正elem_begin，如果先前为0，说明现在是1，修正elem_begin等于elem_end
        if (block_size == 0uz)
        {
            elem_begin(begin, begin + 1uz, begin);
        }
        return *begin;
    }

  public:
    template <typename... V>
    constexpr T &emplace_back(V &&...v)
    {
        auto const block_size = block_elem_size();
        if (elem_end_end != elem_end_last)
        {
            return emplace_back_pre(block_size, std::forward<V>(v)...);
        }
        else
        {
            reserve_one_back();
            return emplace_back_post(block_size, std::forward<V>(v)...);
        }
    }

    constexpr deque(std::size_t const count)
    {
        auto const [block_size, full_blocks, rem_elems] = detail::calc_cap<T>(count);
        construct_guard guard(this);
        extent_block(block_size);
        construct(block_size, full_blocks, rem_elems);
        guard.release();
    }

    constexpr deque(std::size_t const count, T const &t)
    {
        auto const [block_size, full_blocks, rem_elems] = detail::calc_cap<T>(count);
        construct_guard guard(this);
        extent_block(block_size);
        construct(block_size, full_blocks, rem_elems, t);
        guard.release();
    }

  private:
    // 由于subrange不接受input_iterator，因此需要额外提供一个函数
    template <std::input_iterator U, typename V>
    void from_range(U &&begin, V &&end)
    {
        for (; begin != end; ++begin)
        {
            emplace_back(*begin);
        }
    }

    template <std::random_access_iterator U>
    void from_range(U &&begin, U &&end)
    {
        if (begin != end)
        {
            auto const [block_size, full_blocks, rem_elems] = detail::calc_cap<T>(end - begin);
            extent_block(block_size);
            construct(block_size, full_blocks, rem_elems, std::move(begin), std::move(end));
        }
    }

    void from_range(iterator &&begin, iterator &&end)
    {
        if (begin != end)
        {
            bucket_type bucket{begin.block_elem_begin, end.block_elem_begin + 1uz,
                               begin.elem_curr,        begin.elem_end,
                               end.elem_begin,         end.elem_curr};
            auto const block_size = bucket.size();
            extent_block(block_size);
            copy(bucket, block_size);
        }
    }

    template <typename R>
    constexpr void from_range(R &&rg)
    {
        if constexpr (requires { is_iterator(std::ranges::begin(rg)); })
        {
            from_range(std::ranges::begin(rg), std::ranges::end(rg));
        }
        else if constexpr (std::ranges::sized_range<R>)
        {
            if (auto size = std::ranges::size(rg))
            {
                auto const [block_size, full_blocks, rem_elems] = detail::calc_cap<T>(size);
                extent_block(block_size);
                construct(block_size, full_blocks, rem_elems, std::ranges::begin(rg), std::ranges::end(rg));
            }
        }
        else if constexpr (std::random_access_iterator<decltype(std::ranges::begin(rg))>)
        {
            from_range(std::ranges::begin(rg), std::ranges::end(rg));
        }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        else if constexpr (std::ranges::approximately_sized_range<R>)
        {
            if (auto size = std::ranges::reserve_hint(rg))
            {
                reserve_back();
                auto begin = std::ranges::begin(rg);
                auto end = std::ranges::end(rg);
                for (; begin != end; ++begin)
                {
                    emplace_back_noalloc(*begin);
                }
            }
        }
#endif
        else
        {
            from_range(std::ranges::begin(rg), std::ranges::end(rg));
        }
    }

  public:
    template <std::input_iterator U, typename V>
    constexpr deque(U begin, V end)
    {
        construct_guard guard(this);
        from_range(std::move(begin), std::move(end));
        guard.release();
    }

#if defined(__cpp_lib_containers_ranges)
    template <std::ranges::input_range R>  requires std::convertible_to<std::ranges::range_value_t<R>, T>
    constexpr deque(std::from_range_t, R &&rg)
    {
        construct_guard guard(this);
        from_range(rg);
        guard.release();
    }
#endif

    // 复制构造采取按结构复制的方法
    // 不需要经过extent_block的复杂逻辑
    constexpr deque(deque const &other)
    {
        construct_guard guard(this);
        auto const block_size = other.block_elem_size();
        extent_block(block_size);
        copy(other.buckets(), block_size);
        guard.release();
    }

    constexpr deque(deque &&rhs) noexcept
    {
        rhs.swap(*this);
    }

    constexpr deque(std::initializer_list<T> const ilist)
    {
        construct_guard guard(this);
        from_range(std::ranges::views::all(ilist));
        guard.release();
    }

    constexpr deque &operator=(const deque &other)
    {
        if (this != &other)
        {
            clear();
            auto const block_size = other.block_elem_size();
            extent_block(block_size);
            copy(other.buckets(), block_size);
        }
        return *this;
    }

    constexpr deque &operator=(std::initializer_list<T> ilist)
    {
        clear();
        auto const [block_size, full_blocks, rem_elems] = detail::calc_cap<T>(ilist.size());
        extent_block(block_size);
        construct(block_size, full_blocks, rem_elems, std::ranges::begin(ilist), std::ranges::end(ilist));
        return *this;
    }

    constexpr deque &operator=(deque &&other)
    {
        other.swap(*this);
        return *this;
    }

    constexpr void assign_range(deque &&d)
    {
        *this = std::move(d);
    }

    constexpr void assign_range(deque const &d)
    {
        *this = d;
    }

    template <std::ranges::input_range R>
    constexpr void assign_range(R &&rg)
    {
        clear();
        from_range(std::forward<R>(rg));
    }

    constexpr void assign(std::size_t const count, T const &value)
    {
        clear();
        auto const [block_size, full_blocks, rem_elems] = detail::calc_cap<T>(count);
        extent_block(block_size);
        construct(block_size, full_blocks, rem_elems, value);
        /*
        assign_range(std::ranges::views::repeat(value, count));
        */
    }

    template <std::input_iterator U, typename V>
    constexpr void assign(U begin, V end)
    {
        clear();
        from_range(std::move(begin), std::move(end));
    }

    constexpr void assign(std::initializer_list<T> const ilist)
    {
        assign_range(std::ranges::views::all(ilist));
    }

  private:
    // 几乎等于iterator的at，但具有检查和断言
    template <bool throw_exception = false>
    constexpr T &at_impl(std::size_t const pos) const noexcept
    {
        return to_proxy().template at_impl<throw_exception>(pos);
    }

    // 首块有空余时使用
    template <typename... V>
    constexpr T &emplace_front_pre(std::size_t const block_size, V &&...v)
    {
        auto const begin = elem_begin_begin - 1uz;
        std::construct_at(begin, std::forward<V>(v)...); // may throw
        elem_begin_begin = begin;
        if (block_size == 1uz)
        {
#if __has_cpp_attribute(assume)
            [[assume(begin + 1uz == elem_begin_begin)]];
#endif
            elem_end_begin = begin;
        }
        return *begin;
    }

    // 首块没有空余，切换到下一个块
    template <typename... V>
    constexpr T &emplace_front_post(std::size_t const block_size, V &&...v)
    {
        auto const block = block_elem_begin - 1uz;
        auto const first = *block;
        auto const end = first + detail::block_elements_v<T>;
        std::construct_at(end - 1uz, std::forward<V>(v)...); // may throw
        elem_begin(end - 1uz, end, first);
#if __has_cpp_attribute(assume)
        [[assume(block + 1uz == block_elem_begin)]];
#endif
        --block_elem_begin;
        // 修正elem_end
        if (block_size == 0uz)
        {
            elem_end(end - 1uz, end, end);
        }
        return *(end - 1uz);
    }

  public:
    template <typename... V>
    constexpr T &emplace_front(V &&...v)
    {
        auto const block_size = block_elem_size();
        if ((elem_begin_begin != elem_begin_first))
        {
            return emplace_front_pre(block_size, std::forward<V>(v)...);
        }
        else
        {
            reserve_one_front();
            return emplace_front_post(block_size, std::forward<V>(v)...);
        }
    }

    constexpr T &at(std::size_t const pos) noexcept
    {
        return at_impl<true>(pos);
    }

    constexpr T const &at(std::size_t const pos) const noexcept
    {
        return at_impl<true>(pos);
    }

    constexpr T &operator[](std::size_t const pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr T const &operator[](std::size_t const pos) const noexcept
    {
        return at_impl(pos);
    }

    // 不会失败且不移动元素
    constexpr void shrink_to_fit() noexcept
    {
        if (block_alloc_size() == 0uz) // 保证fill_block_alloc_end
        {
            return;
        }
        for (auto const i : std::ranges::subrange{block_alloc_begin, block_elem_begin})
        {
            dealloc_block(i);
        }
        block_alloc_begin = block_elem_begin;
        for (auto const i : std::ranges::subrange{block_elem_end, block_alloc_end})
        {
            dealloc_block(i);
        }
        block_alloc_end = block_elem_end;
        fill_block_alloc_end();
    }

    constexpr void push_back(T const &t)
    {
        emplace_back(t);
    }

    constexpr void push_back(T &&t)
    {
        emplace_back(std::move(t));
    }

    constexpr void push_front(T const &value)
    {
        emplace_front(value);
    }

    constexpr void push_front(T &&value)
    {
        emplace_front(std::move(value));
    }

    // 该函数调用后如果容器大小为0，则使得elem_begin/end为nullptr
    // 这是emplace_back的先决条件
    constexpr void pop_back() noexcept
    {
        assert(not empty());
        --elem_end_end;
        std::destroy_at(elem_end_end);
        to_proxy().pop_back_post();
    }

    // 参考pop_front
    constexpr void pop_front() noexcept
    {
        assert(not empty());
        std::destroy_at(elem_begin_begin);
        ++elem_begin_begin;
        to_proxy().pop_front_post();
    }

    constexpr T &front() noexcept
    {
        assert(not empty());
        return *(elem_begin_begin);
    }

    constexpr T &back() noexcept
    {
        assert(not empty());
        return *(elem_end_end - 1uz);
    }

    constexpr T const &front() const noexcept
    {
        assert(not empty());
        return *(elem_begin_begin);
    }

    constexpr T const &back() const noexcept
    {
        assert(not empty());
        return *(elem_end_end - 1uz);
    }

  private:
    constexpr void pop_back_n(std::size_t const count) noexcept
    {
        for (auto i = 0uz; i != count; ++i)
        {
            pop_back();
        }
    }

    constexpr void pop_front_n(std::size_t const count) noexcept
    {
        for (auto i = 0uz; i != count; ++i)
        {
            pop_front();
        }
    }

    template <bool back>
    struct partial_guard
    {
        deque *d;
        std::size_t const size;

      public:
        constexpr partial_guard(deque *dp, std::size_t const old_size) noexcept : d(dp), size(old_size)
        {
        }

        constexpr void release() noexcept
        {
            d = nullptr;
        }

        constexpr ~partial_guard()
        {
            if (d != nullptr)
            {
                if constexpr (back)
                {
                    d->pop_back_n(d->size() - size);
                }
                else
                {
                    d->pop_front_n(d->size() - size);
                }
            }
        }
    };

    // 用于范围构造，该函数不分配内存
    // 需要在调用前reserve足够大
    template <typename... V>
    constexpr T &emplace_front_noalloc(V &&...v)
    {
        auto const block_size = block_elem_size();
        if (elem_begin_begin != elem_begin_first)
        {
            return emplace_front_pre(block_size, std::forward<V>(v)...);
        }
        else
        {
            return emplace_front_post(block_size, std::forward<V>(v)...);
        }
    }

    // 见emplace_front_noalloc
    template <typename... V>
    constexpr T &emplace_back_noalloc(V &&...v)
    {
        auto const block_size = block_elem_size();
        if (elem_end_end != elem_end_last)
        {
            return emplace_back_pre(block_size, std::forward<V>(v)...);
        }
        else
        {
            return emplace_back_post(block_size, std::forward<V>(v)...);
        }
    }

    template <std::input_iterator U, typename V>
    constexpr void append_range_noguard(U &&begin, V &&end)
    {
        for (; begin != end; ++begin)
        {
            emplace_front(*begin);
        }
    }

    template <std::random_access_iterator U>
    constexpr void append_range_noguard(U &&begin, U &&end)
    {
        reserve_back(std::ranges::size(end - begin));
        for (; begin != end; ++begin)
        {
            emplace_front_noalloc(*begin);
        }
    }

    template <typename R>
    constexpr void append_range_noguard(R &&rg)
    {
        if (std::ranges::empty(rg))
        {
            return;
        }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        if constexpr (std::ranges::approximately_sized_range<R>)
        {
            reserve_back(std::ranges::reserve_hint(rg));
#else
        if constexpr (std::ranges::sized_range<R>)
        {
            reserve_back(std::ranges::size(rg));
#endif
            for (auto &&i : rg)
            {
                emplace_back_noalloc(std::forward<decltype(i)>(i));
            }
        }
        else
        {
            append_range_noguard(std::ranges::begin(rg), std::ranges::end(rg));
        }
    }

    template <std::input_iterator U, typename V>
    constexpr void prepend_range_noguard(U &&first, V &&last)
    {
        auto old_size = size();
        for (; first != last; ++first)
        {
            emplace_front(*first);
        }
        std::ranges::reverse(begin(), begin() + size() - old_size);
    }

    template <std::bidirectional_iterator U>
    constexpr void prepend_range_noguard(U &&first, U &&last)
    {
        for (; first != last;)
        {
            --last;
            emplace_front(*last);
        }
    }

    template <std::random_access_iterator U>
    constexpr void prepend_range_noguard(U &&first, U &&last)
    {
        reserve_front(std::ranges::size(last - first));
        for (; first != last;)
        {
            --last;
            emplace_front_noalloc(*last);
        }
    }

    template <typename R>
    constexpr void prepend_range_noguard(R &&rg)
    {
        if (std::ranges::empty(rg))
        {
            return;
        }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        if constexpr (std::ranges::approximately_sized_range<R> && std::ranges::bidirectional_range<R>)
        {
            reserve_front(std::ranges::reserve_hint(rg));
#else
        if constexpr (std::ranges::sized_range<R> && std::ranges::bidirectional_range<R>)
        {
            reserve_front(std::ranges::size(rg));
#endif
            auto begin = std::ranges::begin(rg);
            auto end = std::ranges::end(rg);
            for (; begin != end;)
            {
                --end;
                emplace_front_noalloc(*end);
            }
        }
        else if constexpr (std::ranges::bidirectional_range<R>)
        {
            prepend_range_noguard(std::ranges::begin(rg), std::ranges::end(rg));
        }
#if defined(__cpp_lib_ranges_reserve_hint) && __cpp_lib_ranges_reserve_hint >= 202502L
        else if constexpr (std::ranges::approximately_sized_range<R>)
        {
            reserve_front(std::ranges::reserve_hint(rg));
            auto count = 0uz;
            for (auto &&i : rg)
            {
                emplace_front_noalloc(std::forward<decltype(i)>(i));
                ++count;
            }
            std::ranges::reverse(begin(), begin() + count);
        }
#endif
        else if constexpr (std::ranges::sized_range<R>)
        {
            auto const count = std::ranges::size(rg);
            reserve_front(count);
            for (auto &&i : rg)
            {
                emplace_front_noalloc(std::forward<decltype(i)>(i));
            }
            std::ranges::reverse(begin(), begin() + count);
        }
        else
        {
            prepend_range_noguard(std::ranges::begin(rg), std::ranges::end(rg));
        }
    }

  public:
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, T>
    constexpr void append_range(R &&rg)
    {
        partial_guard<true> guard(this, size());
        append_range_noguard(std::forward<R>(rg));
        guard.release();
    }

    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, T>
    constexpr void prepend_range(R &&rg)
    {
        auto const old_size = size();
        partial_guard<false> guard(this, old_size);
        prepend_range_noguard(std::forward<R>(rg));
        guard.release();
    }

  private:
    // 收缩专用
    constexpr void resize_shrink(std::size_t const old_size, std::size_t const new_size) noexcept
    {
        assert(old_size > new_size);
        auto const diff = new_size - old_size;
        if constexpr (std::is_trivially_destructible_v<T>)
        {
            auto const head_size = elem_end_end - elem_end_begin;
            if (head_size > new_size)
            {
#if __has_cpp_attribute(assume)
                [[assume(elem_begin_end == elem_end_end)]];
#endif
                elem_begin_end -= diff;
                elem_end_end -= diff;
            }
            else if (head_size < new_size)
            {
                auto const [block_step, elem_step] = detail::calc_pos<T>(head_size, new_size);
                auto const target_block = block_elem_begin + block_step;
                auto const begin = *target_block;
                elem_end(begin, begin + elem_step, begin + detail::block_elements_v<T>);
                block_elem_end = target_block + 1uz;
            }
        }
        else
        {
            pop_back_n(diff);
        }
    }

    template <typename... Ts>
    constexpr void resize_unified(std::size_t const new_size, Ts... ts)
    {
        if (auto const old_size = size(); new_size < old_size)
        {
            resize_shrink(old_size, new_size);
        }
        else
        {
            partial_guard<true> guard(this, old_size);
            auto const diff = new_size - old_size;
            reserve_back(diff);
            for (auto i = 0uz; i != diff; ++i)
            {
                emplace_back_noalloc(ts...);
            }
            guard.release();
        }
    }

  public:
    // 注意必须调用clear，使得空容器的elem_begin/elem_end都为空指针
    constexpr void resize(std::size_t const new_size)
    {
        new_size == 0uz ? clear() : resize_unified(new_size);
    }

    constexpr void resize(std::size_t const new_size, T const &t)
    {
        new_size == 0uz ? clear() : resize_unified(new_size, t);
    }

  private:
    // 用于emplace的辅助函数，调用前需要判断方向
    // 该函数将后半部分向后移动1个位置
    // 从最后一个块开始
    constexpr void back_emplace(Block *const block_curr, T *const elem_curr)
    {
        auto const block_end = block_elem_end;
        auto const block_size = block_end - block_curr + 0uz;
        // 每次移动时留下的空位
        auto last_elem = elem_end_begin;
        // 先记录尾块位置
        auto end = elem_end_end;
        // 再emplace_back
        emplace_back(std::move(back()));
        // 如果大于一个块，那么移动整个尾块
        if (block_size > 1uz)
        {
            auto const begin = last_elem;
            std::ranges::move_backward(begin, end - 1uz, end);
        }
        // 移动中间的块
        if (block_size > 2uz)
        {
            auto target_block_end = block_end - 1uz;
            for (; target_block_end != block_curr + 1uz;)
            {
                --target_block_end;
                auto const begin = *target_block_end;
                auto const end = begin + detail::block_elements_v<T>;
                *last_elem = std::move(*(end - 1uz));
                last_elem = begin;
                std::ranges::move_backward(begin, end - 1uz, end);
            }
        }
        // 移动插入位置所在的块
        // if block_size
        {
            // 如果插入时容器只有一个块，那么采纳之前储存的end作为移动使用的end
            // 否则使用计算出来的end
            if (block_end - block_elem_begin != 1uz)
            {
                end = *block_curr + detail::block_elements_v<T>;
                // 只有一个块时的last_elem也无意义
                *last_elem = std::move(*(end - 1uz));
            }
            // 把插入位置所在块整体右移1
            std::ranges::move_backward(elem_curr, end - 1uz, end);
        }
    }

    // 将前半部分向前移动1
    constexpr void front_emplace(Block *const block_curr, T *const elem_curr)
    {
        auto const block_begin = block_elem_begin;
        auto const block_size = block_curr - block_begin + 1uz;
        // 向前移动后尾部空出来的的后面一个位置
        auto last_elem_end = elem_begin_end;
        // if block_size
        {
            auto const begin = elem_begin_begin;
            auto const block_end = block_elem_end;
            emplace_front(std::move(front()));
            // 如果emplace_front之前只有一个块，那么elem_curr就是终点
            if (block_end - block_begin == 1uz)
            {
                last_elem_end = elem_curr;
            }
            // 否则之前储存的last_elem_end是终点
            std::ranges::move(begin + 1uz, last_elem_end, begin);
        }
        if (block_size > 2uz)
        {
            auto const target_block_begin = block_begin + 1uz;
            for (; target_block_begin != block_curr - 1uz;)
            {
                auto const begin = *target_block_begin;
                auto const end = begin + detail::block_elements_v<T>;
                *(last_elem_end - 1uz) = std::move(*begin);
                last_elem_end = end;
                std::ranges::move(begin + 1uz, end, begin);
            }
        }
        if (block_size > 1uz)
        {
            auto const begin = *block_curr;
            *last_elem_end = std::move(*begin);
            std::ranges::move(begin + 1uz, elem_curr, begin);
        }
    }

  public:
    template <typename... Args>
    constexpr iterator emplace(const_iterator const pos, Args &&...args)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == end_pre)
        {
            emplace_back(std::forward<Args>(args)...);
            return end() - 1uz;
        }
        if (pos == begin_pre)
        {
            emplace_front(std::forward<Args>(args)...);
            return begin();
        }
        // tag construct_at
        T temp(std::forward<Args>(args)...);
        // 此时容器一定不为空
        auto const back_diff = end_pre - pos + 0uz;
        auto const front_diff = pos - begin_pre + 0uz;
        if (back_diff <= front_diff || (block_elem_size() == 1uz && elem_end_end != elem_end_last))
        {
            back_emplace(pos.block_elem_begin, pos.elem_curr);
            auto target = begin() + front_diff;
            *target = std::move(temp);
            return target;
        }
        else
        {
            front_emplace(pos.block_elem_begin, pos.elem_curr);
            auto target = begin() + front_diff;
            *target = std::move(temp);
            return target;
        }
    }

    constexpr iterator insert(const_iterator const pos, T const &value)
    {
        return emplace(pos, value);
    }

    constexpr iterator insert(const_iterator const pos, T &&value)
    {
        return emplace(pos, std::move(value));
    }

  private:
    // 把后半部分倒到前面
    constexpr void move_back_to_front(std::size_t const back_diff)
    {
        reserve_front(back_diff);
        for (auto i = 0uz; i != back_diff; ++i)
        {
            emplace_front_noalloc(std::move(back()));
            pop_back();
        }
    }

    // 逆操作
    constexpr void move_back_to_front_reverse(std::size_t const back_diff)
    {
        reserve_back(back_diff);
        for (auto &i : std::ranges::subrange(begin(), begin() + back_diff) | std::ranges::views::reverse)
        {
            emplace_back_noalloc(std::move(i));
        }
        pop_front_n(back_diff);
    }

    // 把前半部分倒到前面
    constexpr void move_front_to_back(std::size_t const front_diff)
    {
        reserve_back(front_diff);
        for (auto i = 0uz; i != front_diff; ++i)
        {
            emplace_back_noalloc(std::move(front()));
            pop_front();
        }
    }

    // 逆操作
    constexpr void move_front_to_back_reverse(std::size_t const front_diff)
    {
        reserve_front(front_diff);
        for (auto &i : std::ranges::subrange(end() - front_diff, end()))
        {
            emplace_front_noalloc(std::move(i));
        }
        pop_back_n(front_diff);
    }

    static inline constexpr auto synth_three_way = []<class U, class V>(const U &u, const V &v) {
        if constexpr (std::three_way_comparable_with<U, V>)
            return u <=> v;
        else
        {
            if (u < v)
                return std::weak_ordering::less;
            if (v < u)
                return std::weak_ordering::greater;
            return std::weak_ordering::equivalent;
        }
    };

  public:
    template <std::ranges::input_range R>
         requires std::convertible_to<std::ranges::range_value_t<R>, T>
    constexpr iterator insert_range(const_iterator const pos, R &&rg)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == end_pre)
        {
            auto const old_size = size();
            append_range_noguard(std::forward<R>(rg));
            return begin() + old_size;
        }
        if (pos == begin_pre)
        {
            prepend_range_noguard(std::forward<R>(rg));
            return begin();
        }
        auto const back_diff = end_pre - pos + 0uz;
        auto const front_diff = pos - begin_pre + 0uz;
        if (back_diff <= front_diff)
        {
            // 先把后半部分倒到前面，再插入到后面，最后把前面的倒到后面
            move_back_to_front(back_diff);
            append_range_noguard(std::forward<R>(rg));
            move_back_to_front_reverse(back_diff);
            return begin() + front_diff;
        }
        else
        {
            // 先把前半部分倒到后面，再插入到前面，最后把后面的倒到前面
            move_front_to_back(front_diff);
            prepend_range_noguard(std::forward<R>(rg));
            move_front_to_back_reverse(front_diff);
            return begin() + front_diff;
        }
    }

    constexpr iterator insert(const_iterator const pos, std::initializer_list<T> const ilist)
    {
        return insert_range(pos, std::ranges::views::all(ilist));
    }

    // 几乎等于insert_range,但是使用迭代器版本以支持input iterator
    template <std::input_iterator U, typename V>
    constexpr iterator insert(const_iterator const pos, U first, V last)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == end_pre)
        {
            auto const old_size = size();
            append_range_noguard(std::forward<U>(first), std::forward<V>(last));
            return begin() + old_size;
        }
        if (pos == begin_pre)
        {
            prepend_range_noguard(std::forward<U>(first), std::forward<V>(last));
            return begin();
        }
        auto const back_diff = end_pre - pos + 0uz;
        auto const front_diff = pos - begin_pre + 0uz;
        if (back_diff <= front_diff)
        {
            // 先把后半部分倒到前面，再插入到后面，最后把前面的倒到后面
            move_back_to_front(back_diff);
            append_range_noguard(std::forward<U>(first), std::forward<V>(last));
            move_back_to_front_reverse(back_diff);
            return begin() + front_diff;
        }
        else
        {
            // 先把前半部分倒到后面，再插入到前面，最后把后面的倒到前面
            move_front_to_back(front_diff);
            prepend_range_noguard(std::forward<U>(first), std::forward<V>(last));
            move_front_to_back_reverse(front_diff);
            return begin() + front_diff;
        }
    }

    constexpr iterator insert(const_iterator const pos, std::size_t const count, T const &value)
    {
        return insert_range(pos, std::ranges::views::repeat(value, count));
    }

    constexpr bool operator==(deque const &other) const noexcept
    {
        return size() != other.size() ? false
                                      : std::lexicographical_compare(begin(), end(), other.begin(), other.end());
    }

    constexpr auto operator<=>(deque const &other) const noexcept
        requires requires(T const &t, T const &t1) {
            { t < t1 } -> std::convertible_to<bool>;
        }
    {
        return std::lexicographical_compare_three_way(begin(), end(), other.begin(), other.end(), synth_three_way);
    }

    constexpr iterator erase(const_iterator const pos)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (pos == begin_pre)
        {
            pop_front();
            return begin();
        }
        if (pos + 1uz == end_pre)
        {
            pop_back();
            return end();
        }
        auto const back_diff = end_pre - pos + 0uz;
        auto const front_diff = pos - begin_pre + 0uz;
        if (back_diff <= front_diff)
        {
            std::ranges::move((pos + 1uz).remove_const(), end(), pos.remove_const());
            pop_back();
            return begin() + front_diff;
        }
        else
        {
            std::ranges::move_backward(begin(), pos.remove_const(), (pos + 1uz).remove_const());
            pop_front();
            return begin() + front_diff;
        }
    }

    constexpr iterator erase(const_iterator const first, const_iterator const last)
    {
        auto const begin_pre = begin();
        auto const end_pre = end();
        if (first == begin_pre)
        {
            pop_front_n(last - first);
            return begin();
        }
        if (last == end_pre)
        {
            pop_back_n(last - first);
            return end();
        }
        auto const back_diff = end_pre - last + 0uz;
        auto const front_diff = first - begin_pre + 0uz;
        if (back_diff <= front_diff)
        {
            std::ranges::move(last, end(), first.remove_const());
            pop_back();
            return begin() + front_diff;
        }
        else
        {
            std::ranges::move_backward(begin(), first.remove_const(), last.remove_const());
            pop_front();
            return begin() + front_diff;
        }
    }
};

template <std::input_iterator U, typename V>
deque(U, V) -> deque<typename std::iterator_traits<U>::value_type,
                     typename std::allocator<typename std::iterator_traits<U>::value_type>>;

template <std::ranges::input_range R>
deque(std::from_range_t, R &&) -> deque<std::ranges::range_value_t<R>, std::allocator<std::ranges::range_value_t<R>>>;

template <typename T, typename U = T>
constexpr std::size_t erase(deque<T> &c, const U &value)
{
    auto it = std::remove(c.begin(), c.end(), value);
    auto r = c.end() - it;
    c.resize(c.size() - r);
    return r;
}

template <typename T, typename Pred>
constexpr std::size_t erase_if(deque<T> &c, Pred pred)
{
    auto it = std::remove_if(c.begin(), c.end(), pred);
    auto r = c.end() - it;
    c.resize(c.size() - r);
    return r;
}
} // namespace bizwen
