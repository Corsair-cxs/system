#pragma once

#include "./work_contract_mode.h"
#include "./waitable_state.h"

#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>


namespace bcpp::system 
{

    template <work_contract_mode>
    class work_contract;


    template <work_contract_mode T>
    class sub_work_contract_group :
        non_copyable
    {
    public:

        static auto constexpr mode = T;
        using work_contract_type = work_contract<mode>;


        class surrender_token;

        sub_work_contract_group() = default;

        sub_work_contract_group(sub_work_contract_group &&);

        sub_work_contract_group & operator = (sub_work_contract_group &&) = delete;

        sub_work_contract_group
        (
            std::uint64_t
        ) requires (mode == work_contract_mode::non_blocking);

        sub_work_contract_group
        (
            std::uint64_t,
            std::shared_ptr<waitable_state>
        ) requires (mode == work_contract_mode::blocking);

        ~sub_work_contract_group();

        work_contract_type create_contract
        (
            std::function<void()>
        );

        work_contract_type create_contract
        (
            std::function<void()>,
            std::function<void()>
        );

        bool execute_next_contract
        (
            std::uint64_t
        );

        void stop();

    private:

        friend class work_contract<mode>;
        friend class surrender_token;

        static auto constexpr left_addend   = 0x0000000000000001ull;
        static auto constexpr left_mask     = 0x00000000ffffffffull;
        static auto constexpr right_mask    = 0xffffffff00000000ull;
        static auto constexpr right_addend  = 0x0000000100000000ull;

        struct contract
        {
            static auto constexpr surrender_flag    = 0x00000004;
            static auto constexpr execute_flag      = 0x00000002;
            static auto constexpr invoke_flag       = 0x00000001;
        
            std::function<void()>       work_;
            std::function<void()>       surrender_;
            std::atomic<std::int32_t>   flags_;
        };

        void invoke
        (
            work_contract_type const &
        );

        void surrender
        (
            work_contract_type const &
        );        
        
        template <std::size_t>
        void set_contract_flag
        (
            work_contract_type const &
        );

        enum class inclination : std::uint32_t
        {
            left = 0,
            right = 1
        };

        template <inclination>
        std::pair<std::int64_t, std::uint64_t> decrement_contract_count(std::int64_t);

        std::size_t process_contract();

        void process_surrender(std::int64_t);

        void process_contract(std::int64_t);

        void increment_contract_count
        (
            std::int64_t
        ) requires (mode == work_contract_mode::blocking);
        
        void increment_contract_count
        (
            std::int64_t
        ) requires (mode == work_contract_mode::non_blocking);

        union alignas(8) invocation_counter
        {
            invocation_counter():u64_(){static_assert(sizeof(*this) == sizeof(std::uint64_t));}
            std::atomic<std::uint64_t> u64_;
            struct parts
            {
                std::uint32_t left_;
                std::uint32_t right_;
            } u32_;
            std::uint64_t get_count() const
            {
                auto n = u64_.load();
                return ((n >> 32) + (n & 0xffffffff));
            }
        };

        template <std::uint64_t N>
        std::uint64_t select_contract
        (
            std::uint64_t &,
            std::uint64_t
        );

        template <std::uint64_t N>
        std::uint64_t select_invoked_contract_bit
        (
            std::uint64_t,
            std::uint64_t,
            std::uint64_t
        ) const;

        template <std::uint64_t N>
        std::uint64_t get_available_contract_bit
        (
            std::uint64_t,
            std::uint64_t
        );

        std::size_t get_available_contract();

        std::uint64_t                                   capacity_;
        std::vector<invocation_counter>                 invocationCounter_;
        std::vector<std::atomic<std::uint64_t>>         invokedFlag_;

        std::vector<invocation_counter>                 availableCounter_;
        std::vector<std::atomic<std::uint64_t>>         availableFlag_;

        std::vector<contract>                           contracts_;

        std::vector<std::shared_ptr<surrender_token>>   surrenderToken_;

        std::int64_t                                    firstContractIndex_;

        std::mutex                                      mutex_;

        std::shared_ptr<waitable_state>                 waitableState_;

        std::atomic<bool>                               stopped_{false};

    }; // class sub_work_contract_group


    template <work_contract_mode T>
    class sub_work_contract_group<T>::surrender_token
    {
    public:

        surrender_token
        (
            sub_work_contract_group *
        );
        
        std::mutex mutex_;
        sub_work_contract_group * workContractGroup_{};

        bool invoke(work_contract_type const &);

        void orphan();
    };

} // namespace bcpp::system


#include "./work_contract.h"


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::sub_work_contract_group<T>::sub_work_contract_group
(
    std::uint64_t capacity,
    std::shared_ptr<waitable_state> waitableState
) requires (mode == work_contract_mode::blocking) :
    capacity_((capacity < 256) ? 256 : ((capacity + 63) / 64) * 64),
    invocationCounter_(capacity_ / 64),
    invokedFlag_(capacity_ / 64),
    availableCounter_(capacity_ / 64),
    availableFlag_(capacity_ / 64),
    contracts_(capacity_),
    surrenderToken_(capacity_),
    waitableState_(waitableState),
    firstContractIndex_((capacity_ / 64) - 1)
{
    std::uint32_t c = capacity_;
    auto n = 1;
    auto k = 0;
    while (c > 64)
    {
        for (auto i = 0; i < n; ++i)
            availableCounter_[k++].u32_ = {c / 2, c / 2};
        n <<= 1;
        c >>= 1;
    }
    for (auto & _ : availableFlag_)
        _ = 0xffffffffffffffffull;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::sub_work_contract_group<T>::sub_work_contract_group
(
    std::uint64_t capacity
) requires (mode == work_contract_mode::non_blocking) :
    capacity_((capacity < 256) ? 256 : ((capacity + 63) / 64) * 64),
    invocationCounter_(capacity_ / 64),
    invokedFlag_(capacity_ / 64),
    availableCounter_(capacity_ / 64),
    availableFlag_(capacity_ / 64),
    contracts_(capacity_),
    surrenderToken_(capacity_),
    firstContractIndex_((capacity_ / 64) - 1)
{
    std::uint32_t c = capacity_;
    auto n = 1;
    auto k = 0;
    while (c > 64)
    {
        for (auto i = 0; i < n; ++i)
            availableCounter_[k++].u32_ = {c / 2, c / 2};
        n <<= 1;
        c >>= 1;
    }
    for (auto & _ : availableFlag_)
        _ = 0xffffffffffffffffull;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::sub_work_contract_group<T>::sub_work_contract_group
(
    sub_work_contract_group && other
) :
    capacity_(other.capacity_),
    invocationCounter_(std::move(other.invocationCounter_)),
    invokedFlag_(std::move(other.invokedFlag_)),
    availableCounter_(std::move(other.availableCounter_)),
    availableFlag_(std::move(other.availableFlag_)),
    contracts_(std::move(other.contracts_)),
    surrenderToken_(std::move(other.surrenderToken_)),
    waitableState_(std::move(other.waitableState_)),
    firstContractIndex_(other.firstContractIndex_)
{
    other.stopped_ = true;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::sub_work_contract_group<T>::~sub_work_contract_group
(
)
{
    stop();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::sub_work_contract_group<T>::stop
(
)
{
    if (bool wasRunning = !stopped_.exchange(true); wasRunning)
    {
        for (auto & surrenderToken : surrenderToken_)
            if ((bool)surrenderToken)
                surrenderToken->orphan();
    }
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline auto bcpp::system::sub_work_contract_group<T>::create_contract
(
    std::function<void()> function
) -> work_contract_type
{
    return create_contract(function, nullptr);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline auto bcpp::system::sub_work_contract_group<T>::create_contract
(
    std::function<void()> function,
    std::function<void()> surrender
) -> work_contract_type
{

    std::uint32_t contractId = get_available_contract();
    if (contractId == ~0)
        return {}; // no free contracts
    auto & contract = contracts_[contractId];
    contract.flags_ = 0;
    contract.work_ = function;
    contract.surrender_ = surrender;
    auto surrenderToken = surrenderToken_[contractId] = std::make_shared<surrender_token>(this);
    return {this, surrenderToken, contractId};
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::sub_work_contract_group<T>::surrender
(
    work_contract_type const & workContract
)
{
    set_contract_flag<contract::surrender_flag | contract::invoke_flag>(workContract);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::sub_work_contract_group<T>::invoke
(
    work_contract_type const & workContract
)
{
    set_contract_flag<contract::invoke_flag>(workContract);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
template <std::size_t flags_to_set>
inline void bcpp::system::sub_work_contract_group<T>::set_contract_flag
(
    work_contract_type const & workContract
)
{
    static auto constexpr flags_mask = (contract::execute_flag | contract::invoke_flag);
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
template <std::uint64_t N>
inline std::uint64_t bcpp::system::sub_work_contract_group<T>::get_available_contract_bit
(
    std::uint64_t invokedFlags,
    std::uint64_t selection
)
{
    if constexpr (N == 0)
    {
        return selection;
    }
    else
    {
        auto constexpr bits_to_consider = (1 << N) / 2;
        static auto constexpr left_bit_mask = ~((1ull << bits_to_consider) - 1);
        static auto constexpr right_bit_mask = ~left_bit_mask;

        auto leftCount = std::popcount(invokedFlags & left_bit_mask);
        auto rightCount = std::popcount(invokedFlags & right_bit_mask);
        if (leftCount < rightCount)
            return get_available_contract_bit<N - 1>(invokedFlags & right_bit_mask, selection + bits_to_consider);
        return get_available_contract_bit<N - 1>((invokedFlags >> bits_to_consider) & right_bit_mask, selection);
    }
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline std::size_t bcpp::system::sub_work_contract_group<T>::get_available_contract
(
    // find an unclaimed contract
    // do so with consideration to how balanced the tree is
)
{
    auto select_larger_child = [&]
    (
        std::size_t parent
    )
    {
        auto & contractCounter = availableCounter_[parent].u64_;
        auto expected = contractCounter.load();
        auto addend = ((expected & 0xffffffff) > (expected >> 32)) ? left_addend : right_addend;
        while ((expected != 0) && (!contractCounter.compare_exchange_strong(expected, expected - addend)))
            addend = ((expected & 0xffffffff) > (expected >> 32)) ? left_addend : right_addend;
        return expected ? ((parent * 2) + 1 + (addend == right_addend)) : 0;
    };

    auto parent = select_larger_child(0);
    while ((parent) && (parent < firstContractIndex_) && (parent < (capacity_ - 1)))
        parent = select_larger_child(parent);
    if (parent == 0)
        return ~0;

    auto availableFlagIndex = (parent - firstContractIndex_);
    auto & availableFlags = availableFlag_[availableFlagIndex];
    while (true)
    {
        auto expected = availableFlags.load();
        auto contractIndex = get_available_contract_bit<6>(expected, 0);
        auto bit = (0x8000000000000000ull >> contractIndex);
        if (availableFlags.compare_exchange_strong(expected, expected & ~bit))
            return ((availableFlagIndex * 64) + contractIndex);
    }
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::sub_work_contract_group<T>::increment_contract_count
(
    std::int64_t current
) requires (mode == work_contract_mode::blocking)
{
    invokedFlag_[current >> 6] |= ((0x8000000000000000ull) >> (current & 0x3f));
    current >>= 6;
    current += firstContractIndex_;
    std::uint64_t rootCount = 0;
    while (current)
        rootCount = (invocationCounter_[current >>= 1].u64_ += ((current-- & 1ull) ? left_addend : right_addend));
    if ((((rootCount >> 32) + rootCount) & 0xffffffff) == 1)
        waitableState_->increment_activity_count();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::sub_work_contract_group<T>::increment_contract_count
(
    std::int64_t current
) requires (mode == work_contract_mode::non_blocking)
{
    invokedFlag_[current >> 6] |= ((0x8000000000000000ull) >> (current & 0x3f));
    current >>= 6;
    current += firstContractIndex_;
    while (current)
        invocationCounter_[current >>= 1].u64_ += ((current-- & 1ull) ? left_addend : right_addend);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
template <std::uint64_t N>
inline std::uint64_t bcpp::system::sub_work_contract_group<T>::select_invoked_contract_bit
(
    std::uint64_t inclinationFlags,
    std::uint64_t invokedFlags,
    std::uint64_t selection
) const
{
    if constexpr (N == 0)
    {
        return selection;
    }
    else
    {
        auto constexpr bits_to_consider = (1 << N) / 2;
        std::uint64_t constexpr bit_mask[2]{~((1ull << bits_to_consider) - 1), ((1ull << bits_to_consider) - 1)};

        auto preferRight = ((inclinationFlags & 1) == 1);
        if (auto usePreferedSide = ((invokedFlags & bit_mask[preferRight]) != 0); usePreferedSide == preferRight)
            selection += bits_to_consider;
        else
            invokedFlags >>= bits_to_consider;
        return select_invoked_contract_bit<N - 1>(inclinationFlags >> 1, invokedFlags & bit_mask[1], selection);
    }
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
template <bcpp::system::sub_work_contract_group<T>::inclination T_>
inline auto bcpp::system::sub_work_contract_group<T>::decrement_contract_count
(
    std::int64_t parent
) -> std::pair<std::int64_t, std::uint64_t> 
{
    static auto constexpr left_inclination = (T_ == inclination::left);
    static auto constexpr mask = (left_inclination) ? left_mask : right_mask;
    static auto constexpr prefered_addend = (left_inclination) ? left_addend : right_addend;
    static auto constexpr fallback_addend = (left_inclination) ? right_addend : left_addend;

    auto & invocationCounter = invocationCounter_[parent].u64_;
    auto expected = invocationCounter.load();
    auto addend = (expected & mask) ? prefered_addend : fallback_addend;
    while ((expected != 0) && (!invocationCounter.compare_exchange_strong(expected, expected - addend)))
        addend = (expected & mask) ? prefered_addend : fallback_addend;
    return {expected ? (1 + (addend > left_mask)) : 0, expected - addend};
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
template <std::uint64_t N>
inline std::uint64_t bcpp::system::sub_work_contract_group<T>::select_contract
(
    std::uint64_t & inclinationFlags,
    std::uint64_t parent
)
{
    static auto constexpr logical_max_n = ((sizeof(inclinationFlags) * 8) - 1);
    if constexpr (N < logical_max_n)
    {
        static auto constexpr bit = (1ull << N);
        static auto constexpr right = inclination::right;
        static auto constexpr left = inclination::left;

        if (parent >= firstContractIndex_)
        {
            inclinationFlags >>= (N + 1);
            return parent;
        }
        auto [n, _] = ((inclinationFlags & bit) ? decrement_contract_count<right>(parent) : decrement_contract_count<left>(parent));
        return select_contract<N + 1>(inclinationFlags, (parent * 2) + n);
    }
    return 0;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bool bcpp::system::sub_work_contract_group<T>::execute_next_contract
(
    std::uint64_t inclinationFlags
) 
{
    static auto constexpr right = inclination::right;
    static auto constexpr left = inclination::left;

    auto [parent, rootCount] = ((inclinationFlags & 1) ? decrement_contract_count<right>(0) : decrement_contract_count<left>(0));
    if (parent)  
    {
        if constexpr (mode == work_contract_mode::blocking)
        {
            if (rootCount == 0)
                waitableState_->decrement_activity_count();
        }
        // select parent node in heap
        parent = (parent < firstContractIndex_) ? select_contract<1>(inclinationFlags, parent) : parent;
        // select bit which represents the contract to execute
        auto invokedFlagsIndex = (parent - firstContractIndex_);
        auto & invokedFlags = invokedFlag_[invokedFlagsIndex];
        while (true)
        {
            auto expected = invokedFlags.load();
            auto contractIndex = select_invoked_contract_bit<6>(inclinationFlags, expected, 0);
            auto bit = (0x8000000000000000ull >> contractIndex);
            expected |= bit;
            if (invokedFlags.compare_exchange_strong(expected, expected & ~bit))
            {
                process_contract((invokedFlagsIndex * 64) + contractIndex);
                return true;
            }
        }
    }
    return false;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::sub_work_contract_group<T>::process_contract
(
    std::int64_t contractId
)
{
    auto & contract = contracts_[contractId];
    auto & flags = contract.flags_;
    if ((++flags & contract::surrender_flag) != contract::surrender_flag)
    {
        contract.work_();
        if (((flags -= contract::execute_flag) & contract::invoke_flag) == contract::invoke_flag)
            increment_contract_count(contractId);
    }
    else
    {
        process_surrender(contractId);
    }
}