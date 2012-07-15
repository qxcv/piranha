/***************************************************************************
 *   Copyright (C) 2009-2011 by Francesco Biscani                          *
 *   bluescarni@gmail.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef PIRANHA_KRONECKER_MONOMIAL_HPP
#define PIRANHA_KRONECKER_MONOMIAL_HPP

#include <algorithm>
#include <boost/concept/assert.hpp>
#include <boost/integer_traits.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/utility.hpp>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "concepts/degree_key.hpp"
#include "config.hpp"
#include "detail/km_commons.hpp"
#include "detail/kronecker_monomial_fwd.hpp"
#include "exceptions.hpp"
#include "integer.hpp"
#include "kronecker_array.hpp"
#include "math.hpp"
#include "static_vector.hpp"
#include "symbol_set.hpp"
#include "symbol.hpp"

namespace piranha
{

/// Kronecker monomial class.
/**
 * This class represents a multivariate monomial with integral exponents.
 * The values of the exponents are packed in a signed integer using Kronecker substitution, using the facilities provided
 * by piranha::kronecker_array.
 * 
 * This class is a model of the piranha::concept::DegreeKey concept.
 * 
 * \section type_requirements Type requirements
 * 
 * \p T must be suitable for use in piranha::kronecker_array. The default type for \p T is the signed counterpart of \p std::size_t.
 * 
 * \section exception_safety Exception safety guarantee
 * 
 * Unless otherwise specified, this class provides the strong exception safety guarantee for all operations.
 * 
 * \section move_semantics Move semantics
 * 
 * The move semantics of this class are equivalent to the move semantics of C++ signed integral types.
 * 
 * @author Francesco Biscani (bluescarni@gmail.com)
 * 
 * \todo think about having this (and rtkm) return degrees as integer() to solve possible problems of overflowing in conjunction with
 * use in power_series_term (where we add degrees produced by cf and key). Should we do that, see if the safe_adder thingie can go away for good.
 * Note that this will take care of uniforming the output of, e.g., degree() and partial/integrate. Review the code for degree() use after the
 * change has been made to spot possible problems.
 */
template <typename T = std::make_signed<std::size_t>::type>
class kronecker_monomial: detail::kronecker_monomial_tag
{
	public:
		/// Alias for \p T.
		typedef T value_type;
	private:
		typedef kronecker_array<value_type> ka;
	public:
		/// Size type.
		/**
		 * Used to represent the number of variables in the monomial. Equivalent to the size type of
		 * piranha::kronecker_array.
		 */
		typedef typename ka::size_type size_type;
		/// Maximum monomial size.
		static const size_type max_size = 255u;
	private:
		static_assert(max_size <= boost::integer_traits<static_vector<int,1u>::size_type>::const_max,"Invalid max size.");
		// Eval and sub typedef.
		template <typename U>
		struct eval_type
		{
			typedef decltype(math::pow(std::declval<U>(),std::declval<value_type>())) type;
		};
	public:
		/// Vector type used for temporary packing/unpacking.
		typedef static_vector<value_type,max_size> v_type;
		/// Default constructor.
		/**
		 * After construction all exponents in the monomial will be zero.
		 */
		kronecker_monomial():m_value(0) {}
		/// Defaulted copy constructor.
		kronecker_monomial(const kronecker_monomial &) = default;
		/// Defaulted move constructor.
		kronecker_monomial(kronecker_monomial &&) = default;
		/// Constructor from initalizer list.
		/**
		 * The values in the initializer list are intended to represent the exponents of the monomial:
		 * they will be converted to type \p T (if \p T and \p U are not the same type),
		 * encoded using piranha::kronecker_array::encode() and the result assigned to the internal integer instance.
		 * 
		 * @param[in] list initializer list representing the exponents.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - piranha::kronecker_array::encode(),
		 * - \p boost::numeric_cast (in case \p U is not the same as \p T),
		 * - piranha::static_vector::push_back().
		 */
		template <typename U>
		explicit kronecker_monomial(std::initializer_list<U> list):m_value(0)
		{
			v_type tmp;
			for (auto it = list.begin(); it != list.end(); ++it) {
				tmp.push_back(boost::numeric_cast<value_type>(*it));
			}
			m_value = ka::encode(tmp);
		}
		/// Constructor from range.
		/**
		 * Will build internally a vector of values from the input iterators, encode it and assign the result
		 * to the internal integer instance. The value type of the iterator is converted to \p T using
		 * \p boost::numeric_cast.
		 * 
		 * @param[in] start beginning of the range.
		 * @param[in] end end of the range.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - piranha::kronecker_array::encode(),
		 * - \p boost::numeric_cast (in case the value type of \p Iterator is not the same as \p T),
		 * - piranha::static_vector::push_back().
		 */
		template <typename Iterator>
		explicit kronecker_monomial(const Iterator &start, const Iterator &end):m_value(0)
		{
			typedef typename std::iterator_traits<Iterator>::value_type it_v_type;
			v_type tmp;
			std::transform(start,end,std::back_inserter(tmp),[](const it_v_type &v) {return boost::numeric_cast<value_type>(v);});
			m_value = ka::encode(tmp);
		}
		/// Constructor from set of symbols.
		/**
		 * After construction all exponents in the monomial will be zero.
		 * 
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - piranha::kronecker_array::encode(),
		 * - piranha::static_vector::push_back().
		 */
		explicit kronecker_monomial(const symbol_set &args)
		{
			v_type tmp;
			for (auto it = args.begin(); it != args.end(); ++it) {
				tmp.push_back(value_type(0));
			}
			m_value = ka::encode(tmp);
		}
		/// Converting constructor.
		/**
		 * This constructor is for use when converting from one term type to another in piranha::series. It will
		 * set the internal integer instance to the same value of \p other, after having checked that
		 * \p other is compatible with \p args.
		 * 
		 * @param[in] other construction argument.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @throws std::invalid_argument if \p other is not compatible with \p args.
		 */
		explicit kronecker_monomial(const kronecker_monomial &other, const symbol_set &args):m_value(other.m_value)
		{
			if (unlikely(!other.is_compatible(args))) {
				piranha_throw(std::invalid_argument,"incompatible arguments");
			}
		}
		/// Constructor from \p value_type.
		/**
		 * This constructor will initialise the internal integer instance
		 * to \p n.
		 * 
		 * @param[in] n initializer for the internal integer instance.
		 */
		explicit kronecker_monomial(const value_type &n):m_value(n) {}
		/// Trivial destructor.
		~kronecker_monomial() piranha_noexcept_spec(true)
		{
			BOOST_CONCEPT_ASSERT((concept::DegreeKey<kronecker_monomial>));
		}
		/// Defaulted copy assignment operator.
		kronecker_monomial &operator=(const kronecker_monomial &) = default;
		/// Trivial move assignment operator.
		/**
		 * @param[in] other monomial to be assigned to this.
		 * 
		 * @return reference to \p this.
		 */
		kronecker_monomial &operator=(kronecker_monomial &&other) piranha_noexcept_spec(true)
		{
			m_value = std::move(other.m_value);
			return *this;
		}
		/// Set the internal integer instance.
		/**
		 * @param[in] n value to which the internal integer instance will be set.
		 */
		void set_int(const value_type &n)
		{
			m_value = n;
		}
		/// Get internal instance.
		/**
		 * @return value of the internal integer instance.
		 */
		value_type get_int() const
		{
			return m_value;
		}
		/// Compatibility check.
		/**
		 * Monomial is considered incompatible if any of these conditions holds:
		 * 
		 * - the size of \p args is zero and the internal integer is not zero,
		 * - the size of \p args is equal to or larger than the size of the output of piranha::kronecker_array::get_limits(),
		 * - the internal integer is not within the limits reported by piranha::kronecker_array::get_limits().
		 * 
		 * Otherwise, the monomial is considered to be compatible for insertion.
		 * 
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return compatibility flag for the monomial.
		 */
		bool is_compatible(const symbol_set &args) const piranha_noexcept_spec(true)
		{
			// NOTE: the idea here is to avoid unpack()ing for performance reasons: these checks
			// are already part of unpack(), and that's why unpack() is used instead of is_compatible()
			// in other methods.
			const auto s = args.size();
			// No args means the value must also be zero.
			if (s == 0u) {
				return !m_value;
			}
			const auto &limits = ka::get_limits();
			// If we overflow the maximum size available, we cannot use this object as key in series.
			if (s >= limits.size()) {
				return false;
			}
			const auto &l = limits[s];
			// Value is compatible if it is within the bounds for the given size.
			return (m_value >= std::get<1u>(l) && m_value <= std::get<2u>(l));
		}
		/// Ignorability check.
		/**
		 * A monomial is never considered ignorable.
		 * 
		 * @return \p false.
		 */
		bool is_ignorable(const symbol_set &) const piranha_noexcept_spec(true)
		{
			return false;
		}
		/// Merge arguments.
		/**
		 * Merge the new arguments set \p new_args into \p this, given the current reference arguments set
		 * \p orig_args.
		 * 
		 * @param[in] orig_args original arguments set.
		 * @param[in] new_args new arguments set.
		 * 
		 * @return monomial with merged arguments.
		 * 
		 * @throws std::invalid_argument if at least one of these conditions is true:
		 * - the size of \p new_args is not greater than the size of \p orig_args,
		 * - not all elements of \p orig_args are included in \p new_args.
		 * @throws unspecified any exception thrown by:
		 * - piranha::kronecker_array::encode(),
		 * - piranha::static_vector::push_back(),
		 * - unpack().
		 */
		kronecker_monomial merge_args(const symbol_set &orig_args, const symbol_set &new_args) const
		{
			return kronecker_monomial(detail::km_merge_args<v_type,ka>(orig_args,new_args,m_value));
		}
		/// Check if monomial is unitary.
		/**
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return \p true if all the exponents are zero, \p false otherwise.
		 * 
		 * @throws std::invalid_argument if \p this is not compatible with \p args.
		 */
		bool is_unitary(const symbol_set &args) const
		{
			if (unlikely(!is_compatible(args))) {
				piranha_throw(std::invalid_argument,"invalid symbol set");
			}
			// A kronecker code will be zero if all components are zero.
			return !m_value;
		}
		/// Degree.
		/**
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return degree of the monomial.
		 * 
		 * @throws std::overflow_error if the computation of the degree overflows type \p value_type.
		 * @throws unspecified any exception thrown by unpack().
		 */
		value_type degree(const symbol_set &args) const
		{
			const auto tmp = unpack(args);
			return std::accumulate(tmp.begin(),tmp.end(),value_type(0),detail::km_safe_adder<value_type>);
		}
		/// Low degree.
		/**
		 * Equivalent to the degree.
		 * 
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return low degree of the monomial.
		 * 
		 * @throws unspecified any exception thrown by degree().
		 */
		value_type ldegree(const symbol_set &args) const
		{
			return degree(args);
		}
		/// Partial degree.
		/**
		 * Partial degree of the monomial: only the symbols with names in \p active_args are considered during the computation
		 * of the degree. Symbols in \p active_args not appearing in \p args are not considered.
		 * 
		 * @param[in] active_args names of the symbols that will be considered in the computation of the partial degree of the monomial.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return the summation of all the exponents of the monomial corresponding to the symbols in
		 * \p active_args, or <tt>value_type(0)</tt> if no symbols in \p active_args appear in \p args.
		 * 
		 * @throws std::overflow_error if the computation of the degree overflows type \p value_type.
		 * @throws unspecified any exception thrown by unpack().
		 */
		value_type degree(const std::set<std::string> &active_args, const symbol_set &args) const
		{
			return detail::km_partial_degree<v_type,ka>(active_args,args,m_value);
		}
		/// Partial low degree.
		/**
		 * Equivalent to the partial degree.
		 * 
		 * @param[in] active_args names of the symbols that will be considered in the computation of the partial low degree of the monomial.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return the partial low degree.
		 * 
		 * @throws unspecified any exception thrown by degree().
		 */
		value_type ldegree(const std::set<std::string> &active_args, const symbol_set &args) const
		{
			return degree(active_args,args);
		}
		/// Multiply monomial.
		/**
		 * The resulting monomial is computed by adding the exponents of \p this to the exponents of \p other.
		 * 
		 * @param[out] retval result of multiplying \p this by \p other.
		 * @param[in] other multiplicand.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @throws std::overflow_error if the computation of the result overflows type \p value_type.
		 * @throws unspecified any exception thrown by:
		 * - piranha::kronecker_array::encode(),
		 * - unpack(),
		 * - piranha::static_vector::push_back().
		 */
		void multiply(kronecker_monomial &retval, const kronecker_monomial &other, const symbol_set &args) const
		{
			const auto size = args.size();
			const auto tmp1 = unpack(args), tmp2 = other.unpack(args);
			v_type result;
			for (decltype(args.size()) i = 0u; i < size; ++i) {
				result.push_back(detail::km_safe_adder(tmp1[i],tmp2[i]));
			}
			retval.m_value = ka::encode(result);
		}
		/// Hash value.
		/**
		 * @return the internal integer instance, cast to \p std::size_t.
		 */
		std::size_t hash() const
		{
			return static_cast<std::size_t>(m_value);
		}
		/// Equality operator.
		/**
		 * @param[in] other comparison argument.
		 * 
		 * @return \p true if the internal integral instance of \p this is equal to the integral instance of \p other,
		 * \p false otherwise.
		 */
		bool operator==(const kronecker_monomial &other) const
		{
			return m_value == other.m_value;
		}
		/// Inequality operator.
		/**
		 * @param[in] other comparison argument.
		 * 
		 * @return the opposite of operator==().
		 */
		bool operator!=(const kronecker_monomial &other) const
		{
			return m_value != other.m_value;
		}
		/// Name of the linear argument.
		/**
		 * If the monomial is linear in a variable (i.e., all exponents are zero apart from a single unitary
		 * exponent), the name of the variable will be returned. Otherwise, an error will be raised.
		 * 
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return name of the linear variable.
		 * 
		 * @throws std::invalid_argument if the monomial is not linear.
		 * @throws unspecified any exception thrown by unpack().
		 */
		std::string linear_argument(const symbol_set &args) const
		{
			const auto v = unpack(args);
			const auto size = args.size();
			decltype(args.size()) n_linear = 0u, candidate = 0u;
			for (decltype(args.size()) i = 0u; i < size; ++i) {
				integer tmp;
				try {
					tmp = math::integral_cast(v[i]);
				} catch (const std::invalid_argument &) {
					piranha_throw(std::invalid_argument,"exponent is not an integer");
				}
				if (tmp == 0) {
					continue;
				}
				if (tmp != 1) {
					piranha_throw(std::invalid_argument,"exponent is not unitary");
				}
				candidate = i;
				++n_linear;
			}
			if (n_linear != 1u) {
				piranha_throw(std::invalid_argument,"monomial is not linear");
			}
			return args[candidate].get_name();
		}
		/// Exponentiation.
		/**
		 * Will return a monomial corresponding to \p this raised to the <tt>x</tt>-th power. The exponentiation
		 * is computed via multiplication of the exponents by the output of piranha::math::integral_cast()
		 * on \p x.
		 * 
		 * @param[in] x exponent.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return \p this to the power of \p x.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - unpack(),
		 * - piranha::math::integral_cast(),
		 * - the cast and binary multiplication operators of piranha::integer,
		 * - piranha::kronecker_array::encode().
		 */
		template <typename U>
		kronecker_monomial pow(const U &x, const symbol_set &args) const
		{
			auto v = unpack(args);
			const auto size = args.size();
			const integer n = math::integral_cast(x);
			for (decltype(args.size()) i = 0u; i < size; ++i) {
				// NOTE: here operator* produces an integer, which is safely cast back
				// to the signed int type.
				v[i] = static_cast<value_type>(n * v[i]);
			}
			kronecker_monomial retval;
			retval.m_value = ka::encode(v);
			return retval;
		}
		/// Unpack internal integer instance.
		/**
		 * Will decode the internal integral instance into a piranha::static_vector of size equal to the size of \p args.
		 * 
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return piranha::static_vector containing the result of decoding the internal integral instance via
		 * piranha::kronecker_array.
		 * 
		 * @throws std::invalid_argument if the size of \p args is larger than the maximum size of piranha::static_vector.
		 * @throws unspecified any exception thrown by piranha::kronecker_array::decode().
		 */
		v_type unpack(const symbol_set &args) const
		{
			return detail::km_unpack<v_type,ka>(args,m_value);
		}
		/// Print.
		/**
		 * Will print to stream a human-readable representation of the monomial.
		 * 
		 * @param[in] os target stream.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @throws unspecified any exception thrown by unpack() or by streaming instances of \p value_type.
		 */
		void print(std::ostream &os, const symbol_set &args) const
		{
			const auto tmp = unpack(args);
			piranha_assert(tmp.size() == args.size());
			const value_type zero(0), one(1);
			for (decltype(tmp.size()) i = 0u; i < tmp.size(); ++i) {
				if (tmp[i] != zero) {
					os << args[i].get_name();
					if (tmp[i] != one) {
						// NOTE: cast to long long is always safe as it is the
						// widest signed int type.
						os << "**" << static_cast<long long>(tmp[i]);
					}
				}
			}
		}
		/// Print in TeX mode.
		/**
		 * Will print to stream a TeX representation of the monomial.
		 * 
		 * @param[in] os target stream.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @throws unspecified any exception thrown by unpack() or by streaming instances of \p value_type.
		 */
		void print_tex(std::ostream &os, const symbol_set &args) const
		{
			const auto tmp = unpack(args);
			std::ostringstream oss_num, oss_den, *cur_oss;
			const value_type zero(0), one(1);
			value_type cur_value;
			for (decltype(tmp.size()) i = 0u; i < tmp.size(); ++i) {
				cur_value = tmp[i];
				if (cur_value != zero) {
					// NOTE: here negate() is safe because of the symmetry in kronecker_array.
					cur_oss = (cur_value > zero) ? boost::addressof(oss_num) : (math::negate(cur_value),boost::addressof(oss_den));
					(*cur_oss) << "{" << args[i].get_name() << "}";
					if (cur_value != one) {
						(*cur_oss) << "^{" << static_cast<long long>(cur_value) << "}";
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
		/// Partial derivative.
		/**
		 * Will return the partial derivative of \p this with respect to symbol \p s. The result is a pair
		 * consisting of the exponent associated to \p s before differentiation and the monomial itself
		 * after differentiation. If \p s is not in \p args or if the exponent associated to it is zero,
		 * the returned pair will be <tt>(0,monomial{})</tt>.
		 * 
		 * @param[in] s symbol with respect to which the differentiation will be calculated.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return result of the differentiation.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - unpack(),
		 * - piranha::math::is_zero(),
		 * - the cast operator of piranha::integer,
		 * - piranha::kronecker_array::encode().
		 */
		std::pair<integer,kronecker_monomial> partial(const symbol &s, const symbol_set &args) const
		{
			auto v = unpack(args);
			for (decltype(args.size()) i = 0u; i < args.size(); ++i) {
				if (args[i] == s && !math::is_zero(v[i])) {
					integer tmp_n(v[i]);
					v[i] = static_cast<value_type>(tmp_n - 1);
					kronecker_monomial tmp_km;
					tmp_km.m_value = ka::encode(v);
					return std::make_pair(std::move(tmp_n),std::move(tmp_km));
				}
			}
			return std::make_pair(integer{0},kronecker_monomial{});
		}
		/// Integration.
		/**
		 * Will return the antiderivative of \p this with respect to symbol \p s. The result is a pair
		 * consisting of the exponent associated to \p s and the monomial itself
		 * after integration. If \p s is not in \p args, the returned monomial will have an extra exponent
		 * set to 1 in the same position \p s would have if it were added to \p args.
		 * 
		 * If the exponent corresponding to \p s is -1, an error will be produced.
		 * 
		 * @param[in] s symbol with respect to which the integration will be calculated.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return result of the integration.
		 * 
		 * @throws std::invalid_argument if the exponent associated to \p s is -1.
		 * @throws unspecified any exception thrown by:
		 * - unpack(),
		 * - piranha::math::is_zero(),
		 * - piranha::static_vector::push_back(),
		 * - the cast operator of piranha::integer,
		 * - piranha::kronecker_array::encode().
		 */
		std::pair<integer,kronecker_monomial> integrate(const symbol &s, const symbol_set &args) const
		{
			v_type v = unpack(args), retval;
			value_type expo(0), one(1);
			for (decltype(args.size()) i = 0u; i < args.size(); ++i) {
				if (math::is_zero(expo) && s < args[i]) {
					// If we went past the position of s in args and still we
					// have not performed the integration, it means that we need to add
					// a new exponent.
					retval.push_back(one);
					expo = one;
				}
				retval.push_back(v[i]);
				if (args[i] == s) {
					// NOTE: here using i is safe: if retval gained an extra exponent in the condition above,
					// we are never going to land here as args[i] is at this point never going to be s.
					retval[i] = static_cast<value_type>(integer(retval[i]) + 1);
					if (math::is_zero(retval[i])) {
						piranha_throw(std::invalid_argument,"unable to perform monomial integration: negative unitary exponent");
					}
					expo = retval[i];
				}
			}
			// If expo is still zero, it means we need to add a new exponent at the end.
			if (math::is_zero(expo)) {
				retval.push_back(one);
				expo = one;
			}
			return std::make_pair(integer(expo),kronecker_monomial(ka::encode(retval)));
		}
		/// Evaluation.
		/**
		 * The return value will be built by iteratively applying piranha::math::pow() using the values provided
		 * by \p dict as bases and the values in the monomial as exponents. If a symbol in \p args is not found
		 * in \p dict, an error will be raised. If the size of the monomial is zero, 1 will be returned.
		 * 
		 * @param[in] dict dictionary that will be used for substitution.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return the result of evaluating \p this with the values provided in \p dict.
		 * 
		 * @throws std::invalid_argument if a symbol in \p args is not found in \p dict.
		 * @throws unspecified any exception thrown by:
		 * - unpack(),
		 * - construction of the return type,
		 * - lookup operations in \p std::unordered_map,
		 * - piranha::math::pow() or the in-place multiplication operator of the return type.
		 * 
		 * \todo request constructability from 1, multipliability and exponentiability.
		 */
		template <typename U>
		typename eval_type<U>::type evaluate(const std::unordered_map<symbol,U> &dict, const symbol_set &args) const
		{
			typedef typename eval_type<U>::type return_type;
			auto v = unpack(args);
			return_type retval(1);
			const auto it_f = dict.end();
			for (decltype(args.size()) i = 0u; i < args.size(); ++i) {
				const auto it = dict.find(args[i]);
				if (it == it_f) {
					piranha_throw(std::invalid_argument,
						std::string("cannot evaluate monomial: symbol \'") + args[i].get_name() +
						"\' does not appear in dictionary");
				}
				retval *= math::pow(it->second,v[i]);
			}
			return retval;
		}
		/// Substitution.
		/**
		 * The algorithm is equivalent to the one implemented in piranha::monomial::subs().
		 * 
		 * @param[in] s symbol that will be substituted.
		 * @param[in] x quantity that will be substituted in place of \p s.
		 * @param[in] args reference set of piranha::symbol.
		 * 
		 * @return the result of substituting \p x for \p s.
		 * 
		 * @throws unspecified any exception thrown by:
		 * - unpack(),
		 * - construction and assignment of the return value,
		 * - piranha::math::pow(),
		 * - piranha::static_vector::push_back(),
		 * - piranha::kronecker_array::encode().
		 * 
		 * \todo require constructability from int and exponentiability.
		 */
		template <typename U>
		std::pair<typename eval_type<U>::type,kronecker_monomial> subs(const symbol &s, const U &x, const symbol_set &args) const
		{
			typedef typename eval_type<U>::type s_type;
			const auto v = unpack(args);
			v_type new_v;
			s_type retval_s(1);
			for (decltype(args.size()) i = 0u; i < args.size(); ++i) {
				if (args[i] == s) {
					retval_s = math::pow(x,v[i]);
				} else {
					new_v.push_back(v[i]);
				}
			}
			piranha_assert(new_v.size() == v.size() || new_v.size() == v.size() - 1u);
			return std::make_pair(std::move(retval_s),kronecker_monomial(ka::encode(new_v)));
		}
	private:
		value_type m_value;
};

}

namespace std
{

/// Specialisation of \p std::hash for piranha::kronecker_monomial.
template <typename T>
struct hash<piranha::kronecker_monomial<T>>
{
	/// Result type.
	typedef size_t result_type;
	/// Argument type.
	typedef piranha::kronecker_monomial<T> argument_type;
	/// Hash operator.
	/**
	 * @param[in] a argument whose hash value will be computed.
	 * 
	 * @return hash value of \p a computed via piranha::kronecker_monomial::hash().
	 */
	result_type operator()(const argument_type &a) const
	{
		return a.hash();
	}
};

}

#endif
