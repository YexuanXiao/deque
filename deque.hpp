#include <algorithm>
#include <cassert>
#include <concepts>
#include <deque>
#include <iterator>
#include <memory>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>
#include <variant>

// 返回块的大小，一定是4096的整数倍
// 只被block_elements调用
static inline consteval std::size_t calc_block_size(std::size_t const pv) noexcept
{
    // 块的基本大小
    auto constexpr base = 4096uz;
    // 基本大小下，元素最大大小，至少保证16个
    auto constexpr sieve = base / 16uz;

    if (pv < sieve)
    {
        // 在基本大小的1-8倍间找到利用率最高的
        auto block_sz = 0uz;
        auto rmd_pre = std::size_t(-1);
        auto result = 0uz;
        for (auto i = 0uz; i != 8uz; ++i)
        {
            block_sz = base * (i + 1uz);
            auto rmd_cur = (block_sz * i) % pv;
            // 越小利用率越高
            if (rmd_cur < rmd_pre)
            {
                rmd_pre = rmd_cur;
                result = block_sz;
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

template <typename T>
static inline consteval std::size_t block_elements() noexcept
{
    return calc_block_size(sizeof(T)) / sizeof(T);
}

static inline constexpr std::size_t ceil_n(std::size_t const num, std::size_t const n) noexcept
{
    return (num + n - 1uz) / n * n;
}

class deque
{

    using T = int;
    using block = T *;

#if __has_cpp_attribute(msvc::no_unique_address)
    [[msvc::no_unique_address]]
#else
    [[no_unique_address]]
#endif
    std::monostate alloc;
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

    void destroy() noexcept
    {
        // 4种情况，0，1，2，3+个块有元素
        auto const elem_block_size = block_elem_end - block_elem_begin;
        if (elem_block_size)
        {
            for (auto begin = elem_begin_begin; begin != elem_begin_end; ++begin)
            {
                std::destroy_at(begin);
            }
        }
        // 清理中间的块
        if (elem_block_size > 2z)
        {
            auto const target_end = block_alloc_end - 1uz;
            for (auto block_begin = block_elem_begin + 1uz; block_begin != target_end; ++block_begin)
            {
                for (auto begin = block_begin; begin != block_begin + block_elements<T>(); ++begin)
                {
                    std::destroy_at(begin);
                }
            }
        }
        if (elem_block_size > 1z)
        {
            for (auto begin = elem_end_begin; begin != elem_end_end; ++begin)
            {
                std::destroy_at(begin);
            }
        }
        // 清理块数组
        for (auto begin = block_alloc_begin; begin != block_alloc_end; ++begin)
        {
            // todo:
            // delete block;
        }
        // todo:
        // delete block_ctrl_begin;
    }

  public:
    ~deque()
    {
        destroy();
    }
    void swap(deque &other) noexcept
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

    friend void swap(deque &lhs, deque &rhs) noexcept
    {
        lhs.swap(rhs);
    }

    deque(deque &&rhs) noexcept
    {
        rhs.swap(*this);
    }

    auto &front() noexcept
    {
        assert(elem_begin_begin);
        return *(elem_begin_begin);
    }

    auto &back() noexcept
    {
        assert(elem_end_end);
        return *(elem_end_end - 1uz);
    }

    auto size() const noexcept
    {
        auto const elem_block_size = block_elem_end - block_elem_begin;
        auto result = 0uz;

        if (elem_block_size)
        {
            result += elem_begin_end - elem_begin_begin;
        }
        if (elem_block_size > 2z)
        {
            result += (block_alloc_end - block_elem_begin - 2uz) * block_elements<T>();
        }
        if (elem_block_size > 1z)
        {
            result += elem_end_end - elem_end_begin;
        }

        return result;
    }

    struct iterator
    {
        friend deque;
        block *block_elem_begin{};
        T *block_begin{};
        T *block_curr{};
        T *block_end{};

        iterator(block *elem_begin, T *curr, T *begin, T *end) noexcept
            : block_elem_begin(elem_begin), block_curr(curr), block_begin(begin), block_end(end)
        {
        }

      public:
        iterator() noexcept
        {
        }
    };

    // todo:
    // using const_iterator = std::basic_const_iterator<iterator>;

    auto begin() noexcept
    {
        return iterator{block_elem_begin, elem_begin_begin, elem_begin_begin, elem_begin_end};
    }

    auto end() noexcept
    {
        return iterator{block_elem_end, elem_end_end, elem_end_begin, elem_end_end};
    }

  private:
    // 负责分配块数组
    // 构造和扩容时都可以使用
    struct ctrl_alloc
    {
        std::monostate &a;
        block *block_ctrl_begin{}; // A
        block *block_ctrl_end{};   // D

        // 替换块数组到deque
        // 构造时
        // 对空deque安全
        void replace_ctrl(deque &d) const noexcept
        {
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
            d.block_alloc_begin = block_ctrl_begin;
            d.block_alloc_end = block_ctrl_begin;
            d.block_elem_begin = block_ctrl_begin;
            d.block_elem_end = block_ctrl_begin;
        }
        // 扩容时，back为插入元素的方向
        // 对空deque安全
        void replace_ctrl(deque &d, bool const push_back) const noexcept
        {
            block *block_alloc_begin{}; // B
            block *block_alloc_end{};   // C
            // 之前已分配的block数量
            auto const alloc_block_size = d.block_alloc_end - d.block_alloc_begin;
            // case 1           case 2            case 3           case 4
            // A1 B1 → A2 B2   A1       A2       A1 B1   A2       A1       A2 B2
            // |        |       |        |        |    ↘ |        |     ↗ |
            // C1    → C2      B1    → B2       C1      B2       B1       C2
            // |        |       |        |        |    ↘ |        |     ↗ |
            // D1       D2      D1 C1 → C2 D2    D1      C1 D1    C1 D1   D2
            if (push_back) // case 1 4
            {
                block_alloc_begin = block_ctrl_begin;
                block_alloc_end = block_ctrl_begin + alloc_block_size;
            }
            else // case 2 3
            {
                block_alloc_begin = block_ctrl_end - alloc_block_size;
                block_alloc_end = block_ctrl_end;
            }

            std::ranges::copy(d.block_alloc_begin, d.block_alloc_end, block_alloc_begin);

            // 此时仅考虑了ctrl和alloc，没考虑elem（实际储存了元素的块）的位置
            // 而elem和alloc的位置是相对的，无关alloc如何移动，因此先记录位置再还原
            auto const elem_block_offset = d.block_elem_begin - d.block_alloc_begin;
            auto const elem_block_offset2 = d.block_alloc_end - d.block_elem_end;
            d.block_elem_begin = block_alloc_begin + elem_block_offset;
            d.block_elem_end = block_alloc_end - elem_block_offset2;
            // 从guard替换回deque
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
            d.block_alloc_begin = block_alloc_begin;
            d.block_alloc_end = block_alloc_end;
        }
        // 必须是算好的大小
        ctrl_alloc(std::monostate &alloc, std::size_t const ctrl_size) : a(alloc)
        {
            // todo:
            // block_alloc_begin = new T*[size];
            // block_alloc_end = block_alloc_begin + real_alloc_size;
        }
    };

    // base: 4 ratio: 2
    // back 决定块数组优先使用前面还是后面，为true时优先使用前面，即向前移动
    // 该函数只被extent_ctrl调用
    // back为插入元素的方向
    // 对空deque安全
    // 返回需要分配的block数量
    std::size_t alloc_ctrl(std::size_t const add_elem_size, bool const push_back)
    {
        auto result = ceil_n(add_elem_size, block_elements<T>());
        auto const old_size = block_ctrl_end - block_ctrl_begin;
        auto const new_size = old_size + result;
        ctrl_alloc ctrl{alloc, ceil_n(new_size, 4uz)}; // may throw
        ctrl.replace_ctrl(*this, push_back);
        return result;
    }

    // 先就地移动块，back为true时向前移动，否则重新分配块
    // back为插入元素的方向
    // 返回需要分配几个block
    // 对空deque安全
    std::size_t extent_ctrl(std::size_t const add_elem_size, bool const push_back)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_cap = (block_alloc_begin - block_ctrl_begin) * block_elements<T>();
        // 尾部块的cap
        auto const tail_block_cap = (block_ctrl_end - block_alloc_end) * block_elements<T>();
        // 不移动块时cap
        auto non_move_cap = 0uz;
        // 移动块时cap
        auto move_cap = 0uz;
        // 头块或尾块的cap，取决于插入方向
        auto head_or_tail_used = 0uz;

        if (push_back)
        {
            head_or_tail_used = elem_end_end - elem_end_begin;
            // non_move_cap为尾部-尾部已用
            non_move_cap = tail_block_cap - head_or_tail_used;
            // move_cap为头部+尾部-尾部已用
            move_cap = head_block_cap + non_move_cap;
        }
        else
        {
            head_or_tail_used = elem_begin_end - elem_begin_begin;
            // non_move_cap为头部-头部已用
            non_move_cap = head_block_cap - head_or_tail_used;
            // move_cap为头部-头部已用+尾部
            move_cap = non_move_cap + tail_block_cap;
        }

        // 首先如果cap足够，则不需要分配新block
        if (non_move_cap >= add_elem_size) [[likely]]
            return 0uz;

        // 然后按需移动block
        if (push_back & head_block_cap)
        {
            std::ranges::copy(block_alloc_begin, block_alloc_end, block_ctrl_begin);
        }
        else if (push_back & tail_block_cap)
        {
            std::ranges::copy(block_alloc_begin, block_alloc_end, block_ctrl_end);
        }
        // 最后说明容量不够，则重新分配块

        if (move_cap < add_elem_size)
        {
            return alloc_ctrl(add_elem_size - move_cap, push_back);
        }

        return 0uz;
    }
    // 向前分配新block，需要block_size小于等于(block_alloc_begin - block_ctrl_begin)
    // 且不block_alloc_X不是空指针
    void extent_block_front(std::size_t const block_size)
    {
        for (auto i = 0uz; i != block_size; ++i)
        {
            // todo
            // *block_alloc_begin = new T[block_elements<T>()];
            --block_alloc_begin;
        }
    }
    // 向后分配新block，需要block_size小于等于(block_ctrl_end - block_alloc_end)
    // 且不block_alloc_X不是空指针
    void extent_block_back(std::size_t const block_size)
    {
        for (auto i = 0uz; i != block_size; ++i)
        {
            // todo
            // *block_alloc_end = new T[block_elements<T>()];
            ++block_alloc_end;
        }
    }
    // 根据增加的元素数量扩展block和ctrl
    // 无前提条件
    void extent_block(std::size_t const add_elem_size, bool const push_back)
    {
        if (push_back)
        {
            // 首先检查已经alloc的是不是足够
            auto const cap = (block_alloc_end - block_elem_end) * block_elements<T>() - (elem_end_end - elem_end_begin);
            // cap 不够就扩容block
            if (cap <= add_elem_size)
            {
                extent_block_back(extent_ctrl(add_elem_size, push_back));
            }
        }
        else
        {
            auto const cap =
                (block_elem_begin - block_alloc_begin) * block_elements<T>() - (elem_begin_end - elem_begin_begin);
            if (cap <= add_elem_size)
            {
                extent_block_front(extent_ctrl(add_elem_size, push_back));
            }
        }
    }

    struct construct_guard
    {
        deque &d;
        bool released{};
        construct_guard(deque &c) noexcept : d(c)
        {
        }
        void release() noexcept
        {
            released = true;
        }
        ~construct_guard()
        {
            if (!released)
            {
                d.destroy();
            }
        }
    };

  public:
    deque() noexcept = default;
    // 复制构造采取按结构复制的方法
    // 不需要经过extent_block的复杂逻辑
    deque(deque const &other)
    {
        construct_guard guard(*this);
        auto const block_size = other.block_elem_end - other.block_elem_begin;
        // todo: alloc propagate
        // alloc = other.alloc;
        ctrl_alloc const ctrl(alloc, ceil_n(block_size, 4uz)); // may throw
        ctrl.replace_ctrl(*this);
        extent_block_back(block_size); // may throw
        if (block_size)
        {
            elem_begin_first = *block_elem_end;
            ++block_elem_end;
            elem_begin_begin = elem_begin_first + (other.elem_begin_begin - other.elem_begin_first);
            elem_begin_end = elem_begin_first + block_elements<T>();
            for (auto begin = other.elem_begin_begin; begin != other.elem_begin_end; ++begin, ++elem_begin_end)
            {
                std::construct_at(elem_begin_end, *begin); // may throw
            }
        }
        if (block_size > 2z)
        {
            auto const target_block_end = other.block_elem_end - 1uz;
            for (auto target_block_begin = other.block_elem_begin + 1uz; target_block_begin != target_block_end;
                 ++target_block_begin)
            {
                elem_end_begin = *block_elem_end;
                ++block_elem_end;
                elem_end_end = elem_end_begin;
                elem_end_last = elem_end_begin + block_elements<T>();
                for (auto begin = *target_block_begin, end = begin + block_elements<T>(); begin != end;
                     ++begin, ++elem_end_end)
                {
                    std::construct_at(elem_end_end, *begin); // may throw
                }
            }
        }
        if (block_size > 1z)
        {
            elem_end_begin = *block_elem_end;
            ++block_elem_end;
            elem_end_end = elem_end_begin;
            elem_end_last = elem_end_begin + block_elements<T>();
            for (auto begin = other.elem_end_begin; begin != other.elem_end_end; ++begin, ++elem_end_end)
            {
                std::construct_at(elem_end_end, *begin); // may throw
            }
        }

        guard.release();
    }

    template <typename... V>
    void emplace_back(V &&...v)
    {
        if (elem_end_end != elem_end_last)
        {
            std::construct_at(elem_end_end, std::forward<V>(v)...); // may throw
            ++elem_end_end;
        }
        else
        {
            extent_block(1uz, true);
            auto begin = *block_elem_end;
            std::construct_at(begin, std::forward<V>(v)...); // may throw
            elem_end_begin = begin;
            elem_end_last = begin + block_elements<T>();
            elem_end_end = ++begin;
            ++block_elem_end;
        }
    }
};
