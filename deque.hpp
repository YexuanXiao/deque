#include <algorithm>
#include <cassert>
#include <concepts>
#include <deque>
#include <iterator>
#include <memory>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <variant>

// 返回块的大小，一定是4096的整数倍
// 只被block_elements调用
static inline consteval std::size_t calc_block_size(std::size_t pv) noexcept
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
        for (auto i = 0uz; i < 8uz; ++i)
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

static inline consteval std::size_t block_elements(std::size_t pv) noexcept
{
    return calc_block_size(pv) / pv;
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

  public:
    ~deque()
    {
        // 4种情况，0，1，2，3+个块有元素
        auto elem_block_size = block_elem_end - block_elem_begin;
        // 清理中间的块
        if (elem_block_size > 2)
        {
            for (auto block_begin = block_elem_begin + 1; block_begin != block_alloc_end - 1; ++block_begin)
            {
                for (auto begin = block_begin; begin != block_begin + block_elements(sizeof(T)); ++begin)
                {
                    std::destroy_at(begin);
                }
            }
        }
        if (elem_block_size > 1)
        {
            for (auto begin = elem_end_begin; begin != elem_end_end; ++begin)
            {
                std::destroy_at(begin);
            }
        }
        if (elem_block_size > 0)
        {
            for (auto begin = elem_begin_begin; begin != elem_begin_end; ++begin)
            {
                std::destroy_at(begin);
            }
        }
        // 清理块
        for (auto begin = block_alloc_begin; begin != block_alloc_end; ++begin)
        {
            // todo:
            // delete block;
        }
        // todo:
        // delete block_ctrl_begin;
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
    struct ctrl_guard
    {
        std::monostate &a;
        block *block_ctrl_begin{};  // A
        block *block_ctrl_end{};    // D
        block *block_alloc_begin{}; // B
        block *block_alloc_end{};   // C

        // 替换块数组到deque
        // 构造时
        void replace_ctrl(deque &d) noexcept
        {
            std::swap(block_ctrl_begin, d.block_ctrl_begin);
            d.block_ctrl_end = block_ctrl_end;
        }
        // 扩容时，back为插入元素的方向
        void replace_ctrl(deque &d, bool push_back) noexcept
        {
            // 之前已分配的block数量
            auto alloc_block_size = d.block_alloc_end - d.block_alloc_begin;
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
            auto elem_block_offset = d.block_elem_begin - d.block_alloc_begin;
            auto elem_block_offset2 = d.block_alloc_end - d.block_elem_end;
            d.block_elem_begin = block_alloc_begin + elem_block_offset;
            d.block_elem_end = block_alloc_end - elem_block_offset2;
            // 从guard替换回deque
            d.block_ctrl_end = block_alloc_end;
            d.block_alloc_begin = block_alloc_begin;
            d.block_alloc_end = block_alloc_end;
            std::ranges::swap(block_alloc_begin, d.block_ctrl_begin);
        }
        // 必须是算好的大小
        ctrl_guard(deque &d, std::size_t ctrl_size) : a(d.alloc)
        {
            // todo:
            // block_alloc_begin = new T*[size];
            // block_alloc_end = block_alloc_begin + real_alloc_size;
        }
        ~ctrl_guard()
        {
            // todo:
            if (block_alloc_begin)
                ;
            // delete[] block_alloc_begin;
        }
    };

    // base: 4 ratio: 2
    // back 决定块数组优先使用前面还是后面，为true时优先使用前面，即向前移动
    // 该函数只被extent_ctrl调用
    // back为插入元素的方向
    void alloc_ctrl(std::size_t new_ctrl_size, bool push_back = true)
    {
        new_ctrl_size = (new_ctrl_size + 4 - 1) / 4 * 4;
        auto old_size = block_ctrl_end - block_ctrl_begin;
        ctrl_guard ctrl{*this, std::ranges::max({4uz, new_ctrl_size, old_size * 2uz})}; // may throw
        ctrl.replace_ctrl(*this, push_back);
    }

    // 先就地移动块，back为true时向前移动，否则重新分配块
    // back为插入元素的方向
    // 返回需要分配几个block
    std::size_t extent_ctrl(std::size_t add_elem_size, bool push_back = true)
    {
        // 计算现有头尾是否够用
        // 头部块的cap
        auto head_cap_block = (block_alloc_begin - block_ctrl_begin) * block_elements(sizeof(T));
        // 尾部块的cap
        auto tail_cap_block = (block_ctrl_end - block_alloc_end) * block_elements(sizeof(T));
        // 不移动块时cap
        auto non_move_cap = 0uz;
        // 移动块时cap
        auto move_cap = 0uz;
        // 头块或尾块的cap，取决于插入方向
        auto head_or_tail_cap = 0uz;

        if (push_back)
        {
            head_or_tail_cap = elem_end_end - elem_end_begin;
            // non_move_cap为尾部-尾部已用
            non_move_cap = tail_cap_block - head_or_tail_cap;
            // move_cap为头部+尾部-尾部已用
            move_cap = head_cap_block + non_move_cap;
        }
        else
        {
            head_or_tail_cap = elem_begin_end - elem_begin_begin;
            // non_move_cap为头部-头部已用
            non_move_cap = head_cap_block - head_or_tail_cap;
            // move_cap为头部-头部已用+尾部
            move_cap = non_move_cap + tail_cap_block;
        }

        // 首先如果cap足够，则直接进行计算
        if (non_move_cap >= add_elem_size) [[likely]]
        {
            /* */
        }
        // 然后如果经移动后足够则移动
        else if (move_cap >= add_elem_size) [[unlikely]]
        {
            if (push_back)
                std::ranges::copy(block_alloc_begin, block_alloc_end, block_ctrl_begin);
            else
                std::ranges::copy(block_alloc_begin, block_alloc_end, block_ctrl_end);
        }
        // 最后说明容量不够，则重新分配块
        else
        {
            alloc_ctrl(((add_elem_size - move_cap) + block_elements(sizeof(T)) - 1) / block_elements(sizeof(T)));
        }

        // 计算需要分配几个block

        return (add_elem_size - head_or_tail_cap) / block_elements(sizeof(T));
    }
    struct block_guard
    {
        T *data{};
        std::monostate &a;
        block_guard(std::monostate &alloc) noexcept : a(alloc)
        {
            // todo:
            // data = new T[block_elements];
        }
        ~block_guard()
        {
            // todo:
            // delete []data;
        }
    };
    void extent_block(std::size_t add_elem_size, bool push_back = true)
    {
        if (push_back)
        {
            // 首先检查已经alloc的是不是足够
            auto cap = (block_alloc_end - block_elem_end) * block_elements(sizeof(T)) - (elem_end_end - elem_end_begin);
            // cap 不够就扩容block
            if (cap <= add_elem_size)
            {
                auto block_size = extent_ctrl(add_elem_size, push_back);
                for (auto i = 0uz; i != block_size; ++i)
                {
                    // todo
                    // *block_alloc_end = new T[block_elements(sizeof(T))];
                    ++block_alloc_end;
                }
            }
        }
        else
        {
            auto cap = (block_elem_begin - block_alloc_begin) * block_elements(sizeof(T)) -
                       (elem_begin_end - elem_begin_begin);
            if (cap <= add_elem_size)
            {
                auto block_size = extent_ctrl(add_elem_size, push_back);
                for (auto i = 0uz; i != block_size; ++i)
                {
                    // todo
                    // *block_alloc_begin = new T[block_elements(sizeof(T))];
                    --block_alloc_begin;
                }
            }
        }
    }

  public:
    deque() noexcept = default;
    deque(deque const &rhs)
    {
        // todo
    }

    template <typename V>
    void emplace_back(V &&v)
    {
        if (elem_end_begin != elem_end_end)
        {
            std::construct_at(++elem_end_begin, std::forward<V>(v));
        }
        else
        {
            extent_block(1uz, true);
            auto begin = *block_alloc_end;
            std::construct_at(*begin, std::forward<V>(v)); // may throw
            elem_end_begin = begin;
            elem_end_last = begin + block_elements(sizeof(T));
            elem_end_end = ++begin;
        }
    }
};
