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

#include "../src/real.hpp"

#define BOOST_TEST_MODULE real_01_test
#include <boost/test/included/unit_test.hpp>

#define FUSION_MAX_VECTOR_SIZE 20

#include <boost/algorithm/string/predicate.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/include/algorithm.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/lexical_cast.hpp>
#include <complex>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../src/binomial.hpp"
#include "../src/config.hpp"
#include "../src/detail/mpfr.hpp"
#include "../src/init.hpp"
#include "../src/math.hpp"
#include "../src/mp_integer.hpp"
#include "../src/mp_rational.hpp"
#include "../src/pow.hpp"
#include "../src/s11n.hpp"
#include "../src/safe_cast.hpp"
#include "../src/type_traits.hpp"

static_assert(MPFR_PREC_MIN <= 4 && MPFR_PREC_MAX >= 4,
              "The unit tests for piranha::real assume that 4 is a valid value for significand precision.");
static_assert(piranha::real::default_prec < MPFR_PREC_MAX, "The unit tests for piranha::real assume that the default "
                                                           "precision is strictly less-than the maximum MPFR "
                                                           "precision.");

static std::mt19937 rng;
static const int ntries = 1000;

using namespace piranha;

const boost::fusion::vector<char, signed char, short, int, long, long long, unsigned char, unsigned short, unsigned,
                            unsigned long, unsigned long long>
integral_values((char)42, (signed char)42, (short)42, -42, 42L, -42LL, (unsigned char)42, (unsigned short)42, 42U, 42UL,
                42ULL);

struct check_integral_construction {
    template <typename T>
    void operator()(const T &value) const
    {
        if (value > T(0)) {
            BOOST_CHECK_EQUAL("4.00e1", boost::lexical_cast<std::string>(real(value, 4)));
        } else {
            BOOST_CHECK_EQUAL("-4.00e1", boost::lexical_cast<std::string>(real(value, 4)));
        }
    }
};

BOOST_AUTO_TEST_CASE(real_constructors_test)
{
    init();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{}), "0.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"1.23"}), "1.22999999999999999999999999999999998");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"1.23", 4}), "1.25");
    if (MPFR_PREC_MIN > 0) {
        BOOST_CHECK_THROW((real{"1.23", 0}), std::invalid_argument);
        BOOST_CHECK_THROW((real{std::string("1.23"), 0}), std::invalid_argument);
    }
    BOOST_CHECK_THROW((real{"1a"}), std::invalid_argument);
    BOOST_CHECK_THROW((real{"1.a"}), std::invalid_argument);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"@NaN@"}), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"+@NaN@"}), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-@NaN@"}), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"@Inf@"}), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"+@Inf@"}), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-@Inf@"}), "-inf");
    // Copy constructor.
    real r1{"1.23", 4};
    real r2{r1};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "1.25");
    real r3{"-inf"};
    real r4(r3);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r4), "-inf");
    // Move constructor.
    real r5{std::move(r1)};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r5), "1.25");
    real r6{std::move(r3)};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r6), "-inf");
    // Copy ctor with different precision.
    if (MPFR_PREC_MIN > 0) {
        BOOST_CHECK_THROW((real{real{"1.23"}, 0}), std::invalid_argument);
    }
    BOOST_CHECK_EQUAL((real{real{"1.23"}, 4}.get_prec()), ::mpfr_prec_t(4));
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{real{"1.23"}, 4}), "1.25");
    // Generic constructor.
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{0.f, 4}), "0.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{4.f, 4}), "4.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{-4.f, 4}), "-4.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{.5f, 4}), "5.00e-1");
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{0., 4}), "0.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{4., 4}), "4.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{-4., 4}), "-4.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{-.5, 4}), "-5.00e-1");
    }
    if (std::numeric_limits<long double>::is_iec559 && std::numeric_limits<long double>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{0.l, 4}), "0.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{4.l, 4}), "4.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{-4.l, 4}), "-4.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{-.5l, 4}), "-5.00e-1");
    }
    // Construction from integral types.
    boost::fusion::for_each(integral_values, check_integral_construction());
    // Construction from integer and rational.
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{integer(), 4}), "0.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{integer(2), 4}), "2.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{integer(-10), 4}), "-1.00e1");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{mp_integer<16>(-10), 4}), "-1.00e1");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{rational(), 4}), "0.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{rational(2), 4}), "2.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{rational(-10), 4}), "-1.00e1");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{rational(-1, 2), 4}), "-5.00e-1");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{mp_rational<32>(-1, 2), 4}), "-5.00e-1");
    // Type trait.
    BOOST_CHECK((!std::is_constructible<real, std::unordered_map<int, std::string>>::value));
    BOOST_CHECK((!std::is_constructible<real, std::vector<double>>::value));
}

BOOST_AUTO_TEST_CASE(real_sign_test)
{
    BOOST_CHECK_EQUAL(real{}.sign(), 0);
    BOOST_CHECK_EQUAL(real{"1"}.sign(), 1);
    BOOST_CHECK_EQUAL(real{"-10.23"}.sign(), -1);
    BOOST_CHECK_EQUAL(real{"-0."}.sign(), 0);
    BOOST_CHECK_EQUAL(real{"-.0"}.sign(), 0);
    BOOST_CHECK_EQUAL(real{"1.23e5"}.sign(), 1);
    BOOST_CHECK_EQUAL(real{"1.23e-5"}.sign(), 1);
    BOOST_CHECK_EQUAL(real{"-1.23e-5"}.sign(), -1);
    BOOST_CHECK_EQUAL(real{"inf"}.sign(), 1);
    BOOST_CHECK_EQUAL(real{"-inf"}.sign(), -1);
    BOOST_CHECK_EQUAL(real{"nan"}.sign(), 0);
    BOOST_CHECK_EQUAL(real{"-nan"}.sign(), 0);
}

BOOST_AUTO_TEST_CASE(real_precision_test)
{
    BOOST_CHECK_EQUAL(real{}.get_prec(), real::default_prec);
    BOOST_CHECK_EQUAL((real{0.1, MPFR_PREC_MIN + 1}.get_prec()), MPFR_PREC_MIN + 1);
    real r{1};
    r.set_prec(4);
    BOOST_CHECK_EQUAL(r.get_prec(), ::mpfr_prec_t(4));
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
    if (MPFR_PREC_MIN > 0) {
        BOOST_CHECK_THROW(r.set_prec(0), std::invalid_argument);
    }
}

BOOST_AUTO_TEST_CASE(real_swap_test)
{
    real r1{-1, 4}, r2{0};
    r1.swap(r1);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-1.00");
    BOOST_CHECK_EQUAL(r1.get_prec(), ::mpfr_prec_t(4));
    r1.swap(r2);
    BOOST_CHECK_EQUAL(r1.get_prec(), real::default_prec);
    BOOST_CHECK_EQUAL(r2.get_prec(), ::mpfr_prec_t(4));
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "0.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "-1.00");
}

BOOST_AUTO_TEST_CASE(real_negate_test)
{
    real r1{-1, 4};
    r1.negate();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.00");
    r1 = 0;
    r1.negate();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-0.00");
    r1 = "inf";
    r1.negate();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-inf");
    r1.negate();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "inf");
    r1 = "nan";
    r1.negate();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "nan");
    r1 = 0;
    math::negate(r1);
    BOOST_CHECK_EQUAL(r1, 0);
    r1 = "inf";
    math::negate(r1);
    BOOST_CHECK_EQUAL(r1, real{"-inf"});
    r1 = 4;
    math::negate(r1);
    BOOST_CHECK_EQUAL(r1, -4);
}

struct check_integral_assignment {
    template <typename T>
    void operator()(const T &value) const
    {
        real r{0., 4};
        r = value;
        if (value > T(0)) {
            BOOST_CHECK_EQUAL("4.00e1", boost::lexical_cast<std::string>(r));
        } else {
            BOOST_CHECK_EQUAL("-4.00e1", boost::lexical_cast<std::string>(r));
        }
        real tmp(std::move(r));
        r = value;
        if (value > T(0)) {
            BOOST_CHECK_EQUAL("4.20000000000000000000000000000000000e1", boost::lexical_cast<std::string>(r));
        } else {
            BOOST_CHECK_EQUAL("-4.20000000000000000000000000000000000e1", boost::lexical_cast<std::string>(r));
        }
    }
};

BOOST_AUTO_TEST_CASE(real_assignment_test)
{
    real r1{-1, 4}, r2;
    r1 = r1;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-1.00");
    BOOST_CHECK_EQUAL(r1.get_prec(), ::mpfr_prec_t(4));
    r2 = r1;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "-1.00");
    BOOST_CHECK_EQUAL(r2.get_prec(), ::mpfr_prec_t(4));
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-1.00");
    BOOST_CHECK_EQUAL(r1.get_prec(), ::mpfr_prec_t(4));
    real r3;
    r3 = std::move(r2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r3), "-1.00");
    BOOST_CHECK_EQUAL(r3.get_prec(), ::mpfr_prec_t(4));
    // Revive r2.
    r2 = std::move(r1);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "-1.00");
    BOOST_CHECK_EQUAL(r2.get_prec(), ::mpfr_prec_t(4));
    // Revive r1.
    r1 = r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-1.00");
    BOOST_CHECK_EQUAL(r1.get_prec(), ::mpfr_prec_t(4));
    // Assignment from string.
    r1 = "1.2300000000001";
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.25");
    BOOST_CHECK_EQUAL(r1.get_prec(), ::mpfr_prec_t(4));
    r1 = std::string("1.2300000000001");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.25");
    BOOST_CHECK_EQUAL(r1.get_prec(), ::mpfr_prec_t(4));
    // Move r1 away.
    real r4{std::move(r1)};
    r1 = std::string("1.23");
    BOOST_CHECK_EQUAL(r1.get_prec(), real::default_prec);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.22999999999999999999999999999999998");
    BOOST_CHECK_THROW(r1 = "foo_the_bar", std::invalid_argument);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "0.00000000000000000000000000000000000");
    // Assignment to floating-point.
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        r1 = 0.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "0.00000000000000000000000000000000000");
        r1 = 4.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "4.00000000000000000000000000000000000");
        r1 = -.5f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00000000000000000000000000000000000e-1");
        real tmp(std::move(r1));
        r1 = -.5f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00000000000000000000000000000000000e-1");
    }
    r1 = real{0., 4};
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        r1 = 0.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "0.00");
        r1 = 4.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "4.00");
        r1 = -.5;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00e-1");
        real tmp(std::move(r1));
        r1 = -.5;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00000000000000000000000000000000000e-1");
    }
    r1 = real{0., 4};
    if (std::numeric_limits<long double>::is_iec559 && std::numeric_limits<long double>::radix == 2) {
        r1 = 0.l;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "0.00");
        r1 = 4.l;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "4.00");
        r1 = -.5l;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00e-1");
        real tmp(std::move(r1));
        r1 = -.5l;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00000000000000000000000000000000000e-1");
    }
    // Assignment from integrals.
    boost::fusion::for_each(integral_values, check_integral_assignment());
    // Assignment from integer and rational.
    r1.set_prec(4);
    r1 = integer(1);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.00");
    real{std::move(r1)};
    r1 = integer(2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "2.00000000000000000000000000000000000");
    r1 = mp_integer<16>(2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "2.00000000000000000000000000000000000");
    r1.set_prec(4);
    r1 = rational(1, 2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "5.00e-1");
    real{std::move(r1)};
    r1 = -rational(1, 2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00000000000000000000000000000000000e-1");
    r1 = -mp_rational<32>(1, 2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-5.00000000000000000000000000000000000e-1");
    // Type traits.
    BOOST_CHECK((std::is_assignable<real &, int>::value));
    BOOST_CHECK((!std::is_assignable<real &, std::vector<int>>::value));
}

BOOST_AUTO_TEST_CASE(real_is_inf_nan_test)
{
    BOOST_CHECK(!real{}.is_nan());
    BOOST_CHECK(!real{}.is_inf());
    BOOST_CHECK(!real{1}.is_nan());
    BOOST_CHECK(!real{1}.is_inf());
    BOOST_CHECK(real{"nan"}.is_nan());
    BOOST_CHECK(real{"-nan"}.is_nan());
    BOOST_CHECK(!real{"nan"}.is_inf());
    BOOST_CHECK(!real{"-nan"}.is_inf());
    BOOST_CHECK(real{"inf"}.is_inf());
    BOOST_CHECK(real{"-inf"}.is_inf());
    BOOST_CHECK(!real{"inf"}.is_nan());
    BOOST_CHECK(!real{"-inf"}.is_nan());
}

BOOST_AUTO_TEST_CASE(real_is_zero_test)
{
    BOOST_CHECK(real{}.is_zero());
    BOOST_CHECK(!real{2}.is_zero());
    BOOST_CHECK(!real{"inf"}.is_zero());
    BOOST_CHECK(!real{"-inf"}.is_zero());
    BOOST_CHECK(!real{"nan"}.is_zero());
    BOOST_CHECK(!real{"-nan"}.is_zero());
    BOOST_CHECK(math::is_zero(real{}));
    BOOST_CHECK(!math::is_zero(real{2}));
    BOOST_CHECK(!math::is_zero(real{"inf"}));
    BOOST_CHECK(!math::is_zero(real{"-inf"}));
    BOOST_CHECK(!math::is_zero(real{"nan"}));
    BOOST_CHECK(!math::is_zero(real{"-nan"}));
    BOOST_CHECK(has_is_zero<real>::value);
}

struct check_integral_conversion {
    template <typename T>
    void operator()(const T &value) const
    {
        BOOST_CHECK_THROW((void)static_cast<T>(real{"inf"}), std::overflow_error);
        BOOST_CHECK_THROW((void)static_cast<T>(real{"-inf"}), std::overflow_error);
        BOOST_CHECK_THROW((void)static_cast<T>(real{"nan"}), std::overflow_error);
        BOOST_CHECK_THROW((void)static_cast<T>(real{"-nan"}), std::overflow_error);
        BOOST_CHECK_EQUAL(static_cast<T>(real{value}), value);
        if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
            if (value > T(0)) {
                BOOST_CHECK_EQUAL(static_cast<T>(real{static_cast<double>(value) + 0.5}), value);
            } else {
                BOOST_CHECK_EQUAL(static_cast<T>(real{static_cast<double>(value) - 0.5}), value);
            }
        }
        BOOST_CHECK_THROW((void)static_cast<T>(real{integer(std::numeric_limits<T>::max()) * 2}), std::overflow_error);
        if (std::is_signed<T>::value) {
            BOOST_CHECK_THROW((void)static_cast<T>(real{integer(std::numeric_limits<T>::min()) * 2}),
                              std::overflow_error);
        }
    }
};

BOOST_AUTO_TEST_CASE(real_conversion_test)
{
    // Test boolean conversion.
    BOOST_CHECK(!real{});
    BOOST_CHECK(real{"0.5"});
    BOOST_CHECK(real{1});
    // Integer conversion.
    BOOST_CHECK_EQUAL(static_cast<integer>(real{}), 0);
    BOOST_CHECK_EQUAL(static_cast<integer>(real{"1.43111e4"}), 14311);
    BOOST_CHECK_EQUAL(static_cast<integer>(real{"-1.43111e4"}), -14311);
    BOOST_CHECK_EQUAL(static_cast<integer>(real{"1.43119e4"}), 14311);
    BOOST_CHECK_EQUAL(static_cast<integer>(real{"-1.43119e4"}), -14311);
    BOOST_CHECK_EQUAL(static_cast<mp_integer<32>>(real{"-1.43119e4"}), -14311);
    BOOST_CHECK_THROW((void)static_cast<integer>(real{"inf"}), std::overflow_error);
    BOOST_CHECK_THROW((void)static_cast<integer>(real{"-inf"}), std::overflow_error);
    BOOST_CHECK_THROW((void)static_cast<integer>(real{"nan"}), std::overflow_error);
    BOOST_CHECK_THROW((void)static_cast<integer>(real{"-nan"}), std::overflow_error);
    // Integral conversions.
    boost::fusion::for_each(integral_values, check_integral_conversion());
    // FP conversions. Double.
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        BOOST_CHECK_EQUAL(static_cast<double>(real{}), 0.);
        BOOST_CHECK_EQUAL(static_cast<double>(real{-10.}), -10.);
        BOOST_CHECK_EQUAL(static_cast<double>(real{0.5}), .5);
    }
    if (std::numeric_limits<double>::has_quiet_NaN) {
        BOOST_CHECK(static_cast<double>(real{"nan"}) != std::numeric_limits<double>::quiet_NaN());
    } else {
        BOOST_CHECK_THROW((void)static_cast<double>(real{"nan"}), std::overflow_error);
    }
    if (std::numeric_limits<double>::has_infinity) {
        BOOST_CHECK_EQUAL(static_cast<double>(real{"inf"}), std::numeric_limits<double>::infinity());
        BOOST_CHECK_EQUAL(static_cast<double>(real{"-inf"}), -std::numeric_limits<double>::infinity());
    } else {
        BOOST_CHECK_THROW((void)static_cast<double>(real{"inf"}), std::overflow_error);
        BOOST_CHECK_THROW((void)static_cast<double>(real{"-inf"}), std::overflow_error);
    }
    // Float.
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        BOOST_CHECK_EQUAL(static_cast<float>(real{}), 0.f);
        BOOST_CHECK_EQUAL(static_cast<float>(real{-10.f}), -10.f);
        BOOST_CHECK_EQUAL(static_cast<float>(real{0.5f}), .5f);
    }
    if (std::numeric_limits<float>::has_quiet_NaN) {
        BOOST_CHECK(static_cast<float>(real{"nan"}) != std::numeric_limits<float>::quiet_NaN());
    } else {
        BOOST_CHECK_THROW((void)static_cast<float>(real{"nan"}), std::overflow_error);
    }
    if (std::numeric_limits<float>::has_infinity) {
        BOOST_CHECK_EQUAL(static_cast<float>(real{"inf"}), std::numeric_limits<float>::infinity());
        BOOST_CHECK_EQUAL(static_cast<float>(real{"-inf"}), -std::numeric_limits<float>::infinity());
    } else {
        BOOST_CHECK_THROW((void)static_cast<float>(real{"inf"}), std::overflow_error);
        BOOST_CHECK_THROW((void)static_cast<float>(real{"-inf"}), std::overflow_error);
    }
    // FP conversions. Long double.
    if (std::numeric_limits<long double>::is_iec559 && std::numeric_limits<long double>::radix == 2) {
        BOOST_CHECK_EQUAL(static_cast<long double>(real{}), 0.l);
        BOOST_CHECK_EQUAL(static_cast<long double>(real{-10.l}), -10.l);
        BOOST_CHECK_EQUAL(static_cast<long double>(real{0.5l}), .5l);
    }
    if (std::numeric_limits<long double>::has_quiet_NaN) {
        BOOST_CHECK(static_cast<long double>(real{"nan"}) != std::numeric_limits<long double>::quiet_NaN());
    } else {
        BOOST_CHECK_THROW((void)static_cast<long double>(real{"nan"}), std::overflow_error);
    }
    if (std::numeric_limits<long double>::has_infinity) {
        BOOST_CHECK_EQUAL(static_cast<long double>(real{"inf"}), std::numeric_limits<long double>::infinity());
        BOOST_CHECK_EQUAL(static_cast<long double>(real{"-inf"}), -std::numeric_limits<long double>::infinity());
    } else {
        BOOST_CHECK_THROW((void)static_cast<long double>(real{"inf"}), std::overflow_error);
        BOOST_CHECK_THROW((void)static_cast<long double>(real{"-inf"}), std::overflow_error);
    }
    // Rational.
    BOOST_CHECK_EQUAL(static_cast<rational>(real{}), 0);
    BOOST_CHECK_EQUAL(static_cast<rational>(real{1}), 1);
    BOOST_CHECK_EQUAL(static_cast<rational>(real{12}), 12);
    BOOST_CHECK_EQUAL(static_cast<rational>(real{-1234}), -1234);
    BOOST_CHECK_EQUAL(static_cast<rational>(real{"-0.5"}), rational(-1, 2));
    BOOST_CHECK_EQUAL(static_cast<rational>(real{"0.03125"}), rational(1, 32));
    BOOST_CHECK_EQUAL(static_cast<rational>(real{"-7.59375"}), rational(243, -32));
    BOOST_CHECK_EQUAL(static_cast<mp_rational<16>>(real{"-7.59375"}), mp_rational<16>(243, -32));
    BOOST_CHECK_THROW((void)static_cast<rational>(real{"nan"}), std::overflow_error);
    BOOST_CHECK_THROW((void)static_cast<rational>(real{"inf"}), std::overflow_error);
    BOOST_CHECK_THROW((void)static_cast<rational>(real{"-inf"}), std::overflow_error);
// Type traits.
// This should be the same problem as in the explicit integer conversion: IIRC older clang/libc++ versions
// did not consider explicit conversion operators when checking for constructability.
#if !defined(PIRANHA_COMPILER_IS_CLANG)
    BOOST_CHECK((std::is_constructible<float, real>::value));
#endif
    // Check that the type trait above actually works in practice.
    (void)float(real{12});
    BOOST_CHECK((!std::is_constructible<std::vector<float>, real>::value));
}

struct check_in_place_add_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        real r{0., 4};
        r += value;
        BOOST_CHECK_EQUAL(r.get_prec(), ::mpfr_prec_t(4));
        if (value > T(0)) {
            BOOST_CHECK_EQUAL("4.00e1", boost::lexical_cast<std::string>(r));
        } else {
            BOOST_CHECK_EQUAL("-4.00e1", boost::lexical_cast<std::string>(r));
        }
        BOOST_CHECK((std::is_same<real &, decltype(r += value)>::value));
        // Integral on the left.
        T other(1);
        other += real("1.", 4);
        BOOST_CHECK_EQUAL(T(2), other);
        other += real("2.5");
        BOOST_CHECK_EQUAL(T(4), other);
        BOOST_CHECK_THROW(other += real("inf"), std::overflow_error);
        BOOST_CHECK((std::is_same<T &, decltype(other += real("1.", 4))>::value));
        BOOST_CHECK((is_addable_in_place<real, real>::value));
        BOOST_CHECK((is_addable_in_place<real, T>::value));
        BOOST_CHECK((is_addable_in_place<T, real>::value));
        BOOST_CHECK((!is_addable_in_place<const T, real>::value));
        BOOST_CHECK((!is_addable_in_place<real, std::string>::value));
    }
};

struct check_binary_add_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        if (value > T(0)) {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} + value),
                              "4.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value + real{1}),
                              "4.30000000000000000000000000000000000e1");
        } else {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} + value),
                              "-4.10000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value + real{1}),
                              "-4.10000000000000000000000000000000000e1");
        }
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value + real{"inf"}), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} + value), "-inf");
        BOOST_CHECK((std::is_same<real, decltype(real{1} + value)>::value));
        BOOST_CHECK((std::is_same<real, decltype(value + real{1})>::value));
        BOOST_CHECK((std::is_same<real, decltype(real{1} + real{1})>::value));
        BOOST_CHECK((is_addable<real, T>::value));
        BOOST_CHECK((is_addable<T, real>::value));
        BOOST_CHECK((is_addable<real, real>::value));
        BOOST_CHECK((!is_addable<real, std::string>::value));
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} + value).get_prec(), 4);
        BOOST_CHECK_EQUAL((value + real{3, 4}).get_prec(), 4);
    }
};

BOOST_AUTO_TEST_CASE(real_addition_test)
{
    // In-place addition.
    real r1{1, 4}, r2{2, 4};
    r1 += r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "3.00");
    // With self.
    r2 += r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "4.00");
    r1 += real{1};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "4.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(r1.get_prec(), real::default_prec);
    r1 += rational(1, 2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "4.50000000000000000000000000000000000");
    r1 += integer(2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "6.50000000000000000000000000000000000");
    BOOST_CHECK((is_addable<real, integer>::value));
    BOOST_CHECK((is_addable<real, mp_integer<16>>::value));
    BOOST_CHECK((is_addable<mp_integer<16>, real>::value));
    BOOST_CHECK((!is_addable_in_place<const mp_integer<16>, real>::value));
    BOOST_CHECK((is_addable<real, rational>::value));
    BOOST_CHECK((is_addable<real, mp_rational<16>>::value));
    BOOST_CHECK((is_addable<mp_rational<16>, real>::value));
    BOOST_CHECK((!is_addable_in_place<const mp_rational<16>, real>::value));
    // Precision handling.
    BOOST_CHECK_EQUAL((real{3, 4} + integer()).get_prec(), 4);
    BOOST_CHECK_EQUAL((integer() + real{3, 4}).get_prec(), 4);
    BOOST_CHECK_EQUAL((real{3, 4} + rational()).get_prec(), 4);
    BOOST_CHECK_EQUAL((rational() + real{3, 4}).get_prec(), 4);
    auto r1a = real{3, 4};
    BOOST_CHECK_EQUAL((r1a += integer()).get_prec(), 4);
    BOOST_CHECK_EQUAL((r1a += rational()).get_prec(), 4);
    // Rational and integer on the left.
    rational q(1, 2);
    q += real("-1.5");
    BOOST_CHECK_EQUAL(-1, q);
    BOOST_CHECK_THROW(q += real("inf"), std::overflow_error);
    integer n(4);
    n += real("3", 4);
    BOOST_CHECK_EQUAL(n, 7);
    n += real("1.001");
    BOOST_CHECK_EQUAL(n, 8);
    n += real("0.99");
    BOOST_CHECK_EQUAL(n, 8);
    BOOST_CHECK_THROW(n += real("nan"), std::overflow_error);
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        r1 += 2.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "8.50000000000000000000000000000000000");
        r1 += 2.5f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.10000000000000000000000000000000000e1");
        r1 += -4.5f;
        float x = 4.f;
        x += real(".5");
        BOOST_CHECK_EQUAL(x, 4.5f);
        BOOST_CHECK((std::is_same<real &, decltype(r1 += 2.f)>::value));
        BOOST_CHECK((is_addable_in_place<real, float>::value));
        BOOST_CHECK((is_addable_in_place<float, real>::value));
        BOOST_CHECK((!is_addable_in_place<real, std::string>::value));
        BOOST_CHECK((std::is_same<float &, decltype(x += real(".5"))>::value));
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} + 0.f).get_prec(), 4);
        BOOST_CHECK_EQUAL((0.f + real{3, 4}).get_prec(), 4);
        r1a = real{3, 4};
        BOOST_CHECK_EQUAL((r1a += 0.f).get_prec(), 4);
    }
    if (std::numeric_limits<float>::has_infinity) {
        real r;
        r += std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "inf");
        r = 0;
        r += -std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "-inf");
        r += std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        float x = 4.f;
        x += real("inf");
        BOOST_CHECK_EQUAL(x, std::numeric_limits<float>::infinity());
        x = 0.f;
        x += real("-inf");
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<float>::infinity());
        if (std::numeric_limits<float>::has_quiet_NaN) {
            x += real("inf");
            BOOST_CHECK(x != std::numeric_limits<float>::quiet_NaN());
        }
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        r1 += 2.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "8.50000000000000000000000000000000000");
        r1 += 2.5;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.10000000000000000000000000000000000e1");
        r1 += -4.5;
        double x = 4.;
        x += real(".5");
        BOOST_CHECK_EQUAL(x, 4.5);
    }
    if (std::numeric_limits<double>::has_infinity) {
        real r;
        r += std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "inf");
        r = 0;
        r += -std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "-inf");
        r += std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        double x = 4.;
        x += real("inf");
        BOOST_CHECK_EQUAL(x, std::numeric_limits<double>::infinity());
        x = 0.;
        x += real("-inf");
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<double>::infinity());
        if (std::numeric_limits<double>::has_quiet_NaN) {
            x += real("inf");
            BOOST_CHECK(x != std::numeric_limits<double>::quiet_NaN());
        }
    }
    if (std::numeric_limits<long double>::is_iec559 && std::numeric_limits<long double>::radix == 2) {
        r1 += 2.l;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "8.50000000000000000000000000000000000");
        r1 += 2.5l;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.10000000000000000000000000000000000e1");
        r1 += -4.5l;
        long double x = 4.l;
        x += real(".5");
        BOOST_CHECK_EQUAL(x, 4.5l);
    }
    if (std::numeric_limits<long double>::has_infinity) {
        real r;
        r += std::numeric_limits<long double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "inf");
        r = 0;
        r += -std::numeric_limits<long double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "-inf");
        r += std::numeric_limits<long double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        long double x = 4.l;
        x += real("inf");
        BOOST_CHECK_EQUAL(x, std::numeric_limits<long double>::infinity());
        x = 0.l;
        x += real("-inf");
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<long double>::infinity());
        if (std::numeric_limits<long double>::has_quiet_NaN) {
            x += real("inf");
            BOOST_CHECK(x != std::numeric_limits<long double>::quiet_NaN());
        }
    }
    boost::fusion::for_each(integral_values, check_in_place_add_integral());
    // Binary addition.
    r1 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 + r1), "4.00000000000000000000000000000000000");
    r2 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 + r2), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 + r2), "4.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 + r1), "4.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 + real("inf")), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-inf") + r2), "-inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-nan") + r2), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 + integer(1)), "3.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(1) + r2), "3.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 + mp_integer<16>(1)), "3.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_integer<32>(1) + r2), "3.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} + integer(1)), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(1) + real{"inf"}), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 + rational(1, 2)), "2.50");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(rational(1, 2) + r2), "2.50");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 + mp_rational<32>(1, 2)), "2.50");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_rational<8>(1, 2) + r2), "2.50");
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f + real{1, 4}), "1.50");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} + .5f), "1.50");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f + real{"inf"}), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} + .5f), "-inf");
    }
    if (std::numeric_limits<float>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} + std::numeric_limits<float>::infinity()), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<float>::infinity() + real{1}), "-inf");
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 + real{1, 4}), "1.50");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} + .5), "1.50");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 + real{"inf"}), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} + .5), "-inf");
    }
    if (std::numeric_limits<double>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} + std::numeric_limits<double>::infinity()), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<double>::infinity() + real{1}), "-inf");
    }
    if (std::numeric_limits<long double>::is_iec559 && std::numeric_limits<long double>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5l + real{1, 4}), "1.50");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} + .5l), "1.50");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5l + real{"inf"}), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} + .5l), "-inf");
    }
    if (std::numeric_limits<long double>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} + std::numeric_limits<long double>::infinity()),
                          "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<long double>::infinity() + real{1}),
                          "-inf");
    }
    boost::fusion::for_each(integral_values, check_binary_add_integral());
    // Increment operators.
    r2 = 4;
    BOOST_CHECK_EQUAL(++r2, 5);
    BOOST_CHECK_EQUAL(r2++, 5);
    BOOST_CHECK_EQUAL(r2, 6);
    r2 = ".5";
    BOOST_CHECK_EQUAL(++r2, real{"1.5"});
    BOOST_CHECK_EQUAL(r2++, real{"1.5"});
    BOOST_CHECK_EQUAL(r2, real{"2.5"});
}

BOOST_AUTO_TEST_CASE(real_identity_operator)
{
    real r{"1.5"};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(+r), "1.50000000000000000000000000000000000");
    r = "-1.5";
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(+r), "-1.50000000000000000000000000000000000");
}

struct check_in_place_sub_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        real r{0., 4};
        r -= value;
        BOOST_CHECK_EQUAL(r.get_prec(), ::mpfr_prec_t(4));
        if (value > T(0)) {
            BOOST_CHECK_EQUAL("-4.00e1", boost::lexical_cast<std::string>(r));
        } else {
            BOOST_CHECK_EQUAL("4.00e1", boost::lexical_cast<std::string>(r));
        }
        BOOST_CHECK((std::is_same<real &, decltype(r -= value)>::value));
        // Integral on the left.
        T other(1);
        other -= real("1.", 4);
        BOOST_CHECK_EQUAL(T(0), other);
        other -= real("-2.5");
        BOOST_CHECK_EQUAL(T(2), other);
        BOOST_CHECK_THROW(other -= real("inf"), std::overflow_error);
        BOOST_CHECK((std::is_same<T &, decltype(other -= real("1.", 4))>::value));
        BOOST_CHECK((is_subtractable_in_place<real, real>::value));
        BOOST_CHECK((is_subtractable_in_place<real, T>::value));
        BOOST_CHECK((is_subtractable_in_place<T, real>::value));
        BOOST_CHECK((!is_subtractable_in_place<const T, real>::value));
        BOOST_CHECK((!is_subtractable_in_place<real, std::string>::value));
    }
};

struct check_binary_sub_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        if (value > T(0)) {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} - value),
                              "-4.10000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value - real{1}),
                              "4.10000000000000000000000000000000000e1");
        } else {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} - value),
                              "4.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value - real{1}),
                              "-4.30000000000000000000000000000000000e1");
        }
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value - real{"inf"}), "-inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} - value), "inf");
        BOOST_CHECK((std::is_same<real, decltype(real{1} - value)>::value));
        BOOST_CHECK((std::is_same<real, decltype(value - real{1})>::value));
        BOOST_CHECK((std::is_same<real, decltype(real{1} - real{1})>::value));
        BOOST_CHECK((is_subtractable<real, T>::value));
        BOOST_CHECK((is_subtractable<T, real>::value));
        BOOST_CHECK((is_subtractable<real, real>::value));
        BOOST_CHECK((!is_subtractable<real, std::string>::value));
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} - value).get_prec(), 4);
        BOOST_CHECK_EQUAL((value - real{3, 4}).get_prec(), 4);
    }
};

BOOST_AUTO_TEST_CASE(real_subtraction_test)
{
    // In-place subtraction.
    real r1{1, 4}, r2{2, 4};
    r1 -= r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-1.00");
    // With self.
    r2 -= r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "0.00");
    r1 -= real{1};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-2.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(r1.get_prec(), real::default_prec);
    r1 -= rational(1, 2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-2.50000000000000000000000000000000000");
    r1 -= integer(2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-4.50000000000000000000000000000000000");
    BOOST_CHECK((is_subtractable<real, integer>::value));
    BOOST_CHECK((is_subtractable<real, mp_integer<16>>::value));
    BOOST_CHECK((is_subtractable<mp_integer<16>, real>::value));
    BOOST_CHECK((std::is_same<real, decltype(real{} - integer{})>::value));
    BOOST_CHECK((std::is_same<real, decltype(integer{} - real{})>::value));
    BOOST_CHECK((!is_subtractable_in_place<const mp_integer<16>, real>::value));
    BOOST_CHECK((is_subtractable<real, rational>::value));
    BOOST_CHECK((is_subtractable<real, mp_rational<16>>::value));
    BOOST_CHECK((is_subtractable<mp_rational<16>, real>::value));
    BOOST_CHECK((std::is_same<real, decltype(real{} - rational{})>::value));
    BOOST_CHECK((std::is_same<real, decltype(rational{} - real{})>::value));
    BOOST_CHECK((!is_subtractable_in_place<const mp_rational<16>, real>::value));
    // Precision handling.
    BOOST_CHECK_EQUAL((real{3, 4} - integer()).get_prec(), 4);
    BOOST_CHECK_EQUAL((integer() - real{3, 4}).get_prec(), 4);
    BOOST_CHECK_EQUAL((real{3, 4} - rational()).get_prec(), 4);
    BOOST_CHECK_EQUAL((rational() - real{3, 4}).get_prec(), 4);
    auto r1a = real{3, 4};
    BOOST_CHECK_EQUAL((r1a -= integer()).get_prec(), 4);
    BOOST_CHECK_EQUAL((r1a -= rational()).get_prec(), 4);
    // Rational and integer on the left.
    rational q(1, 2);
    q -= real("1.5");
    BOOST_CHECK_EQUAL(-1, q);
    BOOST_CHECK_THROW(q -= real("inf"), std::overflow_error);
    integer n(4);
    n -= real("3", 4);
    BOOST_CHECK_EQUAL(n, 1);
    n -= real("1.001");
    BOOST_CHECK_EQUAL(n, 0);
    n -= real("0.99");
    BOOST_CHECK_EQUAL(n, 0);
    BOOST_CHECK_THROW(n -= real("nan"), std::overflow_error);
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        r1 -= 2.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-6.50000000000000000000000000000000000");
        r1 -= 2.5f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-9.00000000000000000000000000000000000");
        r1 -= -4.5f;
        float x = 4.f;
        x -= real(".5");
        BOOST_CHECK_EQUAL(x, 3.5f);
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} - 0.f).get_prec(), 4);
        BOOST_CHECK_EQUAL((0.f - real{3, 4}).get_prec(), 4);
        r1a = real{3, 4};
        BOOST_CHECK_EQUAL((r1a -= 0.f).get_prec(), 4);
    }
    if (std::numeric_limits<float>::has_infinity) {
        real r;
        r -= std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "-inf");
        r = 0;
        r -= -std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "inf");
        r -= std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        float x = 4.f;
        x -= real("inf");
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<float>::infinity());
        x = 0.f;
        x -= real("-inf");
        BOOST_CHECK_EQUAL(x, std::numeric_limits<float>::infinity());
        if (std::numeric_limits<float>::has_quiet_NaN) {
            x -= real("inf");
            BOOST_CHECK(x != std::numeric_limits<float>::quiet_NaN());
        }
        BOOST_CHECK((std::is_same<real &, decltype(r1 -= 2.f)>::value));
        BOOST_CHECK((is_subtractable_in_place<real, float>::value));
        BOOST_CHECK((is_subtractable_in_place<float, real>::value));
        BOOST_CHECK((!is_subtractable_in_place<real, std::string>::value));
        BOOST_CHECK((std::is_same<float &, decltype(x -= real(".5"))>::value));
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        r1 -= 2.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-6.50000000000000000000000000000000000");
        r1 -= 2.5;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-9.00000000000000000000000000000000000");
        r1 -= -4.5;
        double x = 4.;
        x -= real(".5");
        BOOST_CHECK_EQUAL(x, 3.5);
    }
    if (std::numeric_limits<double>::has_infinity) {
        real r;
        r -= std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "-inf");
        r = 0;
        r -= -std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "inf");
        r -= std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        double x = 4.;
        x -= real("inf");
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<double>::infinity());
        x = 0.;
        x -= real("-inf");
        BOOST_CHECK_EQUAL(x, std::numeric_limits<double>::infinity());
        if (std::numeric_limits<double>::has_quiet_NaN) {
            x -= real("inf");
            BOOST_CHECK(x != std::numeric_limits<double>::quiet_NaN());
        }
    }
    boost::fusion::for_each(integral_values, check_in_place_sub_integral());
    // Binary subtraction.
    r1 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 - r1), "0.00000000000000000000000000000000000");
    r2 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 - r2), "0.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 - r2), "0.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 - r1), "0.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 - real("inf")), "-inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-inf") - r2), "-inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-nan") - r2), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 - integer(1)), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(1) - r2), "-1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 - mp_integer<16>(1)), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_integer<32>(1) - r2), "-1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} - integer(1)), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(1) - real{"inf"}), "-inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 - rational(1, 2)), "1.50");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(rational(1, 2) - r2), "-1.50");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 - mp_rational<16>(1, 2)), "1.50");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_rational<32>(1, 2) - r2), "-1.50");
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f - real{1, 4}), "-5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} - .5f), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f - real{"inf"}), "-inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} - .5f), "inf");
    }
    if (std::numeric_limits<float>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} - std::numeric_limits<float>::infinity()), "-inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<float>::infinity() - real{1}), "-inf");
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 - real{1, 4}), "-5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} - .5), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 - real{"inf"}), "-inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} - .5), "inf");
    }
    if (std::numeric_limits<double>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} - std::numeric_limits<double>::infinity()), "-inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(std::numeric_limits<double>::infinity() - real{1}), "inf");
    }
    boost::fusion::for_each(integral_values, check_binary_sub_integral());
    // Decrement operators.
    r2 = 0;
    BOOST_CHECK_EQUAL(--r2, -1);
    BOOST_CHECK_EQUAL(r2--, -1);
    BOOST_CHECK_EQUAL(r2, -2);
    r2 = "1.5";
    BOOST_CHECK_EQUAL(--r2, real{"0.5"});
    BOOST_CHECK_EQUAL(r2--, real{"0.5"});
    BOOST_CHECK_EQUAL(r2, real{"-0.5"});
}

struct check_in_place_mul_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        real r{"1.5", 4};
        r *= value;
        BOOST_CHECK((std::is_same<real &, decltype(r *= value)>::value));
        BOOST_CHECK_EQUAL(r.get_prec(), ::mpfr_prec_t(4));
        if (value > T(0)) {
            BOOST_CHECK_EQUAL("6.40e1", boost::lexical_cast<std::string>(r));
        } else {
            BOOST_CHECK_EQUAL("-6.40e1", boost::lexical_cast<std::string>(r));
        }
        // Integral on the left.
        T other(1);
        other *= real("2.", 4);
        BOOST_CHECK_EQUAL(T(2), other);
        other *= real("2.5");
        BOOST_CHECK_EQUAL(T(5), other);
        BOOST_CHECK_THROW(other *= real("inf"), std::overflow_error);
        BOOST_CHECK((std::is_same<T &, decltype(other *= real("1.", 4))>::value));
        BOOST_CHECK((is_multipliable_in_place<real, real>::value));
        BOOST_CHECK((is_multipliable_in_place<real, T>::value));
        BOOST_CHECK((is_multipliable_in_place<T, real>::value));
        BOOST_CHECK((!is_multipliable_in_place<const T, real>::value));
        BOOST_CHECK((!is_multipliable_in_place<real, std::string>::value));
    }
};

struct check_binary_mul_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        if (value > T(0)) {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"1.5"} * value),
                              "6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"1.5"}),
                              "6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"inf"}), "inf");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} * value), "inf");
        } else {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"1.5"} * value),
                              "-6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"1.5"}),
                              "-6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"inf"}), "-inf");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} * value), "-inf");
        }
        BOOST_CHECK((std::is_same<real, decltype(real{1} * value)>::value));
        BOOST_CHECK((std::is_same<real, decltype(value * real{1})>::value));
        BOOST_CHECK((std::is_same<real, decltype(real{1} * real{1})>::value));
        BOOST_CHECK((is_multipliable<real, T>::value));
        BOOST_CHECK((is_multipliable<T, real>::value));
        BOOST_CHECK((is_multipliable<real, real>::value));
        BOOST_CHECK((!is_multipliable<real, std::string>::value));
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} * value).get_prec(), 4);
        BOOST_CHECK_EQUAL((value * real{3, 4}).get_prec(), 4);
    }
};

BOOST_AUTO_TEST_CASE(real_multiplication_test)
{
    // In-place multiplication.
    real r1{1, 4}, r2{2, 4};
    r1 *= r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "2.00");
    // With self.
    r2 *= r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "4.00");
    r1 *= real{"1.5"};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "3.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(r1.get_prec(), real::default_prec);
    r1 *= rational(1, 2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.50000000000000000000000000000000000");
    r1 *= integer(2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "3.00000000000000000000000000000000000");
    BOOST_CHECK((is_multipliable<real, integer>::value));
    BOOST_CHECK((is_multipliable<real, mp_integer<16>>::value));
    BOOST_CHECK((is_multipliable<mp_integer<16>, real>::value));
    BOOST_CHECK((!is_multipliable_in_place<const mp_integer<16>, real>::value));
    BOOST_CHECK((is_multipliable<real, rational>::value));
    BOOST_CHECK((is_multipliable<real, mp_rational<16>>::value));
    BOOST_CHECK((is_multipliable<mp_rational<16>, real>::value));
    BOOST_CHECK((!is_multipliable_in_place<const mp_rational<16>, real>::value));
    // Precision handling.
    BOOST_CHECK_EQUAL((real{3, 4} * integer()).get_prec(), 4);
    BOOST_CHECK_EQUAL((integer() * real{3, 4}).get_prec(), 4);
    BOOST_CHECK_EQUAL((real{3, 4} * rational()).get_prec(), 4);
    BOOST_CHECK_EQUAL((rational() * real{3, 4}).get_prec(), 4);
    auto r1a = real{3, 4};
    BOOST_CHECK_EQUAL((r1a *= integer()).get_prec(), 4);
    BOOST_CHECK_EQUAL((r1a *= rational()).get_prec(), 4);
    // Rational and integer on the left.
    rational q(1, 2);
    q *= real(2);
    BOOST_CHECK_EQUAL(1, q);
    BOOST_CHECK_THROW(q *= real("inf"), std::overflow_error);
    integer n(4);
    n *= real("3", 4);
    BOOST_CHECK_EQUAL(n, 12);
    n *= real("1.001");
    BOOST_CHECK_EQUAL(n, 12);
    n *= real("0.99");
    BOOST_CHECK_EQUAL(n, 11);
    BOOST_CHECK_THROW(n *= real("nan"), std::overflow_error);
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        r1 *= 2.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "6.00000000000000000000000000000000000");
        r1 *= 2.5f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.50000000000000000000000000000000000e1");
        r1 = 3;
        float x = 4.f;
        x *= real(".5");
        BOOST_CHECK_EQUAL(x, 2.f);
        BOOST_CHECK((std::is_same<real &, decltype(r1 *= 2.f)>::value));
        BOOST_CHECK((is_multipliable_in_place<real, float>::value));
        BOOST_CHECK((is_multipliable_in_place<float, real>::value));
        BOOST_CHECK((!is_multipliable_in_place<real, std::string>::value));
        BOOST_CHECK((std::is_same<float &, decltype(x *= real(".5"))>::value));
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} * 0.f).get_prec(), 4);
        BOOST_CHECK_EQUAL((0.f * real{3, 4}).get_prec(), 4);
        r1a = real{3, 4};
        BOOST_CHECK_EQUAL((r1a *= 0.f).get_prec(), 4);
    }
    if (std::numeric_limits<float>::has_infinity) {
        real r(1);
        r *= std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "inf");
        r = 0;
        r *= -std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        float x = 4.f;
        x *= real("inf");
        BOOST_CHECK_EQUAL(x, std::numeric_limits<float>::infinity());
        x = 1.f;
        x *= real("-inf");
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<float>::infinity());
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        r1 *= 2.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "6.00000000000000000000000000000000000");
        r1 *= 2.5;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "1.50000000000000000000000000000000000e1");
        r1 = 3;
        double x = 4.;
        x *= real(".5");
        BOOST_CHECK_EQUAL(x, 2.);
    }
    if (std::numeric_limits<double>::has_infinity) {
        real r(1);
        r *= std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "inf");
        r = 0;
        r *= -std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        double x = 4.;
        x *= real("-inf");
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<double>::infinity());
        x = 1.;
        x *= real("inf");
        BOOST_CHECK_EQUAL(x, std::numeric_limits<double>::infinity());
    }
    boost::fusion::for_each(integral_values, check_in_place_mul_integral());
    // Binary multiplication.
    r1 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 * r1), "4.00000000000000000000000000000000000");
    r2 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 * r2), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 * r2), "4.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 * r1), "4.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 * real("inf")), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-inf") * r2), "-inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-nan") * r2), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 * integer(2)), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(2) * r2), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 * mp_integer<16>(2)), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_integer<32>(2) * r2), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} * integer(1)), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(1) * real{"inf"}), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 * rational(1, 2)), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(rational(1, 2) * r2), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 * mp_rational<16>(1, 2)), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_rational<32>(1, 2) * r2), "1.00");
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f * real{1, 4}), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} * .5f), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f * real{"inf"}), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} * .5f), "-inf");
    }
    if (std::numeric_limits<float>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} * std::numeric_limits<float>::infinity()), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<float>::infinity() * real{1}), "-inf");
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 * real{1, 4}), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} * .5), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 * real{"inf"}), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} * .5), "-inf");
    }
    if (std::numeric_limits<double>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} * std::numeric_limits<double>::infinity()), "inf");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<double>::infinity() * real{1}), "-inf");
    }
    boost::fusion::for_each(integral_values, check_binary_mul_integral());
}

struct check_in_place_div_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        real r{"63", 4};
        r /= value;
        BOOST_CHECK((std::is_same<real &, decltype(r /= value)>::value));
        BOOST_CHECK_EQUAL(r.get_prec(), ::mpfr_prec_t(4));
        if (value > T(0)) {
            BOOST_CHECK_EQUAL("1.50", boost::lexical_cast<std::string>(r));
        } else {
            BOOST_CHECK_EQUAL("-1.50", boost::lexical_cast<std::string>(r));
        }
        // Integral on the left.
        T other(4);
        other /= real("2.", 4);
        BOOST_CHECK_EQUAL(T(2), other);
        other /= real(2);
        BOOST_CHECK_EQUAL(T(1), other);
        other /= real("inf");
        BOOST_CHECK_EQUAL(T(0), other);
        r = 1;
        r /= T(0);
        BOOST_CHECK_EQUAL("inf", boost::lexical_cast<std::string>(r));
        r = -1;
        r /= T(0);
        BOOST_CHECK_EQUAL("-inf", boost::lexical_cast<std::string>(r));
        BOOST_CHECK((std::is_same<T &, decltype(other /= real("1.", 4))>::value));
        BOOST_CHECK((is_divisible_in_place<real, real>::value));
        BOOST_CHECK((is_divisible_in_place<real, T>::value));
        BOOST_CHECK((is_divisible_in_place<T, real>::value));
        BOOST_CHECK((!is_divisible_in_place<const T, real>::value));
        BOOST_CHECK((!is_divisible_in_place<real, std::string>::value));
    }
};

struct check_binary_div_integral {
    template <typename T>
    void operator()(const T &value) const
    {
        if (value > T(0)) {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"1.5"} * value),
                              "6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"1.5"}),
                              "6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"inf"}), "inf");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} * value), "inf");
        } else {
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"1.5"} * value),
                              "-6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"1.5"}),
                              "-6.30000000000000000000000000000000000e1");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value * real{"inf"}), "-inf");
            BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} * value), "-inf");
        }
        BOOST_CHECK((std::is_same<real, decltype(real{1} / value)>::value));
        BOOST_CHECK((std::is_same<real, decltype(value / real{1})>::value));
        BOOST_CHECK((std::is_same<real, decltype(real{1} / real{1})>::value));
        BOOST_CHECK((is_divisible<real, T>::value));
        BOOST_CHECK((is_divisible<T, real>::value));
        BOOST_CHECK((is_divisible<real, real>::value));
        BOOST_CHECK((!is_divisible<real, std::string>::value));
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} / value).get_prec(), 4);
        BOOST_CHECK_EQUAL((value / real{3, 4}).get_prec(), 4);
    }
};

BOOST_AUTO_TEST_CASE(real_division_test)
{
    // In-place multiplication.
    real r1{1, 4}, r2{2, 4};
    r1 /= r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "5.00e-1");
    // With self.
    r2 /= r2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2), "1.00");
    r1 /= -real{"2."};
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-2.50000000000000000000000000000000000e-1");
    BOOST_CHECK_EQUAL(r1.get_prec(), real::default_prec);
    r1 /= real();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-inf");
    r1 /= real();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-inf");
    r1 /= real("inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "nan");
    r1 = "-2.5e-1";
    r1 /= rational(1, -2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "5.00000000000000000000000000000000000e-1");
    r1 /= integer(2);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "2.50000000000000000000000000000000000e-1");
    r1 /= integer(0);
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "inf");
    r1 = -1;
    r1 /= rational();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "-inf");
    r1 = 0;
    r1 /= rational();
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "nan");
    r1 = 12;
    BOOST_CHECK((is_divisible<real, integer>::value));
    BOOST_CHECK((is_divisible<real, mp_integer<16>>::value));
    BOOST_CHECK((is_divisible<mp_integer<16>, real>::value));
    BOOST_CHECK((!is_divisible_in_place<const mp_integer<16>, real>::value));
    BOOST_CHECK((is_divisible<real, rational>::value));
    BOOST_CHECK((is_divisible<real, mp_rational<16>>::value));
    BOOST_CHECK((is_divisible<mp_rational<16>, real>::value));
    BOOST_CHECK((!is_divisible_in_place<const mp_rational<16>, real>::value));
    // Precision handling.
    BOOST_CHECK_EQUAL((real{3, 4} / integer(1)).get_prec(), 4);
    BOOST_CHECK_EQUAL((integer(1) / real{3, 4}).get_prec(), 4);
    BOOST_CHECK_EQUAL((real{3, 4} / rational(1)).get_prec(), 4);
    BOOST_CHECK_EQUAL((rational(1) / real{3, 4}).get_prec(), 4);
    auto r1a = real{3, 4};
    BOOST_CHECK_EQUAL((r1a /= integer(1)).get_prec(), 4);
    BOOST_CHECK_EQUAL((r1a /= rational(1)).get_prec(), 4);
    // Rational and integer on the left.
    rational q(1, 2);
    q /= real(2);
    BOOST_CHECK_EQUAL(rational(1, 4), q);
    q /= real("inf");
    BOOST_CHECK_EQUAL(q, 0);
    BOOST_CHECK_THROW(q /= real{}, std::overflow_error);
    BOOST_CHECK_THROW(q /= real{"nan"}, std::overflow_error);
    integer n(4);
    n /= real(2, 4);
    BOOST_CHECK_EQUAL(n, 2);
    n /= real("1.001");
    BOOST_CHECK_EQUAL(n, 1);
    n /= real("0.99");
    BOOST_CHECK_EQUAL(n, 1);
    n /= real("-inf");
    BOOST_CHECK_EQUAL(n, 0);
    BOOST_CHECK_THROW(n /= real{}, std::overflow_error);
    BOOST_CHECK_THROW(n /= real("nan"), std::overflow_error);
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        r1 /= 2.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "6.00000000000000000000000000000000000");
        r1 /= 2.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "3.00000000000000000000000000000000000");
        r1 /= 0.f;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "inf");
        r1 = 12;
        float x = 4.f;
        x /= real(".5");
        BOOST_CHECK_EQUAL(x, 8.f);
        BOOST_CHECK((std::is_same<real &, decltype(r1 /= 2.f)>::value));
        BOOST_CHECK((is_divisible_in_place<real, float>::value));
        BOOST_CHECK((is_divisible_in_place<float, real>::value));
        BOOST_CHECK((!is_divisible_in_place<real, std::string>::value));
        BOOST_CHECK((std::is_same<float &, decltype(x /= real(".5"))>::value));
        // Precision handling.
        BOOST_CHECK_EQUAL((real{3, 4} / 1.f).get_prec(), 4);
        BOOST_CHECK_EQUAL((1.f / real{3, 4}).get_prec(), 4);
        r1a = real{3, 4};
        BOOST_CHECK_EQUAL((r1a /= 1.f).get_prec(), 4);
    }
    if (std::numeric_limits<float>::has_infinity) {
        real r(1);
        r /= std::numeric_limits<float>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "0.00000000000000000000000000000000000");
        r /= 0;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        float x = 4.f;
        x /= real{};
        BOOST_CHECK_EQUAL(x, std::numeric_limits<float>::infinity());
        x = -1.f;
        x /= real{};
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<float>::infinity());
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        r1 /= 2.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "6.00000000000000000000000000000000000");
        r1 /= 2.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "3.00000000000000000000000000000000000");
        r1 /= 0.;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1), "inf");
        r1 = 12;
        double x = 4.;
        x /= real(".5");
        BOOST_CHECK_EQUAL(x, 8.);
    }
    if (std::numeric_limits<double>::has_infinity) {
        real r(1);
        r /= std::numeric_limits<double>::infinity();
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "0.00000000000000000000000000000000000");
        r /= 0;
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r), "nan");
        double x = 4.;
        x /= real{};
        BOOST_CHECK_EQUAL(x, std::numeric_limits<double>::infinity());
        x = -1.;
        x /= real{};
        BOOST_CHECK_EQUAL(x, -std::numeric_limits<double>::infinity());
    }
    boost::fusion::for_each(integral_values, check_in_place_div_integral());
    // Binary division.
    r1 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 / r1), "1.00000000000000000000000000000000000");
    r2 = 2;
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 / r2), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r1 / r2), "1.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 / r1), "1.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 / real("inf", 4)), "0.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-inf") / r2), "-inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real("-nan") / r2), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 / integer(2)), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(2) / r2), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 / mp_integer<16>(2)), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_integer<32>(2) / r2), "1.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"} / integer(1)), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(integer(1) / real{"inf"}),
                      "0.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 / rational(1, 2)), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(rational(1, 2) / r2), "2.50e-1");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(r2 / mp_rational<16>(1, 2)), "4.00");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(mp_rational<8>(1, 2) / r2), "2.50e-1");
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f / real{1, 4}), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} / .5f), "2.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5f / real{"inf"}), "0.00000000000000000000000000000000000");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} / .5f), "-inf");
    }
    if (std::numeric_limits<float>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} / std::numeric_limits<float>::infinity()),
                          "0.00000000000000000000000000000000000");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<float>::infinity() / real{1}), "-inf");
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 / real{1, 4}), "5.00e-1");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1, 4} / .5), "2.00");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(.5 / real{"inf"}), "0.00000000000000000000000000000000000");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"} / .5), "-inf");
    }
    if (std::numeric_limits<double>::has_infinity) {
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{1} / std::numeric_limits<double>::infinity()),
                          "0.00000000000000000000000000000000000");
        BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(-std::numeric_limits<double>::infinity() / real{1}), "-inf");
    }
    boost::fusion::for_each(integral_values, check_binary_div_integral());
}

struct check_binary_equality_integral {
    template <typename T>
    void operator()(const T &) const
    {
        BOOST_CHECK_EQUAL(real{}, T(0));
        BOOST_CHECK_EQUAL(T(1), (real{1, 4}));
        BOOST_CHECK(!(T(4) == real{"inf"}));
        BOOST_CHECK(!(real{"-inf", 4} == T(3)));
        BOOST_CHECK(!(T(4) == real{"nan"}));
        BOOST_CHECK(!(real{"-nan", 4} == T(3)));
        BOOST_CHECK(real{} != T(1));
        BOOST_CHECK(T(1) != real{});
        BOOST_CHECK((is_equality_comparable<real, T>::value));
        BOOST_CHECK((is_equality_comparable<T, real>::value));
        BOOST_CHECK((is_equality_comparable<real>::value));
        BOOST_CHECK((!is_equality_comparable<real, std::string>::value));
        BOOST_CHECK((std::is_same<bool, decltype(real{} == real{})>::value));
    }
};

BOOST_AUTO_TEST_CASE(real_equality_test)
{
    BOOST_CHECK_EQUAL(real{}, real{});
    BOOST_CHECK_EQUAL((real{1, 4}), real{1});
    BOOST_CHECK_EQUAL(real{1}, (real{1, 4}));
    BOOST_CHECK_EQUAL(real{"0.5"}, (real{"0.5", 4}));
    BOOST_CHECK_EQUAL(real{"inf"}, real{"inf"});
    BOOST_CHECK_EQUAL(real{"-inf"}, real{"-inf"});
    BOOST_CHECK(!(real{"-inf"} == real{"inf"}));
    BOOST_CHECK(!(real{"nan"} == real{}));
    BOOST_CHECK(!(real{"nan"} == real{"inf"}));
    BOOST_CHECK(!(real{} == real{"nan"}));
    BOOST_CHECK(!(real{"-inf"} == real{"nan"}));
    BOOST_CHECK(!(real{"nan"} == real{"nan"}));
    BOOST_CHECK(real{"nan"} != real{"nan"});
    BOOST_CHECK(real{"nan"} != real{3});
    BOOST_CHECK(real{"nan"} != real{"inf"});
    BOOST_CHECK(real{0} != real{1});
    // With integer.
    BOOST_CHECK_EQUAL(integer(1), real{1});
    BOOST_CHECK_EQUAL((real{0, 4}), integer(rational(1, 2)));
    BOOST_CHECK(!(integer() == real{"nan"}));
    BOOST_CHECK(integer() != real{"nan"});
    BOOST_CHECK(!(real{"-nan"} == integer(5)));
    BOOST_CHECK(real{"-nan"} != integer(5));
    BOOST_CHECK(!(integer() == real{"inf"}));
    BOOST_CHECK(!(real{"-inf"} == integer(5)));
    BOOST_CHECK(real{1} != integer());
    BOOST_CHECK((is_equality_comparable<real, integer>::value));
    BOOST_CHECK((is_equality_comparable<mp_integer<16>, real>::value));
    BOOST_CHECK((std::is_same<bool, decltype(real{} == mp_integer<8>{})>::value));
    // With rational.
    BOOST_CHECK_EQUAL(rational(1), real{1});
    BOOST_CHECK_EQUAL(real{1}, rational(1));
    BOOST_CHECK_EQUAL(real{"0.5"}, rational(1, 2));
    BOOST_CHECK_EQUAL(rational(1, 2), (real{"0.5", 4}));
    BOOST_CHECK(!(rational() == real{"nan"}));
    BOOST_CHECK(rational() != real{"nan"});
    BOOST_CHECK(!(real{"-nan"} == rational(5, 3)));
    BOOST_CHECK(real{"-nan"} != rational(5, 3));
    BOOST_CHECK(!(rational() == real{"inf"}));
    BOOST_CHECK(!(real{"-inf"} == rational(5)));
    BOOST_CHECK(real{1} != rational(3, 4));
    BOOST_CHECK((is_equality_comparable<real, rational>::value));
    BOOST_CHECK((is_equality_comparable<mp_rational<16>, real>::value));
    BOOST_CHECK((std::is_same<bool, decltype(real{} == mp_rational<8>{})>::value));
    // With floating-point types.
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2
        && std::numeric_limits<float>::has_infinity && std::numeric_limits<float>::has_quiet_NaN) {
        BOOST_CHECK_EQUAL(real{0}, 0.f);
        BOOST_CHECK_EQUAL(1.f, real{1});
        BOOST_CHECK_EQUAL(.5f, (real{".5", 4}));
        BOOST_CHECK(!(1.f == (real{".5", 4})));
        BOOST_CHECK_EQUAL(std::numeric_limits<float>::infinity(), real{"inf"});
        BOOST_CHECK_EQUAL(real{"-inf"}, -std::numeric_limits<float>::infinity());
        BOOST_CHECK(!(real{"inf"} == -std::numeric_limits<float>::infinity()));
        BOOST_CHECK(!(real{5} == std::numeric_limits<float>::quiet_NaN()));
        BOOST_CHECK(real{5} != std::numeric_limits<float>::quiet_NaN());
        BOOST_CHECK(!(5.f == real{"nan"}));
        BOOST_CHECK(5.f != real{"nan"});
        BOOST_CHECK(!(std::numeric_limits<float>::quiet_NaN() == real{"-nan"}));
        BOOST_CHECK(std::numeric_limits<float>::quiet_NaN() != real{"-nan"});
        BOOST_CHECK(0.5f != real{1});
        BOOST_CHECK((is_equality_comparable<real, float>::value));
        BOOST_CHECK((is_equality_comparable<float, real>::value));
        BOOST_CHECK((std::is_same<bool, decltype(real{} == float{})>::value));
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2
        && std::numeric_limits<double>::has_infinity && std::numeric_limits<double>::has_quiet_NaN) {
        BOOST_CHECK_EQUAL(real{0}, 0.);
        BOOST_CHECK_EQUAL(1., real{1});
        BOOST_CHECK_EQUAL(.5, (real{".5", 4}));
        BOOST_CHECK(!(1. == (real{".5", 4})));
        BOOST_CHECK_EQUAL(std::numeric_limits<double>::infinity(), real{"inf"});
        BOOST_CHECK_EQUAL(real{"-inf"}, -std::numeric_limits<double>::infinity());
        BOOST_CHECK(!(real{"inf"} == -std::numeric_limits<double>::infinity()));
        BOOST_CHECK(!(real{5} == std::numeric_limits<double>::quiet_NaN()));
        BOOST_CHECK(real{5} != std::numeric_limits<double>::quiet_NaN());
        BOOST_CHECK(!(5. == real{"nan"}));
        BOOST_CHECK(5. != real{"nan"});
        BOOST_CHECK(!(std::numeric_limits<double>::quiet_NaN() == real{"-nan"}));
        BOOST_CHECK(std::numeric_limits<double>::quiet_NaN() != real{"-nan"});
        BOOST_CHECK(0.5 != real{1});
    }
    BOOST_CHECK((is_equality_comparable<real, long double>::value));
    BOOST_CHECK((is_equality_comparable<long double, real>::value));
    BOOST_CHECK((std::is_same<bool, decltype(real{} == static_cast<long double>(0))>::value));
    boost::fusion::for_each(integral_values, check_binary_equality_integral());
}

struct check_binary_comparison_integral {
    template <typename T>
    void operator()(const T &) const
    {
        BOOST_CHECK(real{-1} < T(0));
        BOOST_CHECK(real{"-inf"} < T(0));
        BOOST_CHECK(T(0) < real{1});
        BOOST_CHECK(T(0) < real{"inf"});
        BOOST_CHECK(real{-1} <= T(0));
        BOOST_CHECK(real{"-inf"} <= T(0));
        BOOST_CHECK(T(0) <= real{0});
        BOOST_CHECK(T(0) <= real{"inf"});
        BOOST_CHECK(real{1} > T(0));
        BOOST_CHECK(real{"inf"} > T(0));
        BOOST_CHECK(T(0) > real{-1});
        BOOST_CHECK(T(0) > real{"-inf"});
        BOOST_CHECK(real{"inf"} >= T(0));
        BOOST_CHECK(real{1} >= T(0));
        BOOST_CHECK(T(0) >= real{0});
        BOOST_CHECK(T(0) >= real{"-inf"});
        // NaNs.
        BOOST_CHECK(!(real{"nan"} < T(0)));
        BOOST_CHECK(!(T(0) < real{"nan"}));
        BOOST_CHECK(!(real{"nan"} <= T(0)));
        BOOST_CHECK(!(T(0) <= real{"-nan"}));
        BOOST_CHECK(!(real{"nan"} > T(0)));
        BOOST_CHECK(!(T(0) > real{"nan"}));
        BOOST_CHECK(!(real{"nan"} >= T(0)));
        BOOST_CHECK(!(T(0) >= real{"nan"}));
        BOOST_CHECK((is_less_than_comparable<real>::value));
        BOOST_CHECK((is_less_than_comparable<real, T>::value));
        BOOST_CHECK((is_less_than_comparable<T, real>::value));
        BOOST_CHECK((std::is_same<bool, decltype(real{-1} < T(0))>::value));
    }
};

BOOST_AUTO_TEST_CASE(real_comparisons_test)
{
    BOOST_CHECK(real{} <= real{});
    BOOST_CHECK(real{} >= real{});
    BOOST_CHECK(!(real{} < real{}));
    BOOST_CHECK(!(real{} > real{}));
    BOOST_CHECK((real{3, 4}) <= real{3});
    BOOST_CHECK((real{3, 4}) >= real{3});
    BOOST_CHECK((real{2, 4}) <= real{3});
    BOOST_CHECK((real{3, 4}) >= real{2});
    BOOST_CHECK(!(real{3, 4} < real{3}));
    BOOST_CHECK(!(real{3, 4} > real{3}));
    BOOST_CHECK(!(real{3, 4} < real{3}));
    BOOST_CHECK(real{4} > real{3});
    BOOST_CHECK(real{3} < real{4});
    BOOST_CHECK(real{"inf"} > real{});
    BOOST_CHECK(real{"inf"} >= real{2});
    BOOST_CHECK(real{"-inf"} < real{"inf"});
    BOOST_CHECK(!(real{"inf"} <= real{"nan"}));
    BOOST_CHECK(!(real{"nan"} >= real{"inf"}));
    // Integer and rational.
    BOOST_CHECK((is_less_than_comparable<real, integer>::value));
    BOOST_CHECK((is_less_than_comparable<integer, real>::value));
    BOOST_CHECK((std::is_same<bool, decltype(real{} < integer{})>::value));
    BOOST_CHECK(real{4} > integer(3));
    BOOST_CHECK(real{4} >= integer(4));
    BOOST_CHECK(real{4} < integer(5));
    BOOST_CHECK(real{4} <= integer(5));
    BOOST_CHECK(real{"inf"} > integer(3));
    BOOST_CHECK(real{4} > mp_integer<8>(3));
    BOOST_CHECK(real{4} >= mp_integer<16>(4));
    BOOST_CHECK(real{4} < mp_integer<32>(5));
    BOOST_CHECK(real{4} <= mp_integer<8>(5));
    BOOST_CHECK(real{"inf"} > integer(3));
    BOOST_CHECK(integer{4} > real{2});
    BOOST_CHECK(!(real{"nan"} > integer(3)));
    BOOST_CHECK(!(real{"nan"} < integer(3)));
    BOOST_CHECK(!(real{"nan"} >= integer(3)));
    BOOST_CHECK(!(real{"nan"} <= integer(3)));
    BOOST_CHECK(!(integer(3) > real{"nan"}));
    BOOST_CHECK(!(integer(3) < real{"nan"}));
    BOOST_CHECK(!(integer(3) >= real{"nan"}));
    BOOST_CHECK(!(integer(3) <= real{"nan"}));
    BOOST_CHECK(real{4} >= integer(3));
    BOOST_CHECK(real{3} >= integer(3));
    BOOST_CHECK(real{"inf"} >= integer(3));
    BOOST_CHECK(real{"inf"} > integer(3));
    BOOST_CHECK(real{"-inf"} < integer(3));
    BOOST_CHECK(real{"-inf"} <= integer(3));
    BOOST_CHECK((is_less_than_comparable<real, rational>::value));
    BOOST_CHECK((is_less_than_comparable<rational, real>::value));
    BOOST_CHECK((std::is_same<bool, decltype(real{} < rational{})>::value));
    BOOST_CHECK(real{4} > rational(3));
    BOOST_CHECK(real{4} >= rational(4));
    BOOST_CHECK(real{4} < rational(5));
    BOOST_CHECK(real{4} <= rational(5));
    BOOST_CHECK(real{"inf"} > rational(3));
    BOOST_CHECK(real{4} > mp_rational<16>(3));
    BOOST_CHECK(real{4} >= mp_rational<8>(4));
    BOOST_CHECK(real{4} < mp_rational<32>(5));
    BOOST_CHECK(real{4} <= mp_rational<16>(5));
    BOOST_CHECK(real{"inf"} > rational(3));
    BOOST_CHECK(rational{4} > real{2});
    BOOST_CHECK(!(real{"nan"} > rational(3)));
    BOOST_CHECK(!(real{"nan"} < rational(3)));
    BOOST_CHECK(!(real{"nan"} >= rational(3)));
    BOOST_CHECK(!(real{"nan"} <= rational(3)));
    BOOST_CHECK(!(rational(3) > real{"nan"}));
    BOOST_CHECK(!(rational(3) < real{"nan"}));
    BOOST_CHECK(!(rational(3) >= real{"nan"}));
    BOOST_CHECK(!(rational(3) <= real{"nan"}));
    BOOST_CHECK(real{4} >= rational(3));
    BOOST_CHECK(real{3} >= rational(3));
    BOOST_CHECK(real{"inf"} >= rational(3));
    BOOST_CHECK(real{"inf"} > rational(3));
    BOOST_CHECK(real{"-inf"} < rational(3));
    BOOST_CHECK(real{"-inf"} <= rational(3));
    // With floating-point types.
    if (std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::radix == 2
        && std::numeric_limits<float>::has_infinity && std::numeric_limits<float>::has_quiet_NaN) {
        BOOST_CHECK(real{1} > 0.5f);
        BOOST_CHECK(0.5f > real{});
        BOOST_CHECK(0.5f < real{1});
        BOOST_CHECK(real{"-inf"} < 0.5f);
        BOOST_CHECK(real{"inf"} >= 0.f);
        BOOST_CHECK(0.f >= real{});
        BOOST_CHECK(0.f <= real{});
        BOOST_CHECK(real{} <= 0.f);
        // NaNs.
        BOOST_CHECK(!(real{1} > std::numeric_limits<float>::quiet_NaN()));
        BOOST_CHECK(!(std::numeric_limits<float>::quiet_NaN() > real{}));
        BOOST_CHECK(!(0.5f < real{"nan"}));
        BOOST_CHECK(!(real{"-inf"} < std::numeric_limits<float>::quiet_NaN()));
        BOOST_CHECK(!(real{"-nan"} >= std::numeric_limits<float>::quiet_NaN()));
        BOOST_CHECK(!(std::numeric_limits<float>::quiet_NaN() >= real{}));
        BOOST_CHECK(!(std::numeric_limits<float>::quiet_NaN() <= real{}));
        BOOST_CHECK(!(real{"nan"} <= std::numeric_limits<float>::quiet_NaN()));
    }
    if (std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::radix == 2
        && std::numeric_limits<double>::has_infinity && std::numeric_limits<double>::has_quiet_NaN) {
        BOOST_CHECK(real{1} > 0.5);
        BOOST_CHECK(0.5 > real{});
        BOOST_CHECK(0.5 < real{1});
        BOOST_CHECK(real{"-inf"} < 0.5);
        BOOST_CHECK(real{"inf"} >= 0.);
        BOOST_CHECK(0. >= real{});
        BOOST_CHECK(0. <= real{});
        BOOST_CHECK(real{} <= 0.);
        // NaNs.
        BOOST_CHECK(!(real{1} > std::numeric_limits<double>::quiet_NaN()));
        BOOST_CHECK(!(std::numeric_limits<double>::quiet_NaN() > real{}));
        BOOST_CHECK(!(0.5 < real{"nan"}));
        BOOST_CHECK(!(real{"-inf"} < std::numeric_limits<double>::quiet_NaN()));
        BOOST_CHECK(!(real{"-nan"} >= std::numeric_limits<double>::quiet_NaN()));
        BOOST_CHECK(!(std::numeric_limits<double>::quiet_NaN() >= real{}));
        BOOST_CHECK(!(std::numeric_limits<double>::quiet_NaN() <= real{}));
        BOOST_CHECK(!(real{"nan"} <= std::numeric_limits<double>::quiet_NaN()));
    }
    boost::fusion::for_each(integral_values, check_binary_comparison_integral());
    BOOST_CHECK((is_less_than_comparable<real, long double>::value));
    BOOST_CHECK((is_less_than_comparable<rational, long double>::value));
    BOOST_CHECK((std::is_same<bool, decltype(0.l < rational{})>::value));
}

BOOST_AUTO_TEST_CASE(real_stream_test)
{
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"nan"}), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"+nan"}), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-nan"}), "nan");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"inf"}), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"+inf"}), "inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"-inf"}), "-inf");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"3"}), "3.00000000000000000000000000000000000");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"30"}), "3.00000000000000000000000000000000000e1");
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{"0.5"}), "5.00000000000000000000000000000000000e-1");
    {
        std::ostringstream oss;
        std::string tmp = "1.50";
        oss << real(tmp, 4);
        BOOST_CHECK_EQUAL(tmp, oss.str());
    }
    {
        std::ostringstream oss;
        std::string tmp = "-5.00e-1";
        oss << real(tmp, 4);
        BOOST_CHECK_EQUAL(tmp, oss.str());
    }
    {
        real tmp{0, 4};
        std::stringstream ss;
        ss << "1.5";
        ss >> tmp;
        BOOST_CHECK_EQUAL(tmp, (real{"1.5", 4}));
    }
    {
        real tmp{0, 4};
        std::stringstream ss;
        ss << "-0.5";
        ss >> tmp;
        BOOST_CHECK_EQUAL(tmp, (real{"-.5", 4}));
    }
}

BOOST_AUTO_TEST_CASE(real_pow_test)
{
    real r1{2, 4};
    BOOST_CHECK_EQUAL(r1.pow(real{2}), 4);
    BOOST_CHECK_EQUAL(real{4}.pow(real{"0.5"}), real{2});
    BOOST_CHECK_EQUAL(r1.pow(real{"inf"}), real{"inf"});
    BOOST_CHECK_EQUAL(r1.pow(real{"-inf"}), 0);
    BOOST_CHECK_EQUAL(real{"inf"}.pow(real{"inf"}), real{"inf"});
    BOOST_CHECK(real{-1}.pow(real{"1.5"}).is_nan());
    // Check precision handling.
    BOOST_CHECK_EQUAL(r1.pow(real(2, 5)).get_prec(), 5);
    BOOST_CHECK_EQUAL(real(2, 3).pow(r1).get_prec(), 4);
    // Check the math:: function.
    BOOST_CHECK((is_exponentiable<real, real>::value));
    BOOST_CHECK((is_exponentiable<real, int>::value));
    BOOST_CHECK((is_exponentiable<int, real>::value));
    BOOST_CHECK((is_exponentiable<real, long double>::value));
    BOOST_CHECK((is_exponentiable<long double, real>::value));
    BOOST_CHECK((is_exponentiable<real, mp_integer<16>>::value));
    BOOST_CHECK((is_exponentiable<mp_integer<8>, real>::value));
    BOOST_CHECK((is_exponentiable<real, mp_rational<16>>::value));
    BOOST_CHECK((is_exponentiable<mp_rational<8>, real>::value));
    BOOST_CHECK_EQUAL(math::pow(real{4}, rational{1, 2}), real{2});
    BOOST_CHECK_EQUAL(math::pow(2.l, real{4}), 16);
    BOOST_CHECK_EQUAL(math::pow(real{3}, rational(1, 4)), math::pow(real{3}, real{rational(1, 4)}));
    // Check precision handling.
    BOOST_CHECK_EQUAL(math::pow(real{4, 4}, 4).get_prec(), 4);
    BOOST_CHECK_EQUAL(math::pow(4, real{4, 4}).get_prec(), 4);
}

struct no_fma {
};

BOOST_AUTO_TEST_CASE(real_fma_test)
{
    real r{4};
    r.multiply_accumulate(real{2}, real{3});
    BOOST_CHECK_EQUAL(r, 10);
    BOOST_CHECK_EQUAL(r.get_prec(), real::default_prec);
    r.multiply_accumulate((real{2, 4}), real{3});
    BOOST_CHECK_EQUAL(r, 16);
    BOOST_CHECK_EQUAL(r.get_prec(), real::default_prec);
    real r2{4, 4};
    r2.multiply_accumulate((real{2, 4}), (real{3, 4}));
    BOOST_CHECK_EQUAL(r2, 10);
    BOOST_CHECK_EQUAL(r2.get_prec(), ::mpfr_prec_t(4));
    r2.multiply_accumulate((real{2, 4}), real{3});
    BOOST_CHECK_EQUAL(r2, 16);
    BOOST_CHECK_EQUAL(r2.get_prec(), real::default_prec);
    r2.multiply_accumulate((real{2, real::default_prec + ::mpfr_prec_t(1)}), real{3});
    BOOST_CHECK_EQUAL(r2, 22);
    BOOST_CHECK_EQUAL(r2.get_prec(), real::default_prec + ::mpfr_prec_t(1));
    // Check the math:: function.
    real r3{5};
    math::multiply_accumulate(r3, real{-4}, real{2});
    BOOST_CHECK_EQUAL(r3, -3);
    BOOST_CHECK((has_multiply_accumulate<real>::value));
    BOOST_CHECK((has_multiply_accumulate<real &>::value));
    BOOST_CHECK((!has_multiply_accumulate<const real>::value));
    BOOST_CHECK((!has_multiply_accumulate<const real &>::value));
    // Test for precision bug in mult_add after the thread_local changes.
    // NOTE: this could actually fail if/when we switch back to the fma() from MPFR,
    // as in that case there might be a single rounding in the whole operation.
    real r4{".2", 200};
    r4.multiply_accumulate(real{"-.2", 200}, real{".2", 200});
    BOOST_CHECK_EQUAL((real{".2", 200} + real{"-.2", 200} * real{".2", 200}), r4);
    // This will be different because in this case the mult is done with 200 bits,
    // and only the final add in 500 bits. While for the fma we are doing everything
    // at 500 bits.
    r4 = real{".2", 500};
    r4.multiply_accumulate(real{"-.2", 200}, real{".2", 200});
    BOOST_CHECK((real{".2", 500} + real{"-.2", 200} * real{".2", 200} != r4));
}

BOOST_AUTO_TEST_CASE(real_sin_cos_test)
{
    BOOST_CHECK_EQUAL((real{0, 4}.cos()), 1);
    BOOST_CHECK_EQUAL(math::cos(real{0, 4}), 1);
    BOOST_CHECK_EQUAL((real{0, 4}).sin(), 0);
    BOOST_CHECK_EQUAL(math::sin(real{0, 4}), 0);
    BOOST_CHECK_EQUAL((real{0, 4}).sin().get_prec(), 4);
    BOOST_CHECK_EQUAL((real{0}).sin().get_prec(), real::default_prec);
}

BOOST_AUTO_TEST_CASE(real_truncate_test)
{
    real r{"inf"};
    r.truncate();
    BOOST_CHECK_EQUAL(r, real{"inf"});
    r = "-inf";
    r.truncate();
    BOOST_CHECK_EQUAL(r, real{"-inf"});
    BOOST_CHECK_EQUAL(r.truncated(), r);
    r = "nan";
    r.truncate();
    BOOST_CHECK(r.is_nan());
    BOOST_CHECK(r.truncated().is_nan());
    r = "5.4";
    r.truncate();
    BOOST_CHECK_EQUAL(r, 5);
    BOOST_CHECK_EQUAL(real{"5.4"}.truncated(), 5);
    r = "-5.4";
    r.truncate();
    BOOST_CHECK_EQUAL(r, -5);
    BOOST_CHECK_EQUAL((-real{"5.4"}).truncated(), -5);
    r.truncate();
    BOOST_CHECK_EQUAL(r, -5);
    r = real{"0.5", 4};
    r.truncate();
    BOOST_CHECK_EQUAL(r, 0);
    BOOST_CHECK_EQUAL(r.get_prec(), 4);
    r = real{"0.5", 4};
    BOOST_CHECK_EQUAL(r.truncated(), 0);
    BOOST_CHECK_EQUAL(r, real("0.5", 4));
}

BOOST_AUTO_TEST_CASE(real_pi_test)
{
    BOOST_CHECK_EQUAL(real{}.pi(), real{"3.14159265358979323846264338327950280"});
    BOOST_CHECK_EQUAL((real{0, 4}.pi()), real{"3.25"});
    BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(real{0, 4}.pi()), "3.25");
}

BOOST_AUTO_TEST_CASE(real_partial_test)
{
    BOOST_CHECK_EQUAL(math::partial(real(), ""), 0);
    BOOST_CHECK_EQUAL(math::partial(real(1), std::string("")), 0);
    BOOST_CHECK_EQUAL(math::partial(real(-10), std::string("")), 0);
}

BOOST_AUTO_TEST_CASE(real_evaluate_test)
{
    BOOST_CHECK_EQUAL(math::evaluate(real(), std::unordered_map<std::string, integer>{}), real());
    BOOST_CHECK_EQUAL(math::evaluate(real(2), std::unordered_map<std::string, int>{}), real(2));
    BOOST_CHECK_EQUAL(math::evaluate(real(-3.5), std::unordered_map<std::string, double>{}), real(-3.5));
    BOOST_CHECK((std::is_same<decltype(math::evaluate(real(), std::unordered_map<std::string, real>{})), real>::value));
}

BOOST_AUTO_TEST_CASE(real_subs_test)
{
    BOOST_CHECK((!has_subs<real, real>::value));
    BOOST_CHECK((!has_subs<real, int>::value));
    BOOST_CHECK((!has_subs<real, std::string>::value));
}

BOOST_AUTO_TEST_CASE(real_ipow_subs_test)
{
    BOOST_CHECK((!has_ipow_subs<real, real>::value));
    BOOST_CHECK((!has_ipow_subs<real, double>::value));
    BOOST_CHECK((!has_ipow_subs<real, integer>::value));
}

BOOST_AUTO_TEST_CASE(real_abs_test)
{
    BOOST_CHECK_EQUAL(real(42).abs(), real(42));
    BOOST_CHECK_EQUAL(real(-42).abs(), real(42));
    BOOST_CHECK_EQUAL(real("inf").abs(), real("inf"));
    BOOST_CHECK_EQUAL(real("-inf").abs(), real("inf"));
    BOOST_CHECK_EQUAL(math::abs(real(42)), real(42));
    BOOST_CHECK_EQUAL(math::abs(real(-42)), real(42));
    BOOST_CHECK_EQUAL(math::abs(real("inf")), real("inf"));
    BOOST_CHECK_EQUAL(math::abs(real("-inf")), real("inf"));
}

BOOST_AUTO_TEST_CASE(real_binomial_test)
{
    // Type traits and return values.
    BOOST_CHECK((has_binomial<real, real>::value));
    BOOST_CHECK((has_binomial<real, int>::value));
    BOOST_CHECK((has_binomial<real, char>::value));
    BOOST_CHECK((has_binomial<real, unsigned>::value));
    BOOST_CHECK((has_binomial<real, long>::value));
    BOOST_CHECK((!has_binomial<real, std::string>::value));
    BOOST_CHECK((has_binomial<long, real>::value));
    BOOST_CHECK((has_binomial<double, real>::value));
    BOOST_CHECK((has_binomial<rational, real>::value));
    BOOST_CHECK((has_binomial<mp_rational<16>, real>::value));
    BOOST_CHECK((std::is_same<real, decltype(math::binomial(real{}, 2))>::value));
    BOOST_CHECK((std::is_same<real, decltype(math::binomial(2, real{}))>::value));
    BOOST_CHECK((std::is_same<real, decltype(math::binomial(rational{}, real{}))>::value));
    BOOST_CHECK((std::is_same<real, decltype(math::binomial(real{}, integer{}))>::value));
    // Invalid values.
    BOOST_CHECK_THROW(math::binomial(real{"inf"}, 4), std::invalid_argument);
    BOOST_CHECK_THROW(math::binomial(real{"nan"}, 4), std::invalid_argument);
    BOOST_CHECK_THROW(math::binomial(4, real{"nan"}), std::invalid_argument);
    BOOST_CHECK_THROW(math::binomial(4, real{"inf"}), std::invalid_argument);
    // Some random testing.
    std::uniform_real_distribution<double> rdist(-100., 100.);
    for (int i = 0; i < ntries; ++i) {
        const double x = rdist(rng), y = rdist(rng);
        // NOTE: at the moment we have nothing to check this against.
        real tmp = math::binomial(real{x}, real{y});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        tmp = math::binomial(real{x, real::default_prec + 1}, rational{y});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK_EQUAL(tmp.get_prec(), real::default_prec + 1);
        tmp = math::binomial(rational{x}, real{y, real::default_prec + 1});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK_EQUAL(tmp.get_prec(), real::default_prec + 1);
        tmp = math::binomial(static_cast<int>(x), real{y, real::default_prec + 1});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK_EQUAL(tmp.get_prec(), real::default_prec + 1);
        tmp = math::binomial(real{x, real::default_prec + 1}, static_cast<int>(y));
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK_EQUAL(tmp.get_prec(), real::default_prec + 1);
        tmp = math::binomial(real{x, real::default_prec + 1}, y);
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK_EQUAL(tmp.get_prec(), real::default_prec + 1);
        tmp = math::binomial(x, real{y, real::default_prec + 1});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK_EQUAL(tmp.get_prec(), real::default_prec + 1);
        // Prec handling.
        tmp = math::binomial(real{x, real::default_prec + 1}, real{y});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK(tmp.get_prec() == real::default_prec + 1);
        tmp = math::binomial(real{x}, real{y, real::default_prec + 1});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        BOOST_CHECK(tmp.get_prec() == real::default_prec + 1);
    }
    std::uniform_int_distribution<int> idist(-100, 100);
    for (int i = 0; i < ntries; ++i) {
        const int x = idist(rng), y = idist(rng);
        real tmp = math::binomial(real{x}, real{y});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        tmp = math::binomial(real{x}, y);
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        tmp = math::binomial(x, real{y});
        BOOST_CHECK((!tmp.is_nan() && !tmp.is_inf()));
        // Check against mp_integer.
        const auto int_res = math::binomial(x, y);
        tmp = math::binomial(real{x}, real{y});
        BOOST_CHECK((int_res == 0 || tmp.abs() < 1E30) || ((tmp - int_res) / int_res).abs() < 1E-30);
    }
    // No negative integers.
    // This coincides with mpmath and wolfram alpha (roughly).
    BOOST_CHECK_EQUAL(math::binomial(real{"12345.6"}, real{"7.89"}), real{"5.99111763882803518776029814739451218e27"});
    // Inf denominator.
    BOOST_CHECK_EQUAL(math::binomial(real{"12345.6"}, real{"-2"}), real{});
    BOOST_CHECK_EQUAL(math::binomial(real{"1234"}, real{"-2"}), real{});
    BOOST_CHECK_EQUAL(math::binomial(real("12345.6", real::default_prec + 1), real{"-2"}).get_prec(),
                      real::default_prec + 1);
    BOOST_CHECK_EQUAL(math::binomial(real("1234"), real("-2", real::default_prec + 1)).get_prec(),
                      real::default_prec + 1);
    // Case 3.
    BOOST_CHECK_EQUAL(math::binomial(real{"-3"}, real{"-5"}), real{"5.99999999999999999999999999999999846"});
    // Case 5.
    BOOST_CHECK_EQUAL(math::binomial(real{"-3"}, real{"2"}), real{"5.99999999999999999999999999999999846"});
    // Case 7.
    BOOST_CHECK_EQUAL(math::binomial(real{"-3"}, real{"-2"}), real{});
    BOOST_CHECK_EQUAL(math::binomial(real("-3", real::default_prec + 1), real{"-2"}).get_prec(),
                      real::default_prec + 1);
    BOOST_CHECK_EQUAL(math::binomial(real("-3"), real("-2", real::default_prec + 1)).get_prec(),
                      real::default_prec + 1);
}

BOOST_AUTO_TEST_CASE(real_is_equality_comparable_test)
{
    BOOST_CHECK(is_equality_comparable<real>::value);
    BOOST_CHECK((is_equality_comparable<real, rational>::value));
    BOOST_CHECK((is_equality_comparable<real, integer>::value));
    BOOST_CHECK((is_equality_comparable<rational, real>::value));
    BOOST_CHECK((is_equality_comparable<integer, real>::value));
    BOOST_CHECK((is_equality_comparable<double, real>::value));
    BOOST_CHECK((is_equality_comparable<real, int>::value));
    BOOST_CHECK((!is_equality_comparable<real, std::string>::value));
}

BOOST_AUTO_TEST_CASE(real_t_subs_test)
{
    BOOST_CHECK((!has_t_subs<real, real, real>::value));
    BOOST_CHECK((!has_t_subs<real, int, int>::value));
    BOOST_CHECK((!has_t_subs<real, int, double>::value));
    BOOST_CHECK((!has_t_subs<real &, int, int>::value));
    BOOST_CHECK((!has_t_subs<const real &, const int &, double &>::value));
    BOOST_CHECK((!has_t_subs<std::string, real, double>::value));
}

BOOST_AUTO_TEST_CASE(real_type_traits_test)
{
    BOOST_CHECK_EQUAL(std::is_nothrow_destructible<real>::value, true);
    BOOST_CHECK_EQUAL(std::is_nothrow_destructible<const real>::value, true);
    BOOST_CHECK(is_differentiable<real>::value);
    BOOST_CHECK(is_differentiable<real &>::value);
    BOOST_CHECK(is_differentiable<const real &>::value);
    BOOST_CHECK(is_differentiable<const real>::value);
    BOOST_CHECK(has_pbracket<real>::value);
    BOOST_CHECK(has_pbracket<real &>::value);
    BOOST_CHECK(has_pbracket<real const &>::value);
    BOOST_CHECK(has_pbracket<real const>::value);
    BOOST_CHECK(has_transformation_is_canonical<real>::value);
    BOOST_CHECK(has_transformation_is_canonical<real &>::value);
    BOOST_CHECK(has_transformation_is_canonical<real const &>::value);
    BOOST_CHECK(has_transformation_is_canonical<real const>::value);
    BOOST_CHECK(!has_degree<real>::value);
    BOOST_CHECK(is_addable<real>::value);
    BOOST_CHECK((is_addable<real, integer>::value));
    BOOST_CHECK((is_addable<integer, real>::value));
    BOOST_CHECK((is_addable<double, real>::value));
    BOOST_CHECK((is_addable<real, double>::value));
    BOOST_CHECK((!is_addable<real, std::complex<double>>::value));
    BOOST_CHECK((!is_addable<std::complex<double>, real>::value));
    BOOST_CHECK(is_subtractable<real>::value);
    BOOST_CHECK((is_subtractable<double, real>::value));
    BOOST_CHECK((is_subtractable<real, integer>::value));
    BOOST_CHECK((is_subtractable<integer, real>::value));
    BOOST_CHECK((is_subtractable<real, double>::value));
    BOOST_CHECK((!is_subtractable<real, std::complex<double>>::value));
    BOOST_CHECK((!is_subtractable<std::complex<double>, real>::value));
    BOOST_CHECK(is_container_element<real>::value);
    BOOST_CHECK(is_ostreamable<real>::value);
    BOOST_CHECK(has_negate<real>::value);
    BOOST_CHECK(has_negate<real &>::value);
    BOOST_CHECK(!has_negate<const real &>::value);
    BOOST_CHECK(!has_negate<const real>::value);
    BOOST_CHECK((std::is_same<decltype(math::negate(*(real *)nullptr)), void>::value));
    BOOST_CHECK((is_evaluable<real, int>::value));
    BOOST_CHECK((is_evaluable<real, double>::value));
    BOOST_CHECK((is_evaluable<real &, double>::value));
    BOOST_CHECK((is_evaluable<real const &, double>::value));
    BOOST_CHECK((is_evaluable<real &&, double>::value));
    BOOST_CHECK(has_sine<real>::value);
    BOOST_CHECK(has_cosine<real>::value);
    BOOST_CHECK(has_sine<real &>::value);
    BOOST_CHECK(has_cosine<real const &>::value);
    BOOST_CHECK(has_cosine<real &&>::value);
}

BOOST_AUTO_TEST_CASE(real_gamma_test)
{
    BOOST_CHECK_EQUAL(real(1).gamma(), 1);
    BOOST_CHECK_EQUAL(real(4).gamma(), 6);
    BOOST_CHECK_EQUAL(real(5).gamma(), 24);
    BOOST_CHECK(real(-4).gamma().is_nan());
    BOOST_CHECK_EQUAL(real(1).gamma().get_prec(), real::default_prec);
    BOOST_CHECK_EQUAL(real(1, 4).gamma().get_prec(), 4);
    // lgamma.
    BOOST_CHECK_EQUAL(real(1).lgamma(), 0);
    BOOST_CHECK_EQUAL(real(1).lgamma().get_prec(), real::default_prec);
    BOOST_CHECK_EQUAL(real(0).lgamma(), real{"+inf"});
    BOOST_CHECK_EQUAL(real("+inf").lgamma(), real{"+inf"});
    BOOST_CHECK_EQUAL(real(-1).lgamma(), real{"+inf"});
    BOOST_CHECK_EQUAL(real(3, 4).lgamma().get_prec(), 4);
}

BOOST_AUTO_TEST_CASE(real_exp_test)
{
    BOOST_CHECK_EQUAL(real(0).exp(), 1);
    BOOST_CHECK_EQUAL(real("inf").exp(), real("+inf"));
    BOOST_CHECK_EQUAL(real("-inf").exp(), real(0));
    BOOST_CHECK(real("nan").exp().is_nan());
    BOOST_CHECK_EQUAL(real(0, 4).exp().get_prec(), 4);
}

BOOST_AUTO_TEST_CASE(real_literal_test)
{
    BOOST_CHECK((std::is_same<std::decay<decltype(1._r)>::type, real>::value));
    BOOST_CHECK_EQUAL(1_r, real{1});
    BOOST_CHECK_EQUAL(1._r, real{1});
    BOOST_CHECK_EQUAL(1.e1_r, real{10});
    if (std::numeric_limits<double>::radix == 2) {
        BOOST_CHECK(1.3_r != real{1.3});
    }
    BOOST_CHECK_EQUAL(1.e-1_r, real{"1e-1"});
}

BOOST_AUTO_TEST_CASE(real_safe_cast_test)
{
    BOOST_CHECK((has_safe_cast<int, real>::value));
    BOOST_CHECK((has_safe_cast<int &, real &&>::value));
    BOOST_CHECK((has_safe_cast<const int &, const real &>::value));
    BOOST_CHECK((has_safe_cast<const int, const real>::value));
    BOOST_CHECK((!has_safe_cast<void, real>::value));
    BOOST_CHECK((has_safe_cast<unsigned, real>::value));
    BOOST_CHECK((has_safe_cast<integer, real>::value));
    BOOST_CHECK((has_safe_cast<rational, real>::value));
    BOOST_CHECK((has_safe_cast<rational &, real>::value));
    BOOST_CHECK((has_safe_cast<rational &&, const real>::value));
    BOOST_CHECK((has_safe_cast<const rational &, const real &>::value));
    BOOST_CHECK((!has_safe_cast<double, real>::value));
    BOOST_CHECK((!has_safe_cast<float, real>::value));
    BOOST_CHECK((!has_safe_cast<real, int>::value));
    BOOST_CHECK((!has_safe_cast<real, float>::value));
    BOOST_CHECK((!has_safe_cast<real, integer>::value));
    BOOST_CHECK((!has_safe_cast<real, rational>::value));
    BOOST_CHECK((!has_safe_cast<real, void>::value));
    BOOST_CHECK_EQUAL(safe_cast<int>(3_r), 3);
    BOOST_CHECK_EQUAL(safe_cast<int>(-3_r), -3);
    BOOST_CHECK_EQUAL(safe_cast<unsigned>(4_r), 4u);
    BOOST_CHECK_EQUAL(safe_cast<integer>(4_r), 4_z);
    BOOST_CHECK_EQUAL(safe_cast<integer>(-4_r), -4_z);
    BOOST_CHECK_EQUAL(safe_cast<rational>(4_r), 4_q);
    BOOST_CHECK_EQUAL(safe_cast<rational>(-4_r), -4_q);
    BOOST_CHECK_EQUAL(safe_cast<rational>(5_r / 2), 5_q / 2);
    BOOST_CHECK_EQUAL(safe_cast<rational>(-5_r / 2), -5_q / 2);
    // Various types of failures.
    BOOST_CHECK_EXCEPTION(safe_cast<int>(3.1_r), safe_cast_failure, [](const safe_cast_failure &e) {
        return boost::contains(e.what(), "as the input real does not represent a finite integral value");
    });
    BOOST_CHECK_THROW(safe_cast<int>(-3.1_r), safe_cast_failure);
    BOOST_CHECK_EXCEPTION(safe_cast<int>(real{"inf"}), safe_cast_failure, [](const safe_cast_failure &e) {
        return boost::contains(e.what(), "as the input real does not represent a finite integral value");
    });
    BOOST_CHECK_THROW(safe_cast<int>(real{"nan"}), safe_cast_failure);
    BOOST_CHECK_EXCEPTION(safe_cast<int>(real{std::numeric_limits<int>::max()} * 2), safe_cast_failure,
                          [](const safe_cast_failure &e) {
                              return boost::contains(e.what(), "as the conversion would not preserve the value");
                          });
    BOOST_CHECK_THROW(safe_cast<int>(real{std::numeric_limits<int>::min()} * 2), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<unsigned>(3.1_r), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<unsigned>(-3_r), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<unsigned>(real{"inf"}), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<unsigned>(real{"nan"}), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<unsigned>(real{std::numeric_limits<unsigned>::max()} * 2), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<integer>(3.1_r), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<integer>(-3.1_r), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<integer>(real{"inf"}), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<integer>(real{"nan"}), safe_cast_failure);
    BOOST_CHECK_THROW(safe_cast<rational>(real{"inf"}), safe_cast_failure);
    BOOST_CHECK_EXCEPTION(safe_cast<rational>(real{"nan"}), safe_cast_failure, [](const safe_cast_failure &e) {
        return boost::contains(e.what(), "as the conversion would not preserve the value");
    });
}

BOOST_AUTO_TEST_CASE(real_serialization_test)
{
    std::uniform_int_distribution<::mpfr_prec_t> prec_dist(::mpfr_prec_t(10), ::mpfr_prec_t(100));
    std::uniform_real_distribution<double> value_dist(-1., 1.);
    for (int i = 0; i < ntries; ++i) {
        // Create a "random" real.
        real tmp(boost::lexical_cast<std::string>(value_dist(rng)), prec_dist(rng));
        // Move to the next number to increase the amount of bits actually used.
        ::mpfr_nextabove(tmp.get_mpfr_t());
        // Write the tmp to an output archive.
        std::stringstream ss;
        {
            boost::archive::text_oarchive oa(ss);
            oa << tmp;
        }
        // Recover the real from the archive.
        real tmp_out;
        {
            boost::archive::text_iarchive ia(ss);
            ia >> tmp_out;
        }
        BOOST_CHECK_EQUAL(tmp, tmp_out);
        BOOST_CHECK_EQUAL(tmp.get_prec(), tmp_out.get_prec());
    }
}

BOOST_AUTO_TEST_CASE(real_mpfr_t_get_test)
{
    real r0{.2};
    BOOST_CHECK(r0.get_mpfr_t() != nullptr);
    const real r1{-1};
    BOOST_CHECK(r1.get_mpfr_t() != nullptr);
}
