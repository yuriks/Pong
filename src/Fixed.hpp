#pragma once

#include <limits>
#include <type_traits>
#include <cstdint>

/** Fixed point arithmetic number type. */
template <typename T, unsigned int FracBits>
class Fixed {
public:
	static const unsigned int INTEGRAL_BITS = std::numeric_limits<T>::digits - FracBits;
	static const unsigned int FRACTIONAL_BITS = FracBits;
	static const T FRACTIONAL_MASK = (1 << FRACTIONAL_BITS) - 1;
	static const T INTEGER_MASK = ~FRACTIONAL_MASK;

	static Fixed raw(T val) {
		Fixed f; f.value = val;
		return f;
	}

	Fixed() { }
	Fixed(T int_part) : value(int_part << FRACTIONAL_BITS) { }
	Fixed(T int_part, T frac_part) : value((int_part << FRACTIONAL_BITS) + frac_part) { }
	Fixed(T int_part, T frac_numer, T frac_denom) : value((int_part << FRACTIONAL_BITS) + (frac_numer << FRACTIONAL_BITS) / frac_denom) { }

	template <typename OtherT, unsigned int OtherFracBits>
	explicit Fixed(Fixed<OtherT, OtherFracBits> o) : value(FracBits < OtherFracBits ?
		o.value >> (OtherFracBits - FracBits) :
		o.value << (FracBits - OtherFracBits))
	{ }

	T integer() const { return value >> FracBits; }

	///////////////////////////////////

	Fixed operator +(Fixed o) const { return Fixed::raw(value + o.value); }
	Fixed operator -(Fixed o) const { return Fixed::raw(value - o.value); }
	Fixed operator -()        const { return Fixed::raw(-value); }

	template <typename OtherT, unsigned int OtherFracBits>
	Fixed<typename std::common_type<T, OtherT>::type, FracBits + OtherFracBits>
		operator *(Fixed<OtherT, OtherFracBits> o) const
	{
		return Fixed<typename std::common_type<T, OtherT>::type, FracBits + OtherFracBits>::raw(value * o.value);
	}
	
	template <typename OtherT, unsigned int OtherFracBits>
	Fixed<typename std::common_type<T, OtherT>::type, FracBits - OtherFracBits>
		operator /(Fixed<OtherT, OtherFracBits> o) const
	{
		return Fixed<typename std::common_type<T, OtherT>::type, FracBits - OtherFracBits>::raw(value / o.value);
	}

	Fixed& operator +=(Fixed o) { value += o.value; return *this; }
	Fixed& operator -=(Fixed o) { value -= o.value; return *this; }

	///////////////////////////////////

	bool operator ==(Fixed o) const { return value == o.value; }
	bool operator !=(Fixed o) const { return value != o.value; }
	bool operator  <(Fixed o) const { return value <  o.value; }
	bool operator  >(Fixed o) const { return value >  o.value; }
	bool operator <=(Fixed o) const { return value <= o.value; }
	bool operator >=(Fixed o) const { return value >= o.value; }

	T value;
};

typedef Fixed<int32_t,  0> fixed32_0;
typedef Fixed<int32_t,  8> fixed24_8;
typedef Fixed<int32_t, 16> fixed16_16;
typedef Fixed<int32_t, 24> fixed8_24;
typedef Fixed<int32_t, 32> fixed0_32;
