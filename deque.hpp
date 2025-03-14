#pragma once

// assert
#include <cassert>
// ptrdiff_t/size_t
#include <cstddef>
// ranges::copy/copy_back_ward/rotate
#include <algorithm>
// strong_ordering
#include <compare>
// iterator concepts/counted_iterator/reverse_iterator/sentinel/iterator tag
#include <iterator>
// construct_at/destroy_at/uninitialized_algorithm
#include <memory>
// add_pointer/remove_pointer/remove_const/add_const/is_const/is_object
#include <type_traits>
// ranges::subrange/sized_range/from_range_t/begin/end/swap
#include <ranges>
// out_of_range
#include <stdexcept>
// span
#include <span>
// move/forward
#include <utility>
// initializer_list
#include <initializer_list>

#if not defined(__cpp_pack_indexing)
// tuple/get
#include <tuple>
#endif

namespace bizwen
{
template <typename T>
class deque;

namespace detail
{

// 测试时使用固定块大小可用于测试每个块有不同元素数量时的情况
// 不要定义它，影响ABI
#if defined(BIZWEN_DEQUE_BASE_BLOCK_SIZE)
consteval std::size_t calc_block(std::size_t pv) noexcept
{
    return BIZWEN_DEQUE_BASE_BLOCK_SIZE;
}
#else
// 返回每个块的大小，一定是4096的整数倍
consteval std::size_t calc_block(std::size_t pv) noexcept
{
    // 块的基本大小
    auto constexpr base = 4096uz;
    // 基本大小下，元素最大大小，至少保证16个
    auto constexpr sieve = base / 16uz;
    auto result = 0uz;
    if (pv < sieve)
    {
        // 在基本大小的1-8倍间找到利用率最高的
        auto block_sz = 0uz;
        auto rmd_pre = std::size_t(-1);
        for (auto i = 0uz; i != 8uz; ++i)
        {
            block_sz = base * (i + 1uz);
            auto rmd_cur = block_sz % pv;
            // 越小利用率越高
            if (rmd_cur < rmd_pre)
            {
                rmd_pre = rmd_cur;
                result = block_sz;
            }
        }
    }
    else
    {
        // 寻找y使得y大于16*元素大小，且y为4096整数倍
        result = (pv * 16uz + base - 1uz) / base * base;
    }
    return result;
}

static_assert(calc_block(1uz) == 4096uz);
static_assert(calc_block(2uz) == 4096uz);
static_assert(calc_block(3uz) == 3uz * 4096uz);
static_assert(calc_block(4uz) == 4096uz);
static_assert(calc_block(5uz) == 5uz * 4096uz);
static_assert(calc_block(6uz) == 3uz * 4096uz);
static_assert(calc_block(7uz) == 7uz * 4096uz);

#endif

template <typename T>
constexpr std::size_t block_elements_v = calc_block(sizeof(T)) / sizeof(T);

template <typename T>
class basic_bucket_type;

template <typename T>
class basic_bucket_iterator
{
    friend basic_bucket_type<std::remove_const_t<T>>;
    friend basic_bucket_iterator<std::add_const_t<T>>;

    using block = std::add_pointer_t<std::remove_const_t<T>>;

    block *block_elem_begin{};
    block *block_elem_end{};
    block *block_elem_curr{};
    T *elem_begin_begin{};
    T *elem_begin_end{};
    T *elem_end_begin{};
    T *elem_end_end{};
    T *elem_curr_begin{};
    T *elem_curr_end{};

    constexpr basic_bucket_iterator(block *block_elem_begin_, block *block_elem_end_, block *block_elem_curr_,
                                    T *elem_begin_begin_, T *elem_begin_end_, T *elem_end_begin_, T *elem_end_end_,
                                    T *elem_curr_begin_, T *elem_curr_end_) noexcept
        : block_elem_begin(block_elem_begin_), block_elem_end(block_elem_end_), block_elem_curr(block_elem_curr_),
          elem_begin_begin(elem_begin_begin_), elem_begin_end(elem_begin_end_), elem_end_begin(elem_end_begin_),
          elem_end_end(elem_end_end_), elem_curr_begin(elem_curr_begin_), elem_curr_end(elem_curr_end_)
    {
    }

    constexpr void plus_and_assign(std::ptrdiff_t pos) noexcept
    {
        block_elem_curr += pos;
        if (block_elem_curr + 1uz == block_elem_end)
        {
            elem_curr_begin = elem_end_begin;
            elem_curr_end = elem_end_end;
        }
        if (block_elem_curr == block_elem_begin)
        {
            elem_curr_begin = elem_begin_begin;
            elem_curr_end = elem_begin_end;
        }
        else
        {
            elem_curr_begin = *block_elem_begin;
            elem_curr_end = elem_begin_begin + block_elements_v<T>;
        }
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
        return *this;
    }

    constexpr basic_bucket_iterator operator++(int) noexcept
    {
        auto temp = *this;
        ++temp;
        return temp;
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
        return *this;
    }

    constexpr basic_bucket_iterator operator--(int) noexcept
    {
        auto temp = *this;
        --temp;
        return temp;
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

    constexpr std::span<T> operator[](std::ptrdiff_t pos)
    {
        auto temp = *this;
        temp += pos;
        return *temp;
    }

    constexpr std::span<T> operator[](std::ptrdiff_t pos) const noexcept
    {
        auto temp = *this;
        temp += pos;
        return *temp;
    }

    constexpr value_type operator*() noexcept
    {
        return {this->elem_curr_begin, this->elem_curr_end};
    }

    constexpr value_type operator*() const noexcept
    {
        return {this->elem_curr_begin, this->elem_curr_end};
    }

    constexpr basic_bucket_iterator &operator+=(std::ptrdiff_t pos) noexcept
    {
        this->plus_and_assign(pos);
        return *this;
    }

    constexpr basic_bucket_iterator &operator-=(std::ptrdiff_t pos) noexcept
    {
        this->plus_and_assign(-pos);
        return *this;
    }

    friend constexpr basic_bucket_iterator operator+(basic_bucket_iterator const &it, std::ptrdiff_t pos) noexcept
    {
        auto temp = it;
        temp.plus_and_assign(pos);
        return temp;
    }

    friend constexpr basic_bucket_iterator operator+(std::ptrdiff_t pos, basic_bucket_iterator const &it) noexcept
    {
        return it + pos;
    }

    friend constexpr basic_bucket_iterator operator-(std::ptrdiff_t pos, basic_bucket_iterator const &it) noexcept
    {
        return it + (-pos);
    }

    friend constexpr basic_bucket_iterator operator-(basic_bucket_iterator const &it, std::ptrdiff_t pos) noexcept
    {
        return it + (-pos);
    }

    constexpr operator basic_bucket_iterator<const T>() const
        requires(not std::is_const_v<T>)
    {
        return {block_elem_begin, block_elem_end, block_elem_curr, elem_begin_begin, elem_begin_end,
                elem_end_begin,   elem_end_end,   elem_curr_begin, elem_curr_end

        };
    }
};

static_assert(std::random_access_iterator<basic_bucket_iterator<int>>);
static_assert(std::random_access_iterator<basic_bucket_iterator<const int>>);

template <typename T>
class basic_bucket_type
{
    friend deque<std::remove_const_t<T>>;
    friend basic_bucket_type<std::remove_const_t<T>>;

    using block = std::add_pointer_t<std::remove_const_t<T>>;

    block *block_elem_begin{};
    block *block_elem_end{};
    T *elem_begin_begin{};
    T *elem_begin_end{};
    T *elem_end_begin{};
    T *elem_end_end{};

    constexpr basic_bucket_type(block *block_elem_begin_, block *block_elem_end_, T *elem_begin_begin_,
                                T *elem_begin_end_, T *elem_end_begin_, T *elem_end_end_) noexcept
        : block_elem_begin(block_elem_begin_), block_elem_end(block_elem_end_), elem_begin_begin(elem_begin_begin_),
          elem_begin_end(elem_begin_end_), elem_end_begin(elem_end_begin_), elem_end_end(elem_end_end_)
    {
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
    using const_iterator = basic_bucket_iterator<std::add_const_t<T>>;
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

    constexpr bool empty() const noexcept
    {
        return size();
    }

  public:
    iterator begin() noexcept
    {
        return {block_elem_begin, block_elem_end, block_elem_begin, elem_begin_begin, elem_begin_end,
                elem_end_begin,   elem_end_end,   elem_begin_begin, elem_begin_end};
    }

    iterator end() noexcept
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

    const_iterator begin() const noexcept
    {
        return {block_elem_begin, block_elem_end, block_elem_begin, elem_begin_begin, elem_begin_end,
                elem_end_begin,   elem_end_end,   elem_begin_begin, elem_begin_end};
    }

    const_iterator end() const noexcept
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

    const_iterator cbegin() const noexcept
    {
        return begin();
    }

    const_iterator cend() const noexcept
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

    constexpr operator basic_bucket_type<const T>() const
        requires(not std::is_const_v<T>)
    {
        return {block_elem_begin, block_elem_end, elem_begin_begin, elem_begin_end, elem_end_begin, elem_end_end};
    }
};

template <typename T>
class basic_deque_iterator
{
    friend deque<std::remove_const_t<T>>;

    using block = std::add_pointer_t<std::remove_const_t<T>>;

    block *block_elem_begin{};
    T *elem_begin{};
    T *elem_curr{};
    T *elem_end{};

    constexpr basic_deque_iterator(block *elem_begin, T *curr, T *begin, T *end) noexcept
        : block_elem_begin(elem_begin), elem_curr(curr), elem_begin(begin), elem_end(end)
    {
    }

    constexpr T &at_impl(std::ptrdiff_t pos) const noexcept
    {
        if (pos >= 0uz)
        {
            // 几乎等于deque的at，但缺少断言
            auto const back_size = elem_end - elem_curr;
            if (back_size > pos)
            {
                return *(elem_curr + pos);
            }
            else
            {
                auto const new_pos = pos - back_size;
                auto const quot = new_pos / detail::block_elements_v<T>;
                auto const rem = new_pos % detail::block_elements_v<T>;
                auto const target_block = block_elem_begin + quot + 1uz;
                return *(*target_block + rem);
            }
        }
        else
        {
            auto const front_size = elem_curr - elem_begin;
            if (front_size > (-pos))
            {
                return *(elem_curr + pos);
            }
            else
            {
                auto const new_pos = pos + front_size;
                auto const quot = new_pos / detail::block_elements_v<T>;
                auto const rem = new_pos % detail::block_elements_v<T>;
                auto const target_block = block_elem_begin + quot - 1uz;
                return *((*target_block) + detail::block_elements_v<T> + rem);
            }
        }
    }

    constexpr void plus_and_assign(std::ptrdiff_t pos) noexcept
    {
        if (pos >= 0uz)
        {
            // 几乎等于at_impl
            auto const back_size = elem_end - elem_curr;
            if (back_size > pos)
            {
                elem_curr += pos;
            }
            else
            {
                auto const new_pos = pos - back_size;
                auto const quot = new_pos / detail::block_elements_v<T>;
                auto const rem = new_pos % detail::block_elements_v<T>;
                auto const target_block = block_elem_begin + quot + 1uz;
                block_elem_begin = target_block;
                elem_begin = *target_block;
                elem_curr = elem_begin + rem;
                elem_end = elem_begin + detail::block_elements_v<T>;
            }
        }
        else
        {
            auto const front_size = elem_curr - elem_begin;
            if (front_size > (-pos))
            {
                elem_curr += pos;
            }
            else
            {
                auto const new_pos = pos + front_size;
                auto const quot = new_pos / detail::block_elements_v<T>;
                auto const rem = new_pos % detail::block_elements_v<T>;
                auto const target_block = block_elem_begin + quot - 1uz;
                block_elem_begin = target_block;
                elem_begin = *target_block;
                elem_end = elem_begin + detail::block_elements_v<T>;
                elem_curr = elem_end + rem;
            }
        }
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
        basic_deque_iterator temp(*this);
        ++temp;
        return temp;
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
        basic_deque_iterator temp(*this);
        --temp;
        return temp;
    }

    constexpr T &operator[](std::ptrdiff_t pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr T &operator[](std::ptrdiff_t pos) const noexcept
    {
        return at_impl(pos);
    }

    constexpr std::ptrdiff_t operator-(basic_deque_iterator const &other) const noexcept
    {
        return (block_elem_begin - other.block_elem_begin) * detail::block_elements_v<T> + (elem_curr - elem_begin) -
               (other.elem_curr - other.elem_begin);
    }

    constexpr basic_deque_iterator &operator+=(std::ptrdiff_t pos) noexcept
    {
        plus_and_assign(pos);
        return *this;
    }

    friend constexpr basic_deque_iterator operator+(basic_deque_iterator const &it, std::ptrdiff_t pos) noexcept
    {
        auto temp = it;
        temp.plus_and_assign(pos);
        return temp;
    }

    friend constexpr basic_deque_iterator operator+(std::ptrdiff_t pos, basic_deque_iterator const &it) noexcept
    {
        return it + pos;
    }

    constexpr basic_deque_iterator &operator-=(std::ptrdiff_t pos) noexcept
    {
        plus_and_assign(-pos);
        return *this;
    }

    friend constexpr basic_deque_iterator operator-(basic_deque_iterator const &it, std::ptrdiff_t pos) noexcept
    {
        return it + (-pos);
    }

    friend constexpr basic_deque_iterator operator-(std::ptrdiff_t pos, basic_deque_iterator const &it) noexcept
    {
        return it + (-pos);
    }

    constexpr operator basic_deque_iterator<const T>() const
        requires(not std::is_const_v<T>)
    {
        return {block_elem_begin, elem_begin, elem_curr, elem_end};
    }
};
} // namespace detail

template <typename T>
class deque
{
    static_assert(std::is_object_v<T>);
    static_assert(not std::is_const_v<T>);

    using block = T *;

    // 块数组的起始地址
    block *block_ctrl_begin{};
    // 块数组的结束地址
    block *block_ctrl_end{};
    // 已分配块的起始地址
    block *block_alloc_begin{};
    // 已分配块结束地址
    block *block_alloc_end{};
    // 已用块的首地址
    block *block_elem_begin{};
    // 已用块的结束地址
    block *block_elem_end{};
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
    不变式：
    迭代器的++在走到last时会主动走到下一个块，由于block_alloc_end的保证，使得最终走到*block_elem_end得到0
    */

    static constexpr std::size_t ceil_n(std::size_t const num, std::size_t const n) noexcept
    {
        return (num + n - 1uz) / n * n;
    }

    constexpr void dealloc_block(block b) noexcept
    {
        delete[] b;
    }

    constexpr block alloc_block()
    {
        return new T[detail::block_elements_v<T>];
    }

    constexpr block *alloc_ctrl(std::size_t size)
    {
        return new block[size];
    }

    constexpr void dealloc_ctrl() noexcept
    {
        delete[] block_ctrl_begin;
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
    // 空deque安全
    constexpr void destruct() noexcept
    {
        destroy_elems();
        // 清理块数组
        for (auto const i : std::ranges::subrange{block_alloc_begin, block_alloc_end})
        {
            dealloc_block(i);
        }
        dealloc_ctrl();
    }

    constexpr void elem_begin(T *begin, T *end, T *first) noexcept
    {
        elem_begin_begin = begin;
        elem_begin_end = end;
        elem_begin_first = first;
    }

    constexpr void elem_end(T *begin, T *end, T *last) noexcept
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
    using const_iterator = detail::basic_deque_iterator<const T>;
    using const_reverse_iterator = std::reverse_iterator<detail::basic_deque_iterator<const T>>;
    using bucket_type = detail::basic_bucket_type<T>;
    using const_bucket_type = detail::basic_bucket_type<const T>;

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
        destruct();
    }

    constexpr bool empty() const noexcept
    {
        return elem_begin_begin == elem_begin_end;
    }

    constexpr void clear() noexcept
    {
        destroy_elems();
        block_elem_begin = nullptr;
        block_elem_end = nullptr;
        elem_begin(nullptr, nullptr, nullptr);
        elem_end(nullptr, nullptr, nullptr);
    }

    constexpr void swap(deque &other) noexcept
    {
        // todo:
        // std::swap(alloc, other.alloc);
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

    constexpr deque(deque &&rhs) noexcept
    {
        // todo: alloc propagrate
        rhs.swap(*this);
    }

    // 空deque安全
    constexpr auto size() const noexcept
    {
        auto const block_size = block_elem_size();
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

    constexpr auto max_size() const noexcept
    {
        return std::size_t(-1) / 2;
    }

    static_assert(std::random_access_iterator<iterator>);
    static_assert(std::output_iterator<iterator, T>);
    static_assert(std::sentinel_for<iterator, iterator>);

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
        block *block_ctrl_begin{}; // A
        block *block_ctrl_end{};   // D

        // 替换块数组到deque
        // 构造时
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
            // 从guard替换回deque
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
        }

        constexpr void replace_ctrl_front() const noexcept
        {
            d.align_elem_alloc_as_ctrl_front(block_ctrl_end);
            d.dealloc_ctrl();
            // 注意顺序
            // 从guard替换回deque
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
        }

        // 参数是新大小
        constexpr ctrl_alloc(deque &dq, std::size_t const ctrl_size) : d(dq)
        {
            // 永远多分配一个，使得block_ctrl_end可以解引用以及*block_elem_end/*block_alloc_end始终合法
            // 通过保证*block_alloc_end为0
            auto const size = ceil_n(ctrl_size + 1uz, 4uz);
            block_ctrl_begin = d.alloc_ctrl(size);
            // 多分配的这个不可见
            block_ctrl_end = block_ctrl_begin + size - 1uz;
        }
    };

    // 对齐控制块
    // 对齐alloc和ctrl的begin，用于push_back
    constexpr void align_alloc_as_ctrl_back() noexcept
    {
        std::ranges::copy(block_alloc_begin, block_alloc_end, block_ctrl_begin);
        auto const block_size = block_alloc_size();
        block_alloc_begin = block_ctrl_begin;
        block_alloc_end = block_ctrl_begin + block_size;
        fill_block_alloc_end();
    }

    // 对齐控制块
    // 对齐alloc和ctrl的end，用于push_front
    constexpr void align_alloc_as_ctrl_front() noexcept
    {
        std::ranges::copy_backward(block_alloc_begin, block_alloc_end, block_ctrl_end);
        auto const block_size = block_alloc_size();
        block_alloc_end = block_ctrl_end;
        block_alloc_begin = block_ctrl_end - block_size;
        fill_block_alloc_end();
    }

    // 对齐控制块
    // 对齐elem和alloc的begin，用于emplace_back
    constexpr void align_elem_as_alloc_back() noexcept
    {
        std::ranges::rotate(block_alloc_begin, block_elem_begin, block_elem_end);
        auto const block_size = block_elem_size();
        block_elem_begin = block_alloc_begin;
        block_elem_end = block_alloc_begin + block_size;
    }

    // 对齐控制块
    // 对齐elem和alloc的end，用于emplace_front
    constexpr void align_elem_as_alloc_front() noexcept
    {
        std::ranges::rotate(block_elem_begin, block_elem_end, block_alloc_end);
        auto const block_size = block_elem_size();
        block_elem_end = block_alloc_end;
        block_elem_begin = block_alloc_end - block_size;
    }

    // ctrl_begin可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_back(block *ctrl_begin) noexcept
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

    // ctrl_begin可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_front(block *ctrl_end) noexcept
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

    // 向back扩展block
    // 对空deque安全
    constexpr void reserve_back(std::size_t const add_elem_size)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_alloc_cap = (block_elem_begin - block_alloc_begin) * detail::block_elements_v<T>;
        // 尾部块的cap
        auto const tail_block_alloc_cap = (block_alloc_end - block_elem_end) * detail::block_elements_v<T>;
        // 尾块的已使用大小
        auto const tail_cap = elem_end_last - elem_end_end;
        // non_move_cap为尾部-尾部已用，不移动块时cap
        auto const non_move_cap = tail_block_alloc_cap + tail_cap;
        // 首先如果cap足够，则不需要分配新block
        if (non_move_cap >= add_elem_size)
        {
            return;
        }
        // move_cap为头部+尾部-尾部已用，移动已分配块的cap
        auto const move_cap = head_block_alloc_cap + non_move_cap;
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
            auto const new_ctrl_size = block_ctrl_size() + add_block_size;
            ctrl_alloc const ctrl{*this, new_ctrl_size}; // may throw
            ctrl.replace_ctrl_back();
        }
        extent_block_back_uncond(add_block_size);
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
            auto const new_ctrl_size = block_ctrl_size() + add_block_size;
            ctrl_alloc const ctrl{*this, new_ctrl_size}; // may throw
            ctrl.replace_ctrl_front();
        }
        // 必须最后执行
        extent_block_front_uncond(add_block_size);
    }

    struct construct_guard
    {
      private:
        deque &d;
        bool released{};

      public:
        constexpr construct_guard(deque &c) noexcept : d(c)
        {
        }

        constexpr void release() noexcept
        {
            released = true;
        }

        constexpr ~construct_guard()
        {
            if (not released)
            {
                d.destruct();
            }
        }
    };

    // 构造函数的辅助函数
    // 需要注意异常安全
    // 调用后可直接填充元素
    constexpr void construct_block(std::size_t block_size)
    {
        ctrl_alloc const ctrl(*this, block_size); // may throw
        ctrl.replace_ctrl();
        extent_block_back_uncond(block_size); // may throw
    }

    // 复制赋值的辅助函数
    // 参数是最小block大小
    // 调用后可直接填充元素
    constexpr void extent_block(std::size_t new_block_size)
    {
        auto const old_alloc_size = block_alloc_size();
        if (old_alloc_size > new_block_size)
        {
            return;
        }
        auto const old_ctrl_size = block_ctrl_size();
        if (old_ctrl_size < new_block_size)
        {
            ctrl_alloc const ctrl(*this, new_block_size); // may throw
            ctrl.replace_ctrl_back();
        }
        else
        {
            align_alloc_as_ctrl_back();
        }
        extent_block_back_uncond(new_block_size - old_alloc_size); // may throw
    }

    // 构造函数和复制赋值的辅助函数，要求block_alloc必须足够大
    constexpr void copy(const_bucket_type other, std::size_t block_size)
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

    // 复制构造采取按结构复制的方法
    // 不需要经过extent_block的复杂逻辑
    constexpr deque(deque const &other)
    {
        // todo: alloc propagate
        // alloc = other.alloc;
        construct_guard guard(*this);
        auto const block_size = other.block_elem_end - other.block_elem_begin;
        construct_block(block_size);
        copy(other.buckets(), block_size);
        guard.release();
    }

  private:
#if defined(__clang__) // make clang happy
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++26-extensions"
#endif
    // 使用count、count和T、或者随机访问迭代器进行构造，用于对应的构造函数
    // 注意异常安全
    template <typename... Ts>
    constexpr void construct(std::size_t block_size, std::size_t quot, std::size_t rem, Ts &&...t)
    {
        // 由于析构优先考虑elem_begin，因此必须独立构造elem_begin
        if (quot)
        {
            auto const begin = *block_elem_end;
            auto const end = begin + detail::block_elements_v<T>;
            if constexpr (sizeof...(Ts) == 0uz)
            {
                std::ranges::uninitialized_default_construct(begin, end);
            }
            else if constexpr (sizeof...(Ts) == 1uz)
            {
                std::ranges::uninitialized_fill(begin, end, t...);
            }
            else if constexpr (sizeof...(Ts) == 2uz)
            {
#if defined(__cpp_pack_indexing)
                auto &src_begin = t...[0uz];
                auto &src_end = t...[1uz];
#else
                auto const x = std::tuple<Ts &...>(t...);
                auto &src_begin = std::get<0uz>(x);
                auto &src_end = std::get<1uz>(x);
#endif
                std::ranges::uninitialized_copy(std::counted_iterator(src_begin, detail::block_elements_v<T>),
                                                std::default_sentinel, begin, std::unreachable_sentinel);
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
        if (quot > 1uz)
        {
            for (auto i = 0uz; i != quot - 1uz; ++i)
            {
                auto const begin = *block_elem_end;
                auto const end = begin + detail::block_elements_v<T>;
                if constexpr (sizeof...(Ts) == 0uz)
                {
                    std::ranges::uninitialized_default_construct(begin, end);
                }
                else if constexpr (sizeof...(Ts) == 1uz)
                {
                    std::ranges::uninitialized_fill(begin, end, t...);
                }
                else if constexpr (sizeof...(Ts) == 2uz)
                {
#if defined(__cpp_pack_indexing)
                    auto &src_begin = t...[0uz];
                    auto &src_end = t...[1uz];
#else
                    auto const x = std::tuple<Ts &...>(t...);
                    auto &src_begin = std::get<0uz>(x);
                    auto &src_end = std::get<1uz>(x);
#endif
                    std::ranges::uninitialized_copy(std::counted_iterator(src_begin, detail::block_elements_v<T>),
                                                    std::default_sentinel, begin, std::unreachable_sentinel);
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
        if (rem)
        {
            auto const begin = *block_elem_end;
            auto const end = begin + rem;
            if constexpr (sizeof...(Ts) == 0uz)
            {
                std::ranges::uninitialized_default_construct(begin, end);
            }
            else if constexpr (sizeof...(Ts) == 1uz)
            {
                std::ranges::uninitialized_fill(begin, end, t...);
            }
            else if constexpr (sizeof...(Ts) == 2uz)
            {
#if defined(__cpp_pack_indexing)
                auto &src_begin = t...[0uz];
                auto &src_end = t...[1uz];
#else
                auto const x = std::tuple<Ts &...>(t...);
                auto &src_begin = std::get<0uz>(x);
                auto &src_end = std::get<1uz>(x);
#endif
                std::ranges::uninitialized_copy(src_begin, src_end, begin, std::unreachable_sentinel);
            }
            else
            {
                static_assert(false);
            }
            if (quot == 0uz) // 注意
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

  public:
    constexpr deque(std::size_t count)
    {
        if (count == 0uz) // 必须
        {
            return;
        }
        auto const quot = count / detail::block_elements_v<T>;
        auto const rem = count % detail::block_elements_v<T>;
        construct_guard guard(*this);
        construct_block(quot + 1uz);
        construct(quot + 1uz, quot, rem);
        guard.release();
    }

    constexpr deque(std::size_t count, T const &t)
    {
        if (count == 0uz) // 必须
        {
            return;
        }
        auto const quot = count / detail::block_elements_v<T>;
        auto const rem = count % detail::block_elements_v<T>;
        construct_guard guard(*this);
        construct_block(quot + 1uz);
        construct(quot + 1uz, quot, rem, t);
        guard.release();
    }

    template <typename... V>
    constexpr T &emplace_back(V &&...v)
    {
        if (elem_end_end != elem_end_last)
        {
            auto const begin = elem_end_end;
            std::construct_at(begin, std::forward<V>(v)...); // may throw
            auto const end = elem_end_end + 1uz;
            elem_end_end = end;
            // 修正elem_begin
            if (block_elem_size() == 1uz)
            {
                elem_begin_end = end;
            }
            return *begin;
        }
        else
        {
            reserve_back(1uz);
            auto const begin = *block_elem_end;
            std::construct_at(begin, std::forward<V>(v)...); // may throw
            elem_end(begin, begin + 1uz, begin + detail::block_elements_v<T>);
            ++block_elem_end;
            // 修正elem_begin
            if (block_elem_size() == 1uz)
            {
                elem_begin(begin, begin + 1uz, begin);
            }
            return *begin;
        }
    }

    template <typename U, typename V>
        requires std::input_iterator<U> && std::sentinel_for<V, U>
    constexpr deque(U begin, V end)
    {
        if (begin == end) // 必须
        {
            return;
        }
        if constexpr (requires { is_iterator(begin); })
        {
            // iterator begin, end;
            bucket_type bucket{begin.block_elem_begin, end.block_elem_begin + 1uz,
                               begin.elem_curr,        begin.elem_end,
                               end.elem_begin,         end.elem_curr};
            auto const block_size = bucket.size();
            construct_guard guard(*this);
            extent_block(block_size);
            copy(bucket, block_size);
            guard.release();
        }
        else if constexpr (std::random_access_iterator<U>)
        {
            auto const count = end - begin;
            auto const quot = count / detail::block_elements_v<T>;
            auto const rem = count % detail::block_elements_v<T>;
            construct_guard guard(*this);
            construct_block(quot + 1uz);
            construct(quot + 1uz, quot, rem, begin, end);
            guard.release();
        }
        else
        {
            construct_guard guard{*this};
            for (; begin != end; ++begin)
            {
                emplace_back(*begin);
            }
            guard.release();
        }
    }

    template <typename R>
    constexpr deque(std::from_range_t, R &&rg) : deque(std::ranges::begin(rg), std::ranges::end(rg))
    {
    }

    template <typename R>
    constexpr deque(std::from_range_t, R &&rg)
        requires std::ranges::sized_range<R>
    {
        auto const count = rg.size();
        if (count == 0uz) // 必须
        {
            return;
        }
        auto const quot = count / detail::block_elements_v<T>;
        auto const rem = count % detail::block_elements_v<T>;
        construct_guard guard(*this);
        construct_block(quot + 1uz);
        construct(quot + 1uz, quot, rem, std::ranges::begin(rg), std::ranges::end(rg));
        guard.release();
    }

    constexpr deque(std::from_range_t, deque const &rg) : deque(rg)
    {
    }

    constexpr deque(std::from_range_t, deque &&rg) : deque(std::move(rg))
    {
    }

    constexpr deque(std::initializer_list<T> init)
    {
        auto const count = init.size();
        if (count == 0uz) // 必须
        {
            return;
        }
        auto const quot = count / detail::block_elements_v<T>;
        auto const rem = count % detail::block_elements_v<T>;
        construct_guard guard(*this);
        construct_block(quot + 1uz);
        construct(quot + 1uz, quot, rem, init.begin(), init.end());
        guard.release();
    }

  private:
    // 赋值的辅助函数
    void reset_block_elem_end() noexcept
    {
        block_elem_end = block_elem_begin;
    }

  public:
    constexpr deque &operator=(const deque &other)
    {
        destroy_elems();
        reset_block_elem_end();
        auto const block_size = other.block_elem_end - other.block_elem_begin;
        if (block_size == 0uz) // 必须
        {
            return *this;
        }
        extent_block(block_size);
        copy(other.buckets(), block_size);
        return *this;
    }

    constexpr deque &operator=(std::initializer_list<T> ilist)
    {
        destroy_elems();
        reset_block_elem_end();
        auto const count = ilist.size();
        if (count == 0uz) // 必须
        {
            return *this;
        }
        auto const quot = count / detail::block_elements_v<T>;
        auto const rem = count % detail::block_elements_v<T>;
        extent_block(quot + 1uz);
        construct(quot + 1uz, quot, rem, ilist.begin(), ilist.end());
        return *this;
    }

    constexpr deque &operator=(deque &&other)
    {
        other.swap(*this);
        return *this;
    }

    constexpr void assign(std::size_t count, const T &value)
    {
        destroy_elems();
        reset_block_elem_end();
        if (count == 0uz) // 必须
        {
            return;
        }
        auto const quot = count / detail::block_elements_v<T>;
        auto const rem = count % detail::block_elements_v<T>;
        extent_block(quot + 1uz);
        construct(quot + 1uz, quot, rem, value);
    }

    template <typename U, typename V>
    constexpr void assign(U begin, V end)
        requires std::input_iterator<U> && std::sentinel_for<V, U>
    {
        destroy_elems();
        reset_block_elem_end();
        if (begin == end) // 必须
        {
            return;
        }
        if constexpr (requires { is_iterator(begin); })
        {
            // iterator begin, end;
            bucket_type bucket{begin.block_elem_begin, end.block_elem_begin + 1uz,
                               begin.elem_curr,        begin.elem_end,
                               end.elem_begin,         end.elem_curr};
            auto const block_size = bucket.size();
            extent_block(block_size);
            copy(bucket, block_size);
        }
        else if constexpr (std::random_access_iterator<U>)
        {
            auto const count = end - begin;
            auto const quot = count / detail::block_elements_v<T>;
            auto const rem = count % detail::block_elements_v<T>;
            extent_block(quot + 1uz);
            construct(quot + 1uz, quot, rem, begin, end);
        }
        else
        {
            for (; begin != end; ++begin)
            {
                emplace_back(*begin);
            }
        }
    }

    constexpr void assign(std::initializer_list<T> ilist)
    {
        destroy_elems();
        reset_block_elem_end();
        auto const count = ilist.size();
        if (count == 0uz) // 必须
        {
            return;
        }
        auto const quot = count / detail::block_elements_v<T>;
        auto const rem = count % detail::block_elements_v<T>;
        extent_block(quot + 1uz);
        construct(quot + 1uz, quot, rem, ilist.begin(), ilist.end());
    }

    template <typename R>
    constexpr void assign_range(R &&rg)
    {
        assign(std::ranges::begin(rg), std::ranges::end(rg));
    }

  private:
    // 几乎等于iterator的at，但具有检查和断言
    template <bool throw_exception = false>
    constexpr T &at_impl(std::size_t pos) const noexcept
    {
        auto const head_size = elem_begin_end - elem_begin_begin;
        if (head_size > pos)
        {
            return *(elem_begin_begin + pos);
        }
        else
        {
            auto const new_pos = pos - head_size;
            auto const quot = new_pos / detail::block_elements_v<T>;
            auto const rem = new_pos % detail::block_elements_v<T>;
            auto const target_block = block_elem_begin + quot + 1uz;
            auto const check1 = target_block < block_elem_end;
            auto const check2 = (target_block + 1uz == block_elem_end) ? (elem_end_begin + rem < elem_end_end) : true;
            if constexpr (throw_exception)
            {
                if (not(check1 && check2))
                    throw std::out_of_range{""};
            }
            else
            {
                assert(check1 && check2);
            }
            auto const result = *target_block + rem;
            return *result;
        }
    }

  public:
    constexpr T &at(std::size_t pos) noexcept
    {
        return at_impl<true>(pos);
    }

    constexpr T const &at(std::size_t pos) const noexcept
    {
        return at_impl<true>(pos);
    }

    constexpr T &operator[](std::size_t pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr T const &operator[](std::size_t pos) const noexcept
    {
        return at_impl(pos);
    }

    // 不会失败且不移动元素
    constexpr void shrink_to_fit() noexcept
    {
        if (block_alloc_size() == 0uz)
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

    template <class... V>
    constexpr T &emplace_front(V &&...v)
    {
        if (elem_begin_begin != elem_begin_first)
        {
            auto const begin = elem_begin_begin - 1uz;
            std::construct_at(begin, std::forward<V>(v)...); // may throw
            elem_begin_begin = begin;
            if (block_elem_size() == 1uz)
            {
                [[assume(begin + 1uz == elem_begin_begin)]];
                elem_end_begin = begin;
            }
            return *begin;
        }
        else
        {
            reserve_front(1uz);
            auto const block = block_elem_begin - 1uz;
            auto const first = *block;
            auto const end = first + detail::block_elements_v<T>;
            std::construct_at(end - 1uz, std::forward<V>(v)...); // may throw
            elem_begin(end - 1uz, end, first);
            [[assume(block + 1uz == block_elem_begin)]];
            --block_elem_begin;
            // 修正elem_end
            if (block_elem_size() == 1uz)
            {
                elem_end(end - 1uz, end, end);
            }
            return *(end - 1uz);
        }
    }

    constexpr void push_front(const T &value)
    {
        emplace_front(value);
    }

    constexpr void push_front(T &&value)
    {
        emplace_front(std::move(value));
    }

    constexpr void pop_back() noexcept
    {
        assert(not empty());
        if (elem_end_begin != elem_end_end)
        {
            --elem_end_end;
            std::destroy_at(elem_end_end);
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
                return;
            }
            if (block_elem_size() == 1uz)
            {
                --elem_begin_end;
            }
        }
        else
        {
            --block_elem_end;
            auto const first = *(block_elem_end - 1uz);
            auto const last = first + detail::block_elements_v<T>;
            auto end = last - 1uz;
            std::destroy_at(end);
            if (block_elem_size() == 1uz)
            {
                elem_end(elem_begin_begin, elem_begin_end, elem_begin_end);
            }
            else
            {
                elem_end(first, end, last);
            }
        }
    }

    constexpr void pop_front() noexcept
    {
        assert(not empty());
        if (elem_begin_begin != elem_begin_end)
        {
            std::destroy_at(elem_begin_begin);
            ++elem_begin_begin;
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
                return;
            }
            if (block_elem_size() == 1uz)
            {
                ++elem_end_begin;
            }
        }
        else
        {
            ++block_elem_begin;
            if (block_elem_size() == 1uz)
            {
                elem_begin(elem_end_begin, elem_end_end, elem_end_begin);
            }
            else
            {
                auto const begin = *block_elem_begin;
                auto const end = begin + detail::block_elements_v<T>;
                std::destroy_at(begin);
                elem_begin(begin + 1uz, end, begin);
            }
        }
    }

    constexpr auto &front() noexcept
    {
        assert(not empty());
        return *(elem_begin_begin);
    }

    constexpr auto &back() noexcept
    {
        assert(not empty());
        return *(elem_end_end - 1uz);
    }

    constexpr auto const &front() const noexcept
    {
        assert(not empty());
        return *(elem_begin_begin);
    }

    constexpr auto const &back() const noexcept
    {
        assert(not empty());
        return *(elem_end_end - 1uz);
    }
};
} // namespace bizwen
