#include "orderbook.hpp"
#include "defi/uint64/matcher.hpp"
#include "pool.hpp"

namespace defi {
namespace {
struct PreparedExtradata {
    struct ExtraData {
        uint64_t cumsum;
        size_t upperBoundCounterpart;
    };
    PreparedExtradata(const Orderbook_uint64& b)
    {
        extraBase.resize(0);
        extraQuote.resize(0);
        const size_t J { b.base_asc_sell().size() };
        const size_t I { b.quote_desc_buy().size() };
        uint64_t cumsumBase { b.base_asc_sell().total_push().value() };
        uint64_t cumsumQuote { 0 };
        size_t j { 0 };
        extraBase.resize(0);
        for (size_t i = 0; i < I; ++i) {
            auto& oq { b.quote_desc_buy()[i] };
            for (; j < J; ++j) {
                auto& ob { b.base_asc_sell()[J - 1 - j] };
                if (ob.limit <= oq.limit)
                    break;
                extraBase.push_back({ cumsumBase, i });
                cumsumBase -= ob.amount.value();
            }
            extraQuote.push_back({ cumsumQuote, j });
            cumsumQuote += oq.amount.value();
        }
        for (; j < J; ++j) {
            extraBase.push_back({ cumsumBase, I });
            cumsumBase -= b.base_asc_sell()[J - 1 - j].amount.value();
        }
    }
    std::vector<ExtraData> extraQuote;
    std::vector<ExtraData> extraBase;
};
}
auto Orderbook_uint64::match(const PoolLiquidity_uint64& p) const
    -> MatchResult_uint64
{
    PreparedExtradata prepared { *this };
    const auto& extraQuote { prepared.extraQuote };
    const auto& extraBase { prepared.extraBase };
    const size_t I { pushQuoteDesc.size() };
    const size_t J { pushBaseAsc.size() };
    Matcher m { p };

    // In the first step (A) we bisect on the price points defined by the orders. To this end
    // we first (A1) bisect on buy order prices and then (A2) bisect on sell order prices.
    // The applied fill amount during this process is always that on the right/upper side of
    // the price. This means, a buy order (pays quote currency) at the current price is
    // considered completely unfilled, whereas a sell order at current price (pays base currency)
    // is considered completely filled.

    // The following lambda `bisect_j` is for (A2):
    auto bisect_j = [&](size_t j0, size_t j1) -> MatchResult_uint64 {
        while (j0 != j1) {
            auto j { (j0 + j1) / 2 };
            m.in.base = extraBase[j].cumsum;
            if (m.bisection_step(pushBaseAsc[J - 1 - j].limit))
                j0 = j + 1;
            else
                j1 = j;
        }
        if (j1 == 0) {
            // In this case either we don't have any sell orders (J == 0) or there is
            // an open interval between the highest sell order price and the next buy
            // order price (or \infty) which does not contain other order prices and
            // must contain the equilibrium price.
            // So we can bisect on price with fixed `in` matched order volume to
            // decide the portion of quote or base that interacts with the pool.
            return m.bisect_dynamic_price();
        } else {
            auto j { j1 - 1 };
            // The index j is the highest index in domain specified in call args where
            // bisection_step returned true.
            // Until now the corresponding sell order was considered completely filled.
            // We now make it completely unfilled to test if this makes bisection_step
            // return false.
            m.in.base = extraBase[j].cumsum - pushBaseAsc[J - 1 - j].amount.value();
            auto price { pushBaseAsc[J - 1 - j].limit };
            if (m.bisection_step(price)) { // Still returns true.
                // At this point we have already seen both bounds of the current section
                // (if it has bounds) defined by adjacent order price levels (buy or sell)
                // and we know we must bisect on price with fixed `in` order volume.
                return m.bisect_dynamic_price();
            } else { // Changes return value to false
                // So we need to bisect on the fill amount of the current order
                return m.bisect_fixed_price(false, extraBase[j].cumsum, m.in.base, price);
            }
        }
    };

    size_t i0 { 0 };
    size_t i1 { I };
    // The following bisection is for (A1).
    while (i0 != i1) {
        auto i { (i0 + i1) / 2 };
        auto& eq { extraQuote[i] };
        auto j { eq.upperBoundCounterpart };
        m.in.base = (j == J ? 0 : extraBase[j].cumsum);
        m.in.quote = eq.cumsum;
        if (m.bisection_step(pushQuoteDesc[i].limit))
            i0 = i + 1;
        else
            i1 = i;
    }
    if (i1 == 0) {
        // Either there were no buy orders (I == 0) or the equilibrium price is in the
        // open interval (p, \infty), where p is the highest buy price. In both cases no
        // buy orders are filled.
        size_t j0 = 0;
        // If there are no buy orders, we search over all sell orders (j1 = J).
        // Otherwise we restrict j in {j0, ... , j1 - 1 } to correspond to sell orders
        // with price in the open interval (p, \infty), where we know the equilibrium is
        // in.
        size_t j1 = (I == 0 ? J : extraQuote[0].upperBoundCounterpart);
        m.in.quote = 0; // We know that no buy orders are filled in this interval.
        return bisect_j(j0, j1);
    } else {
        auto i { i1 - 1 };
        // The index i is the highest index in {0, ... , I - 1} where bisection_step
        // returned true.
        auto& eq { extraQuote[i] };
        auto price { pushQuoteDesc[i].limit };
        auto j { eq.upperBoundCounterpart };
        m.in.base = (j == J ? 0 : extraBase[j].cumsum);
        // Make quote order filled on that price.
        m.in.quote = eq.cumsum + pushQuoteDesc[i].amount.value();
        size_t j0 = extraQuote[i].upperBoundCounterpart;
        // We check if making quote order completely filled changed bisection_step to return false
        if (m.bisection_step(price)) {
            // No ->
            // 1. The equilibrium price is either in the open interval defined by the current and
            //    next lower buy order prices
            // 2. Or, if we have a sell order at current price (which is considered completely filled
            //    right now), we must check whether making it unfilled changes bisection_step to
            //    return false.

            // Find upper bound for index of sell orders with price in the open interval to check next.
            size_t j1 = (i1 < I ? extraQuote[i1].upperBoundCounterpart : J);
            return bisect_j(j0, j1); // This lambda checks 1. and 2.
        } else { // Yes -> So we need to bisect on the fill amount of the current order
            return m.bisect_fixed_price(true, eq.cumsum, m.in.quote, price);
        }
    }
}
} // namespace defi
