/**
 * Copyright (c) 2011-2019 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>
#include <bitcoin/consensus.hpp>
#include <boost/test/unit_test.hpp>

#include "script.hpp"
#include "test.hpp"

BOOST_AUTO_TEST_SUITE(consensus__script_verify)

using namespace libbitcoin::consensus;

// Test case derived from:
// github.com/libbitcoin/libbitcoin-explorer/wiki/How-to-Spend-Bitcoin
#define CONSENSUS_SCRIPT_VERIFY_TX \
    "01000000017d01943c40b7f3d8a00a2d62fa1d560bf739a2368c180615b0a7937c0e883e7c000000006b4830450221008f66d188c664a8088893ea4ddd9689024ea5593877753ecc1e9051ed58c15168022037109f0d06e6068b7447966f751de8474641ad2b15ec37f4a9d159b02af68174012103e208f5403383c77d5832a268c9f71480f6e7bfbdfa44904becacfad66163ea31ffffffff01c8af0000000000001976a91458b7a60f11a904feef35a639b6048de8dd4d9f1c88ac00000000"
#define CONSENSUS_SCRIPT_VERIFY_PREVOUT_SCRIPT \
    "76a914c564c740c6900b93afc9f1bdaef0a9d466adf6ee88ac"

// Test case derived from first witness tx:
#define CONSENSUS_SCRIPT_VERIFY_WITNESS_TX \
    "010000000001015836964079411659db5a4cfddd70e3f0de0261268f86c998a69a143f47c6c83800000000171600149445e8b825f1a17d5e091948545c90654096db68ffffffff02d8be04000000000017a91422c17a06117b40516f9826804800003562e834c98700000000000000004d6a4b424950313431205c6f2f2048656c6c6f20536567576974203a2d29206b656570206974207374726f6e6721204c4c415020426974636f696e20747769747465722e636f6d2f6b6873396e6502483045022100aaa281e0611ba0b5a2cd055f77e5594709d611ad1233e7096394f64ffe16f5b202207e2dcc9ef3a54c24471799ab99f6615847b21be2a6b4e0285918fd025597c5740121021ec0613f21c4e81c4b300426e5e5d30fa651f41e9993223adbe74dbe603c74fb00000000"
#define CONSENSUS_SCRIPT_VERIFY_WITNESS_PREVOUT_SCRIPT \
    "a914642bda298792901eb1b48f654dd7225d99e5e68c87"

// test helper
static verify_result test_verify(const std::string& transaction,
    const std::string& prevout_script, uint64_t value=0,
    uint32_t input_index=0, const uint32_t flags=verify_flags_p2sh,
    bool tx_size_hack=false)
{
    data_chunk tx, prevout;
    BOOST_REQUIRE(decode_base16(tx, transaction));
    BOOST_REQUIRE(decode_base16(prevout, prevout_script));

    if (tx_size_hack)
        tx.push_back(0x42);

    return verify_script(tx, { prevout, value }, input_index, flags);
}

// test helper
static verify_result test_verify_unsigned(const std::string& input_script,
    const std::string& prevout_script, const uint32_t flags)
{
    // Implement once we have vectors.
    static const stack witness;

    // Convert scripts from mnemonic to bytes.
    const auto input = mnemonic_to_data(input_script);
    const auto prevout = mnemonic_to_data(prevout_script);

    return verify_unsigned_script({ prevout, 0 }, input, witness, flags);
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__value_overflow__verify_prevout_value_overflow)
{
    data_chunk tx{ 0x42 }, prevout;
    BOOST_REQUIRE(decode_base16(prevout, CONSENSUS_SCRIPT_VERIFY_PREVOUT_SCRIPT));
    BOOST_REQUIRE(verify_script(tx, { prevout, 0xffffffffffffffff }, 0, 0) == verify_value_overflow);
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__invalid_tx__tx_invalid)
{
    const verify_result result = test_verify("42", "42");
    BOOST_REQUIRE_EQUAL(result, verify_result_tx_invalid);
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__invalid_input__tx_input_invalid)
{
    const verify_result result = test_verify(CONSENSUS_SCRIPT_VERIFY_TX, CONSENSUS_SCRIPT_VERIFY_PREVOUT_SCRIPT, 0, 1);
    BOOST_REQUIRE_EQUAL(result, verify_result_tx_input_invalid);
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__oversized_tx__tx_size_invalid)
{
#ifndef NDEBUG
    const verify_result result = test_verify(CONSENSUS_SCRIPT_VERIFY_TX, CONSENSUS_SCRIPT_VERIFY_PREVOUT_SCRIPT, 0, 0, verify_flags_p2sh, +1);
    BOOST_REQUIRE_EQUAL(result, verify_result_tx_size_invalid);
#endif // NDEBUG
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__incorrect_pubkey_hash__equalverify)
{
    const verify_result result = test_verify(CONSENSUS_SCRIPT_VERIFY_TX, "76a914c564c740c6900b93afc9f1bdaef0a9d466adf6ef88ac");
    BOOST_REQUIRE_EQUAL(result, verify_result_equalverify);
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__valid__true)
{
    const verify_result result = test_verify(CONSENSUS_SCRIPT_VERIFY_TX, CONSENSUS_SCRIPT_VERIFY_PREVOUT_SCRIPT);
    BOOST_REQUIRE_EQUAL(result, verify_result_eval_true);
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__valid_nested_p2wpkh__true)
{
    static const auto index = 0u;
    static const auto value = 500000u;
    static const uint32_t flags =
        verify_flags_p2sh |
        verify_flags_dersig |
        verify_flags_nulldummy |
        verify_flags_checklocktimeverify |
        verify_flags_checksequenceverify |
        verify_flags_witness;

    const verify_result result = test_verify(CONSENSUS_SCRIPT_VERIFY_WITNESS_TX, CONSENSUS_SCRIPT_VERIFY_WITNESS_PREVOUT_SCRIPT, value, index, flags);
    BOOST_REQUIRE_EQUAL(result, verify_result_eval_true);
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__bip16__valid)
{
    for (const auto& test: valid_bip16_scripts)
    {
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) == verify_result_eval_true, test.description);
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_p2sh) == verify_result_eval_true, test.description);
    }
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__bip16__invalidated)
{
    for (const auto& test: invalidated_bip16_scripts)
    {
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) == verify_result_eval_true, test.description);
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_p2sh) != verify_result_eval_true, test.description);
    }
}

////// Requires tx test for locktime values (set on vectors, see libbitcoin tests).
////BOOST_AUTO_TEST_CASE(consensus__script_verify__bip65__valid)
////{
////    for (const auto& test: valid_bip65_scripts)
////    {
////        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) == verify_result_eval_true, test.description);
////        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_checklocktimeverify) == verify_result_eval_true, test.description);
////    }
////}
////
////// Requires tx test for locktime values (set on vectors, see libbitcoin tests).
////BOOST_AUTO_TEST_CASE(consensus__script_verify__bip65__invalid)
////{
////    for (const auto& test: invalid_bip65_scripts)
////    {
////        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) != verify_result_eval_true, test.description);
////        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_checklocktimeverify) != verify_result_eval_true, test.description);
////    }
////}
////
////// Requires tx test for locktime values (set on vectors, see libbitcoin tests).
////BOOST_AUTO_TEST_CASE(consensus__script_verify__bip65__invalidated)
////{
////    for (const auto& test: invalidated_bip65_scripts)
////    {
////        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) != verify_result_eval_true, test.description);
////        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_checklocktimeverify) != verify_result_eval_true, test.description);
////    }
////}

BOOST_AUTO_TEST_CASE(consensus__script_verify__multisig__valid)
{
    for (const auto& test: valid_multisig_scripts)
    {
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) == verify_result_eval_true, test.description);
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_dersig) == verify_result_eval_true, test.description);
    }
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__multisig__invalid)
{
    for (const auto& test: invalid_multisig_scripts)
    {
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) != verify_result_eval_true, test.description);
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_dersig) != verify_result_eval_true, test.description);
    }
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__context_free__valid)
{
    for (const auto& test: valid_context_free_scripts)
    {
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) == verify_result_eval_true, test.description);
    }
}

BOOST_AUTO_TEST_CASE(consensus__script_verify__context_free__invalid)
{
    for (const auto& test: invalid_context_free_scripts)
    {
        BOOST_CHECK_MESSAGE(test_verify_unsigned(test.input, test.output, verify_flags_none) != verify_result_eval_true, test.description);
    }
}

BOOST_AUTO_TEST_CASE(script__parse__not_invalid)
{
    for (const auto& test: not_invalid_parse_scripts)
    {
        mnemonic_to_data(test.input, true);
        mnemonic_to_data(test.output, true);
    }
}

BOOST_AUTO_TEST_CASE(script__parse_syntax__invalid_input)
{
    for (const auto& test: invalid_syntax_scripts)
    {
        mnemonic_to_data(test.input, false);
        mnemonic_to_data(test.output, true);
    }
}

BOOST_AUTO_TEST_CASE(script__parse_push_not_overflow__valid)
{
    for (const auto& test: valid_push_data_scripts)
    {
        mnemonic_to_data(test.input, true);
        mnemonic_to_data(test.output, true);
    }
}

BOOST_AUTO_TEST_CASE(script__parse_push_overflow__invalid_input)
{
    for (const auto& test: invalid_overflowed_push_data_scripts)
    {
        mnemonic_to_data(test.input, false);
        mnemonic_to_data(test.output, true);
    }
}

BOOST_AUTO_TEST_SUITE_END()
