// Input generators for oracle cases. Portable and header-only, so the Linux
// tests pin their sequences; the runner feeds them to CaseContext::ForEachInput.
//
// A generator has `using value_type`, `std::optional<value_type> Next()`
// (std::nullopt at the end) and `static constexpr bool kBounded`. Bounded
// generators (IntRange, Edges) are swept to the end; Random is unbounded and
// ForEachInput stops it after the run's --iterations.
//
// Random draws from splitmix64, not from <random>'s distributions, whose
// output differs between standard libraries: the same seed gives the same
// inputs on every platform and compiler.
#pragma once

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <vector>

namespace oracle {

// One splitmix64 step (Steele, Lea and Flood): advances *state, returns 64
// bits.
inline uint64_t SplitMix64(uint64_t* state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

// Every value of [lo, hi], in order (hi included; empty when hi < lo).
template <class T>
class IntRangeGen {
    static_assert(std::is_integral_v<T>, "IntRange takes an integral type");

public:
    using value_type = T;
    static constexpr bool kBounded = true;

    IntRangeGen(T lo, T hi)
        : next_(lo)
        , hi_(hi)
        , done_(hi < lo)
    {
    }

    std::optional<T> Next()
    {
        if (done_) {
            return std::nullopt;
        }
        const T value = next_;
        if (next_ == hi_) {
            done_ = true; // no overflow at the type's maximum
        } else {
            ++next_;
        }
        return value;
    }

private:
    T next_;
    T hi_;
    bool done_;
};

template <class T>
IntRangeGen<T> IntRange(T lo, T hi)
{
    return IntRangeGen<T>(lo, hi);
}

// The listed values, in order.
template <class T>
class EdgesGen {
public:
    using value_type = T;
    static constexpr bool kBounded = true;

    explicit EdgesGen(std::vector<T> values)
        : values_(std::move(values))
    {
    }

    std::optional<T> Next()
    {
        if (index_ == values_.size()) {
            return std::nullopt;
        }
        return values_[index_++];
    }

private:
    std::vector<T> values_;
    size_t index_ = 0;
};

template <class T>
EdgesGen<T> Edges(std::initializer_list<T> values)
{
    return EdgesGen<T>(std::vector<T>(values));
}

// An endless stream of T from splitmix64(seed): every bit pattern of T, or,
// with Random<T>(seed, lo, hi), lo + bits % (hi - lo + 1) (a slight modulo
// bias, but the same on every platform). Signed values are the low bits in
// two's complement.
template <class T>
class RandomGen {
    static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>, "Random takes a non-bool integral type");
    using Unsigned = std::make_unsigned_t<T>;

public:
    using value_type = T;
    static constexpr bool kBounded = false;

    explicit RandomGen(uint64_t seed)
        : state_(seed)
    {
    }

    RandomGen(uint64_t seed, T lo, T hi)
        : state_(seed)
        , lo_(lo)
        // hi - lo is taken back to Unsigned before widening: for 8- and
        // 16-bit T the subtraction is done in int and can be negative.
        , span_(static_cast<uint64_t>(static_cast<Unsigned>(static_cast<Unsigned>(hi) - static_cast<Unsigned>(lo))) + 1)
        , ranged_(true)
    {
        assert(lo <= hi);
    }

    std::optional<T> Next()
    {
        const uint64_t bits = SplitMix64(&state_);
        if (ranged_ && span_ != 0) { // span_ wraps to 0 for a 64-bit full range
            return static_cast<T>(static_cast<Unsigned>(static_cast<Unsigned>(lo_) + bits % span_));
        }
        return static_cast<T>(static_cast<Unsigned>(bits));
    }

private:
    uint64_t state_;
    T lo_ = 0;
    uint64_t span_ = 0;
    bool ranged_ = false;
};

template <class T>
RandomGen<T> Random(uint64_t seed)
{
    return RandomGen<T>(seed);
}

template <class T>
RandomGen<T> Random(uint64_t seed, T lo, T hi)
{
    return RandomGen<T>(seed, lo, hi);
}

} // namespace oracle
