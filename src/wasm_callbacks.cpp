#include "defi/uint64/orderbook.hpp"
#include "general/funds.hpp"
#include "nlohmann/json.hpp"
#include <emscripten.h>
#include <stdio.h>
using namespace std;

// global variables
std::string returnString;
std::optional<Funds_uint64> poolToken;
std::optional<Funds_uint64> poolWart;
defi::Orderbook_uint64 bso;
uint32_t feeE4 { 5 };
TokenDecimals baseDecimals { 3 };

using json = nlohmann::json;
json pool_json(const defi::PoolLiquidity_uint64& pool)
{
    auto baseTotal { pool.base.to_decimal(baseDecimals) };
    auto quoteTotal { pool.quote.as_wart() };
    // double base
    return { { "base", baseTotal.to_string() },
        { "quote", quoteTotal.to_string() },
        { "price",
            quoteTotal.to_double() / baseTotal.to_double() } };
}

json match_result()
{
    json errors {
        { "poolToken", !poolToken.has_value() },
        { "poolWart", !poolWart.has_value() },
    };
    if (!poolToken || !poolWart)
        return { { "parseErrors", errors } };

    const defi::PoolLiquidity_uint64 p { *poolToken, *poolWart };
    auto pTmp { p };
    auto match_res { bso.match(p) };
    json buys(json::array());

    auto fquote { match_res.filled.quote };
    for (size_t i = 0; i < bso.quote_desc_buy().size(); ++i) {
        auto order { bso.quote_desc_buy()[i] };
        auto filled { std::min(order.amount, fquote) };
        fquote.subtract_assert(filled);
        buys.push_back({ { "amount", order.amount.as_wart().to_string() },
            { "filled", filled.as_wart().to_string() },
            { "limit", order.limit.to_double_adjusted(baseDecimals) } });
    }

    json sells(json::array());
    auto J { bso.base_asc_sell().size() };
    auto fbase { match_res.filled.base };
    for (size_t j = 0; j < J; ++j) {
        auto order { bso.base_asc_sell()[j] };
        auto filled { std::min(fbase, order.amount) };
        fbase.subtract_assert(filled);
        sells.push_back({ { "amount", order.amount.to_decimal(baseDecimals).to_string() },
            { "filled", filled.to_decimal(baseDecimals).to_string() },
            { "limit", order.limit.to_double_adjusted(baseDecimals) } });
    }
    std::reverse(sells.begin(), sells.end());

    auto json_price { [](const defi::BaseQuote_uint64& bq) -> json {
        return json(bq.price_double(baseDecimals, TokenDecimals::WART.value()));
    } };

    const auto& toPool { match_res.toPool };
    auto poolBaseQuote { [&]() -> defi::BaseQuote_uint64 {
        if (toPool) {
            if (toPool->is_quote())
                return {
                    pTmp.buy(toPool->amount(), feeE4),
                    toPool->amount()
                };
            else {
                return {
                    toPool->amount(),
                    pTmp.sell(toPool->amount(), feeE4),
                };
            }
        }
        return { 0, 0 };
    }() };

    auto matched { match_res.filled };
    if (auto& toPool { match_res.toPool }) {
        if (toPool->is_quote()) {
            matched.quote.subtract_assert(toPool->amount());
        } else {
            matched.base.subtract_assert(toPool->amount());
        }
    }

    auto toPoolJson = [&]() -> json {
        if (toPool) {
            return { { "isQuote", toPool->is_quote() },
                { "base", poolBaseQuote.base.to_decimal(baseDecimals).to_string() },
                { "quote", poolBaseQuote.quote.as_wart().to_string() },
                { "price", json_price(poolBaseQuote) } };
        };
        return nullptr;
    };

    auto filledBuyer { matched };
    auto filledSeller { matched };
    if (toPool) {
        if (toPool->is_quote()) {
            filledBuyer.add_assert(poolBaseQuote);
        } else {
            filledSeller.add_assert(poolBaseQuote);
        }
    }

    return json { { "parseErrors", errors },
        { "match",
            { { "buys", buys },
                { "sells", sells },
                { "poolBefore", pool_json(p) },
                { "toPool", toPoolJson() },
                { "filled",
                    {
                        { "outBaseSeller", filledSeller.base.to_decimal(baseDecimals).to_string() },
                        { "inQuoteSeller", filledSeller.quote.as_wart().to_string() },
                        { "priceSeller", json_price(filledSeller) },
                        { "outQuoteBuyer", filledBuyer.quote.as_wart().to_string() },
                        { "inBaseBuyer", filledBuyer.base.to_decimal(baseDecimals).to_string() },
                        { "priceBuyer", json_price(filledBuyer) },
                    } },
                { "matched",
                    { { "base", matched.base.to_decimal(baseDecimals).to_string() },
                        { "quote", matched.quote.as_wart().to_string() },
                        { "price", matched.quote.is_zero() ? json(nullptr) : json_price(matched) } } },
                { "poolAfter", pool_json(pTmp) } } } };
}

template <typename callable>
requires std::is_invocable_r_v<json, callable, json>
const char* wrap_fun(const callable& fun, const char* c)
{
    returnString = [&]() {
        try {
            return fun(json::parse(std::string_view(c))).dump();
        } catch (std::runtime_error& e) {
            return json { { "error", e.what() } }.dump();
        }
    }();
    return returnString.c_str();
}

defi::Order_uint64 parse_order(json j, TokenDecimals decimals)
{
    auto price { [&]() {
        try {
            return Price_uint64::from_string(j["price"].get<std::string>()).value();
        } catch (...) {
            throw std::runtime_error("Cannot get price");
        }
    }() };
    auto amount { [&]() {
        try {
            std::string s { j["amount"].get<std::string>() };
            if (auto o { Funds_uint64::parse(s, decimals) })
                return *o;

        } catch (...) {
        }
        throw std::runtime_error("Cannot parse amount");
    }() };
    return { amount, price };
}

json edit_pool(json j)
{
    try {
        poolToken = Funds_uint64::parse(j["token"].get<std::string>(), baseDecimals);
    } catch (...) {
        poolToken.reset();
    }
    try {
        poolWart = Wart::try_parse(j["wart"].get<std::string>()).value_or_null();
    } catch (...) {
        poolWart.reset();
    }
    return match_result();
}

json delete_order(json j)
{
    try {
        bool base = j["base"].get<bool>();
        auto i = j["index"].get<size_t>();
        if (base)
            bso.delete_base(i);
        else
            bso.delete_quote(i);
    } catch (...) {
    }
    return match_result();
}

json add_buy(json j)
{
    auto order { parse_order(j, TokenDecimals::WART) };
    bso.insert_quote(order);
    return match_result();
}

json add_sell(json j)
{
    auto order { parse_order(j, baseDecimals) };
    bso.insert_base(order);
    return match_result();
}

json set_fee(json j)
{
    try {
        auto e4 { j["E4"].get<int>() };
        if (e4 >= 10000) {
            throw std::runtime_error("Fee value must be denoted as multiple of 0.0001 i.e. as integer in 0...9999.");
        }
        feeE4 = e4;
    } catch (...) {
        throw std::runtime_error("Can't extract integer at \'E4\' key.");
    }
    return match_result();
}

json clear_and_set_base_decimals(json j)
{
    auto iter { j.find("baseDecimals") };
    if (iter != j.end()) {
        try {
            auto d { iter->get<int>() };
            if (d > 0 && d < 255) { // can convert to uint8_t
                if (true) {
                    Result<TokenDecimals> td { TokenDecimals::from_number(d) };
                    if (td.has_value()) {
                        baseDecimals = td.value();
                        goto extracted;
                    }
                }
            }
        } catch (...) {
        };
        throw std::runtime_error("Cannot extract decimals at key 'baseDecimals'");
    }
extracted:
    poolToken.reset();
    poolWart.reset();
    bso.clear();
    return match_result();
}

extern "C" {
EMSCRIPTEN_KEEPALIVE
const char* addBuy(const char* json) { return wrap_fun(add_buy, json); }

EMSCRIPTEN_KEEPALIVE
const char* addSell(const char* json) { return wrap_fun(add_sell, json); }

EMSCRIPTEN_KEEPALIVE
const char* editPool(const char* json) { return wrap_fun(edit_pool, json); }

EMSCRIPTEN_KEEPALIVE
const char* deleteOrder(const char* json) { return wrap_fun(delete_order, json); }

EMSCRIPTEN_KEEPALIVE
const char* setFee(const char* json) { return wrap_fun(set_fee, json); }

EMSCRIPTEN_KEEPALIVE
const char* clearAndSetBaseDecimals(const char* json) { return wrap_fun(clear_and_set_base_decimals, json); }
}
