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

#ifndef PIRANHA_MONOMIAL_HPP
#define PIRANHA_MONOMIAL_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "array_key.hpp"
#include "config.hpp"
#include "detail/cf_mult_impl.hpp"
#include "detail/prepare_for_print.hpp"
#include "detail/safe_integral_adder.hpp"
#include "exceptions.hpp"
#include "forwarding.hpp"
#include "is_cf.hpp"
#include "is_key.hpp"
#include "math.hpp"
#include "mp_integer.hpp"
#include "mp_rational.hpp"
#include "pow.hpp"
#include "s11n.hpp"
#include "safe_cast.hpp"
#include "symbol.hpp"
#include "symbol_set.hpp"
#include "term.hpp"
#include "type_traits.hpp"

namespace piranha
{

// Fwd declaration.
template <typename, typename>
class monomial;
}

// Implementation of the Boost s11n api.
namespace boost
{
namespace serialization
{

template <typename Archive, typename T, typename S>
inline void save(Archive &ar, const piranha::boost_s11n_key_wrapper<piranha::monomial<T, S>> &k, unsigned)
{
    if (unlikely(k.key().size() != k.ss().size())) {
        piranha_throw(std::invalid_argument, "incompatible symbol set in monomial serialization: the reference "
                                             "symbol set has a size of "
                                                 + std::to_string(k.ss().size())
                                                 + ", while the monomial being serialized has a size of "
                                                 + std::to_string(k.key().size()));
    }
    piranha::boost_save(ar, k.key().m_container);
}

template <typename Archive, typename T, typename S>
inline void load(Archive &ar, piranha::boost_s11n_key_wrapper<piranha::monomial<T, S>> &k, unsigned)
{
    piranha::boost_load(ar, k.key().m_container);
    if (unlikely(k.key().size() != k.ss().size())) {
        piranha_throw(std::invalid_argument, "incompatible symbol set in monomial serialization: the reference "
                                             "symbol set has a size of "
                                                 + std::to_string(k.ss().size())
                                                 + ", while the monomial being deserialized has a size of "
                                                 + std::to_string(k.key().size()));
    }
}

template <typename Archive, typename T, typename S>
inline void serialize(Archive &ar, piranha::boost_s11n_key_wrapper<piranha::monomial<T, S>> &k, unsigned version)
{
    split_free(ar, k, version);
}
}
}

namespace piranha
{

/// Monomial class.
/**
 * This class extends piranha::array_key to define a series key type representing monomials, that is, objects of the
 * form:
 * \f[
 * x_0^{y_0} \cdot x_1^{y_1} \cdot \ldots \cdot x_n^{y_n}.
 * \f]
 * The type \p T represents the type of the exponents \f$ y_i \f$, which are stored in a flat array.
 *
 * This class satisfies the piranha::is_key type trait.
 *
 * ## Type requirements ##
 *
 * \p T and \p S must be suitable for use as first and third template arguments in piranha::array_key. Additionally,
 * \p T must be copy-assignable and it must satisfy the following type-traits:
 * - piranha::has_is_unitary,
 * - piranha::is_ostreamable,
 * - piranha::has_negate.
 *
 * ## Exception safety guarantee ##
 *
 * Unless noted otherwise, this class provides the same exception safety guarantee as piranha::array_key.
 *
 * ## Move semantics ##
 *
 * Move semantics is equivalent to piranha::array_key's move semantics.
 */
template <typename T, typename S = std::integral_constant<std::size_t, 0u>>
class monomial : public array_key<T, monomial<T, S>, S>
{
    PIRANHA_TT_CHECK(has_is_unitary, T);
    PIRANHA_TT_CHECK(is_ostreamable, T);
    PIRANHA_TT_CHECK(has_negate, T);
    PIRANHA_TT_CHECK(std::is_copy_assignable, T);
    using base = array_key<T, monomial<T, S>, S>;
    // Eval and subs type definition.
    template <typename U, typename = void>
    struct eval_type_ {
    };
    template <typename U>
    using e_type = decltype(math::pow(std::declval<U const &>(), std::declval<T const &>()));
    template <typename U>
    struct eval_type_<U, typename std::enable_if<is_multipliable_in_place<e_type<U>>::value
                                                 && std::is_constructible<e_type<U>, int>::value
                                                 && detail::is_pmappable<U>::value>::type> {
        using type = e_type<U>;
    };
    template <typename U>
    using eval_type = typename eval_type_<U>::type;
    // Enabler for ctor from init list.
    template <typename U>
    using init_list_enabler =
        typename std::enable_if<std::is_constructible<base, std::initializer_list<U>>::value, int>::type;
    // Multiplication and division.
    template <typename Cf, typename U>
    using multiply_enabler = typename std::enable_if<detail::true_tt<decltype(std::declval<U const &>().vector_add(
                                                         std::declval<U &>(), std::declval<U const &>()))>::value
                                                         && detail::true_tt<detail::cf_mult_enabler<Cf>>::value,
                                                     int>::type;
    template <typename U>
    using monomial_multiply_enabler =
        typename std::enable_if<detail::true_tt<decltype(std::declval<U const &>().vector_add(
                                    std::declval<U &>(), std::declval<U const &>()))>::value,
                                int>::type;
    template <typename U>
    using monomial_divide_enabler =
        typename std::enable_if<detail::true_tt<decltype(std::declval<U const &>().vector_sub(
                                    std::declval<U &>(), std::declval<U const &>()))>::value,
                                int>::type;
    // Enabler for linear argument.
    template <typename U>
    using linarg_enabler = typename std::enable_if<has_safe_cast<integer, U>::value, int>::type;
    // Enabler and machinery for pow.
    // When the exponent is an integral, promote to integer during exponentiation
    // for safe operations.
    template <typename U, typename std::enable_if<std::is_integral<U>::value, int>::type = 0>
    static integer get_pow_arg(const U &n)
    {
        return integer(n);
    }
    // Otherwise, just return the unchanged reference.
    template <typename U, typename std::enable_if<!std::is_integral<U>::value, int>::type = 0>
    static const U &get_pow_arg(const U &x)
    {
        return x;
    }
    using pow_type = typename std::conditional<std::is_integral<T>::value, integer &&, const T &>::type;
    template <typename U>
    using pow_enabler =
        typename std::enable_if<has_safe_cast<T, decltype(std::declval<pow_type>() * std::declval<const U &>())>::value,
                                int>::type;
    // Machinery to determine the degree type.
    // NOTE: add_type<U> will be promoted to a int/unsigned in case U is a short int.
    template <typename U>
    using add_type = decltype(std::declval<const U &>() + std::declval<const U &>());
    template <typename U>
    using degree_type = typename std::enable_if<std::is_constructible<add_type<U>, int>::value
                                                    && is_addable_in_place<add_type<U>, U>::value,
                                                add_type<U>>::type;
    // Helpers to add exponents in the degree computation.
    template <typename U, typename std::enable_if<std::is_integral<U>::value, int>::type = 0>
    static void expo_add(degree_type<U> &retval, const U &n)
    {
        detail::safe_integral_adder(retval, static_cast<degree_type<U>>(n));
    }
    template <typename U, typename std::enable_if<!std::is_integral<U>::value, int>::type = 0>
    static void expo_add(degree_type<U> &retval, const U &x)
    {
        retval += x;
    }
    // Integrate utils.
    // In-place increment by one, checked for integral types.
    template <typename U, typename std::enable_if<std::is_integral<U>::value, int>::type = 0>
    static void ip_inc(U &x)
    {
        if (unlikely(x == std::numeric_limits<U>::max())) {
            piranha_throw(std::invalid_argument, "positive overflow error in the calculation of the "
                                                 "integral of a monomial");
        }
        x = static_cast<U>(x + U(1));
    }
    template <typename U, typename std::enable_if<!std::is_integral<U>::value, int>::type = 0>
    static void ip_inc(U &x)
    {
        x = x + U(1);
    }
    template <typename U>
    using integrate_enabler =
        typename std::enable_if<std::is_assignable<U &, decltype(std::declval<U &>() + std::declval<U>())>::value,
                                int>::type;
    // Partial utils.
    // In-place decrement by one, checked for integral types.
    template <typename U, typename std::enable_if<std::is_integral<U>::value, int>::type = 0>
    static void ip_dec(U &x)
    {
        if (unlikely(x == std::numeric_limits<U>::min())) {
            piranha_throw(std::invalid_argument, "negative overflow error in the calculation of the "
                                                 "partial derivative of a monomial");
        }
        x = static_cast<U>(x - U(1));
    }
    template <typename U, typename std::enable_if<!std::is_integral<U>::value, int>::type = 0>
    static void ip_dec(U &x)
    {
        x = x - U(1);
    }
    template <typename U>
    using partial_enabler =
        typename std::enable_if<std::is_assignable<U &, decltype(std::declval<U &>() - std::declval<U>())>::value,
                                int>::type;
    // Subs support.
    // NOTE: this can obviously be compressed and implemented more cleanly with a single enable_if, but unfortunately
    // when this pattern is used both here and in k_monomial, a weird compiler error results with both GCC 4.8 and 4.9.
    // We need to check with GCC 5 what happens, and in case the problem is still there, it needs to be reported.
    template <typename U>
    using subs_type__ = decltype(math::pow(std::declval<const U &>(), std::declval<const T &>()));
    template <typename U, typename = void>
    struct subs_type_ {
    };
    template <typename U>
    struct subs_type_<U,
                      typename std::enable_if<std::is_constructible<subs_type__<U>, int>::value
                                              && std::is_assignable<subs_type__<U> &, subs_type__<U>>::value>::type> {
        using type = subs_type__<U>;
    };
    template <typename U>
    using subs_type = typename subs_type_<U>::type;
    // ipow subs support.
    template <typename U>
    using ipow_subs_type__ = decltype(math::pow(std::declval<const U &>(), std::declval<const integer &>()));
    template <typename U, typename = void>
    struct ipow_subs_type_ {
    };
    template <typename U>
    struct ipow_subs_type_<U, typename std::enable_if<std::is_constructible<ipow_subs_type__<U>, int>::value
                                                      && std::is_assignable<ipow_subs_type__<U> &,
                                                                            ipow_subs_type__<U>>::value
                                                      && has_safe_cast<rational, typename base::value_type>::value
                                                      && is_subtractable_in_place<typename base::value_type,
                                                                                  integer>::value>::type> {
        using type = ipow_subs_type__<U>;
    };
    template <typename U>
    using ipow_subs_type = typename ipow_subs_type_<U>::type;
    // Enabler for ctor from range.
    template <typename Iterator>
    using it_ctor_enabler =
        typename std::enable_if<is_input_iterator<Iterator>::value
                                    && has_safe_cast<T, typename std::iterator_traits<Iterator>::value_type>::value,
                                int>::type;
    // Less-than operator.
    template <typename U>
    using comparison_enabler = typename std::enable_if<is_less_than_comparable<U>::value, int>::type;

public:
    /// Arity of the multiply() method.
    static const std::size_t multiply_arity = 1u;
    /// Defaulted default constructor.
    monomial() = default;
    /// Defaulted copy constructor.
    monomial(const monomial &) = default;
    /// Defaulted move constructor.
    monomial(monomial &&) = default;
    /// Constructor from initializer list.
    /**
     * \note
     * This constructor is enabled only if the corresponding constructor in piranha::array_key is enabled.
     *
     * @param list initializer list.
     *
     * @see piranha::array_key's constructor from initializer list.
     */
    template <typename U, init_list_enabler<U> = 0>
    explicit monomial(std::initializer_list<U> list) : base(list)
    {
    }
    /// Constructor from range and symbol set.
    /**
     * \note
     * This constructor is enabled only if \p Iterator is an input iterator whose value type
     * can be cast safely to \p T.
     *
     * This constructor will copy the elements from the range defined by \p begin and \p end into \p this,
     * using piranha::safe_cast() for any necessary type conversion. If the final size of \p this is different
     * from the size of \p s, a runtime error will be produced. This constructor is used by
     * piranha::polynomial::find_cf().
     *
     * @param begin beginning of the range.
     * @param end end of the range.
     * @param s reference symbol set.
     *
     * @throws std::invalid_argument if the final size of \p this and the size of \p s differ.
     * @throws unspecified any exception thrown by:
     * - piranha::safe_cast(),
     * - push_back().
     */
    template <typename Iterator, it_ctor_enabler<Iterator> = 0>
    explicit monomial(Iterator begin, Iterator end, const symbol_set &s)
    {
        for (; begin != end; ++begin) {
            this->push_back(safe_cast<T>(*begin));
        }
        if (unlikely(this->size() != s.size())) {
            piranha_throw(std::invalid_argument, "invalid monomial");
        }
    }
    /// Constructor from range.
    /**
     * \note
     * This constructor is enabled only if \p Iterator is an input iterator whose value type
     * can be cast safely to \p T.
     *
     * This constructor will copy the elements from the range defined by \p begin and \p end into \p this,
     * using piranha::safe_cast() for any necessary type conversion.
     *
     * @param begin beginning of the range.
     * @param end end of the range.
     *
     * @throws unspecified any exception thrown by:
     * - piranha::safe_cast(),
     * - push_back().
     */
    template <typename Iterator, it_ctor_enabler<Iterator> = 0>
    explicit monomial(Iterator begin, Iterator end)
    {
        for (; begin != end; ++begin) {
            this->push_back(safe_cast<T>(*begin));
        }
    }
    PIRANHA_FORWARDING_CTOR(monomial, base)
    /// Trivial destructor.
    ~monomial()
    {
        PIRANHA_TT_CHECK(is_key, monomial);
    }
    /// Copy assignment operator.
    /**
     * @param other the assignment argument.
     *
     * @return a reference to \p this.
     *
     * @throws unspecified any exception thrown by the assignment operator of the base class.
     */
    monomial &operator=(const monomial &other) = default;
    /// Defaulted move assignment operator.
    /**
     * @param other the assignment argument.
     *
     * @return a reference to \p this.
     */
    monomial &operator=(monomial &&other) = default;
    /// Compatibility check
    /**
     * A monomial and a set of arguments are compatible if their sizes coincide.
     *
     * @param args reference arguments set.
     *
     * @return <tt>this->size() == args.size()</tt>.
     */
    bool is_compatible(const symbol_set &args) const noexcept
    {
        return (this->size() == args.size());
    }
    /// Ignorability check.
    /**
     * A monomial is never ignorable by definition.
     *
     * @param args reference arguments set.
     *
     * @return \p false.
     */
    bool is_ignorable(const symbol_set &args) const noexcept
    {
        (void)args;
        piranha_assert(is_compatible(args));
        return false;
    }
    /// Check if monomial is unitary.
    /**
     * A monomial is unitary if, for all its elements, piranha::math::is_zero() returns \p true.
     *
     * @param args reference set of piranha::symbol.
     *
     * @return \p true if the monomial is unitary, \p false otherwise.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws unspecified any exception thrown by piranha::math::is_zero().
     */
    bool is_unitary(const symbol_set &args) const
    {
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        return std::all_of(this->begin(), this->end(), [](const T &element) { return math::is_zero(element); });
    }
    /// Degree.
    /**
     * \note
     * This method is enabled only if \p T is addable and the type resulting from the addition is constructible from \p
     * int
     * and monomial::value_type can be added in-place to it.
     *
     * This method will return the degree of the monomial, computed via the summation of the exponents of the monomial.
     * If \p T is a C++ integral type, the addition of the exponents will be checked for overflow.
     *
     * @param args reference set of piranha::symbol.
     *
     * @return the degree of the monomial.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws std::overflow_error if the exponent type is a C++ integral type and the computation
     * of the degree overflows.
     * @throws unspecified any exception thrown by the invoked constructor or arithmetic operators.
     */
    template <typename U = T>
    degree_type<U> degree(const symbol_set &args) const
    {
        // This is a fast check, better always to keep it.
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "invalid arguments set");
        }
        degree_type<U> retval(0);
        for (const auto &x : *this) {
            expo_add(retval, x);
        }
        return retval;
    }
    /// Partial degree.
    /**
     * \note
     * This method is enabled only if \p T is addable and the type resulting from the addition is constructible from \p
     * int
     * and monomial::value_type can be added in-place to it.
     *
     * This method will return the partial degree of the monomial, computed via the summation of the exponents of the
     * monomial.
     * If \p T is a C++ integral type, the addition of the exponents will be checked for overflow.
     *
     * The \p p argument is used to indicate which exponents are to be taken into account when computing the partial
     * degree.
     * Exponents not in \p p will be discarded during the computation of the partial degree.
     *
     * @param p positions of the symbols to be considered.
     * @param args reference set of piranha::symbol.
     *
     * @return the partial degree of the monomial.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ, or if \p p is
     * not compatible with the monomial.
     * @throws std::overflow_error if the exponent type is a C++ integral type and the computation
     * of the degree overflows.
     * @throws unspecified any exception thrown by the invoked constructor or arithmetic operators.
     */
    template <typename U = T>
    degree_type<U> degree(const symbol_set::positions &p, const symbol_set &args) const
    {
        if (unlikely(args.size() != this->size() || (p.size() && p.back() >= this->size()))) {
            piranha_throw(std::invalid_argument, "invalid arguments set or positions");
        }
        auto cit = this->begin();
        degree_type<U> retval(0);
        for (const auto &i : p) {
            expo_add(retval, cit[i]);
        }
        return retval;
    }
    /// Low degree (equivalent to the degree).
    /**
     * @param args reference set of piranha::symbol.
     *
     * @return the output of degree(const symbol_set &args) const.
     *
     * @throws unspecified any exception thrown by degree(const symbol_set &args) const.
     */
    template <typename U = T>
    degree_type<U> ldegree(const symbol_set &args) const
    {
        return degree(args);
    }
    /// Partial low degree (equivalent to the partial degree).
    /**
     * @param p positions of the symbols to be considered.
     * @param args reference set of piranha::symbol.
     *
     * @return the output of degree(const symbol_set::positions &, const symbol_set &) const.
     *
     * @throws unspecified any exception thrown by degree(const symbol_set::positions &, const symbol_set &) const.
     */
    template <typename U = T>
    degree_type<U> ldegree(const symbol_set::positions &p, const symbol_set &args) const
    {
        return degree(p, args);
    }
    /// Name of the linear argument.
    /**
     * \note
     * This method is enabled only if the exponent type supports piranha::safe_cast() to piranha::integer.
     *
     * If the monomial is linear in a variable (i.e., all exponents are zero apart from a single unitary
     * exponent), the name of the variable will be returned. Otherwise, an error will be raised.
     *
     * @param args reference set of piranha::symbol.
     *
     * @return name of the linear variable.
     *
     * @throws std::invalid_argument if the monomial is not linear or if the sizes of \p args and \p this differ.
     * @throws unspecified any exception thrown by:
     * - piranha::safe_cast(),
     * - piranha::math::is_zero() or piranha::is_unitary().
     */
    template <typename U = T, linarg_enabler<U> = 0>
    std::string linear_argument(const symbol_set &args) const
    {
        if (!is_compatible(args)) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        typedef typename base::size_type size_type;
        const size_type size = this->size();
        size_type n_linear = 0u, candidate = 0u;
        for (size_type i = 0u; i < size; ++i) {
            integer tmp;
            try {
                tmp = safe_cast<integer>((*this)[i]);
            } catch (const safe_cast_failure &) {
                piranha_throw(std::invalid_argument, "exponent is not an integer");
            }
            if (math::is_zero(tmp)) {
                continue;
            }
            if (!math::is_unitary(tmp)) {
                piranha_throw(std::invalid_argument, "exponent is not unitary");
            }
            candidate = i;
            ++n_linear;
        }
        if (n_linear != 1u) {
            piranha_throw(std::invalid_argument, "monomial is not linear");
        }
        return args[static_cast<decltype(args.size())>(candidate)].get_name();
    }
    /// Monomial exponentiation.
    /**
     * \note
     * This method is enabled if the exponent type (or its promoted piranha::integer counterpart)
     * is multipliable by \p U and the result type can be cast safely back to the exponent type.
     *
     * Will return a monomial corresponding to \p this raised to the <tt>x</tt>-th power. The exponentiation
     * is computed via the multiplication of the exponents by \p x. If the exponent type is a C++
     * integral type, each exponent will be promoted to piranha::integer before the exponentiation
     * takes place.
     *
     * @param x exponent.
     * @param args reference set of piranha::symbol.
     *
     * @return \p this to the power of \p x.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws unspecified any exception thrown by monomial copy construction
     * or in-place multiplication of exponents by \p x, and by the invoked
     * operations on piranha::integer, if any.
     */
    template <typename U, pow_enabler<U> = 0>
    monomial pow(const U &x, const symbol_set &args) const
    {
        typedef typename base::size_type size_type;
        if (!is_compatible(args)) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        // Init with zeroes.
        monomial retval(args);
        const size_type size = retval.size();
        for (decltype(retval.size()) i = 0u; i < size; ++i) {
            retval[i] = safe_cast<T>(get_pow_arg((*this)[i]) * x);
        }
        return retval;
    }
    /// Partial derivative.
    /**
     * \note
     * This method is enabled only if the exponent type is subtractable and the result of the operation
     * can be assigned back to the exponent type.
     *
     * This method will return the partial derivative of \p this with respect to the symbol at the position indicated by
     * \p p.
     * The result is a pair consisting of the exponent associated to \p p before differentiation and the monomial itself
     * after differentiation. If \p p is empty or if the exponent associated to it is zero,
     * the returned pair will be <tt>(0,monomial{args})</tt>.
     *
     * If the exponent type is an integral type, then the decrement-by-one operation on the affected exponent is checked
     * for negative overflow.
     *
     * @param p position of the symbol with respect to which the differentiation will be calculated.
     * @param args reference set of piranha::symbol.
     *
     * @return result of the differentiation.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ, if the size of \p p is
     * greater than one, if the position specified by \p p is invalid or if the computation on integral exponents
     * results in an overflow error.
     * @throws unspecified any exception thrown by:
     * - monomial and exponent construction,
     * - the exponent type's subtraction operator,
     * - piranha::math::is_zero().
     */
    template <typename U = T, partial_enabler<U> = 0>
    std::pair<U, monomial> partial(const symbol_set::positions &p, const symbol_set &args) const
    {
        if (!is_compatible(args)) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        // Cannot take derivative wrt more than one variable, and the position of that variable
        // must be compatible with the monomial.
        if (p.size() > 1u || (p.size() == 1u && p.back() >= args.size())) {
            piranha_throw(std::invalid_argument, "invalid size of symbol_set::positions");
        }
        // Derivative wrt a variable not in the monomial: position is empty, or refers to a
        // variable with zero exponent.
        // NOTE: safe to take this->begin() here, as the checks on the positions above ensure
        // there is a valid position and hence the size must be not zero.
        if (!p.size() || math::is_zero(this->begin()[*p.begin()])) {
            return std::make_pair(U(0), monomial(args));
        }
        // Copy of the original monomial.
        monomial m(*this);
        auto m_b = m.begin();
        // Original exponent.
        U v(m_b[*p.begin()]);
        // Decrement the exponent in the monomial.
        ip_dec(m_b[*p.begin()]);
        return std::make_pair(std::move(v), std::move(m));
    }
    /// Integration.
    /**
     * \note
     * This method is enabled only if the exponent type is addable and the result of the operation
     * can be assigned back to the exponent type.
     *
     * Will return the antiderivative of \p this with respect to symbol \p s. The result is a pair
     * consisting of the exponent associated to \p s increased by one and the monomial itself
     * after integration. If \p s is not in \p args, the returned monomial will have an extra exponent
     * set to 1 in the same position \p s would have if it were added to \p args.
     *
     * If the exponent corresponding to \p s is -1, an error will be produced.
     *
     * If the exponent type is an integral type, then the increment-by-one operation on the affected exponent is checked
     * for negative overflow.
     *
     * @param s symbol with respect to which the integration will be calculated.
     * @param args reference set of piranha::symbol.
     *
     * @return result of the integration.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ,
     * if the exponent associated to \p s is -1, or if the computation on integral exponents
     * results in an overflow error.
     * @throws unspecified any exception thrown by:
     * - piranha::math::is_zero(),
     * - exponent construction,
     * - push_back(),
     * - the exponent type's addition and assignment operators.
     */
    template <typename U = T, integrate_enabler<U> = 0>
    std::pair<U, monomial> integrate(const symbol &s, const symbol_set &args) const
    {
        typedef typename base::size_type size_type;
        if (!is_compatible(args)) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        monomial retval;
        T expo(0), one(1);
        for (size_type i = 0u; i < args.size(); ++i) {
            if (math::is_zero(expo) && s < args[i]) {
                // If we went past the position of s in args and still we
                // have not performed the integration, it means that we need to add
                // a new exponent.
                retval.push_back(one);
                expo = one;
            }
            retval.push_back((*this)[i]);
            if (args[i] == s) {
                // NOTE: here using i is safe: if retval gained an extra exponent in the condition above,
                // we are never going to land here as args[i] is at this point never going to be s.
                ip_inc(retval[i]);
                if (math::is_zero(retval[i])) {
                    piranha_throw(std::invalid_argument,
                                  "unable to perform monomial integration: negative unitary exponent");
                }
                expo = retval[i];
            }
        }
        // If expo is still zero, it means we need to add a new exponent at the end.
        if (math::is_zero(expo)) {
            retval.push_back(one);
            expo = one;
        }
        return std::make_pair(std::move(expo), std::move(retval));
    }
    /// Print.
    /**
     * This method will print to stream a human-readable representation of the monomial.
     *
     * @param os target stream.
     * @param args reference set of piranha::symbol.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws unspecified any exception resulting from:
     * - printing exponents to stream and the public interface of \p os,
     * - piranha::math::is_zero(), piranha::math::is_unitary().
     */
    void print(std::ostream &os, const symbol_set &args) const
    {
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        bool empty_output = true;
        for (typename base::size_type i = 0u; i < this->size(); ++i) {
            if (!math::is_zero((*this)[i])) {
                // If we are going to print a symbol, and something has been printed before,
                // then we are going to place the multiplication sign.
                if (!empty_output) {
                    os << '*';
                }
                os << args[i].get_name();
                empty_output = false;
                if (!math::is_unitary((*this)[i])) {
                    os << "**" << detail::prepare_for_print((*this)[i]);
                }
            }
        }
    }
    /// Print in TeX mode.
    /**
     * Will print to stream a TeX representation of the monomial.
     *
     * @param os target stream.
     * @param args reference set of piranha::symbol.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws unspecified any exception resulting from:
     * - construction, comparison and assignment of exponents,
     * - printing exponents to stream and the public interface of \p os,
     * - piranha::math::negate(), piranha::math::is_zero(), piranha::math::is_unitary().
     */
    void print_tex(std::ostream &os, const symbol_set &args) const
    {
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        std::ostringstream oss_num, oss_den, *cur_oss;
        const T zero(0);
        T cur_value;
        for (typename base::size_type i = 0u; i < this->size(); ++i) {
            cur_value = (*this)[i];
            if (!math::is_zero(cur_value)) {
                // NOTE: the sequencing rules of the ternary operator ensure that math::negate(cur_value) is
                // evaluated after zero < cur_value.
                cur_oss
                    = (zero < cur_value) ? std::addressof(oss_num) : (math::negate(cur_value), std::addressof(oss_den));
                (*cur_oss) << "{" << args[i].get_name() << "}";
                if (!math::is_unitary(cur_value)) {
                    (*cur_oss) << "^{" << detail::prepare_for_print(cur_value) << "}";
                }
            }
        }
        const std::string num_str = oss_num.str(), den_str = oss_den.str();
        if (!num_str.empty() && !den_str.empty()) {
            os << "\\frac{" << num_str << "}{" << den_str << "}";
        } else if (!num_str.empty() && den_str.empty()) {
            os << num_str;
        } else if (num_str.empty() && !den_str.empty()) {
            os << "\\frac{1}{" << den_str << "}";
        }
    }
    /// Evaluation.
    /**
     * \note
     * This method is available only if \p U satisfies the following requirements:
     * - it can be used in piranha::symbol_set::positions_map,
     * - it can be used in piranha::math::pow() with the monomial exponents as powers, yielding a type \p eval_type,
     * - \p eval_type is constructible from \p int,
     * - \p eval_type is multipliable in place.
     *
     * The return value will be built by iteratively applying piranha::math::pow() using the values provided
     * by \p pmap as bases and the values in the monomial as exponents. If the size of the monomial is zero, 1 will be
     * returned. If \p args is not compatible with \p this and \p pmap, or the positions in \p pmap do not reference
     * only and all the exponents in the monomial, an error will be thrown.
     *
     * @param pmap piranha::symbol_set::positions_map that will be used for substitution.
     * @param args reference set of piranha::symbol.
     *
     * @return the result of evaluating \p this with the values provided in \p pmap.
     *
     * @throws std::invalid_argument if there exist an incompatibility between \p this,
     * \p args or \p pmap.
     * @throws unspecified any exception thrown by:
     * - construction of the return type,
     * - piranha::math::pow() or the in-place multiplication operator of the return type.
     */
    template <typename U>
    eval_type<U> evaluate(const symbol_set::positions_map<U> &pmap, const symbol_set &args) const
    {
        using return_type = eval_type<U>;
        using size_type = typename base::size_type;
        // NOTE: the positions map must have the same number of elements as this, and it must
        // be made of consecutive positions [0,size - 1].
        if (unlikely(pmap.size() != this->size() || (pmap.size() && pmap.back().first != pmap.size() - 1u))) {
            piranha_throw(std::invalid_argument, "invalid positions map for evaluation");
        }
        // Args must be compatible with this.
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        return_type retval(1);
        auto it = pmap.begin();
        for (size_type i = 0u; i < this->size(); ++i, ++it) {
            piranha_assert(it != pmap.end() && it->first == i);
            retval *= math::pow(it->second, (*this)[i]);
        }
        piranha_assert(it == pmap.end());
        return retval;
    }
    /// Substitution.
    /**
     * \note
     * This method is enabled only if:
     * - \p U can be raised to the value type, yielding a type \p subs_type,
     * - \p subs_type can be constructed from \p int and it is assignable.
     *
     * Substitute the symbol called \p s in the monomial with quantity \p x. The return value is vector containing one
     * pair in which the first
     * element is the result of substituting \p s with \p x (i.e., \p x raised to the power of the exponent
     * corresponding
     * to \p s), and the second element the monomial after the substitution has been performed (i.e., with the exponent
     * corresponding to \p s set to zero). If \p s is not in \p args, the return value will be <tt>(1,this)</tt> (i.e.,
     * the
     * monomial is unchanged and the substitution yields 1).
     *
     * @param s name of the symbol that will be substituted.
     * @param x quantity that will be substituted in place of \p s.
     * @param args reference set of piranha::symbol.
     *
     * @return the result of substituting \p x for \p s.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws unspecified any exception thrown by:
     * - construction and assignment of the return value,
     * - construction of an exponent from zero,
     * - piranha::math::pow(),
     * - piranha::array_key::push_back().
     */
    template <typename U>
    std::vector<std::pair<subs_type<U>, monomial>> subs(const std::string &s, const U &x, const symbol_set &args) const
    {
        using s_type = subs_type<U>;
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        std::vector<std::pair<s_type, monomial>> retval;
        s_type retval_s(1);
        monomial retval_key;
        for (typename base::size_type i = 0u; i < this->size(); ++i) {
            if (args[i].get_name() == s) {
                retval_s = math::pow(x, (*this)[i]);
                retval_key.push_back(T(0));
            } else {
                retval_key.push_back((*this)[i]);
            }
        }
        piranha_assert(retval_key.size() == this->size());
        retval.push_back(std::make_pair(std::move(retval_s), std::move(retval_key)));
        return retval;
    }
    /// Substitution of integral power.
    /**
     * \note
     * This method is enabled only if:
     * - \p U can be raised to a piranha::integer power, yielding a type \p subs_type,
     * - \p subs_type is constructible from \p int and assignable,
     * - the value type of the monomial can be cast safely to piranha::rational and it supports
     *   in-place subtraction with piranha::integer.
     *
     * Substitute the symbol called \p s to the power of \p n with quantity \p x. The return value is a vector
     * containing a single pair in which the first
     * element is the result of the substitution, and the second element the monomial after the substitution has been
     * performed.
     * If \p s is not in \p args, the return value will be <tt>(1,this)</tt> (i.e., the
     * monomial is unchanged and the substitution yields 1).
     *
     * The method will substitute also \p s to powers higher than \p n in absolute value.
     * For instance, substitution of <tt>y**2</tt> with \p a in <tt>y**7</tt> will produce <tt>a**3 * y</tt>, and
     * substitution of <tt>y**-2</tt> with \p a in <tt>y**-7</tt> will produce <tt>a**3 * y**-1</tt>.
     *
     * @param s name of the symbol that will be substituted.
     * @param n power of \p s that will be substituted.
     * @param x quantity that will be substituted in place of \p s to the power of \p n.
     * @param args reference set of piranha::symbol.
     *
     * @return the result of substituting \p x for \p s to the power of \p n.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws unspecified any exception thrown by:
     * - construction and assignment of the return value,
     * - construction of piranha::rational,
     * - piranha::safe_cast(),
     * - piranha::math::pow(),
     * - piranha::array_key::push_back(),
     * - the in-place subtraction operator of the exponent type.
     */
    template <typename U>
    std::vector<std::pair<ipow_subs_type<U>, monomial>> ipow_subs(const std::string &s, const integer &n, const U &x,
                                                                  const symbol_set &args) const
    {
        using s_type = ipow_subs_type<U>;
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        s_type retval_s(1);
        monomial retval_key;
        for (typename base::size_type i = 0u; i < this->size(); ++i) {
            retval_key.push_back((*this)[i]);
            if (args[i].get_name() == s) {
                const rational tmp(safe_cast<rational>((*this)[i]) / n);
                if (tmp >= 1) {
                    const auto tmp_t = static_cast<integer>(tmp);
                    retval_s = math::pow(x, tmp_t);
                    // NOTE: chance for a multsub, or maybe a multadd
                    // after changing the sign of tmp_t?
                    retval_key[i] -= tmp_t * n;
                }
            }
        }
        std::vector<std::pair<s_type, monomial>> retval;
        retval.push_back(std::make_pair(std::move(retval_s), std::move(retval_key)));
        return retval;
    }
    /// Multiply terms with a monomial key.
    /**
     * \note
     * This method is enabled only if the following conditions hold:
     * - piranha::array_key::vector_add() is enabled for \p t1,
     * - \p Cf satisfies piranha::is_cf and piranha::has_mul3.
     *
     * Multiply \p t1 by \p t2, storing the result in the only element of \p res. If \p Cf is an instance of
     * piranha::mp_rational, then
     * only the numerators of the coefficients will be multiplied.
     *
     * This method offers the basic exception safety guarantee.
     *
     * @param res return value.
     * @param t1 first argument.
     * @param t2 second argument.
     * @param args reference set of arguments.
     *
     * @throws std::invalid_argument if the size of \p t1 differs from the size of \p args.
     * @throws unspecified any exception thrown by piranha::array_key::vector_add(), or by piranha::math::mul3().
     */
    // NOTE: it should be ok to use this method (and the one below) with overlapping arguments, as this is allowed
    // in small_vector::add().
    template <typename Cf, typename U = monomial, multiply_enabler<Cf, U> = 0>
    static void multiply(std::array<term<Cf, monomial>, multiply_arity> &res, const term<Cf, monomial> &t1,
                         const term<Cf, monomial> &t2, const symbol_set &args)
    {
        term<Cf, monomial> &t = res[0u];
        // NOTE: the check on the monomials' size is in vector_add().
        if (unlikely(t1.m_key.size() != args.size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        // Coefficient.
        detail::cf_mult_impl(t.m_cf, t1.m_cf, t2.m_cf);
        // Now deal with the key.
        t1.m_key.vector_add(t.m_key, t2.m_key);
    }
    /// Multiply monomials.
    /**
     * \note
     * This method is enabled only if piranha::array_key::vector_add() is enabled for \p a.
     *
     * Multiply \p a by \p b, storing the result in \p out.
     *
     * This method offers the basic exception safety guarantee.
     *
     * @param out return value.
     * @param a first argument.
     * @param b second argument.
     * @param args reference set of arguments.
     *
     * @throws std::invalid_argument if the size of \p a differs from the size of \p args.
     * @throws unspecified any exception thrown by piranha::array_key::vector_add().
     */
    template <typename U = monomial, monomial_multiply_enabler<U> = 0>
    static void multiply(monomial &out, const monomial &a, const monomial &b, const symbol_set &args)
    {
        if (unlikely(a.size() != args.size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        a.vector_add(out, b);
    }
    /// Divide monomials.
    /**
     * \note
     * This method is enabled only if piranha::array_key::vector_sub() is enabled for \p a.
     *
     * Divide \p a by \p b, storing the result in \p out.
     *
     * This method offers the basic exception safety guarantee.
     *
     * @param out return value.
     * @param a first argument.
     * @param b second argument.
     * @param args reference set of arguments.
     *
     * @throws std::invalid_argument if the size of \p a differs from the size of \p args.
     * @throws unspecified any exception thrown by piranha::array_key::vector_sub().
     */
    template <typename U = monomial, monomial_divide_enabler<U> = 0>
    static void divide(monomial &out, const monomial &a, const monomial &b, const symbol_set &args)
    {
        if (unlikely(a.size() != args.size())) {
            piranha_throw(std::invalid_argument, "invalid size of arguments set");
        }
        a.vector_sub(out, b);
    }
    /// Comparison operator.
    /**
     * The two monomials will be compared lexicographically.
     *
     * @param other comparison argument.
     *
     * @return \p true if \p this is lexicographically less than \p other, \p false otherwise.
     *
     * @throws std::invalid_argument if the sizes of \p this and \p other differ.
     * @throws unspecified any exception thrown by <tt>std::lexicographical_compare()</tt>.
     */
    bool operator<(const monomial &other) const
    {
        const auto sbe1 = this->size_begin_end();
        const auto sbe2 = other.size_begin_end();
        if (unlikely(std::get<0u>(sbe1) != std::get<0u>(sbe2))) {
            piranha_throw(std::invalid_argument, "mismatched sizes in monomial comparison");
        }
        return std::lexicographical_compare(std::get<1u>(sbe1), std::get<2u>(sbe1), std::get<1u>(sbe2),
                                            std::get<2u>(sbe2));
    }
    /// Extract the vector of exponents.
    /**
     * This method will write into \p out the content of \p this. If necessary, \p out will
     * be resized to match the size of \p this.
     *
     * Note that this method does not offer a strong exception safety guarantee, as it uses
     * <tt>std::copy()</tt> internally to copy the exponents to \p out.
     *
     * @param out vector into which the exponents will be copied.
     * @param args reference set of arguments.
     *
     * @throws std::invalid_argument if the sizes of \p args and \p this differ.
     * @throws unspecified any exception thrown by:
     * - piranha::safe_cast(),
     * - the resizing of \p out,
     * - the copy assignment of the exponents.
     */
    void extract_exponents(std::vector<typename base::value_type> &out, const symbol_set &args) const
    {
        using v_size_type = decltype(out.size());
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "mismatch in the number of arguments");
        }
        if (unlikely(out.size() != this->size())) {
            out.resize(safe_cast<v_size_type>(this->size()));
        }
        auto sbe = this->size_begin_end();
        std::copy(std::get<1u>(sbe), std::get<2u>(sbe), out.begin());
    }
    /// Split.
    /**
     * This method will split \p this into two monomials: the second monomial will contain the exponent
     * of the first variable in \p args, the first monomial will contain all the other exponents.
     *
     * @param args reference arguments set.
     *
     * @return a pair of monomials, the second one containing the first exponent, the first one containing all the
     * other exponents.
     *
     * @throws std::invalid_argument if the size of \p args is different from the size of \p this or less than 2.
     * @throws unspecified any exception thrown by the constructor from a range.
     */
    std::pair<monomial, monomial> split(const symbol_set &args) const
    {
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "mismatch in the number of arguments");
        }
        if (unlikely(this->size() < 2u)) {
            piranha_throw(std::invalid_argument, "only monomials with 2 or more variables can be split");
        }
        auto sbe = this->size_begin_end();
        return std::make_pair(monomial(std::get<1u>(sbe) + 1, std::get<2u>(sbe)),
                              monomial(std::get<1u>(sbe), std::get<1u>(sbe) + 1));
    }
    /// Detect negative exponents.
    /**
     * This method will return \p true if at least one exponent is less than zero, \p false otherwise.
     *
     * @param args reference arguments set.
     *
     * @return \p true if at least one exponent is less than zero, \p false otherwise.
     *
     * @throws std::invalid_argument if the size of \p args is different from the size of \p this.
     * @throws unspecified any exception thrown by:
     * - the construction of an exponent from an \p int,
     * - the comparison operator of the exponent type.
     */
    bool has_negative_exponent(const symbol_set &args) const
    {
        using value_type = typename base::value_type;
        if (unlikely(args.size() != this->size())) {
            piranha_throw(std::invalid_argument, "mismatch in the number of arguments");
        }
        auto sbe = this->size_begin_end();
        const value_type zero(0);
        return std::any_of(std::get<1u>(sbe), std::get<2u>(sbe), [&zero](const value_type &e) { return e < zero; });
    }

private:
#if !defined(PIRANHA_DOXYGEN_INVOKED)
    // Make friend with the s11n functions.
    template <typename Archive, typename T1, typename S1>
    friend void
    boost::serialization::save(Archive &, const piranha::boost_s11n_key_wrapper<piranha::monomial<T1, S1>> &, unsigned);
    template <typename Archive, typename T1, typename S1>
    friend void boost::serialization::load(Archive &, piranha::boost_s11n_key_wrapper<piranha::monomial<T1, S1>> &,
                                           unsigned);
#endif
#if defined(PIRANHA_WITH_MSGPACK)
    // Enablers for msgpack serialization.
    template <typename Stream>
    using msgpack_pack_enabler
        = enable_if_t<conjunction<is_msgpack_stream<Stream>,
                                  has_msgpack_pack<Stream, typename base::container_type>>::value,
                      int>;
    template <typename U>
    using msgpack_convert_enabler = enable_if_t<has_msgpack_convert<typename U::container_type>::value, int>;

public:
    /// Serialize in msgpack format.
    /**
     * \note
     * This method is enabled only if \p Stream satisfies piranha::is_msgpack_stream and the internal container type
     * satisfies piranha::has_msgpack_pack.
     *
     * This method will pack \p this into \p packer using the format \p f.
     *
     * @param packer the target packer.
     * @param f the serialization format.
     * @param s reference arguments set.
     *
     * @throws std::invalid_argument if the sizes of \p s and \p this differ.
     * @throws unspecified any exception thrown by piranha::msgpack_pack().
     */
    template <typename Stream, msgpack_pack_enabler<Stream> = 0>
    void msgpack_pack(msgpack::packer<Stream> &packer, msgpack_format f, const symbol_set &s) const
    {
        if (unlikely(this->size() != s.size())) {
            piranha_throw(std::invalid_argument, "incompatible symbol set in monomial serialization: the reference "
                                                 "symbol set has a size of "
                                                     + std::to_string(s.size())
                                                     + ", while the monomial being serialized has a size of "
                                                     + std::to_string(this->size()));
        }
        piranha::msgpack_pack(packer, this->m_container, f);
    }
    /// Deserialize from msgpack object.
    /**
     * \note
     * This method is activated only if the internal container type satisfies piranha::has_msgpack_convert.
     *
     * This method will deserialize \p o into \p this using the format \p f.
     * This method provides the basic exception safety guarantee.
     *
     * @param o msgpack object that will be deserialized.
     * @param f serialization format.
     * @param s reference arguments set.
     *
     * @throws std::invalid_argument if the size of the deserialized array differs from the size of \p s.
     * @throws unspecified any exception thrown by piranha::msgpack_convert().
     */
    template <typename U = monomial, msgpack_convert_enabler<U> = 0>
    void msgpack_convert(const msgpack::object &o, msgpack_format f, const symbol_set &s)
    {
        piranha::msgpack_convert(this->m_container, o, f);
        if (unlikely(this->size() != s.size())) {
            piranha_throw(std::invalid_argument, "incompatible symbol set in monomial serialization: the reference "
                                                 "symbol set has a size of "
                                                     + std::to_string(s.size())
                                                     + ", while the monomial being deserialized has a size of "
                                                     + std::to_string(this->size()));
        }
    }
#endif
};

template <typename T, typename S>
const std::size_t monomial<T, S>::multiply_arity;

inline namespace impl
{

// Enablers for the boost s11n methods.
template <typename Archive, typename T, typename S>
using monomial_boost_save_enabler
    = enable_if_t<has_boost_save<Archive, typename monomial<T, S>::container_type>::value>;

template <typename Archive, typename T, typename S>
using monomial_boost_load_enabler
    = enable_if_t<has_boost_load<Archive, typename monomial<T, S>::container_type>::value>;
}

/// Specialisation of piranha::boost_save() for piranha::monomial.
/**
 * \note
 * This specialisation is enabled only if piranha::monomial::container_type satisfies piranha::has_boost_save.
 *
 * @throws std::invalid_argument if the size of the monomial differs from the size of the piranha::symbol_set.
 * @throws unspecified any exception thrown by piranha::bost_save() or by the public interface of
 * piranha::boost_s11n_key_wrapper.
 */
template <typename Archive, typename T, typename S>
struct boost_save_impl<Archive, boost_s11n_key_wrapper<monomial<T, S>>, monomial_boost_save_enabler<Archive, T, S>>
    : boost_save_via_boost_api<Archive, boost_s11n_key_wrapper<monomial<T, S>>> {
};

/// Specialisation of piranha::boost_load() for piranha::monomial.
/**
 * \note
 * This specialisation is enabled only if piranha::monomial::container_type satisfies piranha::has_boost_load.
 *
 * @throws std::invalid_argument if the size of the deserialized monomial differs from the size of
 * the piranha::symbol_set.
 * @throws unspecified any exception thrown by piranha::bost_load() or by the public interface of
 * piranha::boost_s11n_key_wrapper.
 */
template <typename Archive, typename T, typename S>
struct boost_load_impl<Archive, boost_s11n_key_wrapper<monomial<T, S>>, monomial_boost_load_enabler<Archive, T, S>>
    : boost_load_via_boost_api<Archive, boost_s11n_key_wrapper<monomial<T, S>>> {
};
}

namespace std
{

/// Specialisation of \p std::hash for piranha::monomial.
/**
 * Functionally equivalent to the \p std::hash specialisation for piranha::array_key.
 */
template <typename T, typename S>
struct hash<piranha::monomial<T, S>> : public hash<piranha::array_key<T, piranha::monomial<T, S>, S>> {
};
}

#endif
