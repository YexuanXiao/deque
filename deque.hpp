#include <algorithm>
#include <cassert>
#include <concepts>
#include <deque>
#include <iterator>
#include <memory>
#include <new>
#include <ranges>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>
#include <variant>

// 返回每个块的元素数量
// 块的大小，一定是4096的整数倍
template <typename T>
consteval std::size_t block_elements() noexcept
{
    auto constexpr pv = sizeof(T);
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
            auto rmd_cur = (block_sz * i) % pv;
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

    return result / pv;
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

    static constexpr std::size_t ceil_n(std::size_t const num, std::size_t const n) noexcept
    {
        return (num + n - 1uz) / n * n;
    }

    // 空deque安全，但执行后必须手动维护状态合法
    constexpr void destroy_elems() noexcept
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
            auto block_begin = block_elem_begin + 1uz;
            auto const block_end = block_elem_end - 1uz;
            for (; block_begin != block_end; ++block_begin)
            {
                auto begin = *block_begin;
                auto const end = begin + block_elements<T>();
                for (; begin != end; ++begin)
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
    }
    // 空deque安全
    constexpr void destruct() noexcept
    {
        destroy_elems();
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
    constexpr ~deque()
    {
        destruct();
    }

    constexpr bool empty() noexcept
    {
        return elem_begin_begin != elem_begin_end;
    }

    constexpr void clear() noexcept
    {
        destroy_elems();
        block_elem_begin = nullptr;
        block_elem_end = nullptr;
        elem_begin_begin = nullptr;
        elem_begin_end = nullptr;
        elem_begin_first = nullptr;
        elem_end_begin = nullptr;
        elem_end_end = nullptr;
        elem_end_last = nullptr;
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

    constexpr auto &front() noexcept
    {
        assert(elem_begin_begin);
        return *(elem_begin_begin);
    }

    constexpr auto &back() noexcept
    {
        assert(elem_end_end);
        return *(elem_end_end - 1uz);
    }

    // 空deque安全
    constexpr auto size() const noexcept
    {
        auto const elem_block_size = block_elem_end - block_elem_begin;
        auto result = 0uz;
        if (elem_block_size)
        {
            result += elem_begin_end - elem_begin_begin;
        }
        if (elem_block_size > 2z)
        {
            result += (block_elem_end - block_elem_begin - 2z) * block_elements<T>();
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

        constexpr iterator(block *elem_begin, T *curr, T *begin, T *end) noexcept
            : block_elem_begin(elem_begin), block_curr(curr), block_begin(begin), block_end(end)
        {
        }

      public:
        constexpr iterator() noexcept
        {
        }
    };

    // todo:
    // using const_iterator = std::basic_const_iterator<iterator>;

    constexpr auto begin() noexcept
    {
        return iterator{block_elem_begin, elem_begin_begin, elem_begin_begin, elem_begin_end};
    }

    constexpr auto end() noexcept
    {
        return iterator{block_elem_end, elem_end_end, elem_end_begin, elem_end_end};
    }

  private:
    // case 1           case 2            case 3           case 4
    // A1 B1 → A2 B2   A1       A2       A1 B1   A2       A1       A2 B2
    // |        |       |        |        |    ↘ |        |     ↗ |
    // C1    → C2      B1    → B2       C1      B2       B1       C2
    // |        |       |        |        |    ↘ |        |     ↗ |
    // D1       D2      D1 C1 → C2 D2    D1      C1 D1    C1 D1   D2
    // case 1 4: back
    // case 2 3: front
    // 对齐控制块
    // 对齐alloc和ctrl的begin，用于push_back
    constexpr void align_alloc_as_ctrl_back() noexcept
    {
        std::ranges::copy(block_alloc_begin, block_alloc_end, block_ctrl_begin);
        auto const block_size = block_alloc_end - block_alloc_begin;
        block_alloc_begin = block_ctrl_begin;
        block_alloc_end = block_ctrl_begin + block_size;
    }

    // 对齐控制块
    // 对齐alloc和ctrl的end，用于push_front
    constexpr void align_alloc_as_ctrl_front() noexcept
    {
        std::ranges::copy_backward(block_alloc_begin, block_alloc_end, block_ctrl_end);
        auto const block_size = block_alloc_end - block_alloc_begin;
        block_alloc_end = block_ctrl_end;
        block_alloc_begin = block_ctrl_end - block_size;
    }

    // 对齐控制块
    // 对齐elem和alloc的begin，用于push_back
    constexpr void align_elem_as_alloc_back() noexcept
    {
        std::ranges::rotate(block_alloc_begin, block_elem_begin, block_elem_end);
        auto const block_size = block_elem_end - block_elem_begin;
        block_elem_begin = block_alloc_begin;
        block_elem_end = block_alloc_begin + block_size;
    }

    // 对齐控制块
    // 对齐elem和alloc的end，用于push_front
    constexpr void align_elem_as_alloc_front() noexcept
    {
        std::ranges::rotate(block_elem_begin, block_elem_end, block_alloc_end);
        auto const block_size = block_elem_end - block_elem_begin;
        block_elem_end = block_alloc_end;
        block_elem_begin = block_alloc_end - block_size;
    }

    // ctrl_begin可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_back(block *ctrl_begin) noexcept
    {
        auto const alloc_block_size = block_alloc_end - block_alloc_begin;
        auto const elem_block_size = block_elem_end - block_elem_begin;
        auto const alloc_elem_offset_front = block_elem_begin - block_alloc_begin;
        // 将elem_begin和alloc_begin都对齐到ctrl_begin
        std::ranges::copy(block_elem_begin, block_elem_end, block_ctrl_begin);
        std::ranges::copy(block_alloc_begin, block_elem_begin, block_ctrl_begin + elem_block_size);
        std::ranges::copy(block_elem_end, block_alloc_end,
                          block_ctrl_begin + elem_block_size + alloc_elem_offset_front);
        block_alloc_begin = ctrl_begin;
        block_alloc_end = ctrl_begin + alloc_block_size;
        block_elem_begin = ctrl_begin;
        block_elem_end = ctrl_begin + elem_block_size;
    }

    // ctrl_begin可以是自己或者新ctrl的
    // 对齐控制块所有指针
    constexpr void align_elem_alloc_as_ctrl_front(block *ctrl_end) noexcept
    {
        auto const alloc_block_size = block_alloc_end - block_alloc_begin;
        auto const elem_block_size = block_elem_end - block_elem_begin;
        auto const alloc_elem_offset_front = block_elem_begin - block_alloc_begin;
        std::ranges::copy_backward(block_elem_begin, block_elem_end, block_ctrl_end);
        std::ranges::copy_backward(block_alloc_begin, block_elem_begin, block_ctrl_end - elem_block_size);
        std::ranges::copy_backward(block_elem_end, block_alloc_end,
                                   block_ctrl_end - elem_block_size - alloc_elem_offset_front);
        block_alloc_end = ctrl_end;
        block_alloc_begin = ctrl_end - alloc_block_size;
        block_elem_end = ctrl_end;
        block_elem_begin = ctrl_end - elem_block_size;
    }

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
        constexpr void replace_ctrl(deque &d) const noexcept
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
        constexpr void replace_ctrl_back(deque &d) const noexcept
        {

            d.align_elem_alloc_as_ctrl_back(block_ctrl_begin);

            // todo:
            // delete d.block_ctrl_begin;
            // 注意顺序
            // 从guard替换回deque
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
        }

        constexpr void replace_ctrl_front(deque &d) const noexcept
        {
            d.align_elem_alloc_as_ctrl_front(block_ctrl_begin);
            // todo:
            // delete d.block_ctrl_begin;
            // 注意顺序
            // 从guard替换回deque
            d.block_ctrl_begin = block_ctrl_begin;
            d.block_ctrl_end = block_ctrl_end;
        }

        // 参数是新大小
        constexpr ctrl_alloc(std::monostate &alloc, std::size_t const ctrl_size) : a(alloc)
        {
            // ceil_n(ctrl_size, 4uz);
            // todo:
            // block_alloc_begin = new T*[size];
            // block_alloc_end = block_alloc_begin + ctrl_size;
        }
    };

    // 向前分配新block，需要block_size小于等于(block_alloc_begin - block_ctrl_begin)
    // 且不block_alloc_X不是空指针
    constexpr void extent_block_front_uncond(std::size_t const block_size)
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
    constexpr void extent_block_back_uncond(std::size_t const block_size)
    {
        for (auto i = 0uz; i != block_size; ++i)
        {
            // todo
            // *block_alloc_end = new T[block_elements<T>()];
            ++block_alloc_end;
        }
    }

    // 向back扩展block
    // 对空deque安全
    constexpr void reserve_back(std::size_t const add_elem_size)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_alloc_cap = (block_elem_begin - block_alloc_begin) * block_elements<T>();
        // 尾部块的cap
        auto const tail_block_alloc_cap = (block_alloc_end - block_elem_end) * block_elements<T>();
        // 尾块的已使用大小
        auto const head_or_tail_used = elem_end_end - elem_end_begin;
        // non_move_cap为尾部-尾部已用，不移动块时cap
        auto const non_move_cap = tail_block_alloc_cap - head_or_tail_used;
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
        auto const add_block_size = (add_elem_size - move_cap + block_elements<T>() - 1uz) / block_elements<T>();
        // 获得目前控制块容许容量
        auto const ctrl_cap =
            ((block_alloc_begin - block_ctrl_begin) + (block_ctrl_end - block_alloc_end)) * block_elements<T>() +
            move_cap;
        // 如果容许容量足够，那么移动alloc
        if (ctrl_cap >= add_elem_size)
        {
            align_elem_alloc_as_ctrl_back(block_ctrl_begin);
        }
        else
        {
            // 否则扩展控制块
            auto const new_ctrl_size = block_ctrl_end - block_ctrl_begin + add_block_size;
            ctrl_alloc const ctrl{alloc, 4uz}; // may throw
            ctrl.replace_ctrl_back(*this);
        }
        extent_block_back_uncond(add_block_size);
    }

    // 从front扩展block，空deque安全
    constexpr void reserve_front(std::size_t const add_elem_size)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto const head_block_alloc_cap = (block_elem_begin - block_alloc_begin) * block_elements<T>();
        // 尾部块的cap
        auto const tail_block_alloc_cap = (block_alloc_end - block_elem_end) * block_elements<T>();
        // 头块的已使用大小
        auto const head_used = elem_begin_end - elem_begin_begin;
        // non_move_cap为头部-头部已用，不移动块时cap
        auto const non_move_cap = head_block_alloc_cap - head_used;
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
            std::ranges::copy(block_elem_begin, block_elem_end, block_alloc_end);
            return;
        }
        // 计算需要分配多少块数组，无论接下来是什么逻辑都直接使用它
        auto const add_block_size = (add_elem_size - move_cap + block_elements<T>() - 1uz) / block_elements<T>();
        // 获得目前控制块容许容量
        auto const ctrl_cap =
            ((block_alloc_begin - block_ctrl_begin) + (block_ctrl_end - block_alloc_end)) * block_elements<T>() +
            move_cap;

        if (ctrl_cap >= add_elem_size)
        {
            align_elem_alloc_as_ctrl_front(block_ctrl_begin);
        }
        else
        {
            // 否则扩展控制块
            auto const new_ctrl_size = block_ctrl_end - block_ctrl_begin + add_block_size;
            ctrl_alloc const ctrl{alloc, new_ctrl_size}; // may throw
            ctrl.replace_ctrl_front(*this);
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
            if (!released)
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
        ctrl_alloc const ctrl(alloc, block_size); // may throw
        ctrl.replace_ctrl(*this);
        extent_block_back_uncond(block_size); // may throw
    }

    // 复制赋值的辅助函数
    // 参数是最小block大小
    // 调用后可直接填充元素
    constexpr void extent_block(std::size_t new_block_size)
    {
        auto const old_alloc_size = block_alloc_end - block_alloc_begin;
        if (old_alloc_size > new_block_size)
        {
            return;
        }
        auto const old_ctrl_size = block_ctrl_end - block_ctrl_begin;
        if (old_ctrl_size < new_block_size)
        {
            ctrl_alloc const ctrl(alloc, new_block_size); // may throw
            ctrl.replace_ctrl_back(*this);
        }
        else
        {
            align_alloc_as_ctrl_back();
        }
        extent_block_back_uncond(new_block_size - old_alloc_size); // may throw
    }

    // 构造函数和复制赋值的辅助函数，要求block_alloc必须足够大
    constexpr void copy(deque const &other, std::size_t block_size)
    {
        if (block_size)
        {
            // 此时最为特殊，因为只有一个有效快时，可以从头部生长也可以从尾部生长
            // 当从头部生长时，elem_first_end==*block_elem_begin+block_elements<T>();
            // 当从尾部生长时，elem_end_begin==*block_elem_begin
            // 析构永远按头部的begin和end进行
            auto const elem_size = other.elem_begin_end - other.elem_begin_end;
            // 按从前到后生长，也就是只有一个block有效且block后半有空闲
            if (other.elem_end_begin == other.elem_begin_first)
            {
                elem_end_begin = *block_elem_begin;
                elem_end_end = elem_end_begin + elem_size;
                elem_end_last = elem_end_begin + block_elements<T>();
                elem_begin_begin = elem_end_begin;
                // 复制永远按头部进行复制，和析构一致
                elem_begin_end = elem_begin_begin;
                elem_begin_first = elem_end_begin;
            }
            else
            {
                // 否则按第一个block是后到前生长
                elem_begin_first = *block_elem_begin;
                elem_begin_begin = elem_begin_first + block_elements<T>() - elem_size;
                elem_begin_end = elem_begin_begin;
                elem_end_begin = elem_begin_begin;
                elem_end_end = elem_begin_begin + elem_size;
                elem_end_last = elem_end_end;
            }
            ++block_elem_end;
            std::ranges::uninitialized_copy(other.elem_begin_begin, other.elem_begin_end, elem_begin_end,
                                            std::unreachable_sentinel);
            elem_begin_end += elem_size;
        }
        if (block_size > 2z)
        {
            auto const target_block_end = other.block_elem_end - 1uz;
            auto target_block_begin = other.block_elem_begin + 1uz;
            for (; target_block_begin != target_block_end; ++target_block_begin) [[likely]]
            {
                elem_end_begin = *block_elem_end;
                ++block_elem_end;
                elem_end_end = elem_end_begin;
                // 由于回滚完全不在乎last，因此此处不用设置
                // elem_end_last = elem_end_begin + block_elements<T>();
                auto begin = *target_block_begin;
                std::ranges::uninitialized_copy(begin, begin + block_elements<T>(), elem_end_end,
                                                std::unreachable_sentinel);
                elem_end_end += block_elements<T>();
            }
        }
        if (block_size > 1z)
        {
            elem_end_begin = *block_elem_end;
            ++block_elem_end;
            elem_end_end = elem_end_begin;
            elem_end_last = elem_end_begin + block_elements<T>();
            std::ranges::uninitialized_copy(other.elem_end_begin, other.elem_end_end, elem_end_end,
                                            std::unreachable_sentinel);
            elem_end_end += (other.elem_end_end - other.elem_end_begin);
        }
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
        copy(other, block_size);
        guard.release();
    }

  private:
    // 使用count、count和T、或者随机访问迭代器进行构造，用于对应的构造函数
    // 注意异常安全
    template <typename... Ts>
    constexpr void construct_n(std::size_t block_size, std::size_t quot, std::size_t rem, Ts &&...t)
    {
        // 由于析构优先考虑elem_begin，因此必须独立构造elem_begin
        if (quot)
        {
            elem_begin_begin = *block_elem_end;
            ++block_elem_end;
            elem_begin_end = elem_begin_begin;
            elem_begin_first = elem_begin_begin;
            elem_end_begin = elem_begin_begin;
            elem_end_end = elem_begin_begin + block_elements<T>();
            elem_end_last = elem_end_end;
            auto const end = elem_begin_begin + block_elements<T>();
            if constexpr (sizeof...(Ts) == 0uz)
            {
                std::ranges::uninitialized_default_construct(elem_begin_begin, end);
            }
            else if constexpr (sizeof...(Ts) == 1uz)
            {
                std::ranges::uninitialized_fill(elem_begin_begin, end, t...);
            }
            else if constexpr (sizeof...(Ts) == 2uz)
            {
#if defined(__cpp_pack_indexing)
                auto &begin = t...[0uz];
                auto &end = t...[1uz];
#else
                auto x = std::tuple<Ts &...>(t...);
                auto &begin = std::get<0uz>(x);
                auto &end = std::get<1uz>(x);
#endif
                std::ranges::uninitialized_copy(std::counted_iterator(begin, block_elements<T>()),
                                                std::default_sentinel, elem_begin_begin, std::unreachable_sentinel);
                begin += block_elements<T>();
            }
            else
            {
                static_assert(false);
            }
            elem_begin_end = end;
        }
        for (auto i = 0uz; i != quot - 1uz; ++i)
        {
            elem_end_begin = *block_elem_end;
            ++block_elem_end;
            elem_end_end = elem_end_begin;
            // 由于回滚完全不在乎last，因此此处不用设置
            // elem_end_last = elem_end_begin + block_elements<T>();
            auto const end = elem_begin_begin + block_elements<T>();
            if constexpr (sizeof...(Ts) == 0uz)
            {
                std::ranges::uninitialized_default_construct(elem_begin_begin, end);
            }
            else if constexpr (sizeof...(Ts) == 1uz)
            {
                std::ranges::uninitialized_fill(elem_begin_begin, end, t...);
            }
            else if constexpr (sizeof...(Ts) == 2uz)
            {
#if defined(__cpp_pack_indexing)
                auto &begin = t...[0uz];
                auto &end = t...[1uz];
#else
                auto x = std::tuple<Ts &...>(t...);
                auto &begin = std::get<0uz>(x);
                auto &end = std::get<1uz>(x);
#endif
                std::ranges::uninitialized_copy(std::counted_iterator(begin, block_elements<T>()),
                                                std::default_sentinel, elem_begin_begin, std::unreachable_sentinel);
                begin += block_elements<T>();
            }
            else
            {
                static_assert(false);
            }
            elem_begin_end = end;
        }
        if (rem)
        {
            elem_end_begin = *block_elem_end;
            ++block_elem_end;
            elem_end_end = elem_end_begin;
            elem_end_last = elem_end_begin + block_elements<T>();
            auto const end = elem_begin_begin + rem;
            if constexpr (sizeof...(Ts) == 0uz)
            {
                std::ranges::uninitialized_default_construct(elem_begin_begin, end);
            }
            else if constexpr (sizeof...(Ts) == 1uz)
            {
                std::ranges::uninitialized_fill(elem_begin_begin, end, t...);
            }
            else if constexpr (sizeof...(Ts) == 2uz)
            {
#if defined(__cpp_pack_indexing)
                auto &begin = t...[0uz];
                auto &end = t...[1uz];
#else
                auto x = std::tuple<Ts &...>(t...);
                auto &begin = std::get<0uz>(x);
                auto &end = std::get<1uz>(x);
#endif
                std::ranges::uninitialized_copy(begin, end, elem_begin_begin, std::unreachable_sentinel);
            }
            else
            {
                static_assert(false);
            }
            elem_begin_end = end;
        }
    }

  public:
    constexpr deque(std::size_t count)
    {
        auto const quot = count / block_elements<T>();
        auto const rem = count % block_elements<T>();
        construct_guard guard(*this);
        construct_block(quot + 1uz);
        construct_n(quot + 1uz, quot, rem);
        guard.release();
    }

    constexpr deque(std::size_t count, T const &t)
    {
        auto const quot = count / block_elements<T>();
        auto const rem = count % block_elements<T>();
        construct_guard guard(*this);
        construct_block(quot + 1uz);
        construct_n(quot + 1uz, quot, rem, t);
        guard.release();
    }

    template <typename... V>
    constexpr T &emplace_back(V &&...v)
    {
        if (elem_end_end != elem_end_last)
        {
            auto begin = elem_end_end;
            std::construct_at(begin, std::forward<V>(v)...); // may throw
            ++elem_end_end;
            // 修正elem_begin
            if ((block_elem_end - block_elem_begin) == 1z)
            {
                ++elem_begin_end;
            }
            return *begin;
        }
        else
        {
            reserve_back(1uz);
            auto begin = *block_elem_end;
            std::construct_at(begin, std::forward<V>(v)...); // may throw
            elem_end_begin = begin;
            elem_end_last = begin + block_elements<T>();
            elem_end_end = begin + 1uz;
            ++block_elem_end;
            // 修正elem_begin
            if ((block_elem_end - block_elem_begin) == 1z)
            {
                elem_begin_first = begin;
                elem_begin_begin = begin;
                elem_begin_end = begin + 1uz;
            }
            return *begin;
        }
    }

    template <typename U, typename V>
        requires std::input_iterator<U> && std::sentinel_for<V, U>
    constexpr deque(U begin, V end)
    {
        if constexpr (std::random_access_iterator<U>)
        {
            auto const count = end - begin;
            auto const quot = count / block_elements<T>();
            auto const rem = count % block_elements<T>();
            construct_guard guard(*this);
            construct_block(quot + 1uz);
            construct_n(quot + 1uz, quot, rem, begin, end);
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

    constexpr deque(std::initializer_list<T> init)
    {
        auto const count = init.size();
        auto const quot = count / block_elements<T>();
        auto const rem = count % block_elements<T>();
        construct_guard guard(*this);
        construct_block(quot + 1uz);
        construct_n(quot + 1uz, quot, rem, init.begin(), init.end());
        guard.release();
    }

    constexpr void push_back(T const &t)
    {
        emplace_back(t);
    }

    constexpr void push_back(T &&t) noexcept
    {
        emplace_back(std::move(t));
    }

    constexpr deque &operator=(const deque &other)
    {
        destroy_elems();
        auto const block_size = other.block_elem_end - other.block_alloc_begin;
        extent_block(block_size);
        copy(other, block_size);
        return *this;
    }

    constexpr deque &operator=(std::initializer_list<T> ilist)
    {
        destroy_elems();
        auto const count = ilist.size();
        auto const quot = count / block_elements<T>();
        auto const rem = count % block_elements<T>();
        extent_block(quot + 1uz);
        construct_n(quot + 1uz, quot, rem, ilist.begin(), ilist.end());
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
        auto const quot = count / block_elements<T>();
        auto const rem = count % block_elements<T>();
        extent_block(quot + 1uz);
        construct_n(quot + 1uz, quot, rem, value);
    }

    template <typename U, typename V>
    constexpr void assign(U begin, V end)
        requires std::input_iterator<U> && std::sentinel_for<V, U>
    {
        destroy_elems();
        if constexpr (std::random_access_iterator<U>)
        {
            auto const count = end - begin;
            auto const quot = count / block_elements<T>();
            auto const rem = count % block_elements<T>();
            extent_block(quot + 1uz);
            construct_n(quot + 1uz, quot, rem, begin, end);
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
        auto const count = ilist.size();
        auto const quot = count / block_elements<T>();
        auto const rem = count % block_elements<T>();
        extent_block(quot + 1uz);
        construct_n(quot + 1uz, quot, rem, ilist.begin(), ilist.end());
    }

    template <typename R>
    constexpr void assign_range(R &&rg)
    {
        assign(std::ranges::begin(rg), std::ranges::end(rg));
    }

  private:
    constexpr T &at_impl(std::size_t pos) const noexcept
    {
        auto const head_size = elem_begin_end - elem_begin_begin;
        if (head_size <= pos)
        {
            return *(elem_begin_begin + pos);
        }
        else
        {
            auto const new_pos = pos - head_size;
            auto const quot = new_pos / block_elements<T>();
            auto const rem = new_pos / block_elements<T>();
            auto const elem_block_size = block_elem_end - block_elem_begin;
            if (quot + 2uz < elem_block_size)
            {
                std::terminate();
            }
            auto const tail_size = elem_end_end - elem_end_begin;
            if (rem > tail_size)
            {
                std::terminate();
            }
            return *(*(block_elem_begin + quot + 2uz) + rem);
        }
    }

  public:
    constexpr T &at(std::size_t pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr T const &at(std::size_t pos) const noexcept
    {
        return at_impl(pos);
    }

    constexpr T &operator[](std::size_t pos) noexcept
    {
        return at_impl(pos);
    }

    constexpr T const &operator[](std::size_t pos) const noexcept
    {
        return at_impl(pos);
    }
};
