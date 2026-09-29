#pragma once
#include "pool.hpp"
#include "types.hpp"

#include <numeric>

namespace defi {


class FilledAndPool {
public:
    FilledAndPool(BaseQuote_uint64 in, PoolLiquidity_uint64 pool)
        : in { std::move(in) }
        , pool { std::move(pool) }
    {
    }
    std::optional<NonzeroDelta_uint64> balance_pool_interaction() const;

    BaseQuote_uint64 in;
    PoolLiquidity_uint64 pool;

    MatchResult_uint64 bisect_dynamic_price() const
    {
        return { balance_pool_interaction(), in };
    }
};

class Matcher : public FilledAndPool {
public:
    Matcher(PoolLiquidity_uint64 p)
        : FilledAndPool({ 0, 0 }, std::move(p))
    {
    }

    // Returns whether for the next bisection step we must (true) or must not (false) induce
    // (a) increase of matched quote amount or
    // (b) reduction of matched base amount or
    // (c) reduction of price argument
    // If no action must be taked (correct price) return value is in.excess(p).isQuote
    // The returned bool value of this function is **non-increasing** in (a), (b) and (c) above.
    // Works also for degenerated pool (base and quote can be 0)
    bool bisection_step(Price_uint64 p)
    {
        Delta_uint64 toPool { in.excess(p) };
        // CASE DISTINCTION
        // based on toPool.isQuote to reflect different behavior in CASE (I) and (III) in bisect_fixed_price.
        auto res {
            (toPool.isQuote ? pool.rel_quote_price(toPool.amount.value(), p) != std::strong_ordering::greater
                            : pool.rel_base_price(toPool.amount.value(), p) == std::strong_ordering::less)
        };
        if (res) {
            toPool0 = toPool;
        } else {
            toPool1 = toPool;
        }
        // Even with the above case distinction the returned bool value of this function is non-increasing 
        // a single of the following parameter changes
        // (a) increase of matched quote amount or
        // (b) reduction of matched base amount or
        // (c) reduction of price argument
        // while the other 2 parameters remain constant.
        return res;
    };

    FillResult_uint64 bisect_fixed_price(const bool isQuote,
        const Funds_uint64 fill0, // bisection_step should return true for this value
        const Funds_uint64 fill1, // bisection_step should return false on this value
        Price_uint64 p)
    {
        // If isQuote == true then this function will be called with fill0 < fill1
        //    and this fill amount will correspond to total quote amount filled.
        // If isQuote == false, then this function will be called with fill0 > fill1
        //    and this fill amount will correspond to total base amount filled.

        assert(toPool0.has_value() // by the time this function is executed, we have
            && toPool1.has_value()); // seen bisection_step return true and false .
        Funds_uint64 v0 { fill0 };
        Funds_uint64 v1 { fill1 };
        auto& v { isQuote ? in.quote : in.base };
        while (true) {
            Funds_uint64 tmp { std::midpoint(v0.value(), v1.value()) };
            if (tmp == v0)
                break;

            v = tmp;
            if (bisection_step(p))
                v0 = v;
            else
                v1 = v;
        }
        // Now at v0, bisection_step is true while
        // at v1 = v0 + 1, bisection_step is false

        auto* pDelta { &*toPool0 }; // pointer for result, we may change it below.
        // We now have three possible cases:
        if (toPool0->isQuote == true) { // CASE (I)
            // In this case toPool1->isQuote == true and toPool1->amount >= toPool0->amount because price is fixed and
            // v1 is smaller for isQuote == false, and larger for isQuote == true which both increases
            // the toPool quote amount returned by the excess function.
            assert(toPool1->isQuote == true);
            // It follows that at v1 the pool price strictly overshoots the fill price, see the bisection_step condition:
            // pool.rel_quote_price(toPool.amount.value(), p) != std::strong_ordering::greater which we assumed evaluates
            // to false.
            // As a convention, we only want to buy from pool while pool price stays cheaper or equal, i.e. does not overshoot
            // the fill price. This convention was selected to represent lazy pool interaction, we do not use the pool 
            // more than strictly necessary.
            // So we use v0 and toPool0 for the return value.
            v = v0; // save v0 to the reference of `in` member
            pDelta = &*toPool0;
        } else if (toPool1->isQuote == true) { // toPool1->isQuote == true && toPool0->isQuote == false, CASE (II)
            // In this case toPool0->amount == 0 or toPool1->amount == 0 by the fact that v1 = v0 + 1 and the structure of the
            // excess function (check this by thinking about both, isQuote == true and isQuote == false). In this case
            // we don't want to interact with the pool, i.e. select the case where amount == 0.
            // This rule aligns with the convention of using the pool conservatively and also ensures that this function
            // can be applied to degenerated pools without liquidity which have no defined price.
            if (toPool0->amount.is_zero()) {
                v = v0; // save v0 to the reference of `in` member
                pDelta = &*toPool0;
            } else {
                assert(toPool1->amount.is_zero());
                v = v1; // save v0 to the reference of `in` member
                pDelta = &*toPool1;
            }
        } else { // toPool0->isQuote == false && toPool1->isQuote == false, CASE (III)
            // In both cases we push base currency to the pool, i.e. sell to the pool. From the bisection_step function 
            // the condition pool.rel_base_price(toPool.amount.value(), p) == std::strong_ordering::less) is true for
            // toPool == toPool0 and false for toPool == toPool1, i.e. when selling to the pool, 
            // Similar in the first case, as a convention this time we only want to sell to pool buying while pool price stays 
            // greater or equal, i.e. does not undershoot. Note that this is the opposite of what we selected as a convention
            // in the first case. The reason is that we again want to to represent lazy pool interaction, i.e. we do not want
            // to use the pool more than strictly necessary but this time we are selling instead of buying so we cross the 
            // pool price from the other direction.
            // Note that this different intention (In case (I) we do not want to overshoot, in case (II) we do not want to 
            // undershoot the pool price) is the reason for the case distinction in the bisection_step function.
            v = v1; // save v0 to the reference of `in` member
            pDelta = &*toPool1;
        }
        auto toPool { [&]() -> std::optional<NonzeroDelta_uint64> {
            return Funds_uint64 { pDelta->amount }.nonzero().transform([&](NonzeroFunds_uint64 nz) {
                return NonzeroDelta_uint64(pDelta->isQuote, nz);
            });
        }() };
        return { .toPool { toPool }, .filled { in } };
    };
    std::optional<Delta_uint64> toPool0;
    std::optional<Delta_uint64> toPool1;
};
}
