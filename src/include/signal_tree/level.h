#pragma once

#include "./node.h"

#include <type_traits>
#include <concepts>
#include <array>


namespace bcpp::implementation::signal_tree
{

    template <std::size_t N0, std::size_t N1>
    requires ((std::popcount(N0) == 1) && (std::popcount(N1) == 1))
    struct level_traits
    {
        static auto constexpr number_of_nodes = N0;
        static auto constexpr node_capacity = N1;
    };


    template <typename T> concept level_traits_concept = std::is_same_v<T, level_traits<T::number_of_nodes, T::node_capacity>>;
    template <typename T> concept root_level_traits = ((level_traits_concept<T>) && (T::number_of_nodes == 1));
    template <typename T> concept leaf_level_traits = ((level_traits_concept<T>) && (T::node_capacity == 64));
    template <typename T> concept non_leaf_level_traits = ((level_traits_concept<T>) && (!leaf_level_traits<T>));


    template <level_traits_concept T>
    struct level;


    template <level_traits_concept T> 
    struct child_level{using type = struct{};};


    template <non_leaf_level_traits T>
    struct child_level<T>
    {
        static auto constexpr number_of_child_nodes = T::number_of_nodes * node<node_traits<T::node_capacity>>::number_of_counters;
        using type = level<level_traits<number_of_child_nodes, node<node_traits<T::node_capacity>>::child_type::capacity>>;
    };


    //=============================================================================
    template <level_traits_concept T>
    struct alignas(64) level
    {
        using node_type = node<node_traits<T::node_capacity>>;
        using child_level_type = child_level<T>::type;
        using counter_index = std::uint64_t;
        using bias_flags = std::uint64_t;
        using node_index = std::uint64_t;
        
        static auto constexpr node_capacity = T::node_capacity;
        static auto constexpr node_count = T::number_of_nodes;
        static auto constexpr counters_per_node = node_type::number_of_counters;

        bool empty() const noexcept;

        bool set
        (
            counter_index
        ) noexcept;

        counter_index select
        (
            bias_flags
        ) noexcept;

    private:

        template <level_traits_concept> friend struct level;

        counter_index select
        (
            bias_flags,
            node_index
        ) noexcept;

        std::array<node_type, node_count>   nodes_;

        child_level_type                    childLevel_;
    };

} // namespace bcpp::implementation::signal_tree


//=============================================================================
template <bcpp::implementation::signal_tree::root_level_traits T>
inline bool bcpp::implementation::signal_tree::level<T>::empty
(
) const noexcept
{
    return nodes_[0].empty();
}


//=============================================================================
template <bcpp::implementation::signal_tree::root_level_traits T>
inline auto bcpp::implementation::signal_tree::level<T>::select
(
    // return the index of a counter which is not zero (indicates that one of the leaf nodes
    // represented by this counter is set - a non zero value)
    bias_flags biasFlags
) noexcept -> counter_index
{
    return select(biasFlags, 0);
}


//=============================================================================
template <bcpp::implementation::signal_tree::level_traits_concept T>
inline bool bcpp::implementation::signal_tree::level<T>::set
(
    // increment the counter associated with the specified index (id of a leaf node)
    std::uint64_t signalIndex
) noexcept
{
    if constexpr (leaf_level_traits<T>)
    {
        // try to set the leaf and return true on success, false on failure
        return nodes_[signalIndex / node_capacity].set(signalIndex);
    }
    else
    {
        if (childLevel_.set(signalIndex))
        {
            nodes_[signalIndex / node_capacity].set(signalIndex);
            return true;
        }
        return false;
    }
}


//=============================================================================
template <bcpp::implementation::signal_tree::level_traits_concept T>
inline auto bcpp::implementation::signal_tree::level<T>::select
(
    // return the index of a child counter which is not zero (indicates that one of the leaf nodes
    // represented by this counter is set - a non zero value)
    bias_flags biasFlags,
    node_index nodeIndex
) noexcept -> counter_index
{
    static auto constexpr bias_bits_consumed_to_select_counter = minimum_bit_count(counters_per_node) - 1;

    auto selectedCounter = nodes_[nodeIndex].select(biasFlags);
    biasFlags <<= bias_bits_consumed_to_select_counter;

    if constexpr (root_level_traits<T>)
    {
        if (selectedCounter == ~0)
            return ~0;
    }
    
    if constexpr (leaf_level_traits<T>)
    {
        return selectedCounter;
    }
    else
    {
        static auto constexpr bias_bits_consumed_to_select_child_counter = minimum_bit_count(child_level_type::node_capacity) - 1;
        return ((selectedCounter << bias_bits_consumed_to_select_child_counter) | 
                childLevel_.select(biasFlags, selectedCounter + (nodeIndex * counters_per_node)));
    }
}
