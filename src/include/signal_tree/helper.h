#pragma once

#include <bit>
#include <cstdint>
#include <concepts>


namespace bcpp
{

    //=========================================================================
    static inline consteval auto minimum_bit_count
    (
        std::integral auto value
    )
    {
        return ((sizeof(value) * 8) - std::countl_zero(value));
    }


    //=========================================================================
    static inline consteval auto is_power_of_two
    (
        std::integral auto value
    )
    {
        return (std::popcount(value) == 1);
    }


    //=========================================================================
    template <std::size_t, std::size_t>
    struct sub_counter_arity;


    //=============================================================================
    template <std::size_t counter_capacity>
    struct sub_counter_arity<counter_capacity, 0>
    {
        static auto constexpr value = 0;
    };


    //=============================================================================
    template <std::size_t counter_capacity, std::size_t N>
    struct sub_counter_arity
    {
        static auto constexpr bits_per_counter = (64ull - std::countl_zero(counter_capacity / N));
        static auto constexpr bits_required = (bits_per_counter * N);
        static auto constexpr value = (bits_required <= 64) ? N : 
                sub_counter_arity<counter_capacity, N / 2>::value;
    };


    //=========================================================================
    template <std::size_t counter_capacity>
    static auto constexpr sub_counter_arity_v = sub_counter_arity<counter_capacity, 64>::value;


    //=========================================================================
    // these will be type rich types in the near future
    using tree_index = std::uint64_t;
    
} // namespace bcpp
