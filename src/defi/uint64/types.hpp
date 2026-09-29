#pragma once
#include "defi/uint64/price.hpp"
#include "general/funds.hpp"
#include "price.hpp"
namespace defi {
struct BaseQuote_uint64;
struct Delta_uint64 {
    bool operator==(const Delta_uint64&) const = default;
    bool isQuote { false };
    Funds_uint64 amount;
    BaseQuote_uint64 base_quote() const;
};
struct NonzeroDelta_uint64 {
    explicit NonzeroDelta_uint64(bool isQuote, NonzeroFunds_uint64 amount)
        : isQuote_(std::move(isQuote))
        , amount_(std::move(amount))
    {
    }
    Delta_uint64 get() const { return { isQuote_, amount_ }; }
    bool is_quote() const { return isQuote_; }
    auto amount() const { return amount_; }
    bool operator==(const NonzeroDelta_uint64&) const = default;

private:
    bool isQuote_ { false };
    NonzeroFunds_uint64 amount_;
};
struct BaseQuote_uint64 {
    Funds_uint64 base;
    Funds_uint64 quote;
    BaseQuote_uint64(Funds_uint64 base, Funds_uint64 quote)
        : base(base)
        , quote(quote)
    {
    }
    BaseQuote_uint64(Reader& r)
        : base(r)
        , quote(r) { };
    bool operator==(const BaseQuote_uint64&) const = default;

    // Computes excess at specific price rounded towards 0.
    // For exactly no excess, isQuote = true
    Delta_uint64 excess(Price_uint64 p) const
    {
        auto q { multiply_ceil(base.value(), p) };
        if (q.has_value() && *q <= quote) // too much quote
            return { true, diff_assert(quote, Funds_uint64::from_value_throw(*q)) };
        // Here we know ceil(p * base) > quote but `quote` is an integer, so base * p > quote.
        // This implies so p != 0 and quote / p < base.

        auto b { divide_ceil(quote.value(), p) };
        assert(b.has_value()); // cannot overflow since quote / p < base
        assert(*b <= base); // ceil(quote / p) <= base because quote / p < base and base is an integer.
        return { false, diff_assert(base, Funds_uint64::from_value_throw(*b)) };
    }
    [[nodiscard]] friend BaseQuote_uint64 diff_assert(const BaseQuote_uint64& lhs, const BaseQuote_uint64& rhs)
    {
        return { diff_assert(lhs.base, rhs.base), diff_assert(lhs.quote, rhs.quote) };
    }
    void subtract_assert(const BaseQuote_uint64& add)
    {
        *this = diff_assert(*this, add);
    }
    [[nodiscard]] friend BaseQuote_uint64 sum_assert(const BaseQuote_uint64& lhs, const BaseQuote_uint64& rhs)
    {
        return { sum_assert(lhs.base, rhs.base), sum_assert(lhs.quote, rhs.quote) };
    }
    void add_assert(const BaseQuote_uint64& add)
    {
        *this = sum_assert(*this, add);
    }
    auto price() const { return PriceRelative_uint64::from_fraction(quote.value(), base.value()); }

    double price_double(TokenDecimals baseDecimals, TokenDecimals quoteDecimals) const
    {
        return double(quote.value()) / double(base.value()) * std::pow(10.0, -(quoteDecimals.value() - baseDecimals.value()));
    }
};
inline BaseQuote_uint64 Delta_uint64::base_quote() const
{
    if (isQuote)
        return { 0, amount };
    return { amount, 0 };
}

struct FillResult_uint64 {
    std::optional<NonzeroDelta_uint64> toPool;
    BaseQuote_uint64 filled;
    bool empty() const { return !toPool.has_value() && filled.base.is_zero() && filled.quote.is_zero(); }
    bool operator==(const FillResult_uint64&) const = default;
};

using MatchResult_uint64 = FillResult_uint64;

struct Order_uint64 {
    Order_uint64(Reader& r)
        : amount(r)
        , limit(r) { };
    Order_uint64(Funds_uint64 amount, Price_uint64 limit)
        : amount(std::move(amount))
        , limit(std::move(limit)) { };
    Funds_uint64 amount;
    Price_uint64 limit;
};

}
