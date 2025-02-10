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

class deque
{

    using T = int;

    struct block
    {
        T *data{};
        block(std::monostate &alloc)
        {
            // todo:
            // data = new T[block_size(sizeof(T))];
        }
        ~block()
        {
            // todo:
            // delete []data;
        }
    };
#if __has_cpp_attribute(msvc::no_unique_address)
    [[msvc::no_unique_address]]
#else
    [[no_unique_address]]
#endif
    std::monostate alloc{};
    // 块数组的起始地址
    block *block_alloc_begin{};
    // 块数组的结束地址
    block *block_alloc_end{};
    // 有效块的起始地址
    block *block_elem_begin{};
    // 有效块的结束地址
    block *block_elem_end{};
    // 首个元素地址
    T *elem_begin{};
    // 尾后元素地址
    T *elem_end{};
    // 首个有效块的首元素地址
    T *elem_begin_begin{};
    // 首个有效块的结束分配地址
    T *elem_begin_end{};
    // 有效末尾块的首元素地址
    T *elem_end_begin{};
    // 有效末尾块的结束分配地址
    T *elem_end_end{};

    // 返回块的大小，一定是4096的整数倍
    // 只被block_elements调用
    static consteval std::size_t calc_block_size(std::size_t pv) noexcept
    {
        // 块的基本大小
        auto constexpr base = 4096uz;
        // 基本大小下，元素最大大小，至少保证16个
        auto constexpr sieve = base / 16uz;

        if (pv < sieve)
        {
            // 在基本大小的1-8倍间找到利用率最高的
            auto block_sz = base;
            auto rmd_pre = std::size_t(-1);
            auto result = 0uz;
            for (auto i = 0uz; i < 8uz; ++i)
            {
                block_sz *= (i + 1uz);
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

    inline static constexpr std::size_t block_elements = calc_block_size(sizeof(T)) / sizeof(T);

  public:
    void swap(deque &other) noexcept
    {
        // todo:
        // std::swap(alloc, other.alloc);
        using std::ranges::swap;
        swap(block_alloc_begin, other.block_alloc_begin);
        swap(block_alloc_end, other.block_alloc_end);
        swap(block_elem_begin, other.block_elem_begin);
        swap(block_elem_end, other.block_elem_end);
        swap(elem_begin, other.elem_begin);
        swap(elem_end, other.elem_end);
        swap(elem_begin_begin, other.elem_begin_begin);
        swap(elem_begin_end, other.elem_begin_end);
        swap(elem_end_begin, other.elem_end_begin);
        swap(elem_end_end, other.elem_end_end);
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
        assert(elem_begin);
        return *elem_begin;
    }

    auto &back() noexcept
    {
        assert(elem_end);
        return *(elem_end - 1uz);
    }

    struct iterator
    {
        friend deque;
        block *block_elem_begin{};
        block *block_elem_end{};
        T *elem_begin{};
        T *elem_end{};

        iterator(block *elem_begin, block *elem_end, T *begin, T *end) noexcept
            : block_elem_begin(elem_begin), block_elem_end(elem_end), elem_begin(begin), elem_end(end)
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
        return iterator{block_elem_begin, block_elem_end, elem_begin, elem_end};
    }

    auto end() noexcept
    {
        return iterator{};
    }

  private:
    struct controller
    {
        block *block_alloc_begin{};
        block *block_alloc_end{};
        block *block_elem_begin{};
        block *block_elem_end{};

        // 替换块数组到deque，back为插入元素的方向
        void replace_block(deque &d, std::size_t old_size, bool push_back) noexcept
        {
            // case 1           case 2            case 3           case 4
            // A1 B1 → A2 B2   A1       A2       A1 B1   A2       A1       A2 B2
            // |        |       |        |        |    ↘ |        |     ↗ |
            // C1    → C2      B1    → B2       C1      B2       B1       C2
            // |        |       |        |        |    ↘ |        |     ↗ |
            // D1       D2      D1 C1 → C2 D2    D1      C1 D1    C1 D1   D2
            if (push_back) // case 1 4
            {
                block_elem_begin = block_alloc_begin;
                block_elem_end = block_alloc_begin + old_size;
            }
            else // case 2 3
            {
                block_elem_begin = block_alloc_end - old_size;
                block_elem_end = block_alloc_end;
            }

            std::ranges::copy(d.block_elem_begin, d.block_elem_end, block_elem_begin);

            std::swap(block_alloc_begin, d.block_alloc_begin);
            d.block_alloc_end = block_alloc_end;
            d.block_elem_begin = block_elem_begin;
            d.block_elem_end = block_elem_end;
        }
        controller(std::size_t ctrl_size)
        {
            // todo:
            // block_alloc_begin = new T*[size];
            // block_alloc_end = block_alloc_begin + real_alloc_size;
        }
        ~controller()
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
        auto old_size = block_alloc_end - block_alloc_begin;
        controller ctrl{std::ranges::max({4uz, new_ctrl_size, old_size * 2uz})}; // may throw
        ctrl.replace_block(*this, old_size, push_back);
    }

    // 尝试就地移动块，back为true时向前移动
    // back为插入元素的方向
    bool inplace_extent_ctrl(std::size_t added_elem_size, bool push_back = true)
    {
        // 计算现有头尾是否够用
        auto head_cap = (block_elem_begin - block_alloc_begin) * block_elements;
        auto tail_cap = (block_alloc_end - block_elem_end) * block_elements;
        if (push_back)
        {
            // avai为头部+尾部-尾部已用
            auto avai_size = head_cap + tail_cap - (elem_end - elem_end_begin);
            if (avai_size >= added_elem_size)
            {
                std::ranges::copy(block_elem_begin, block_elem_end, block_alloc_begin);
                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            // avai为头部-头部已用+尾部
            auto avai_size = head_cap - (elem_begin - elem_begin_begin) + tail_cap;
            if (avai_size >= added_elem_size)
            {
                std::ranges::copy(block_elem_begin, block_elem_end, block_alloc_end);
                return true;
            }
            else
            {
                return false;
            }
        }
    }

  public:
    deque() noexcept = default;
    deque(deque const &rhs)
    {
        controller ctrl(rhs.block_elem_end - rhs.block_elem_begin);
    }

    void push_back(T const &v)
    {
        if (elem_end != elem_end_end)
        {
            std::construct_at(elem_end, v);
            ++elem_end;
        }
        else
        {
        }
    }
};
