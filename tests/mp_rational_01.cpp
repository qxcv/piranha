/* Copyright 2009-2016 Francesco Biscani (bluescarni@gmail.com)

This file is part of the Piranha library.

The Piranha library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 3 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The Piranha library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the Piranha library.  If not,
see https://www.gnu.org/licenses/. */

#include "../src/mp_rational.hpp"

#define BOOST_TEST_MODULE mp_rational_01_test
#include <boost/test/included/unit_test.hpp>

#define FUSION_MAX_VECTOR_SIZE 20

#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/include/algorithm.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/lexical_cast.hpp>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "../src/binomial.hpp"
#include "../src/config.hpp"
#include "../src/detail/gmp.hpp"
#include "../src/exceptions.hpp"
#include "../src/init.hpp"
#include "../src/math.hpp"
#include "../src/mp_integer.hpp"
#include "../src/pow.hpp"
#include "../src/print_tex_coefficient.hpp"
#include "../src/s11n.hpp"
#include "../src/safe_cast.hpp"
#include "../src/type_traits.hpp"

static std::mt19937 rng;
static const int ntries = 1000;

using namespace piranha;

using size_types = boost::mpl::vector<std::integral_constant<int, 0>, std::integral_constant<int, 8>,
                                      std::integral_constant<int, 16>, std::integral_constant<int, 32>
#if defined(PIRANHA_UINT128_T)
                                      ,
                                      std::integral_constant<int, 64>
#endif
                                      >;

using mpq_struct_t = std::remove_extent<::mpq_t>::type;

// Simple RAII holder for GMP rationals.
struct mpq_raii {
    mpq_raii()
    {
        ::mpq_init(&m_mpq);
        piranha_assert(mpq_numref(&m_mpq)->_mp_alloc >= 0);
        piranha_assert(mpq_denref(&m_mpq)->_mp_alloc >= 0);
    }
    mpq_raii(const mpq_raii &) = delete;
    mpq_raii(mpq_raii &&) = delete;
    mpq_raii &operator=(const mpq_raii &) = delete;
    mpq_raii &operator=(mpq_raii &&) = delete;
    ~mpq_raii()
    {
        if (mpq_numref(&m_mpq)->_mp_d != nullptr) {
            ::mpq_clear(&m_mpq);
        }
    }
    mpq_struct_t m_mpq;
};

static inline std::string mpq_lexcast(const mpq_raii &m)
{
    std::ostringstream os;
    const std::size_t size_base10_num = ::mpz_sizeinbase(mpq_numref(&m.m_mpq), 10),
                      size_base10_den = ::mpz_sizeinbase(mpq_denref(&m.m_mpq), 10),
                      max = std::numeric_limits<std::size_t>::max();
    if (size_base10_den > max - 3u || size_base10_num > max - (size_base10_den + 3u)) {
        piranha_throw(std::overflow_error, "number of digits is too large");
    }
    const auto total_size = size_base10_num + (size_base10_den + 3u);
    std::vector<char> tmp;
    tmp.resize(static_cast<std::vector<char>::size_type>(total_size));
    if (unlikely(tmp.size() != total_size)) {
        piranha_throw(std::overflow_error, "number of digits is too large");
    }
    os << ::mpq_get_str(&tmp[0u], 10, &m.m_mpq);
    return os.str();
}

// Constructors and assignments.
struct constructor_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        std::cout << "NBits,size,align: " << T::value << ',' << sizeof(q_type) << ',' << alignof(q_type) << '\n';
        q_type q;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q), "0");
        BOOST_CHECK_EQUAL(q.num(), 0);
        BOOST_CHECK_EQUAL(q.den(), 1);
        BOOST_CHECK(q.is_canonical());
        q_type q0(0, 1);
        BOOST_CHECK_EQUAL(q0.num(), 0);
        BOOST_CHECK_EQUAL(q0.den(), 1);
        BOOST_CHECK(q0.is_canonical());
        q_type q1(0, 2);
        BOOST_CHECK_EQUAL(q1.num(), 0);
        BOOST_CHECK_EQUAL(q1.den(), 1);
        BOOST_CHECK(q1.is_canonical());
        q_type q2(0, -3);
        BOOST_CHECK_EQUAL(q2.num(), 0);
        BOOST_CHECK_EQUAL(q2.den(), 1);
        BOOST_CHECK(q2.is_canonical());
        BOOST_CHECK_THROW((q_type{1, 0}), zero_division_error);
        BOOST_CHECK_THROW((q_type{-1, 0}), zero_division_error);
        BOOST_CHECK((std::is_constructible<q_type, int, int>::value));
        BOOST_CHECK((std::is_constructible<q_type, int, unsigned>::value));
        BOOST_CHECK((std::is_constructible<q_type, char, long>::value));
        BOOST_CHECK((std::is_constructible<q_type, int_type, long>::value));
        BOOST_CHECK(!(std::is_constructible<q_type, double, long>::value));
        BOOST_CHECK(!(std::is_constructible<q_type, float, double>::value));
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{2, 4}), "1/2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{-2, 4}), "-1/2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{-2, -4}), "1/2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{2, -4}), "-1/2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{4, -2}), "-2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{0, -2}), "0");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{0, -10}), "0");
        // Random testing.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        mpq_raii m;
        detail::mpz_raii z;
        for (int i = 0; i < ntries; ++i) {
            const int a = dist(rng), b = dist(rng);
            if (b == 0) {
                continue;
            }
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m.m_mpq);
            q_type tmp_q{a, b};
            BOOST_CHECK_EQUAL(mpq_lexcast(m), boost::lexical_cast<std::string>(tmp_q));
            // Check that it is canonical and that canonicalisation doest not change the value.
            BOOST_CHECK(tmp_q.is_canonical());
            q_type copy = tmp_q;
            copy.canonicalise();
            BOOST_CHECK_EQUAL(mpq_lexcast(m), boost::lexical_cast<std::string>(copy));
        }
        // Copy/move ctor/assignment.
        q_type q3(1, -15);
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q3), boost::lexical_cast<std::string>(q_type(q3)));
        q3._num() *= std::numeric_limits<int>::max();
        q3.canonicalise();
        auto q5(q3);
        q_type q4(std::move(q3));
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q4), boost::lexical_cast<std::string>(q_type(q5)));
        q_type q6;
        q6 = q5;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q6), boost::lexical_cast<std::string>(q_type(q5)));
        q6 = std::move(q5);
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q6), boost::lexical_cast<std::string>(q_type(q4)));
        // Move assignment to self.
        q6 = std::move(q6);
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q6), boost::lexical_cast<std::string>(q_type(q4)));
        // Construction from interoperable types.
        q_type q7{7};
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q7), "7");
        q_type q8{-8l};
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q8), "-8");
        q_type q9{100ull};
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q9), "100");
        q_type q10{(signed char)(-3)};
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q10), "-3");
        for (int i = 0; i < ntries; ++i) {
            const int tmp = dist(rng);
            q_type tmp_q{tmp};
            ::mpq_set_si(&m.m_mpq, static_cast<long>(tmp), 1ul);
            BOOST_CHECK_EQUAL(mpq_lexcast(m), boost::lexical_cast<std::string>(tmp_q));
        }
        // Floating-point.
        const unsigned dradix = static_cast<unsigned>(std::numeric_limits<double>::radix);
        {
            double tmp(1);
            tmp /= dradix;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{tmp}),
                              std::string("1/") + boost::lexical_cast<std::string>(dradix));
            tmp /= dradix;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{tmp}),
                              std::string("1/") + boost::lexical_cast<std::string>(int_type(dradix).pow(2)));
            tmp = 0.;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{tmp}), "0");
            const unsigned ldradix = static_cast<unsigned>(std::numeric_limits<long double>::radix);
            long double ltmp(1);
            ltmp /= ldradix;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{ltmp}),
                              std::string("1/") + boost::lexical_cast<std::string>(ldradix));
            ltmp /= ldradix;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{ltmp}),
                              std::string("1/") + boost::lexical_cast<std::string>(int_type(ldradix).pow(2)));
            ltmp = 0.;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{ltmp}), "0");
            // This was a bug: we did not fix the sign when the input was a negative integral value.
            BOOST_CHECK_EQUAL(q_type{-1.}, -1);
            BOOST_CHECK_EQUAL(q_type{-2.}, -2);
            BOOST_CHECK_EQUAL(q_type{1.}, 1);
            BOOST_CHECK_EQUAL(q_type{2.}, 2);
        }
        // Random testing.
        std::uniform_real_distribution<double> ddist(0, std::numeric_limits<double>::max());
        for (int i = 0; i < ntries / 10; ++i) {
            const double tmp = ddist(rng);
            q_type tmp_q{tmp};
            ::mpq_set_d(&m.m_mpq, tmp);
            BOOST_CHECK_EQUAL(mpq_lexcast(m), boost::lexical_cast<std::string>(tmp_q));
        }
        std::uniform_real_distribution<double> ddist2(-5, 5);
        for (int i = 0; i < ntries / 10; ++i) {
            const double tmp = ddist2(rng);
            q_type tmp_q{tmp};
            ::mpq_set_d(&m.m_mpq, tmp);
            BOOST_CHECK_EQUAL(mpq_lexcast(m), boost::lexical_cast<std::string>(tmp_q));
        }
        if (std::numeric_limits<double>::has_infinity) {
            BOOST_CHECK_THROW(q_type{std::numeric_limits<double>::infinity()}, std::invalid_argument);
        }
        if (std::numeric_limits<double>::has_quiet_NaN) {
            BOOST_CHECK_THROW(q_type{std::numeric_limits<double>::quiet_NaN()}, std::invalid_argument);
        }
        // Constructor from string.
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{"0"}), "0");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{"1"}), "1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{"2/-1"}), "-2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{"-4/-2"}), "2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{"-432132131123/-289938282"}),
                          boost::lexical_cast<std::string>(q_type{int_type{"-432132131123"}, int_type{"-289938282"}}));
        BOOST_CHECK_THROW(q_type{"-/-2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"/2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3 /2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/ 2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/2 "}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"+3/2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/+2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3a/2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/2 1"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/2a1"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/02"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"03/2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/5/2"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"3/0"}, zero_division_error);
        BOOST_CHECK_THROW(q_type{"0/0"}, zero_division_error);
        BOOST_CHECK_THROW(q_type{"3/-0"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"0/-0"}, std::invalid_argument);
        BOOST_CHECK_THROW(q_type{"-0/-0"}, std::invalid_argument);
        // Random testing.
        for (int i = 0; i < ntries; ++i) {
            const int tmp_num = dist(rng), tmp_den = dist(rng);
            if (tmp_den == 0) {
                continue;
            }
            q_type tmp_q{boost::lexical_cast<std::string>(tmp_num) + "/" + boost::lexical_cast<std::string>(tmp_den)};
            ::mpz_set_si(mpq_numref(&m.m_mpq), static_cast<long>(tmp_num));
            ::mpz_set_si(mpq_denref(&m.m_mpq), static_cast<long>(tmp_den));
            ::mpq_canonicalize(&m.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m), boost::lexical_cast<std::string>(tmp_q));
        }
        // Generic assignment.
        q_type q11;
        q11 = 0;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "0");
        q11 = 1ull;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "1");
        q11 = -short(3);
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "-3");
        BOOST_CHECK((std::is_same<decltype(q11 = static_cast<signed char>(-3)), q_type &>::value));
        q11 = int_type{45};
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "45");
        q11 = 1. / dradix;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11),
                          std::string("1/") + boost::lexical_cast<std::string>(dradix));
        q11 = 1. / -static_cast<int>(dradix);
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11),
                          std::string("-1/") + boost::lexical_cast<std::string>(dradix));
        if (std::numeric_limits<float>::has_infinity) {
            BOOST_CHECK_THROW((q11 = std::numeric_limits<float>::infinity()), std::invalid_argument);
        }
        // Assignment from string.
        q11 = "0";
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "0");
        q11 = std::string("0");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "0");
        q11 = "-2";
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "-2");
        q11 = std::string("-2");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "-2");
        q11 = "-2/-10";
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "1/5");
        q11 = std::string("-2/-10");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "1/5");
        q11 = "0/-10";
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "0");
        q11 = std::string("0/-10");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "0");
        q11 = std::string("3/4");
        BOOST_CHECK_THROW((q11 = std::string("1/0")), zero_division_error);
        BOOST_CHECK_THROW((q11 = std::string("1/-0")), std::invalid_argument);
        BOOST_CHECK_THROW((q11 = std::string("-0/4")), std::invalid_argument);
        BOOST_CHECK_THROW((q11 = std::string("0/0")), zero_division_error);
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q11), "3/4");
        BOOST_CHECK((std::is_same<q_type &, decltype(q11 = std::string("1/0"))>::value));
        BOOST_CHECK((std::is_same<q_type &, decltype(q11 = "1/0")>::value));
        // Check move semantics.
        // A largish rational that forces to use GMP integers, at least on some platforms. The move operation
        // of a GMP int will reset the original value to zero, hence we need to take care of the denominator going to
        // zero.
        q_type tmp00{"231238129381029380129830980980198109890381238120398190/"
                     "2312093812093812903809128310298301928309128390128390128390128390183"};
        q11 = std::move(tmp00);
        BOOST_CHECK_EQUAL(tmp00.den(), 1);
        BOOST_CHECK(tmp00.is_canonical());
        auto q12(std::move(q11));
        BOOST_CHECK_EQUAL(q11.den(), 1);
        BOOST_CHECK(q11.is_canonical());
        // Constructor from GMP mpq.
        BOOST_CHECK((std::is_constructible<q_type, ::mpq_t>::value));
        mpq_raii gmp_q;
        ::mpq_set_si(&m.m_mpq, 1, 2);
        BOOST_CHECK_EQUAL((q_type{&m.m_mpq}), (q_type{1, 2}));
        ::mpq_set_si(&m.m_mpq, -3, 7);
        BOOST_CHECK_EQUAL((q_type{&m.m_mpq}), (q_type{-3, 7}));
        ::mpq_set_si(&m.m_mpq, -3, 2);
        BOOST_CHECK_EQUAL((q_type{&m.m_mpq}), (q_type{-3, 2}));
        ::mpq_set_si(&m.m_mpq, 0, 1);
        BOOST_CHECK_EQUAL((q_type{&m.m_mpq}), 0);
        ::mpq_set_si(&m.m_mpq, 1, 0);
        BOOST_CHECK_THROW(q_type{&m.m_mpq}, zero_division_error);
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_constructor_test)
{
    init();
    boost::mpl::for_each<size_types>(constructor_tester());
    // Test we are not gobbling in mp_ints with different bit widths.
    BOOST_CHECK((!std::is_constructible<mp_rational<16>, mp_integer<32>>::value));
    BOOST_CHECK((!std::is_constructible<mp_rational<32>, mp_integer<16>>::value));
}

struct ll_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        q_type q;
        q._num() = int_type{3};
        BOOST_CHECK_EQUAL(q.num(), 3);
        BOOST_CHECK_EQUAL(q.den(), 1);
        q._den() = int_type{4};
        BOOST_CHECK_EQUAL(q.den(), 4);
        BOOST_CHECK(q.is_canonical());
        q._den() = int_type{6};
        BOOST_CHECK_EQUAL(q.den(), 6);
        BOOST_CHECK(!q.is_canonical());
        q._set_den(int_type{2});
        BOOST_CHECK_EQUAL(q.den(), 2);
        q._num() = int_type{4};
        BOOST_CHECK(!q.is_canonical());
        q.canonicalise();
        BOOST_CHECK(q.is_canonical());
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q), "2");
        q._num() = int_type{0};
        q._set_den(int_type{3});
        BOOST_CHECK(!q.is_canonical());
        q.canonicalise();
        BOOST_CHECK_EQUAL(q.num(), 0);
        BOOST_CHECK_EQUAL(q.den(), 1);
        BOOST_CHECK(q.is_canonical());
        BOOST_CHECK_THROW(q._set_den(int_type{0}), std::invalid_argument);
        BOOST_CHECK_THROW(q._set_den(int_type{-1}), std::invalid_argument);
        BOOST_CHECK_EQUAL(q.num(), 0);
        BOOST_CHECK_EQUAL(q.den(), 1);
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_low_level_test)
{
    boost::mpl::for_each<size_types>(ll_tester());
}

struct conversion_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        q_type q;
        BOOST_CHECK_EQUAL(static_cast<int>(q), 0);
        BOOST_CHECK_EQUAL(static_cast<signed char>(q), 0);
        BOOST_CHECK_EQUAL(static_cast<double>(q), 0.);
        q_type q1(-3, 5);
        BOOST_CHECK_EQUAL(static_cast<int>(q1), 0);
        BOOST_CHECK_EQUAL(static_cast<int_type>(q1), 0);
        BOOST_CHECK_EQUAL(static_cast<double>(q1), -3. / 5.);
        q_type q2(20, -5);
        BOOST_CHECK_EQUAL(static_cast<int>(q2), -4);
        BOOST_CHECK_EQUAL(static_cast<int_type>(q2), -4);
        BOOST_CHECK_EQUAL(static_cast<long double>(q2), 20.l / -5.l);
        q_type q3(std::numeric_limits<long long>::max());
        q3._num() += 1;
        BOOST_CHECK_THROW((void)static_cast<long long>(q3), std::overflow_error);
        if (std::numeric_limits<long double>::has_infinity) {
            q_type q4(std::numeric_limits<long double>::max());
            q4._num() *= q4._num();
            BOOST_CHECK_EQUAL(static_cast<long double>(q4), std::numeric_limits<long double>::infinity());
        }
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_conversion_test)
{
    boost::mpl::for_each<size_types>(conversion_tester());
}

BOOST_AUTO_TEST_CASE(mp_rational_literal_test)
{
    auto q0 = 123_q;
    BOOST_CHECK((std::is_same<rational, decltype(q0)>::value));
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0), "123");
    q0 = -4_q;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0), "-4");
    BOOST_CHECK_THROW((q0 = 123.45_q), std::invalid_argument);
    auto q1 = 3 / 4_q;
    BOOST_CHECK((std::is_same<rational, decltype(q1)>::value));
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q1), "3/4");
    q1 = -4 / 2_q;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q1), "-2");
    BOOST_CHECK_THROW((q1 = -3 / 0_q), zero_division_error);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q1), "-2");
}

struct plus_tester {
    template <typename T>
    void operator()(const T &)
    {
        using math::gcd;
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        {
            // Identity operator.
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{3, 11}),
                              boost::lexical_cast<std::string>(+q_type{3, 11}));
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{-6, 4}),
                              boost::lexical_cast<std::string>(+q_type{6, -4}));
            // Increment operators.
            q_type q0;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(++q0), "1");
            q0 = -1;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0++), "-1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0), "0");
            q0 = "3/2";
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(++q0), "5/2");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0++), "5/2");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0), "7/2");
            // Some simple checks.
            q_type a{1, 2};
            a += q_type{3, 5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "11/10");
            a += q_type{4, -5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "3/10");
            a += q_type{-4, 5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-1/2");
            a += int_type(-5);
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-11/2");
            a += 7;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "3/2");
            int n = 1;
            n += a;
            BOOST_CHECK_EQUAL(n, 2);
            int_type nn(-2);
            nn += a;
            BOOST_CHECK_EQUAL(nn, 0);
            double x(-3);
            if (std::numeric_limits<double>::radix == 2) {
                x += a;
                BOOST_CHECK_EQUAL(x, -1.5);
            }
            // Check return types.
            BOOST_CHECK((std::is_same<q_type &, decltype(a += a)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a += 1)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a += int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a += 1.)>::value));
            BOOST_CHECK((std::is_same<int &, decltype(n += a)>::value));
            BOOST_CHECK((std::is_same<int_type &, decltype(nn += a)>::value));
            BOOST_CHECK((std::is_same<double &, decltype(x += a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a + a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a + 1)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(1ull + a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a + int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(int_type(1) + a)>::value));
            BOOST_CHECK((std::is_same<double, decltype(a + 1.)>::value));
            BOOST_CHECK((std::is_same<long double, decltype(1.l + a)>::value));
            // Check type trait.
            BOOST_CHECK(is_addable_in_place<q_type>::value);
            BOOST_CHECK((is_addable_in_place<q_type, int>::value));
            BOOST_CHECK((is_addable_in_place<q_type, int_type>::value));
            BOOST_CHECK((is_addable_in_place<q_type, float>::value));
            BOOST_CHECK((!is_addable_in_place<q_type, std::string>::value));
            BOOST_CHECK(is_addable<q_type>::value);
            BOOST_CHECK((is_addable<q_type, int>::value));
            BOOST_CHECK((is_addable<q_type, int_type>::value));
            BOOST_CHECK((is_addable<q_type, float>::value));
            BOOST_CHECK((!is_addable<q_type, std::string>::value));
            BOOST_CHECK((is_addable<int, q_type>::value));
            BOOST_CHECK((is_addable<int_type, q_type>::value));
            BOOST_CHECK((is_addable<float, q_type>::value));
            BOOST_CHECK((!is_addable<std::string, q_type>::value));
            BOOST_CHECK((is_addable_in_place<int, q_type>::value));
            BOOST_CHECK((is_addable_in_place<int_type, q_type>::value));
            BOOST_CHECK((is_addable_in_place<double, q_type>::value));
            // Check const fails.
            BOOST_CHECK((!is_addable_in_place<const double, q_type>::value));
            BOOST_CHECK((!is_addable_in_place<const int, q_type>::value));
            // Check operations with self.
            a += a.num();
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "9/2");
            a += a;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "9");
            // Check with same den.
            a = "3/4";
            a += q_type{-7, 4};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-1");
            // Floats.
            if (std::numeric_limits<double>::has_infinity) {
                const auto old_a(a);
                BOOST_CHECK_THROW((a += std::numeric_limits<double>::infinity()), std::invalid_argument);
                BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), boost::lexical_cast<std::string>(old_a));
                double xx = std::numeric_limits<double>::infinity();
                xx += a;
                BOOST_CHECK_EQUAL(xx, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(xx + a, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(a + xx, std::numeric_limits<double>::infinity());
            }
        }
        // Random testing with integral types.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        mpq_raii m0, m1, m2;
        detail::mpz_raii z;
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng), e = dist(rng), f = dist(rng);
            if (b == 0 || d == 0) {
                continue;
            }
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            // Check first with unitary dens.
            q_type qa{a}, qc{c};
            qa += qc;
            BOOST_CHECK_EQUAL(qa, int_type{a} + int_type{c});
            qa += qa;
            BOOST_CHECK_EQUAL(qa, 2 * (int_type{a} + int_type{c}));
            qa += qa.num();
            BOOST_CHECK_EQUAL(qa, 4 * (int_type{a} + int_type{c}));
            BOOST_CHECK_EQUAL(qa.den(), 1);
            BOOST_CHECK_EQUAL(gcd(qa.num(), qa.den()).abs(), 1);
            qa += qa.den();
            BOOST_CHECK_EQUAL(qa, 4 * (int_type{a} + int_type{c}) + 1);
            BOOST_CHECK_EQUAL(qa.den(), 1);
            BOOST_CHECK_EQUAL(gcd(qa.num(), qa.den()).abs(), 1);
            // Unitary den on the left.
            qa = a;
            qc = q_type{c, d};
            qa += qc;
            BOOST_CHECK_EQUAL(qa.num(), int_type{a} * qc.den() + qc.num());
            BOOST_CHECK_EQUAL(qa.den(), qc.den());
            BOOST_CHECK_EQUAL(gcd(qa.num(), qa.den()).abs(), 1);
            // Unitary den on the right.
            qa = a;
            auto old_qc(qc);
            qc += qa;
            BOOST_CHECK_EQUAL(qc.num(), int_type{a} * old_qc.den() + old_qc.num());
            BOOST_CHECK_EQUAL(qc.den(), old_qc.den());
            BOOST_CHECK_EQUAL(gcd(qc.num(), qc.den()).abs(), 1);
            // Now with generic dens.
            q_type q0{a, b}, q1{c, d};
            q0 += q1;
            ::mpq_add(&m0.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, e);
            ::mpz_addmul(mpq_numref(&m0.m_mpq), mpq_denref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 += int_type(e);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, f);
            ::mpz_addmul(mpq_numref(&m0.m_mpq), mpq_denref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 += f;
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            // In-place with integral on the left.
            if (a < 0) {
                auto old_a = a;
                a += q_type{3, 2};
                // +2 because we are in negative territory and conversion from rational
                // to int truncates towards zero.
                BOOST_CHECK_EQUAL(old_a + 2, a);
                int_type an(old_a);
                an += q_type{3, 2};
                BOOST_CHECK_EQUAL(old_a + 2, an);
            }
            // Binary.
            BOOST_CHECK_EQUAL(q_type{a} + q_type{c}, int_type{a} + int_type{c});
            q0 = q_type{a, b};
            q1 = q_type{c, d};
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            ::mpq_add(&m2.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 + q1));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q1 + q0));
            // With int_type.
            ::mpz_set_si(&z.m_mpz, e);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_addmul(mpq_numref(&m2.m_mpq), mpq_denref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 + int_type{e}));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(int_type{e} + q0));
            // With int.
            ::mpz_set_si(&z.m_mpz, f);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_addmul(mpq_numref(&m2.m_mpq), mpq_denref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 + f));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(f + q0));
        }
        // Random testing.
        std::uniform_real_distribution<double> ddist(-1., 1.);
        for (int i = 0; i < ntries; ++i) {
            // In-place, rational on the left.
            double x = ddist(rng), y = ddist(rng), x_copy(x);
            q_type tmp_q{x}, tmp_copy{tmp_q};
            tmp_q += y;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(tmp_q),
                              boost::lexical_cast<std::string>(q_type{static_cast<double>(tmp_copy) + y}));
            // In-place, float on the left.
            x += tmp_copy;
            BOOST_CHECK_EQUAL(x, x_copy + static_cast<double>(tmp_copy));
            // Binary.
            BOOST_CHECK_EQUAL(x + tmp_q, x + static_cast<double>(tmp_q));
            BOOST_CHECK_EQUAL(tmp_q + x, static_cast<double>(tmp_q) + x);
        }
    }
};

struct minus_tester {
    template <typename T>
    void operator()(const T &)
    {
        using math::gcd;
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        {
            // Negation.
            q_type tmp00{0};
            BOOST_CHECK(has_negate<q_type>::value);
            tmp00.negate();
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(tmp00), "0");
            tmp00 = std::numeric_limits<unsigned long long>::max();
            math::negate(tmp00);
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(tmp00),
                              std::string("-")
                                  + boost::lexical_cast<std::string>(std::numeric_limits<unsigned long long>::max()));
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{3, 11}),
                              boost::lexical_cast<std::string>(-q_type{3, -11}));
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{-6, 4}),
                              boost::lexical_cast<std::string>(-q_type{6, 4}));
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q_type{}), boost::lexical_cast<std::string>(-q_type{}));
            // Decrement operators.
            q_type q0;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(--q0), "-1");
            q0 = -1;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0--), "-1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0), "-2");
            q0 = "3/2";
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(--q0), "1/2");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0--), "1/2");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(q0), "-1/2");
            // Some simple checks.
            q_type a{1, 2};
            a -= q_type{3, 5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-1/10");
            a -= q_type{4, -5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "7/10");
            a -= q_type{-4, 5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "3/2");
            a -= int_type(-5);
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "13/2");
            a -= 7;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-1/2");
            int n = 1;
            n -= a;
            BOOST_CHECK_EQUAL(n, 1);
            int_type nn(-2);
            nn -= a;
            BOOST_CHECK_EQUAL(nn, -1);
            double x(-3);
            if (std::numeric_limits<double>::radix == 2) {
                x -= a;
                BOOST_CHECK_EQUAL(x, -2.5);
            }
            // Check return types.
            BOOST_CHECK((std::is_same<q_type &, decltype(a -= a)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a -= 1)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a -= int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a -= 1.)>::value));
            BOOST_CHECK((std::is_same<int &, decltype(n -= a)>::value));
            BOOST_CHECK((std::is_same<int_type &, decltype(nn -= a)>::value));
            BOOST_CHECK((std::is_same<double &, decltype(x -= a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a - a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a - 1)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(1ull - a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a - int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(int_type(1) - a)>::value));
            BOOST_CHECK((std::is_same<double, decltype(a - 1.)>::value));
            BOOST_CHECK((std::is_same<long double, decltype(1.l - a)>::value));
            // Check type trait.
            BOOST_CHECK(is_subtractable_in_place<q_type>::value);
            BOOST_CHECK((is_subtractable_in_place<q_type, int>::value));
            BOOST_CHECK((is_subtractable_in_place<q_type, int_type>::value));
            BOOST_CHECK((is_subtractable_in_place<q_type, float>::value));
            BOOST_CHECK((!is_subtractable_in_place<q_type, std::string>::value));
            BOOST_CHECK(is_subtractable<q_type>::value);
            BOOST_CHECK((is_subtractable<q_type, int>::value));
            BOOST_CHECK((is_subtractable<q_type, int_type>::value));
            BOOST_CHECK((is_subtractable<q_type, float>::value));
            BOOST_CHECK((!is_subtractable<q_type, std::string>::value));
            BOOST_CHECK((is_subtractable<int, q_type>::value));
            BOOST_CHECK((is_subtractable<int_type, q_type>::value));
            BOOST_CHECK((is_subtractable<float, q_type>::value));
            BOOST_CHECK((!is_subtractable<std::string, q_type>::value));
            BOOST_CHECK((is_subtractable_in_place<int, q_type>::value));
            BOOST_CHECK((is_subtractable_in_place<int_type, q_type>::value));
            BOOST_CHECK((is_subtractable_in_place<double, q_type>::value));
            // Check const fails.
            BOOST_CHECK((!is_subtractable_in_place<const double, q_type>::value));
            BOOST_CHECK((!is_subtractable_in_place<const int, q_type>::value));
            // Check operations with self.
            a -= a.num();
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "1/2");
            a -= a;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "0");
            // Check with same den.
            a = "3/4";
            a -= q_type{9, 4};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-3/2");
            // Floats.
            if (std::numeric_limits<double>::has_infinity) {
                const auto old_a(a);
                BOOST_CHECK_THROW((a -= std::numeric_limits<double>::infinity()), std::invalid_argument);
                BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), boost::lexical_cast<std::string>(old_a));
                double xx = std::numeric_limits<double>::infinity();
                xx -= a;
                BOOST_CHECK_EQUAL(xx, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(xx - a, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(a - xx, -std::numeric_limits<double>::infinity());
            }
        }
        // Random testing with integral types.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        mpq_raii m0, m1, m2;
        detail::mpz_raii z;
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng), e = dist(rng), f = dist(rng);
            if (b == 0 || d == 0) {
                continue;
            }
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            // Check first with unitary dens.
            q_type qa{a}, qc{c};
            qa -= qc;
            BOOST_CHECK_EQUAL(qa, int_type{a} - int_type{c});
            qa -= qa;
            qa += 2;
            BOOST_CHECK_EQUAL(qa, 2);
            qa -= qa.num();
            BOOST_CHECK_EQUAL(qa, 0);
            BOOST_CHECK_EQUAL(qa.den(), 1);
            qa -= qa.den();
            BOOST_CHECK_EQUAL(qa, -1);
            BOOST_CHECK_EQUAL(qa.den(), 1);
            // Unitary den on the left.
            qa = a;
            qc = q_type{c, d};
            qa -= qc;
            BOOST_CHECK_EQUAL(qa.num(), int_type{a} * qc.den() - qc.num());
            BOOST_CHECK_EQUAL(qa.den(), qc.den());
            BOOST_CHECK_EQUAL(gcd(qa.num(), qa.den()).abs(), 1);
            // Unitary den on the right.
            qa = a;
            auto old_qc(qc);
            qc -= qa;
            BOOST_CHECK_EQUAL(qc.num(), -int_type{a} * old_qc.den() + old_qc.num());
            BOOST_CHECK_EQUAL(qc.den(), old_qc.den());
            BOOST_CHECK_EQUAL(gcd(qc.num(), qc.den()).abs(), 1);
            // Now generic dens.
            q_type q0{a, b}, q1{c, d};
            q0 -= q1;
            ::mpq_sub(&m0.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, e);
            ::mpz_submul(mpq_numref(&m0.m_mpq), mpq_denref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 -= int_type(e);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, f);
            ::mpz_submul(mpq_numref(&m0.m_mpq), mpq_denref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 -= f;
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            // In-place with integral on the left.
            if (a > 0) {
                auto old_a = a;
                a -= q_type{3, 2};
                BOOST_CHECK_EQUAL(old_a - 2, a);
                int_type an(old_a);
                an -= q_type{3, 2};
                BOOST_CHECK_EQUAL(old_a - 2, an);
            }
            // Binary.
            BOOST_CHECK_EQUAL(q_type{a} - q_type{c}, int_type{a} - int_type{c});
            q0 = q_type{a, b};
            q1 = q_type{c, d};
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            ::mpq_sub(&m2.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 - q1));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(-(q1 - q0)));
            // With int_type.
            ::mpz_set_si(&z.m_mpz, e);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_submul(mpq_numref(&m2.m_mpq), mpq_denref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 - int_type{e}));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(-(int_type{e} - q0)));
            // With int.
            ::mpz_set_si(&z.m_mpz, f);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_submul(mpq_numref(&m2.m_mpq), mpq_denref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 - f));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(-(f - q0)));
        }
        // Random testing.
        std::uniform_real_distribution<double> ddist(-1., 1.);
        for (int i = 0; i < ntries; ++i) {
            // In-place, rational on the left.
            double x = ddist(rng), y = ddist(rng), x_copy(x);
            q_type tmp_q{x}, tmp_copy{tmp_q};
            tmp_q -= y;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(tmp_q),
                              boost::lexical_cast<std::string>(q_type{static_cast<double>(tmp_copy) - y}));
            // In-place, float on the left.
            x -= tmp_copy;
            BOOST_CHECK_EQUAL(x, x_copy - static_cast<double>(tmp_copy));
            // Binary.
            BOOST_CHECK_EQUAL(x - tmp_q, x - static_cast<double>(tmp_q));
            BOOST_CHECK_EQUAL(tmp_q - x, static_cast<double>(tmp_q) - x);
        }
    }
};

struct mult_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        {
            // Some simple checks.
            q_type a{1, 2};
            a *= q_type{3, 5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "3/10");
            a *= q_type{4, -5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-6/25");
            a *= q_type{-4, 5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "24/125");
            a *= int_type(-5);
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-24/25");
            a *= 7;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-168/25");
            int n = 2;
            n *= a;
            BOOST_CHECK_EQUAL(n, -13);
            int_type nn(-2);
            nn *= a;
            BOOST_CHECK_EQUAL(nn, 13);
            a = "5/2";
            double x(-3);
            if (std::numeric_limits<double>::radix == 2) {
                x *= a;
                BOOST_CHECK_EQUAL(x, -7.5);
            }
            // Check return types.
            BOOST_CHECK((std::is_same<q_type &, decltype(a *= a)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a *= 1)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a *= int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a *= 1.)>::value));
            BOOST_CHECK((std::is_same<int &, decltype(n *= a)>::value));
            BOOST_CHECK((std::is_same<int_type &, decltype(nn *= a)>::value));
            BOOST_CHECK((std::is_same<double &, decltype(x *= a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a * a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a * 1)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(1ull * a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a * int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(int_type(1) * a)>::value));
            BOOST_CHECK((std::is_same<double, decltype(a * 1.)>::value));
            BOOST_CHECK((std::is_same<long double, decltype(1.l * a)>::value));
            // Check type trait.
            BOOST_CHECK(is_multipliable_in_place<q_type>::value);
            BOOST_CHECK((is_multipliable_in_place<q_type, int>::value));
            BOOST_CHECK((is_multipliable_in_place<q_type, int_type>::value));
            BOOST_CHECK((is_multipliable_in_place<q_type, float>::value));
            BOOST_CHECK((!is_multipliable_in_place<q_type, std::string>::value));
            BOOST_CHECK(is_multipliable<q_type>::value);
            BOOST_CHECK((is_multipliable<q_type, int>::value));
            BOOST_CHECK((is_multipliable<q_type, int_type>::value));
            BOOST_CHECK((is_multipliable<q_type, float>::value));
            BOOST_CHECK((!is_multipliable<q_type, std::string>::value));
            BOOST_CHECK((is_multipliable<int, q_type>::value));
            BOOST_CHECK((is_multipliable<int_type, q_type>::value));
            BOOST_CHECK((is_multipliable<float, q_type>::value));
            BOOST_CHECK((!is_multipliable<std::string, q_type>::value));
            BOOST_CHECK((is_multipliable_in_place<int, q_type>::value));
            BOOST_CHECK((is_multipliable_in_place<int_type, q_type>::value));
            BOOST_CHECK((is_multipliable_in_place<double, q_type>::value));
            BOOST_CHECK((!is_multipliable_in_place<const double, q_type>::value));
            BOOST_CHECK((!is_multipliable_in_place<const int, q_type>::value));
            // Check operations with self.
            a *= a.num();
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "25/2");
            a *= a;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "625/4");
            // Check with same den.
            a = "3/4";
            a *= q_type{9, 4};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "27/16");
            // Floats.
            if (std::numeric_limits<double>::has_infinity) {
                const auto old_a(a);
                BOOST_CHECK_THROW((a *= std::numeric_limits<double>::infinity()), std::invalid_argument);
                BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), boost::lexical_cast<std::string>(old_a));
                double xx = std::numeric_limits<double>::infinity();
                xx *= a;
                BOOST_CHECK_EQUAL(xx, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(xx * a, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(a * xx, std::numeric_limits<double>::infinity());
            }
        }
        // Random testing with integral types.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        mpq_raii m0, m1, m2;
        detail::mpz_raii z;
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng), e = dist(rng), f = dist(rng);
            if (b == 0 || d == 0) {
                continue;
            }
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            // Check first with unitary dens.
            q_type qa{a}, qc{c};
            qa *= qc;
            auto tmp_z = int_type{a} * int_type{c};
            BOOST_CHECK_EQUAL(qa, tmp_z);
            qa *= qa;
            BOOST_CHECK_EQUAL(qa, tmp_z * tmp_z);
            qa *= qa.num();
            BOOST_CHECK_EQUAL(qa, tmp_z * tmp_z * tmp_z * tmp_z);
            BOOST_CHECK_EQUAL(qa.den(), 1);
            qa *= qa.den();
            BOOST_CHECK_EQUAL(qa, tmp_z * tmp_z * tmp_z * tmp_z);
            BOOST_CHECK_EQUAL(qa.den(), 1);
            q_type q0{a, b}, q1{c, d};
            q0 *= q1;
            ::mpq_mul(&m0.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, e);
            ::mpz_mul(mpq_numref(&m0.m_mpq), mpq_numref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 *= int_type(e);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, f);
            ::mpz_mul(mpq_numref(&m0.m_mpq), mpq_numref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 *= f;
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            // In-place with integral on the left.
            if (a > 0 && a <= std::numeric_limits<int>::max() / 3) {
                auto old_a = a;
                a *= q_type{3, 2};
                BOOST_CHECK_EQUAL((old_a * 3) / 2, a);
                int_type an(old_a);
                an *= q_type{3, 2};
                BOOST_CHECK_EQUAL((old_a * 3) / 2, an);
            }
            // Binary.
            BOOST_CHECK_EQUAL(q_type{a} * q_type{c}, int_type{a} * int_type{c});
            q0 = q_type{a, b};
            q1 = q_type{c, d};
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            ::mpq_mul(&m2.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 * q1));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q1 * q0));
            // With int_type.
            ::mpz_set_si(&z.m_mpz, e);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_mul(mpq_numref(&m2.m_mpq), mpq_numref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 * int_type{e}));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(int_type{e} * q0));
            // With int.
            ::mpz_set_si(&z.m_mpz, f);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_mul(mpq_numref(&m2.m_mpq), mpq_numref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 * f));
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(f * q0));
        }
        // Random testing.
        std::uniform_real_distribution<double> ddist(-1., 1.);
        for (int i = 0; i < ntries; ++i) {
            // In-place, rational on the left.
            double x = ddist(rng), y = ddist(rng), x_copy(x);
            q_type tmp_q{x}, tmp_copy{tmp_q};
            tmp_q *= y;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(tmp_q),
                              boost::lexical_cast<std::string>(q_type{static_cast<double>(tmp_copy) * y}));
            // In-place, float on the left.
            x *= tmp_copy;
            BOOST_CHECK_EQUAL(x, x_copy * static_cast<double>(tmp_copy));
            // Binary.
            BOOST_CHECK_EQUAL(x * tmp_q, x * static_cast<double>(tmp_q));
            BOOST_CHECK_EQUAL(tmp_q * x, static_cast<double>(tmp_q) * x);
        }
    }
};

struct div_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        {
            // Some simple checks.
            q_type a{1, 2};
            a /= q_type{3, 5};
            BOOST_CHECK_THROW(a /= q_type{}, zero_division_error);
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "5/6");
            a /= q_type{4, -5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-25/24");
            a /= q_type{-4, 5};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "125/96");
            a /= int_type(-5);
            BOOST_CHECK_THROW(a /= int_type{}, zero_division_error);
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-25/96");
            a /= 7;
            BOOST_CHECK_THROW(a /= 0, zero_division_error);
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "-25/672");
            int n = 2;
            n /= a;
            BOOST_CHECK_THROW(n /= q_type{}, zero_division_error);
            BOOST_CHECK_EQUAL(n, -53);
            int_type nn(-2);
            nn /= a;
            BOOST_CHECK_THROW(nn /= q_type{}, zero_division_error);
            BOOST_CHECK_EQUAL(nn, 53);
            a = "2/5";
            double x(-3);
            if (std::numeric_limits<double>::radix == 2) {
                x /= a;
                BOOST_CHECK_EQUAL(x, -7.5);
            }
            if (std::numeric_limits<double>::has_infinity) {
                x /= q_type{};
                BOOST_CHECK_EQUAL(x, -std::numeric_limits<double>::infinity());
            }
            // Check return types.
            BOOST_CHECK((std::is_same<q_type &, decltype(a /= a)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a /= 1)>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a /= int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type &, decltype(a /= 1.)>::value));
            BOOST_CHECK((std::is_same<int &, decltype(n /= a)>::value));
            BOOST_CHECK((std::is_same<int_type &, decltype(nn /= a)>::value));
            BOOST_CHECK((std::is_same<double &, decltype(x /= a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a / a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a / 1)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(1ull / a)>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(a / int_type(1))>::value));
            BOOST_CHECK((std::is_same<q_type, decltype(int_type(1) / a)>::value));
            BOOST_CHECK((std::is_same<double, decltype(a / 1.)>::value));
            BOOST_CHECK((std::is_same<long double, decltype(1.l / a)>::value));
            // Check type trait.
            BOOST_CHECK(is_divisible_in_place<q_type>::value);
            BOOST_CHECK((is_divisible_in_place<q_type, int>::value));
            BOOST_CHECK((is_divisible_in_place<q_type, int_type>::value));
            BOOST_CHECK((is_divisible_in_place<q_type, float>::value));
            BOOST_CHECK((!is_divisible_in_place<q_type, std::string>::value));
            BOOST_CHECK(is_divisible<q_type>::value);
            BOOST_CHECK((is_divisible<q_type, int>::value));
            BOOST_CHECK((is_divisible<q_type, int_type>::value));
            BOOST_CHECK((is_divisible<q_type, float>::value));
            BOOST_CHECK((!is_divisible<q_type, std::string>::value));
            BOOST_CHECK((is_divisible<int, q_type>::value));
            BOOST_CHECK((is_divisible<int_type, q_type>::value));
            BOOST_CHECK((is_divisible<float, q_type>::value));
            BOOST_CHECK((!is_divisible<std::string, q_type>::value));
            BOOST_CHECK((is_divisible_in_place<int, q_type>::value));
            BOOST_CHECK((is_divisible_in_place<int_type, q_type>::value));
            BOOST_CHECK((is_divisible_in_place<double, q_type>::value));
            BOOST_CHECK((!is_divisible_in_place<const double, q_type>::value));
            BOOST_CHECK((!is_divisible_in_place<const int, q_type>::value));
            // Check operations with self.
            a /= a.num();
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "1/5");
            a /= a.den();
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "1/25");
            a /= a;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "1");
            a = 0;
            BOOST_CHECK_THROW((a /= a), zero_division_error);
            // Check with same den.
            a = "3/4";
            a /= q_type{9, 4};
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(a), "1/3");
            // Floats.
            if (std::numeric_limits<double>::has_infinity) {
                a /= std::numeric_limits<double>::infinity();
                BOOST_CHECK(math::is_zero(a));
                double xx = std::numeric_limits<double>::infinity();
                xx /= a;
                BOOST_CHECK_EQUAL(xx, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(xx / a, std::numeric_limits<double>::infinity());
                BOOST_CHECK_EQUAL(a / xx, 0.);
            }
        }
        // Random testing with integral types.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        mpq_raii m0, m1, m2;
        detail::mpz_raii z;
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng), e = dist(rng), f = dist(rng);
            // Nothing must be zero if we are going to switch operands around.
            if (a == 0 || b == 0 || c == 0 || d == 0) {
                continue;
            }
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            q_type q0{a, b}, q1{c, d};
            q0 /= q1;
            ::mpq_div(&m0.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, e);
            ::mpz_mul(mpq_denref(&m0.m_mpq), mpq_denref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 /= int_type(e);
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            ::mpz_set_si(&z.m_mpz, f);
            ::mpz_mul(mpq_denref(&m0.m_mpq), mpq_denref(&m0.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m0.m_mpq);
            q0 /= f;
            BOOST_CHECK_EQUAL(mpq_lexcast(m0), boost::lexical_cast<std::string>(q0));
            // In-place with integral on the left.
            if (a > 0 && a <= std::numeric_limits<int>::max() / 2) {
                auto old_a = a;
                a /= q_type{3, 2};
                BOOST_CHECK_EQUAL((old_a * 2) / 3, a);
                int_type an(old_a);
                an /= q_type{3, 2};
                BOOST_CHECK_EQUAL((old_a * 2) / 3, an);
            }
            // Binary.
            q0 = q_type{a, b};
            q1 = q_type{c, d};
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            ::mpq_div(&m2.m_mpq, &m0.m_mpq, &m1.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 / q1));
            ::mpq_div(&m2.m_mpq, &m1.m_mpq, &m0.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q1 / q0));
            // With int_type.
            ::mpz_set_si(&z.m_mpz, e);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_mul(mpq_denref(&m2.m_mpq), mpq_denref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 / int_type{e}));
            ::mpz_swap(mpq_numref(&m2.m_mpq), mpq_denref(&m2.m_mpq));
            // Canonicalise because we might have a sign mismatch.
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(int_type{e} / q0));
            // With int.
            ::mpz_set_si(&z.m_mpz, f);
            ::mpq_set(&m2.m_mpq, &m0.m_mpq);
            ::mpz_mul(mpq_denref(&m2.m_mpq), mpq_denref(&m2.m_mpq), &z.m_mpz);
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(q0 / f));
            ::mpz_swap(mpq_numref(&m2.m_mpq), mpq_denref(&m2.m_mpq));
            ::mpq_canonicalize(&m2.m_mpq);
            BOOST_CHECK_EQUAL(mpq_lexcast(m2), boost::lexical_cast<std::string>(f / q0));
        }
        // Random testing.
        std::uniform_real_distribution<double> ddist(-1., 1.);
        for (int i = 0; i < ntries; ++i) {
            // In-place, rational on the left.
            double x = ddist(rng), y = ddist(rng), x_copy(x);
            if (y == 0. || x == 0.) {
                continue;
            }
            q_type tmp_q{x}, tmp_copy{tmp_q};
            tmp_q /= y;
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(tmp_q),
                              boost::lexical_cast<std::string>(q_type{static_cast<double>(tmp_copy) / y}));
            // In-place, float on the left.
            x /= tmp_copy;
            BOOST_CHECK_EQUAL(x, x_copy / static_cast<double>(tmp_copy));
            // Binary.
            BOOST_CHECK_EQUAL(x / tmp_q, x / static_cast<double>(tmp_q));
            BOOST_CHECK_EQUAL(tmp_q / x, static_cast<double>(tmp_q) / x);
        }
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_arith_test)
{
    boost::mpl::for_each<size_types>(plus_tester());
    boost::mpl::for_each<size_types>(minus_tester());
    boost::mpl::for_each<size_types>(mult_tester());
    boost::mpl::for_each<size_types>(div_tester());
    // Check with different bits size.
    BOOST_CHECK((!is_addable_in_place<mp_rational<16>, mp_rational<32>>::value));
    BOOST_CHECK((!is_addable_in_place<mp_rational<16>, mp_integer<32>>::value));
    BOOST_CHECK((!is_addable<mp_rational<16>, mp_rational<32>>::value));
    BOOST_CHECK((!is_addable<mp_rational<32>, mp_rational<16>>::value));
    BOOST_CHECK((!is_addable<mp_rational<16>, mp_integer<32>>::value));
    BOOST_CHECK((!is_addable<mp_integer<32>, mp_rational<16>>::value));
    BOOST_CHECK((!is_subtractable<mp_rational<16>, mp_rational<32>>::value));
    BOOST_CHECK((!is_subtractable<mp_rational<32>, mp_rational<16>>::value));
    BOOST_CHECK((!is_subtractable<mp_rational<16>, mp_integer<32>>::value));
    BOOST_CHECK((!is_subtractable<mp_integer<32>, mp_rational<16>>::value));
}

struct is_zero_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        q_type q;
        BOOST_CHECK(math::is_zero(q));
        q = 1;
        BOOST_CHECK(!math::is_zero(q));
        q = "-3/5";
        BOOST_CHECK(!math::is_zero(q));
        q = "1";
        q -= 1;
        BOOST_CHECK(math::is_zero(q));
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_is_zero_test)
{
    boost::mpl::for_each<size_types>(is_zero_tester());
}

struct mpq_view_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        {
            q_type q;
            {
                auto v = q.get_mpq_view();
                BOOST_CHECK_EQUAL(mpq_cmp_si(v.get(), 0, 1u), 0);
                BOOST_CHECK(!std::is_copy_constructible<decltype(v)>::value);
                BOOST_CHECK(std::is_move_constructible<decltype(v)>::value);
                BOOST_CHECK(!std::is_copy_assignable<decltype(v) &>::value);
                BOOST_CHECK(!std::is_move_assignable<decltype(v) &>::value);
            }
            q = q_type(4, -3);
            {
                auto v = q.get_mpq_view();
                BOOST_CHECK_EQUAL(mpq_cmp_si(v.get(), -4, 3u), 0);
            }
            q = q_type(4, 16);
            {
                auto v = q.get_mpq_view();
                BOOST_CHECK_EQUAL(mpq_cmp_si(v.get(), 1, 4u), 0);
            }
        }
        // Random testing.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        mpq_raii mq;
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng);
            if (b == 0) {
                continue;
            }
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&mq.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&mq.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&mq.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&mq.m_mpq);
            q_type q{a, b};
            auto v = q.get_mpq_view();
            BOOST_CHECK_EQUAL(::mpq_cmp(v, &mq.m_mpq), 0);
            BOOST_CHECK_EQUAL(::mpq_cmp(&mq.m_mpq, v), 0);
        }
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_mpq_view_test)
{
    boost::mpl::for_each<size_types>(mpq_view_tester());
}

struct equality_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        // A few simple tests.
        BOOST_CHECK_EQUAL(q_type{}, q_type{0});
        BOOST_CHECK_EQUAL(q_type{0}, q_type{});
        BOOST_CHECK_EQUAL(q_type{-5}, q_type{-5});
        BOOST_CHECK_EQUAL(q_type(-5, 4), q_type(5, -4));
        BOOST_CHECK_EQUAL(q_type(10, 8), q_type(-5, -4));
        BOOST_CHECK_EQUAL(q_type(10, 5), int_type(2));
        BOOST_CHECK_EQUAL(int_type(2), q_type(10, 5));
        BOOST_CHECK_EQUAL(q_type(10, 5), 2);
        BOOST_CHECK_EQUAL(2, q_type(10, 5));
        BOOST_CHECK_EQUAL(q_type(10, 5), 2u);
        BOOST_CHECK_EQUAL(2ll, q_type(10, 5));
        const auto radix = std::numeric_limits<double>::radix;
        BOOST_CHECK_EQUAL(q_type(3, radix), 3. / radix);
        BOOST_CHECK_EQUAL(3. / radix, q_type(3, radix));
        BOOST_CHECK(q_type(10, 8) != q_type(-5, -3));
        BOOST_CHECK(q_type(10, 8) != q_type(-5, 4));
        BOOST_CHECK(q_type(10, 8) != q_type(-5, 4));
        BOOST_CHECK(q_type(10, 8) != int_type(1));
        BOOST_CHECK(int_type(1) != q_type(10, 8));
        BOOST_CHECK(1 != q_type(10, 8));
        BOOST_CHECK(q_type(10, 8) != 1);
        BOOST_CHECK(char(2) != q_type(10, 8));
        BOOST_CHECK(q_type(10, 8) != 2ull);
        const auto radix_f = std::numeric_limits<float>::radix;
        BOOST_CHECK(5.f / radix_f != q_type(2, radix_f));
        BOOST_CHECK(q_type(2, radix_f) != 5.f / radix_f);
        // Random testing.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng);
            if (b == 0) {
                continue;
            }
            q_type q0{a, b}, q1{a, b};
            BOOST_CHECK_EQUAL(q0, q1);
            BOOST_CHECK_EQUAL(q1, q0);
            q1 += 1;
            BOOST_CHECK(q0 != q1);
            BOOST_CHECK(q1 != q0);
            q0 *= b;
            BOOST_CHECK_EQUAL(q0, int_type(a));
            BOOST_CHECK_EQUAL(int_type(a), q0);
            BOOST_CHECK_EQUAL(q0, a);
            BOOST_CHECK_EQUAL(a, q0);
            q0 += q_type{1, 2};
            BOOST_CHECK(q0 != int_type(a));
            BOOST_CHECK(int_type(a) != q0);
            BOOST_CHECK(q0 != a);
            BOOST_CHECK(a != q0);
        }
    }
};

struct less_than_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        // A few simple tests.
        BOOST_CHECK(!(q_type{} < q_type{0}));
        BOOST_CHECK(!(q_type{0} < q_type{}));
        BOOST_CHECK(q_type{-6} < q_type{-5});
        BOOST_CHECK(!(q_type{-5} < q_type{-6}));
        BOOST_CHECK(q_type(1, 2) < q_type(3, 4));
        BOOST_CHECK(q_type(1, 3) < q_type(1, 2));
        BOOST_CHECK(q_type(1, -2) < q_type(1, 3));
        BOOST_CHECK(int_type() < q_type(1, 3));
        BOOST_CHECK(int_type(4) < q_type(41, 10));
        BOOST_CHECK(q_type(39, -10) < int_type(8));
        BOOST_CHECK(0 < q_type(1, 3));
        BOOST_CHECK((unsigned short)4 < q_type(41, 10));
        BOOST_CHECK(q_type(39, -10) < 8ll);
        const auto radix = std::numeric_limits<double>::radix;
        BOOST_CHECK(q_type(1, radix) < 1.);
        BOOST_CHECK(1. < q_type(1 + radix, radix));
        const auto radix_f = std::numeric_limits<float>::radix;
        BOOST_CHECK(q_type(1, radix_f) < 1.f);
        BOOST_CHECK(1.f < q_type(radix_f + radix_f, radix_f));
        // Random testing.
        mpq_raii m0, m1;
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng);
            if (b == 0 || d == 0) {
                continue;
            }
            // Some checks with equal dens.
            BOOST_CHECK_EQUAL((q_type{a, 1} < q_type{c, 1}), int_type{a} < int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 5} < q_type{c, 5}), int_type{a} < int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 2} < q_type{c, 2}), int_type{a} < int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 10} < q_type{c, 10}), int_type{a} < int_type{c});
            q_type q0{a, b}, q1{c, d};
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            bool cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) < 0;
            BOOST_CHECK_EQUAL(cmp, q0 < q1);
            // Transform the second rational operand into an integral.
            ::mpz_set_ui(mpq_denref(&m1.m_mpq), 1u);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) < 0;
            BOOST_CHECK_EQUAL(cmp, q0 < q1.num());
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) < 0;
            BOOST_CHECK_EQUAL(cmp, q1.num() < q0);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) < 0;
            // NOTE: the static casts here are ok because num/den are int originally.
            BOOST_CHECK_EQUAL(cmp, q0 < static_cast<int>(q1.num()));
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) < 0;
            BOOST_CHECK_EQUAL(cmp, static_cast<int>(q1.num()) < q0);
        }
    }
};

struct geq_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        // A few simple tests.
        BOOST_CHECK(q_type{} >= q_type{0});
        BOOST_CHECK(q_type{0} >= q_type{});
        BOOST_CHECK(!(q_type{-6} >= q_type{-5}));
        BOOST_CHECK(q_type{-5} >= q_type{-6});
        BOOST_CHECK(q_type(3, 4) >= q_type(1, 2));
        BOOST_CHECK(q_type(1, 2) >= q_type(1, 3));
        BOOST_CHECK(q_type(1, 3) >= q_type(1, -2));
        BOOST_CHECK(q_type(1, 3) >= int_type());
        BOOST_CHECK(q_type(41, 10) >= int_type(4));
        BOOST_CHECK(int_type(8) >= q_type(39, -10));
        BOOST_CHECK(q_type(1, 3) >= 0);
        BOOST_CHECK(q_type(41, 10) >= (unsigned short)4);
        BOOST_CHECK(8ll >= q_type(39, -10));
        const auto radix = std::numeric_limits<double>::radix;
        BOOST_CHECK(1. >= q_type(1, radix));
        BOOST_CHECK(q_type(1 + radix, radix) >= 1.);
        const auto radix_f = std::numeric_limits<float>::radix;
        BOOST_CHECK(1.f >= q_type(1, radix_f));
        BOOST_CHECK(q_type(radix_f + radix_f, radix_f) >= 1.f);
        // Random testing.
        mpq_raii m0, m1;
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng);
            if (b == 0 || d == 0) {
                continue;
            }
            // Some checks with equal dens.
            BOOST_CHECK_EQUAL((q_type{a, 1} >= q_type{c, 1}), int_type{a} >= int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 5} >= q_type{c, 5}), int_type{a} >= int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 2} >= q_type{c, 2}), int_type{a} >= int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 10} >= q_type{c, 10}), int_type{a} >= int_type{c});
            q_type q0{a, b}, q1{c, d};
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            bool cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) >= 0;
            BOOST_CHECK_EQUAL(cmp, q0 >= q1);
            // Transform the second rational operand into an integral.
            ::mpz_set_ui(mpq_denref(&m1.m_mpq), 1u);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) >= 0;
            BOOST_CHECK_EQUAL(cmp, q0 >= q1.num());
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) >= 0;
            BOOST_CHECK_EQUAL(cmp, q1.num() >= q0);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) >= 0;
            // NOTE: the static casts here are ok because num/den are int originally.
            BOOST_CHECK_EQUAL(cmp, q0 >= static_cast<int>(q1.num()));
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) >= 0;
            BOOST_CHECK_EQUAL(cmp, static_cast<int>(q1.num()) >= q0);
        }
    }
};

struct greater_than_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        // A few simple tests.
        BOOST_CHECK(!(q_type{} > q_type{0}));
        BOOST_CHECK(!(q_type{0} > q_type{}));
        BOOST_CHECK(q_type{-5} > q_type{-6});
        BOOST_CHECK(!(q_type{-6} > q_type{-5}));
        BOOST_CHECK(q_type(3, 4) > q_type(1, 2));
        BOOST_CHECK(q_type(1, 2) > q_type(1, 3));
        BOOST_CHECK(q_type(1, 3) > q_type(1, -2));
        BOOST_CHECK(q_type(1, 3) > int_type());
        BOOST_CHECK(q_type(41, 10) > int_type(4));
        BOOST_CHECK(int_type(8) > q_type(39, -10));
        BOOST_CHECK(q_type(1, 3) > 0);
        BOOST_CHECK(q_type(41, 10) > (unsigned short)4);
        BOOST_CHECK(8ll > q_type(39, -10));
        const auto radix = std::numeric_limits<double>::radix;
        BOOST_CHECK(1. > q_type(1, radix));
        BOOST_CHECK(q_type(1 + radix, radix) > 1.);
        const auto radix_f = std::numeric_limits<float>::radix;
        BOOST_CHECK(1.f > q_type(1, radix_f));
        BOOST_CHECK(q_type(radix_f + radix_f, radix_f) > 1.f);
        // Random testing.
        mpq_raii m0, m1;
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng);
            if (b == 0 || d == 0) {
                continue;
            }
            // Some checks with equal dens.
            BOOST_CHECK_EQUAL((q_type{a, 1} > q_type{c, 1}), int_type{a} > int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 5} > q_type{c, 5}), int_type{a} > int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 2} > q_type{c, 2}), int_type{a} > int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 10} > q_type{c, 10}), int_type{a} > int_type{c});
            q_type q0{a, b}, q1{c, d};
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            bool cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) > 0;
            BOOST_CHECK_EQUAL(cmp, q0 > q1);
            // Transform the second rational operand into an integral.
            ::mpz_set_ui(mpq_denref(&m1.m_mpq), 1u);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) > 0;
            BOOST_CHECK_EQUAL(cmp, q0 > q1.num());
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) > 0;
            BOOST_CHECK_EQUAL(cmp, q1.num() > q0);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) > 0;
            // NOTE: the static casts here are ok because num/den are int originally.
            BOOST_CHECK_EQUAL(cmp, q0 > static_cast<int>(q1.num()));
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) > 0;
            BOOST_CHECK_EQUAL(cmp, static_cast<int>(q1.num()) > q0);
        }
    }
};

struct leq_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        // A few simple tests.
        BOOST_CHECK(q_type{} <= q_type{0});
        BOOST_CHECK(q_type{0} <= q_type{});
        BOOST_CHECK(!(q_type{-5} <= q_type{-6}));
        BOOST_CHECK(q_type{-6} <= q_type{-5});
        BOOST_CHECK(q_type(1, 2) <= q_type(3, 4));
        BOOST_CHECK(q_type(1, 3) <= q_type(1, 2));
        BOOST_CHECK(q_type(1, -2) <= q_type(1, 3));
        BOOST_CHECK(int_type() <= q_type(1, 3));
        BOOST_CHECK(int_type(4) <= q_type(41, 10));
        BOOST_CHECK(q_type(39, -10) <= int_type(8));
        BOOST_CHECK(0 <= q_type(1, 3));
        BOOST_CHECK((unsigned short)4 <= q_type(41, 10));
        BOOST_CHECK(q_type(39, -10) <= 8ll);
        const auto radix = std::numeric_limits<double>::radix;
        BOOST_CHECK(q_type(1, radix) <= 1.);
        BOOST_CHECK(1. <= q_type(1 + radix, radix));
        const auto radix_f = std::numeric_limits<float>::radix;
        BOOST_CHECK(q_type(1, radix_f) <= 1.f);
        BOOST_CHECK(1.f <= q_type(radix_f + radix_f, radix_f));
        // Random testing.
        mpq_raii m0, m1;
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), c = dist(rng), d = dist(rng);
            if (b == 0 || d == 0) {
                continue;
            }
            // Some checks with equal dens.
            BOOST_CHECK_EQUAL((q_type{a, 1} <= q_type{c, 1}), int_type{a} <= int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 5} <= q_type{c, 5}), int_type{a} <= int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 2} <= q_type{c, 2}), int_type{a} <= int_type{c});
            BOOST_CHECK_EQUAL((q_type{a, 10} <= q_type{c, 10}), int_type{a} <= int_type{c});
            q_type q0{a, b}, q1{c, d};
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            if (d > 0) {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(d));
            } else {
                ::mpq_set_si(&m1.m_mpq, static_cast<long>(c), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m1.m_mpq), static_cast<long>(d));
            }
            ::mpq_canonicalize(&m1.m_mpq);
            bool cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) <= 0;
            BOOST_CHECK_EQUAL(cmp, q0 <= q1);
            // Transform the second rational operand into an integral.
            ::mpz_set_ui(mpq_denref(&m1.m_mpq), 1u);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) <= 0;
            BOOST_CHECK_EQUAL(cmp, q0 <= q1.num());
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) <= 0;
            BOOST_CHECK_EQUAL(cmp, q1.num() <= q0);
            cmp = ::mpq_cmp(&m0.m_mpq, &m1.m_mpq) <= 0;
            // NOTE: the static casts here are ok because num/den are int originally.
            BOOST_CHECK_EQUAL(cmp, q0 <= static_cast<int>(q1.num()));
            cmp = ::mpq_cmp(&m1.m_mpq, &m0.m_mpq) <= 0;
            BOOST_CHECK_EQUAL(cmp, static_cast<int>(q1.num()) <= q0);
        }
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_cmp_test)
{
    boost::mpl::for_each<size_types>(equality_tester());
    boost::mpl::for_each<size_types>(less_than_tester());
    boost::mpl::for_each<size_types>(geq_tester());
    boost::mpl::for_each<size_types>(greater_than_tester());
    boost::mpl::for_each<size_types>(leq_tester());
}

struct pow_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        // A few simple tests.
        BOOST_CHECK_EQUAL(q_type().pow(0), 1);
        BOOST_CHECK_EQUAL(q_type().pow(0u), 1);
        BOOST_CHECK_EQUAL(q_type().pow(int_type()), 1);
        BOOST_CHECK_EQUAL(q_type().pow(1), 0);
        BOOST_CHECK_EQUAL(q_type().pow(2u), 0);
        BOOST_CHECK_EQUAL(q_type().pow(3), 0);
        BOOST_CHECK_EQUAL(q_type().pow(4ull), 0);
        BOOST_CHECK_EQUAL(q_type().pow(int_type(5)), 0);
        BOOST_CHECK_EQUAL(q_type().pow((unsigned char)5), 0);
        BOOST_CHECK_THROW(q_type().pow(-1), zero_division_error);
        BOOST_CHECK_THROW(q_type().pow(char(-2)), zero_division_error);
        BOOST_CHECK_THROW(q_type().pow(-3ll), zero_division_error);
        BOOST_CHECK_THROW(q_type().pow(int_type(-3)), zero_division_error);
        BOOST_CHECK_EQUAL(q_type(23, 45).pow(7), q_type(3404825447ull, 373669453125ull));
        BOOST_CHECK_EQUAL(q_type(-23, 45).pow(7), q_type(-3404825447ll, 373669453125ull));
        BOOST_CHECK_EQUAL(q_type(-23, 45).pow(-7), q_type(373669453125ull, -3404825447ll));
        // Random testing.
        std::uniform_int_distribution<int> dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        std::uniform_int_distribution<int> edist(-10, 10);
        mpq_raii m0, m1;
        for (int i = 0; i < ntries; ++i) {
            int a = dist(rng), b = dist(rng), exp = edist(rng);
            // Check the numerator as well as we don't want to run into zero division.
            if (a == 0 || b == 0) {
                continue;
            }
            q_type q0{a, b};
            // The mpq set function only works with unsigned type at the denom. Bypass using mpz directly.
            if (b > 0) {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(b));
            } else {
                ::mpq_set_si(&m0.m_mpq, static_cast<long>(a), static_cast<unsigned long>(1));
                ::mpz_set_si(mpq_denref(&m0.m_mpq), static_cast<long>(b));
            }
            ::mpq_canonicalize(&m0.m_mpq);
            auto q1 = q0.pow(exp);
            if (exp >= 0) {
                ::mpz_pow_ui(mpq_numref(&m1.m_mpq), mpq_numref(&m0.m_mpq), static_cast<unsigned long>(exp));
                ::mpz_pow_ui(mpq_denref(&m1.m_mpq), mpq_denref(&m0.m_mpq), static_cast<unsigned long>(exp));
            } else {
                ::mpz_pow_ui(mpq_numref(&m1.m_mpq), mpq_denref(&m0.m_mpq), static_cast<unsigned long>(-exp));
                ::mpz_pow_ui(mpq_denref(&m1.m_mpq), mpq_numref(&m0.m_mpq), static_cast<unsigned long>(-exp));
                ::mpq_canonicalize(&m1.m_mpq);
            }
            BOOST_CHECK_EQUAL(mpq_lexcast(m1), boost::lexical_cast<std::string>(q1));
            // Test math::pow().
            BOOST_CHECK_EQUAL(math::pow(q0, exp), q1);
            auto q2 = q0.pow(int_type(exp));
            BOOST_CHECK_EQUAL(q1, q2);
            BOOST_CHECK_EQUAL(math::pow(q0, int_type(exp)), q1);
        }
        // Rational-fp.
        const auto radix = std::numeric_limits<double>::radix;
        BOOST_CHECK((is_exponentiable<q_type, float>::value));
        BOOST_CHECK((is_exponentiable<q_type, double>::value));
        BOOST_CHECK_EQUAL(math::pow(q_type(1, radix), 2.), std::pow(1. / radix, 2.));
        BOOST_CHECK((std::is_same<float, decltype(math::pow(q_type(1), 1.f))>::value));
        BOOST_CHECK((std::is_same<long double, decltype(math::pow(q_type(1, radix), 2.l))>::value));
        // Rational-rational.
        BOOST_CHECK((is_exponentiable<q_type, q_type>::value));
        BOOST_CHECK_THROW(math::pow(q_type(1, radix), q_type(1, radix)), std::invalid_argument);
        BOOST_CHECK_EQUAL(math::pow(q_type(2, 3), q_type(2)), q_type(4, 9));
        BOOST_CHECK_EQUAL(math::pow(q_type(2, 3), q_type(-2)), q_type(9, 4));
        BOOST_CHECK((std::is_same<q_type, decltype(math::pow(q_type(1, radix), q_type(1, radix)))>::value));
        // Special cases.
        BOOST_CHECK_EQUAL(math::pow(q_type(1), q_type(2, 3)), 1);
        BOOST_CHECK_EQUAL(math::pow(q_type(1), q_type(-2, 3)), 1);
        BOOST_CHECK_EQUAL(math::pow(q_type(0), q_type(2, 3)), 0);
        BOOST_CHECK_EQUAL(math::pow(q_type(0), q_type(0)), 1);
        BOOST_CHECK_THROW(math::pow(q_type(0), q_type(-2, 3)), zero_division_error);
        // Fp-rational.
        BOOST_CHECK((is_exponentiable<float, q_type>::value));
        BOOST_CHECK((is_exponentiable<double, q_type>::value));
        BOOST_CHECK((std::is_same<decltype(math::pow(2., q_type(1, radix))), double>::value));
        BOOST_CHECK((std::is_same<decltype(math::pow(2.f, q_type(1, radix))), float>::value));
        BOOST_CHECK_EQUAL(math::pow(2., q_type(1, radix)), std::pow(2., 1. / radix));
        // Integral-rational.
        BOOST_CHECK((is_exponentiable<int, q_type>::value));
        BOOST_CHECK((is_exponentiable<int_type, q_type>::value));
        BOOST_CHECK((std::is_same<int_type, decltype(math::pow(2, q_type(1, radix)))>::value));
        BOOST_CHECK((std::is_same<int_type, decltype(math::pow(int_type(2), q_type(1, radix)))>::value));
        BOOST_CHECK_THROW(math::pow(2, q_type(1, radix)), std::invalid_argument);
        BOOST_CHECK_THROW(math::pow(int_type(2), q_type(1, radix)), std::invalid_argument);
        BOOST_CHECK_EQUAL(math::pow(2, q_type(2)), 4);
        BOOST_CHECK_EQUAL(math::pow(int_type(3), q_type(2)), 9);
        // Special cases.
        BOOST_CHECK_EQUAL(math::pow(1, q_type(2, 3)), 1);
        BOOST_CHECK_EQUAL(math::pow(int_type(1), q_type(2, 3)), 1);
        BOOST_CHECK_EQUAL(math::pow(1, q_type(2, -3)), 1);
        BOOST_CHECK_EQUAL(math::pow(int_type(1), q_type(-2, 3)), 1);
        BOOST_CHECK_EQUAL(math::pow(0, q_type(2, 3)), 0);
        BOOST_CHECK_EQUAL(math::pow(int_type(0), q_type(2, 3)), 0);
        BOOST_CHECK_EQUAL(math::pow(0, q_type(0)), 1);
        BOOST_CHECK_EQUAL(math::pow(int_type(0), q_type(0, 3)), 1);
        BOOST_CHECK_THROW(math::pow(0, q_type(-1, 3)), zero_division_error);
        BOOST_CHECK_THROW(math::pow(int_type(0), q_type(-1, 3)), zero_division_error);
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_pow_test)
{
    boost::mpl::for_each<size_types>(pow_tester());
    BOOST_CHECK((!is_exponentiable<mp_rational<16>, mp_rational<32>>::value));
    BOOST_CHECK((!is_exponentiable<mp_rational<16>, mp_integer<32>>::value));
    BOOST_CHECK((!is_exponentiable<mp_integer<32>, mp_rational<16>>::value));
    BOOST_CHECK((!is_exponentiable<mp_integer<32>, std::string>::value));
}

struct abs_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        BOOST_CHECK_EQUAL(q_type{}.abs(), q_type(0));
        BOOST_CHECK_EQUAL(q_type(1, 3).abs(), q_type(1, 3));
        BOOST_CHECK_EQUAL(q_type(1, -3).abs(), q_type(1, 3));
        BOOST_CHECK_EQUAL(q_type(-4, -3).abs(), q_type(4, 3));
        BOOST_CHECK_EQUAL(q_type(-4, 5).abs(), q_type(4, 5));
        BOOST_CHECK_EQUAL(math::abs(q_type(-4, 5)), q_type(4, 5));
        BOOST_CHECK_EQUAL(math::abs(q_type(4, -5)), q_type(4, 5));
        BOOST_CHECK_EQUAL(math::abs(q_type(-4, -5)), q_type(4, 5));
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_abs_test)
{
    boost::mpl::for_each<size_types>(abs_tester());
}

struct hash_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        BOOST_CHECK(is_hashable<q_type>::value);
        BOOST_CHECK_EQUAL(q_type{3}.hash(), std::hash<q_type>()(q_type{3}));
        BOOST_CHECK(q_type{3}.hash() != std::hash<q_type>()(q_type{3, 2}));
        BOOST_CHECK_EQUAL(q_type(4, 5).hash(), std::hash<q_type>()(q_type(-8, -10)));
        q_type q0{123, -456};
        std::size_t h = q0.num().hash();
        boost::hash_combine(h, std::hash<int_type>()(q0.den()));
        BOOST_CHECK_EQUAL(h, q0.hash());
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_hash_test)
{
    boost::mpl::for_each<size_types>(hash_tester());
}

struct print_tex_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        BOOST_CHECK(has_print_tex_coefficient<q_type>::value);
        std::ostringstream ss;
        print_tex_coefficient(ss, q_type(0));
        BOOST_CHECK_EQUAL(ss.str(), "0");
        ss.str("");
        print_tex_coefficient(ss, q_type(-1));
        BOOST_CHECK_EQUAL(ss.str(), "-1");
        ss.str("");
        print_tex_coefficient(ss, q_type(1));
        BOOST_CHECK_EQUAL(ss.str(), "1");
        ss.str("");
        print_tex_coefficient(ss, q_type(1, 2));
        BOOST_CHECK_EQUAL(ss.str(), "\\frac{1}{2}");
        ss.str("");
        print_tex_coefficient(ss, q_type(1, -2));
        BOOST_CHECK_EQUAL(ss.str(), "-\\frac{1}{2}");
        ss.str("");
        print_tex_coefficient(ss, q_type(-14, 21));
        BOOST_CHECK_EQUAL(ss.str(), "-\\frac{2}{3}");
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_print_tex_test)
{
    boost::mpl::for_each<size_types>(print_tex_tester());
}

struct sin_cos_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        BOOST_CHECK_EQUAL(math::sin(q_type()), 0);
        BOOST_CHECK_EQUAL(math::cos(q_type()), 1);
        BOOST_CHECK((std::is_same<q_type, decltype(math::cos(q_type()))>::value));
        BOOST_CHECK((std::is_same<q_type, decltype(math::sin(q_type()))>::value));
        BOOST_CHECK_THROW(math::sin(q_type(1)), std::invalid_argument);
        BOOST_CHECK_THROW(math::cos(q_type(1)), std::invalid_argument);
        BOOST_CHECK(has_sine<q_type>::value);
        BOOST_CHECK(has_cosine<q_type>::value);
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_sin_cos_test)
{
    boost::mpl::for_each<size_types>(sin_cos_tester());
}

struct sep_tester {
    template <typename T>
    using edict = std::unordered_map<std::string, T>;
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        BOOST_CHECK_EQUAL(math::partial(q_type{1}, ""), 0);
        BOOST_CHECK((std::is_same<q_type, decltype(math::partial(q_type{1}, ""))>::value));
        BOOST_CHECK(is_differentiable<q_type>::value);
        BOOST_CHECK_EQUAL(math::evaluate(q_type{12}, edict<int>{{"", 1}}), 12);
        BOOST_CHECK_EQUAL(math::evaluate(q_type{10}, edict<double>{{"", 1.321}}), 10);
        BOOST_CHECK((is_evaluable<q_type, int>::value));
        BOOST_CHECK((is_evaluable<q_type, double>::value));
        BOOST_CHECK((std::is_same<q_type, decltype(math::evaluate(q_type{10}, edict<double>{{"", 1.321}}))>::value));
        BOOST_CHECK((!has_subs<q_type, q_type>::value));
        BOOST_CHECK((!has_subs<q_type, int>::value));
        BOOST_CHECK((!has_subs<q_type, long double>::value));
        BOOST_CHECK((!has_ipow_subs<q_type, int>::value));
        BOOST_CHECK((!has_ipow_subs<q_type, double>::value));
        BOOST_CHECK((!has_ipow_subs<q_type, float>::value));
        BOOST_CHECK((!has_ipow_subs<q_type, unsigned short>::value));
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_sep_test)
{
    boost::mpl::for_each<size_types>(sep_tester());
}

struct binomial_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using int_type = typename q_type::int_type;
        // Some checks with integral values on top.
        BOOST_CHECK_EQUAL(q_type{}.binomial(0), 1);
        BOOST_CHECK_EQUAL(q_type{}.binomial(1), 0);
        BOOST_CHECK_EQUAL(q_type{1}.binomial(1), 1ull);
        BOOST_CHECK_EQUAL(q_type{5}.binomial(3), int_type{10});
        BOOST_CHECK_EQUAL(q_type{-5}.binomial(4), 70);
        BOOST_CHECK_EQUAL(q_type{-5}.binomial(4), int_type{70});
        // Negative int on bottom, rational top.
        BOOST_CHECK_EQUAL(q_type(5, 3).binomial(-4), 0);
        BOOST_CHECK_EQUAL(q_type(5, 3).binomial(int_type{-1}), 0);
        // Type traits checking.
        BOOST_CHECK((has_binomial<q_type, int_type>::value));
        BOOST_CHECK((has_binomial<q_type, int>::value));
        BOOST_CHECK((has_binomial<q_type, unsigned short>::value));
        BOOST_CHECK((std::is_same<q_type, decltype(math::binomial(q_type{}, 4))>::value));
        BOOST_CHECK((std::is_same<q_type, decltype(math::binomial(q_type{}, int_type{4}))>::value));
        BOOST_CHECK((std::is_same<q_type, decltype(math::binomial(q_type{}, char(4)))>::value));
        BOOST_CHECK((has_binomial<q_type, double>::value));
        BOOST_CHECK((std::is_same<double, decltype(math::binomial(q_type{}, 4.))>::value));
        BOOST_CHECK((has_binomial<q_type, q_type>::value));
        BOOST_CHECK((std::is_same<double, decltype(math::binomial(q_type{}, q_type{}))>::value));
        BOOST_CHECK((has_binomial<int, q_type>::value));
        BOOST_CHECK((has_binomial<int_type, q_type>::value));
        BOOST_CHECK((has_binomial<long, q_type>::value));
        BOOST_CHECK((std::is_same<double, decltype(math::binomial(4, q_type{}))>::value));
        BOOST_CHECK((std::is_same<double, decltype(math::binomial(int_type(4), q_type{}))>::value));
        BOOST_CHECK((std::is_same<double, decltype(math::binomial((long long)4, q_type{}))>::value));
        // Some simple checks.
        BOOST_CHECK_EQUAL(q_type(3, 4).binomial(2), -q_type(3, 32));
        BOOST_CHECK_EQUAL(q_type(3, 4).binomial(10), -q_type(1057485, 268435456ll));
        BOOST_CHECK_EQUAL(q_type(3, 4).binomial(0), 1);
        BOOST_CHECK_EQUAL(q_type(3, 4).binomial(-1), 0);
        BOOST_CHECK_EQUAL(q_type(3, 4).binomial(-2), 0);
        BOOST_CHECK_EQUAL(q_type(3, 4).binomial(-10ll), 0);
        BOOST_CHECK_EQUAL(q_type(3, -4).binomial(0), 1);
        BOOST_CHECK_EQUAL(q_type(3, -4).binomial(1), -q_type(3, 4));
        BOOST_CHECK_EQUAL(q_type(3, -4).binomial(5), -q_type(4389, 8192));
        BOOST_CHECK_EQUAL(q_type(3, -4).binomial(-1), 0);
        BOOST_CHECK_EQUAL(q_type(3, -4).binomial(-5), 0);
        BOOST_CHECK_EQUAL(math::binomial(1, q_type{3, 4}), math::binomial(1., 3. / 4.));
        BOOST_CHECK_EQUAL(math::binomial(q_type{2, 3}, q_type{3, 4}), math::binomial(2. / 3., 3. / 4.));
        BOOST_CHECK_EQUAL(math::binomial(1.2, q_type{3, 4}), math::binomial(1.2, 3. / 4.));
        BOOST_CHECK_EQUAL(math::binomial(q_type{3, 4}, 1.2), math::binomial(3. / 4., 1.2));
        // NOTE: cannot really do random testing at the moment as the implementation of
        // generic_binomial is way too slow.
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_binomial_test)
{
    boost::mpl::for_each<size_types>(binomial_tester());
    BOOST_CHECK((!has_binomial<mp_rational<32>, mp_rational<16>>::value));
    BOOST_CHECK((!has_binomial<mp_rational<16>, mp_rational<32>>::value));
    BOOST_CHECK((!has_binomial<mp_rational<16>, mp_integer<32>>::value));
    BOOST_CHECK((!has_binomial<mp_rational<32>, mp_integer<16>>::value));
}

struct stream_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        q_type q(42, -5);
        std::stringstream ss;
        ss << q;
        BOOST_CHECK_EQUAL(ss.str(), "-42/5");
        q = 0;
        ss >> q;
        BOOST_CHECK_EQUAL(q, q_type(-42, 5));
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_stream_test)
{
    boost::mpl::for_each<size_types>(stream_tester());
}

struct safe_cast_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        using z_type = mp_integer<T::value>;
        // From q conversions.
        BOOST_CHECK((has_safe_cast<int, q_type>::value));
        BOOST_CHECK((has_safe_cast<int &, const q_type>::value));
        BOOST_CHECK((has_safe_cast<int &&, const q_type &>::value));
        BOOST_CHECK((!has_safe_cast<void, const q_type &>::value));
        BOOST_CHECK((has_safe_cast<unsigned, q_type>::value));
        BOOST_CHECK((has_safe_cast<unsigned &, const q_type>::value));
        BOOST_CHECK((has_safe_cast<unsigned &&, const q_type &>::value));
        BOOST_CHECK((has_safe_cast<const unsigned &, const q_type &>::value));
        BOOST_CHECK((has_safe_cast<z_type, q_type>::value));
        BOOST_CHECK_EQUAL(safe_cast<int>(q_type{0}), 0);
        BOOST_CHECK_EQUAL(safe_cast<int>(q_type{-4}), -4);
        BOOST_CHECK_EQUAL(safe_cast<unsigned>(q_type{0}), 0u);
        BOOST_CHECK_EQUAL(safe_cast<unsigned>(q_type{42}), 42u);
        BOOST_CHECK_EQUAL(safe_cast<z_type>(q_type{0} / 2), 0);
        BOOST_CHECK_EQUAL(safe_cast<z_type>(q_type{-42} / 2), -21);
        // Various types of failures.
        BOOST_CHECK_EXCEPTION(safe_cast<int>(q_type{std::numeric_limits<int>::max()} + 1), safe_cast_failure,
                              [](const safe_cast_failure &e) {
                                  return boost::contains(e.what(), "as the conversion cannot preserve the valu");
                              });
        BOOST_CHECK_EXCEPTION(safe_cast<int>(q_type{std::numeric_limits<int>::min()} - 1), safe_cast_failure,
                              [](const safe_cast_failure &e) {
                                  return boost::contains(e.what(), "as the conversion cannot preserve the valu");
                              });
        BOOST_CHECK_EXCEPTION(safe_cast<int>(q_type{-4} / 3), safe_cast_failure, [](const safe_cast_failure &e) {
            return boost::contains(e.what(), "as the rational value as non-unitary denominator");
        });
        BOOST_CHECK_THROW(safe_cast<unsigned>(q_type{-4}), safe_cast_failure);
        BOOST_CHECK_THROW(safe_cast<unsigned>(q_type{4} / 3), safe_cast_failure);
        BOOST_CHECK_THROW(safe_cast<z_type>(q_type{4} / 3), safe_cast_failure);
        // To q conversions.
        BOOST_CHECK((has_safe_cast<q_type, int>::value));
        BOOST_CHECK((has_safe_cast<const q_type, int &>::value));
        BOOST_CHECK((has_safe_cast<const q_type &, int &&>::value));
        BOOST_CHECK((has_safe_cast<const q_type &, const int &>::value));
        BOOST_CHECK((!has_safe_cast<q_type, void>::value));
        BOOST_CHECK((has_safe_cast<q_type, unsigned>::value));
        BOOST_CHECK((has_safe_cast<q_type, z_type>::value));
        BOOST_CHECK((has_safe_cast<q_type, double>::value));
        BOOST_CHECK_EQUAL(safe_cast<q_type>(-4), q_type{-4});
        BOOST_CHECK_EQUAL(safe_cast<q_type>(0), q_type{0});
        BOOST_CHECK_EQUAL(safe_cast<q_type>(4u), q_type{4});
        BOOST_CHECK_EQUAL(safe_cast<q_type>(0u), q_type{0u});
        BOOST_CHECK_EQUAL(safe_cast<q_type>(z_type{4}), q_type{4});
        BOOST_CHECK_EQUAL(safe_cast<q_type>(z_type{0}), q_type{0});
        // Floating point.
        static constexpr auto r = std::numeric_limits<double>::radix;
        BOOST_CHECK_EQUAL(safe_cast<q_type>(1. / r), q_type(1, r));
        BOOST_CHECK_EQUAL(safe_cast<q_type>(-13. / (r * r * r)), q_type(-13, r * r * r));
        if (std::numeric_limits<double>::has_infinity && std::numeric_limits<double>::has_quiet_NaN) {
            BOOST_CHECK_EXCEPTION(safe_cast<q_type>(std::numeric_limits<double>::infinity()), safe_cast_failure,
                                  [](const safe_cast_failure &e) {
                                      return boost::contains(e.what(),
                                                             "as the conversion would not preserve the value");
                                  });
            BOOST_CHECK_THROW(safe_cast<q_type>(std::numeric_limits<double>::quiet_NaN()), safe_cast_failure);
        }
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_safe_cast_test)
{
    boost::mpl::for_each<size_types>(safe_cast_tester());
}

struct serialization_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        std::uniform_int_distribution<int> int_dist(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        q_type tmp;
        for (int i = 0; i < ntries; ++i) {
            auto num(int_dist(rng)), den(int_dist(rng));
            q_type q{num, den};
            std::stringstream ss;
            {
                boost::archive::text_oarchive oa(ss);
                oa << q;
            }
            {
                boost::archive::text_iarchive ia(ss);
                ia >> tmp;
            }
            BOOST_CHECK_EQUAL(tmp, q);
            // NOTE: we don't check for the static/dynamic character here as the following could happen:
            // when initing q, a GCD computation is run which could turn num/den into dynamic from static
            // even if the final result fits into static. Then when deserializing, we would have a static
            // vs dynamic mismatch.
        }
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_serialization_test)
{
    boost::mpl::for_each<size_types>(serialization_tester());
}

struct is_unitary_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        BOOST_CHECK(!math::is_unitary(q_type{}));
        BOOST_CHECK(!math::is_unitary(q_type{-1}));
        BOOST_CHECK(!math::is_unitary(q_type{-1, 5}));
        BOOST_CHECK(!math::is_unitary(q_type{1, 5}));
        BOOST_CHECK(!math::is_unitary(q_type{5, -5}));
        BOOST_CHECK(math::is_unitary(q_type{1}));
        BOOST_CHECK(math::is_unitary(q_type{-1, -1}));
        BOOST_CHECK(math::is_unitary(q_type{-5, -5}));
        BOOST_CHECK(math::is_unitary(q_type{5, 5}));
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_is_unitary_test)
{
    boost::mpl::for_each<size_types>(is_unitary_tester());
}

struct divexact_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        BOOST_CHECK(has_exact_division<q_type>::value);
        q_type out;
        math::divexact(out, q_type{3}, q_type{-2});
        BOOST_CHECK_EQUAL(out, q_type{3} / q_type{-2});
        math::divexact(out, q_type{8}, q_type{-2});
        BOOST_CHECK_EQUAL(out, q_type{-4});
        BOOST_CHECK_THROW(math::divexact(out, q_type{8}, q_type{0}), zero_division_error);
        math::divexact(out, q_type{0}, q_type{-2});
        BOOST_CHECK_EQUAL(out, 0);
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_divexact_test)
{
    boost::mpl::for_each<size_types>(divexact_tester());
}

struct ero_tester {
    template <typename T>
    void operator()(const T &)
    {
        using q_type = mp_rational<T::value>;
        BOOST_CHECK(has_exact_ring_operations<q_type>::value);
        BOOST_CHECK(has_exact_ring_operations<const q_type>::value);
        BOOST_CHECK(has_exact_ring_operations<q_type &&>::value);
        BOOST_CHECK(has_exact_ring_operations<volatile q_type &&>::value);
    }
};

BOOST_AUTO_TEST_CASE(mp_rational_ero_test)
{
    boost::mpl::for_each<size_types>(ero_tester());
}
