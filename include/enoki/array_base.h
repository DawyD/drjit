/*
    enoki/array_base.h -- Base classes of all Enoki array data structures

    Enoki is a C++ template library that enables transparent vectorization
    of numerical kernels using SIMD instruction sets available on current
    processor architectures.

    Copyright (c) 2017 Wenzel Jakob <wenzel.jakob@epfl.ch>

    All rights reserved. Use of this source code is governed by a BSD-style
    license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "array_router.h"

NAMESPACE_BEGIN(enoki)

NAMESPACE_BEGIN(detail)

template <typename Derived> struct MaskWrapper {
    Derived &d;
    typename Derived::Mask m;
    template <typename T> ENOKI_INLINE void operator=(T value) { d.massign_(value, m); }
    template <typename T> ENOKI_INLINE void operator+=(T value) { d.madd_(value, m); }
    template <typename T> ENOKI_INLINE void operator-=(T value) { d.msub_(value, m); }
    template <typename T> ENOKI_INLINE void operator*=(T value) { d.mmul_(value, m); }
    template <typename T> ENOKI_INLINE void operator/=(T value) { d.mdiv_(value, m); }
    template <typename T> ENOKI_INLINE void operator|=(T value) { d.mor_(value, m); }
    template <typename T> ENOKI_INLINE void operator&=(T value) { d.mand_(value, m); }
    template <typename T> ENOKI_INLINE void operator^=(T value) { d.mxor_(value, m); }
};

NAMESPACE_END(detail)

template <typename Type_, typename Derived_> struct ArrayBase {
    // -----------------------------------------------------------------------
    //! @{ \name Curiously Recurring Template design pattern
    // -----------------------------------------------------------------------

    /// Alias to the derived type
    using Derived = Derived_;

    /// Cast to derived type
    ENOKI_INLINE Derived &derived()             { return (Derived &) *this; }

    /// Cast to derived type (const version)
    ENOKI_INLINE const Derived &derived() const { return (Derived &) *this; }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Basic declarations
    // -----------------------------------------------------------------------

    /// Actual type underlying the derived array
    using Type = Type_;

    /// Base type underlying the derived array (i.e. without references etc.)
    using Value = std::remove_reference_t<Type>;

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Iterators
    // -----------------------------------------------------------------------

    ENOKI_INLINE const Value *begin() const {
        ENOKI_CHKSCALAR return derived().data();
    }

    ENOKI_INLINE Value *begin() {
        ENOKI_CHKSCALAR return derived().data();
    }

    ENOKI_INLINE const Value *end() const {
        ENOKI_CHKSCALAR return derived().data() + derived().size();
    }

    ENOKI_INLINE Value *end() {
        ENOKI_CHKSCALAR return derived().data() + derived().size();
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Element access
    // -----------------------------------------------------------------------

    /// Array indexing operator with bounds checks in debug mode
    ENOKI_INLINE Value &operator[](size_t i) {
        #if !defined(NDEBUG) && !defined(ENOKI_DISABLE_RANGE_CHECK)
            if (i >= derived().size())
                throw std::out_of_range(
                    "ArrayBase: out of range access (tried to access index " +
                    std::to_string(i) + " in an array of size " +
                    std::to_string(derived().size()) + ")");
        #endif
        return derived().coeff(i);
    }

    /// Array indexing operator with bounds checks in debug mode, const version
    ENOKI_INLINE const Value &operator[](size_t i) const {
        #if !defined(NDEBUG) && !defined(ENOKI_DISABLE_RANGE_CHECK)
            if (i >= derived().size())
                throw std::out_of_range(
                    "ArrayBase: out of range access (tried to access index " +
                    std::to_string(i) + " in an array of size " +
                    std::to_string(derived().size()) + ")");
        #endif
        return derived().coeff(i);
    }

    //! @}
    // -----------------------------------------------------------------------
};

/**
 * \brief Base class containing rudimentary operations and type aliases used by
 * all static and dynamic array implementations
 *
 * This data structure provides various rudimentary operations that are implemented
 * using functionality provided by the target-specific 'Derived' subclass (e.g.
 * ``operator+=`` using ``operator+`` and ``operator=``). This avoids a
 * considerable amount of repetition in target-specific specializations. The
 * implementation makes use of the Curiously Recurring Template design pattern,
 * which enables inlining and other compiler optimizations.
 */
template <typename Type_, size_t Size_, bool Approx_, RoundingMode Mode_,
          typename Derived_>
struct StaticArrayBase : ArrayBase<Type_, Derived_> {
    using Base = ArrayBase<Type_, Derived_>;
    using typename Base::Derived;
    using typename Base::Type;
    using typename Base::Value;
    using Base::derived;

    /// Size of the first sub-array (used to split this array into two parts)
    static constexpr size_t Size1 = detail::lpow2(Size_);

    /// Size of the second sub-array (used to split this array into two parts)
    static constexpr size_t Size2 = Size_ - Size1;

    /// First sub-array type (used to split this array into two parts)
    using Array1 = Array<Type_, Size1, Approx_, Mode_>;

    /// Second sub-array type (used to split this array into two parts)
    using Array2 = Array<Type_, Size2, Approx_, Mode_>;

    /// Value data type all the way at the lowest level
    using Scalar = scalar_t<Value>;

    /// Is this array exclusively for mask usage? (overridden in some subclasses)
    static constexpr bool IsMask = std::is_same<Value, bool>::value;

    /// Number of array entries
    static constexpr size_t Size = Size_;
    static constexpr size_t ActualSize = Size;

    /// Are arithmetic operations approximate?
    static constexpr bool Approx = Approx_;

    /// Rounding mode of arithmetic operations
    static constexpr RoundingMode Mode = Mode_;

    /// Type alias for a similar-shaped array over a different type
    template <typename T, typename T2 = Derived>
    using ReplaceType = Array<T, T2::Size>;

    static_assert(std::is_same<Scalar, float>::value || !Approx,
                  "Approximate math library functions are only supported in "
                  "single precision mode!");

    static_assert(!std::is_integral<Value>::value || Mode == RoundingMode::Default,
                  "Integer arrays require Mode == RoundingMode::Default");

    StaticArrayBase() = default;

    /// Assign from a compatible array type
    template <
        typename Type2, size_t Size2, bool Approx2, RoundingMode Mode2,
        typename Derived2,
        std::enable_if_t<std::is_assignable<Value &, Type2>::value, int> = 0>
    ENOKI_INLINE Derived &operator=(
        const StaticArrayBase<Type2, Size2, Approx2, Mode2, Derived2> &a) {
        static_assert(Derived2::Size == Derived::Size, "Size mismatch!");
        if (std::is_arithmetic<Value>::value) { ENOKI_TRACK_SCALAR }
        for (size_t i = 0; i < Size; ++i)
            derived().coeff(i) = a.derived().coeff(i);
        return derived();
    }

    ENOKI_INLINE Derived &operator=(Scalar value) {
        derived() = Derived(value);
        return derived();
    }

    using Base::operator[];
    template <typename T = Derived>
    ENOKI_INLINE detail::MaskWrapper<T> operator[](typename T::Mask m) {
        return detail::MaskWrapper<T>{ derived(), m };
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Mathematical support library
    // -----------------------------------------------------------------------

    /// Element-wise test for NaN values
    ENOKI_INLINE auto isnan_() const {
        return !eq(derived(), derived());
    }

    /// Element-wise test for +/- infinity
    ENOKI_INLINE auto isinf_() const {
        return eq(abs(derived()), std::numeric_limits<Scalar>::infinity());
    }

    /// Element-wise test for finiteness
    ENOKI_INLINE auto isfinite_() const {
        return abs(derived()) < std::numeric_limits<Scalar>::max();
    }

    /// Left rotation operation fallback implementation
    template <typename T = Scalar, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    ENOKI_INLINE auto rol_(size_t k) const {
        using Expr = expr_t<Derived>;
        if (!std::is_signed<Scalar>::value) {
            constexpr size_t mask = 8 * sizeof(Scalar) - 1u;
            return Expr((derived() << (k & mask)) | (derived() >> ((~k + 1u) & mask)));
        } else {
            return Expr(uint_array_t<Expr>(derived()).rol_(k));
        }
    }

    /// Right rotation operation fallback implementation
    template <typename T = Scalar, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    ENOKI_INLINE auto ror_(size_t k) const {
        using Expr = expr_t<Derived>;
        if (!std::is_signed<Scalar>::value) {
            constexpr size_t mask = 8 * sizeof(Scalar) - 1u;
            return Expr((derived() >> (k & mask)) | (derived() << ((~k + 1u) & mask)));
        } else {
            return Expr(uint_array_t<Expr>(derived()).ror_(k));
        }
    }

    /// Left rotation operation fallback implementation
    template <typename T = Scalar, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    ENOKI_INLINE auto rolv_(const Derived &d) const {
        using Expr = expr_t<Derived>;
        if (!std::is_signed<Scalar>::value) {
            Expr mask(Scalar(8 * sizeof(Scalar) - 1u));
            return Expr((derived() << (d & mask)) | (derived() >> ((~d + Scalar(1)) & mask)));
        } else {
            return Expr(uint_array_t<Expr>(derived()).rolv_(d));
        }
    }

    /// Right rotation operation fallback implementation
    template <typename T = Scalar, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    ENOKI_INLINE auto rorv_(const Derived &d) const {
        using Expr = expr_t<Derived>;
        if (!std::is_signed<Scalar>::value) {
            Expr mask(Scalar(8 * sizeof(Scalar) - 1u));
            return Expr((derived() >> (d & mask)) | (derived() << ((~d + Scalar(1)) & mask)));
        } else {
            return Expr(uint_array_t<Expr>(derived()).rorv_(d));
        }
    }

    /// Left rotation operation fallback implementation (immediate)
    template <size_t Imm, typename T = Scalar, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    ENOKI_INLINE auto roli_() const {
        using Expr = expr_t<Derived>;
        if (!std::is_signed<Scalar>::value) {
            constexpr size_t mask = 8 * sizeof(Scalar) - 1u;
            return Expr(sli<Imm & mask>(derived()) | sri<((~Imm + 1u) & mask)>(derived()));
        } else {
            return Expr(uint_array_t<Expr>(derived()).template roli_<Imm>());
        }
    }

    /// Right rotation operation fallback implementation (immediate)
    template <size_t Imm, typename T = Scalar, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    ENOKI_INLINE auto rori_() const {
        using Expr = expr_t<Derived>;
        if (!std::is_signed<Scalar>::value) {
            constexpr size_t mask = 8 * sizeof(Scalar) - 1u;
            return Expr(sri<Imm & mask>(derived()) | sli<((~Imm + 1u) & mask)>(derived()));
        } else {
            return Expr(uint_array_t<Expr>(derived()).template rori_<Imm>());
        }
    }

    /// Arithmetic NOT operation fallback
    ENOKI_INLINE auto not_() const {
        using Expr = expr_t<Derived>;
        const Expr mask(memcpy_cast<Scalar>(typename int_array_t<Expr>::Scalar(-1)));
        return Expr(derived() ^ mask);
    }

    /// Arithmetic unary negation operation fallback
    ENOKI_INLINE auto neg_() const {
        using Expr = expr_t<Derived>;
        if (std::is_floating_point<Value>::value)
            return derived() ^ Expr(Scalar(-0.f));
        else
            return ~derived() + Expr(Scalar(1));
    }

    /// Reciprocal fallback implementation
    ENOKI_INLINE auto rcp_() const {
        return expr_t<Derived>(Scalar(1)) / derived();
    }

    /// Reciprocal square root fallback implementation
    ENOKI_INLINE auto rsqrt_() const {
        return expr_t<Derived>(Scalar(1)) / sqrt(derived());
    }

    /// Fused multiply-add fallback implementation
    ENOKI_INLINE auto fmadd_(const Derived &b, const Derived &c) const {
        return derived() * b + c;
    }

    /// Fused multiply-subtract fallback implementation
    ENOKI_INLINE auto fmsub_(const Derived &b, const Derived &c) const {
        return derived() * b - c;
    }

    /// Fused multiply-add/subtract fallback implementation
    ENOKI_INLINE auto fmaddsub_(const Derived &b, const Derived &c) const {
        expr_t<Derived> result;
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i) {
            if (i % 2 == 0)
                result.coeff(i) = fmsub(derived().coeff(i), b.coeff(i), c.coeff(i));
            else
                result.coeff(i) = fmadd(derived().coeff(i), b.coeff(i), c.coeff(i));
        }
        return result;
    }

    /// Fused multiply-subtract/add fallback implementation
    ENOKI_INLINE auto fmsubadd_(const Derived &b, const Derived &c) const {
        expr_t<Derived> result;
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i) {
            if (i % 2 == 0)
                result.coeff(i) = fmadd(derived().coeff(i), b.coeff(i), c.coeff(i));
            else
                result.coeff(i) = fmsub(derived().coeff(i), b.coeff(i), c.coeff(i));
        }
        return result;
    }

    /// Dot product fallback implementation
    ENOKI_INLINE Value dot_(const Derived &a) const { return hsum(derived() * a); }

    /// Nested horizontal sum
    ENOKI_INLINE auto hsum_nested_() const { return hsum_nested(hsum(derived())); }

    /// Nested horizontal product
    ENOKI_INLINE auto hprod_nested_() const { return hprod_nested(hprod(derived())); }

    /// Nested horizontal minimum
    ENOKI_INLINE auto hmin_nested_() const { return hmin_nested(hmin(derived())); }

    /// Nested horizontal maximum
    ENOKI_INLINE auto hmax_nested_() const { return hmax_nested(hmax(derived())); }

    /// Nested all() mask operation
    ENOKI_INLINE bool all_nested_() const { return all_nested(all(derived())); }

    /// Nested any() mask operation
    ENOKI_INLINE bool any_nested_() const { return any_nested(any(derived())); }

    /// Nested none() mask operation
    ENOKI_INLINE bool none_nested_() const { return !any_nested(any(derived())); }

    /// Nested count() mask operation
    ENOKI_INLINE auto count_nested_() const { return hsum_nested(count(derived())); }

    /// Shuffle operation fallback implementation
    template <size_t ... Indices> ENOKI_INLINE auto shuffle_() const {
        static_assert(sizeof...(Indices) == Size ||
                      sizeof...(Indices) == Derived::Size, "shuffle(): Invalid size!");
        expr_t<Derived> out;
        size_t idx = 0;
        ENOKI_CHKSCALAR bool result[] = { (out.coeff(idx++) = derived().coeff(Indices), false)... };
        (void) idx; (void) result;
        return out;
    }

    /// Prefetch operation fallback implementation
    template <size_t Stride, bool Write, size_t Level, typename Index>
    static ENOKI_INLINE void prefetch_(const void *mem, const Index &index) {
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            prefetch<Value, Stride, Write, Level>(mem, index.coeff(i));
    }

    /// Masked prefetch operation fallback implementation
    template <size_t Stride, bool Write, size_t Level, typename Index, typename Mask>
    static ENOKI_INLINE void prefetch_(const void *mem, const Index &index,
                                       const Mask &mask) {
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            prefetch<Value, Stride, Write, Level>(mem, index.coeff(i), mask.coeff(i));
    }

    /// Gather operation fallback implementation
    template <size_t Stride, typename Index>
    static ENOKI_INLINE auto gather_(const void *mem, const Index &index) {
        expr_t<Derived> result;
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            result.coeff(i) = gather<Value, Stride>(mem, index.coeff(i));
        return result;
    }

    /// Masked gather operation fallback implementation
    template <size_t Stride, typename Index, typename Mask>
    static ENOKI_INLINE auto gather_(const void *mem, const Index &index,
                                     const Mask &mask) {
        expr_t<Derived> result;
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            result.coeff(i) = gather<Value, Stride>(mem, index.coeff(i), mask.coeff(i));
        return result;
    }

    /// Scatter operation fallback implementation
    template <size_t Stride, typename Index>
    ENOKI_INLINE void scatter_(void *mem, const Index &index) const {
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            scatter<Stride>(mem, derived().coeff(i), index.coeff(i));
    }

    /// Masked scatter operation fallback implementation
    template <size_t Stride, typename Index, typename Mask>
    ENOKI_INLINE void scatter_(void *mem, const Index &index,
                               const Mask &mask) const {
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            scatter<Stride>(mem, derived().coeff(i), index.coeff(i), mask.coeff(i));
    }

    /// Compressing store fallback implementation
    template <typename Mask>
    ENOKI_INLINE void store_compress_(void *&mem, const Mask &mask) const {
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            store_compress(mem, derived().coeff(i), mask.coeff(i));
    }

    void resize_(size_t size) {
        if (size != Derived::Size)
            throw std::length_error("Incompatible size for static array");
    }

    /// Combined gather-modify-scatter operation without conflicts (fallback implementation)
    template <size_t Stride, typename Index, typename Func>
    static ENOKI_INLINE void transform_(void *mem, const Index &index, const Func &func) {
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            transform<Value, Stride>(mem, index.coeff(i), func);
    }

    /// Combined gather-modify-scatter operation without conflicts (fallback implementation)
    template <size_t Stride, typename Index, typename Func, typename Mask>
    static ENOKI_INLINE void transform_(void *mem, const Index &index,
                                 const Func &func, const Mask &mask) {
        ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
            transform<Value, Stride>(mem, index.coeff(i), func, mask.coeff(i));
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Fallback implementations of masked operations
    // -----------------------------------------------------------------------

    #define ENOKI_MASKED_OPERATOR_FALLBACK(name, expr) \
        template <typename T = Derived> \
        ENOKI_INLINE void m##name##_(const expr_t<T> &e, const mask_t<T> &m) { \
            derived() = select(m, expr, derived()); \
        }

    ENOKI_MASKED_OPERATOR_FALLBACK(assign, e)
    ENOKI_MASKED_OPERATOR_FALLBACK(add, derived() + e)
    ENOKI_MASKED_OPERATOR_FALLBACK(sub, derived() - e)
    ENOKI_MASKED_OPERATOR_FALLBACK(mul, derived() * e)
    ENOKI_MASKED_OPERATOR_FALLBACK(div, derived() / e)
    ENOKI_MASKED_OPERATOR_FALLBACK(or, derived() | e)
    ENOKI_MASKED_OPERATOR_FALLBACK(and, derived() & e)
    ENOKI_MASKED_OPERATOR_FALLBACK(xor, derived() ^ e)

    #undef ENOKI_MASKED_OPERATOR_FALLBACK

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Trigonometric and inverse trigonometric functions
    // -----------------------------------------------------------------------

    auto sin_() const {
        using Float = Scalar;
        using Expr = expr_t<Derived>;
        using IntArray = int_array_t<Expr>;
        using Int = scalar_t<IntArray>;

        Expr r;
        if (Approx) {
            /* Sine function approximation based on CEPHES.
               Excellent accuracy in the domain |x| < 8192

               Redistributed under a BSD license with permission of the author, see
               https://github.com/deepmind/torch-cephes/blob/master/LICENSE.txt

             - sin (in [-8192, 8192]):
               * avg abs. err = 6.61896e-09
               * avg rel. err = 1.37888e-08
                  -> in ULPs  = 0.166492
               * max abs. err = 5.96046e-08
                 (at x=-8191.31)
               * max rel. err = 1.76826e-06
                 -> in ULPs   = 19
                 (at x=-6374.29)
            */

            Expr x = abs(derived());

            /* Scale by 4/Pi and get the integer part */
            IntArray j(x * Float(1.27323954473516));

            /* Map zeros to origin; if (j & 1) j += 1 */
            j = (j + Int(1)) & Int(~1u);

            /* Cast back to a floating point value */
            Expr y(j);

            /* Determine sign of result */
            Expr sign = detail::sign_mask(reinterpret_array<Expr>(sli<29>(j)) ^ derived());

            /* Extended precision modular arithmetic */
            x = x - y * Float(0.78515625)
                  - y * Float(2.4187564849853515625e-4)
                  - y * Float(3.77489497744594108e-8);

            Expr z = x * x;

            Expr s(Float(-1.9515295891e-4));
            s = fmadd(s, z, Expr(Float( 8.3321608736e-3)));
            s = fmadd(s, z, Expr(Float(-1.6666654611e-1)));
            s *= z * x;
            s += x;

            Expr c(Float(2.443315711809948e-5));
            c = fmadd(c, z, Expr(Float(-1.388731625493765e-3)));
            c = fmadd(c, z, Expr(Float(4.166664568298827e-2)));
            c *= z * z;
            c -= Float(0.5) * z;
            c += Float(1);

            auto polymask = reinterpret_array<mask_t<Expr>>(
                eq(j & Int(2), zero<IntArray>()));

            r = select(polymask, s, c) ^ sign;
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = sin(derived().coeff(i));
        }
        return r;
    }

    auto cos_() const {
        using Float = Scalar;
        using Expr = expr_t<Derived>;
        using IntArray = int_array_t<Expr>;
        using Int = scalar_t<IntArray>;

        Expr r;
        if (Approx) {
            /* Cosine function approximation based on CEPHES.
               Excellent accuracy in the domain |x| < 8192

               Redistributed under a BSD license with permission of the author, see
               https://github.com/deepmind/torch-cephes/blob/master/LICENSE.txt

             - cos (in [-8192, 8192]):
               * avg abs. err = 6.59965e-09
               * avg rel. err = 1.37432e-08
                  -> in ULPs  = 0.166141
               * max abs. err = 5.96046e-08
                 (at x=-8191.05)
               * max rel. err = 3.13993e-06
                 -> in ULPs   = 47
                 (at x=-6199.93)
            */

            Expr x = abs(derived());

            /* Scale by 4/Pi and get the integer part */
            IntArray j(x * Float(1.27323954473516));

            /* Map zeros to origin; if (j & 1) j += 1 */
            j = (j + Int(1)) & Int(~1u);

            /* Cast back to a floating point value */
            Expr y(j);

            /* Determine sign of result */
            Expr sign = detail::sign_mask(reinterpret_array<Expr>(sli<29>(~(j - Int(2)))));

            /* Extended precision modular arithmetic */
            x = x - y * Float(0.78515625)
                  - y * Float(2.4187564849853515625e-4)
                  - y * Float(3.77489497744594108e-8);

            Expr z = x * x;

            Expr s(Float(-1.9515295891e-4));
            s = fmadd(s, z, Expr(Float( 8.3321608736e-3)));
            s = fmadd(s, z, Expr(Float(-1.6666654611e-1)));
            s *= z * x;
            s += x;

            Expr c(Float(2.443315711809948e-5));
            c = fmadd(c, z, Expr(Float(-1.388731625493765e-3)));
            c = fmadd(c, z, Expr(Float(4.166664568298827e-2)));
            c *= z * z;
            c -= Float(0.5) * z;
            c += Float(1);

            auto polymask = reinterpret_array<mask_t<Expr>>(
                eq(j & Int(2), zero<IntArray>()));

            r = select(polymask, c, s) ^ sign;
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = cos(derived().coeff(i));
        }
        return r;
    }

    auto sincos_() const {
        using Float = Scalar;
        using Expr = expr_t<Derived>;
        using IntArray = int_array_t<Expr>;
        using Int = scalar_t<IntArray>;

        Expr s_out, c_out;

        if (Approx) {
            /* Joint sine & cosine function approximation based on CEPHES.
               Excellent accuracy in the domain |x| < 8192

               Redistributed under a BSD license with permission of the author, see
               https://github.com/deepmind/torch-cephes/blob/master/LICENSE.txt

             - sin (in [-8192, 8192]):
               * avg abs. err = 6.61896e-09
               * avg rel. err = 1.37888e-08
                  -> in ULPs  = 0.166492
               * max abs. err = 5.96046e-08
                 (at x=-8191.31)
               * max rel. err = 1.76826e-06
                 -> in ULPs   = 19
                 (at x=-6374.29)

             - cos (in [-8192, 8192]):
               * avg abs. err = 6.59965e-09
               * avg rel. err = 1.37432e-08
                  -> in ULPs  = 0.166141
               * max abs. err = 5.96046e-08
                 (at x=-8191.05)
               * max rel. err = 3.13993e-06
                 -> in ULPs   = 47
                 (at x=-6199.93)
            */

            Expr x = abs(derived());

            /* Scale by 4/Pi and get the integer part */
            IntArray j(x * Float(1.27323954473516));

            /* Map zeros to origin; if (j & 1) j += 1 */
            j = (j + Int(1)) & Int(~1u);

            /* Cast back to a floating point value */
            Expr y(j);

            /* Determine sign of result */
            Expr sign_sin = detail::sign_mask(reinterpret_array<Expr>(sli<29>(j)) ^ derived());
            Expr sign_cos = detail::sign_mask(reinterpret_array<Expr>(sli<29>(~(j - Int(2)))));

            /* Extended precision modular arithmetic */
            x = x - y * Float(0.78515625)
                  - y * Float(2.4187564849853515625e-4)
                  - y * Float(3.77489497744594108e-8);

            Expr z = x * x;

            Expr s(Float(-1.9515295891e-4));
            s = fmadd(s, z, Expr(Float( 8.3321608736e-3)));
            s = fmadd(s, z, Expr(Float(-1.6666654611e-1)));
            s *= z * x;
            s += x;

            Expr c(Float(2.443315711809948e-5));
            c = fmadd(c, z, Expr(Float(-1.388731625493765e-3)));
            c = fmadd(c, z, Expr(Float(4.166664568298827e-2)));
            c *= z * z;
            c -= Float(0.5) * z;
            c += Float(1);

            auto polymask = reinterpret_array<mask_t<Expr>>(
                eq(j & Int(2), zero<IntArray>()));

            s_out = select(polymask, s, c) ^ sign_sin;
            c_out = select(polymask, c, s) ^ sign_cos;
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i) {
                c_out.coeff(i) = cos(derived().coeff(i));
                s_out.coeff(i) = sin(derived().coeff(i));
            }
        }

        return std::make_pair(s_out, c_out);
    }

    auto tan_() const {
        using Expr = expr_t<Derived>;

        Expr r;
        if (Approx) {
            /*
             - tan (in [-8192, 8192]):
               * avg abs. err = 4.63693e-06
               * avg rel. err = 3.60191e-08
                  -> in ULPs  = 0.435442
               * max abs. err = 0.8125
                 (at x=-6199.93)
               * max rel. err = 3.12284e-06
                 -> in ULPs   = 30
                 (at x=-7406.3)
            */

            auto sc = sincos(derived());
            r = sc.first / sc.second;
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = tan(derived().coeff(i));
        }
        return r;
    }

    auto csc_() const { return rcp(sin(derived())); }

    auto sec_() const { return rcp(cos(derived())); }

    auto cot_() const {
        auto sc = sincos(derived());
        return sc.second / sc.first;
    }

    auto asin_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
               MiniMax fit by Wenzel Jakob, May 2016

             - asin (in [-1, 1]):
               * avg abs. err = 8.29328e-08
               * avg rel. err = 2.92697e-07
                  -> in ULPs  = 3.51903
               * max abs. err = 3.72529e-07
                 (at x=-0.176019)
               * max rel. err = 4.38672e-06
                 -> in ULPs   = 61
                 (at x=0.053627)
            */

            Expr x_          = derived();
            Expr x           = abs(x_);
            auto invalidMask = x > Float(1);
            auto negate      = x_ < zero<Expr>();

            // How to find these:
            // f[x_] = MiniMaxApproximation[
            //    ArcCos[x]/Sqrt[1 - x], {x, {0, 1 - 1^20}, 6, 0},
            //    WorkingPrecision -> 30][[2, 1]]
            Expr t = Float(+0.00227944990024845419940890);
            t = fmadd(t, x, Expr(Float(-0.01109688980710918972127294)));
            t = fmadd(t, x, Expr(Float(+0.02684475831352801832421248)));
            t = fmadd(t, x, Expr(Float(-0.04877412052802108370460564)));
            t = fmadd(t, x, Expr(Float(+0.08874905480758988950198278)));
            t = fmadd(t, x, Expr(Float(-0.21458470981542561024897117)));
            t = fmadd(t, x, Expr(Float(1.57079616508886408344826942)));

            t = t * sqrt(Float(1) - x);
            t = Expr(Float(M_PI_2)) - t;

            r = select(
                x > Float(.5e-1),
                (t - (Expr(Float(2)) & negate) * t),
                x_ + x_*x_*x_*Expr(Float(1.0/6.0))
            ) | invalidMask;
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = asin(derived().coeff(i));
        }
        return r;
    }

    auto acos_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
               MiniMax fit by Wenzel Jakob, May 2016

             - acos (in [-1, 1]):
               * avg abs. err = 9.28426e-08
               * avg rel. err = 6.35844e-08
                  -> in ULPs  = 0.774026
               * max abs. err = 4.76837e-07
                 (at x=-0.670102)
               * max rel. err = 2.81112e-07
                 -> in ULPs   = 4
                 (at x=0.543923)
            */
            Expr x           = abs(derived());
            auto invalidMask = x > Float(1);
            auto negate      = derived() < zero<Expr>();

            // How to find these:
            // f[x_] = MiniMaxApproximation[
            //    ArcCos[x]/Sqrt[1 - x], {x, {0, 1 - 1^20}, 6, 0},
            //    WorkingPrecision -> 30][[2, 1]]

            Expr t = Float(+0.00227944990024845419940890);
            t = fmadd(t, x, Expr(Float(-0.01109688980710918972127294)));
            t = fmadd(t, x, Expr(Float(+0.02684475831352801832421248)));
            t = fmadd(t, x, Expr(Float(-0.04877412052802108370460564)));
            t = fmadd(t, x, Expr(Float(+0.08874905480758988950198278)));
            t = fmadd(t, x, Expr(Float(-0.21458470981542561024897117)));
            t = fmadd(t, x, Expr(Float(1.57079616508886408344826942)));

            t = t * sqrt(Float(1) - x);
            t = t - (Expr(Float(2)) & negate) * t;

            r = (t + (Expr(Float(M_PI)) & negate)) | invalidMask;
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = acos(derived().coeff(i));
        }
        return r;
    }

    auto atan2_(const Derived &x) const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
               MiniMax fit by Wenzel Jakob, May 2016

             - atan2() tested via atan() (in [-1, 1]):
               * avg abs. err = 1.81543e-07
               * avg rel. err = 4.15224e-07
                  -> in ULPs  = 4.9197
               * max abs. err = 5.96046e-07
                 (at x=-0.976062)
               * max rel. err = 7.73931e-07
                 -> in ULPs   = 12
                 (at x=-0.015445)
            */
            Expr y         = derived(),
                 absX      = abs(x),
                 absY      = abs(y),
                 minVal    = min(absY, absX),
                 maxVal    = max(absX, absY),
                 scale     = Float(1) / maxVal,
                 scaledMin = minVal * scale,
                 z         = scaledMin * scaledMin;

            // How to find these:
            // f[x_] = MiniMaxApproximation[ArcTan[Sqrt[x]]/Sqrt[x],
            //         {x, {1/10000, 1}, 6, 0}, WorkingPrecision->20][[2, 1]]

            Expr t = Float(0.0078613793713198150252);
            t = fmadd(t, z, Expr(Float(-0.037006525670417265220)));
            t = fmadd(t, z, Expr(Float(+0.083863120428809689910)));
            t = fmadd(t, z, Expr(Float(-0.13486708938456973185)));
            t = fmadd(t, z, Expr(Float(+0.19881342388439013552)));
            t = fmadd(t, z, Expr(Float(-0.33326497518773606976)));
            t = fmadd(t, z, Expr(Float(+0.99999934166683966009)));

            t = t * scaledMin;

            t = select(absY > absX, Float(M_PI_2) - t, t);
            t = select(x < zero<Expr>(), Float(M_PI) - t, t);
            r = select(y < zero<Expr>(), -t, t);
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = atan2(derived().coeff(i), x.coeff(i));
        }
        return r;
    }

    auto atan_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        if (Approx) {
            return atan2(derived(), Expr(Float(1)));
        } else {
            Expr r;
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = atan(derived().coeff(i));
            return r;
        }
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Exponential function, logarithm, power
    // -----------------------------------------------------------------------

    auto exp_() const {
        using Expr = expr_t<Derived>;

        Expr r;
        if (Approx) {
            /* Exponential function approximation based on CEPHES

               Redistributed under a BSD license with permission of the author, see
               https://github.com/deepmind/torch-cephes/blob/master/LICENSE.txt

             - exp (in [-20, 30]):
               * avg abs. err = 7155.01
               * avg rel. err = 2.35929e-08
                  -> in ULPs  = 0.273524
               * max abs. err = 1.04858e+06
                 (at x=29.8057)
               * max rel. err = 1.192e-07
                 -> in ULPs   = 1
                 (at x=-19.9999)
            */
            using Float = Scalar;

            const Expr inf(std::numeric_limits<Float>::infinity());
            const Expr maxRange(Float(+88.3762626647949));
            const Expr minRange(Float(-88.3762626647949));

            Expr x(derived());

            auto mask1 = x > maxRange;
            auto mask2 = x < minRange;

            /* Express e^x = e^g 2^n
                 = e^g e^(n loge(2))
                 = e^(g + n loge(2))
            */
            Expr n = floor(Float(1.44269504088896341) * x + Float(0.5));
            x -= n * Float(0.693359375);
            x -= n * Float(-2.12194440e-4);

            /* Rational approximation for exponential
               of the fractional part:
                  e^x = 1 + 2x P(x^2) / (Q(x^2) - P(x^2))
             */
            Expr z(Float(1.9875691500e-4));
            z = fmadd(z, x, Expr(Float(1.3981999507e-3)));
            z = fmadd(z, x, Expr(Float(8.3334519073e-3)));
            z = fmadd(z, x, Expr(Float(4.1665795894e-2)));
            z = fmadd(z, x, Expr(Float(1.6666665459e-1)));
            z = fmadd(z, x, Expr(Float(5.0000001201e-1)));
            z = z * x * x;
            z += x + Float(1);

            r = select(mask1, inf, select(mask2, zero<Expr>(), ldexp(z, n)));
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = exp(derived().coeff(i));
        }
        return r;
    }

    auto log_() const {
        using Expr = expr_t<Derived>;

        Expr r;
        if (Approx) {
            /* Logarithm function approximation based on CEPHES

               Redistributed under a BSD license with permission of the author, see
               https://github.com/deepmind/torch-cephes/blob/master/LICENSE.txt

             - log (in [1e-20, 1000]):
               * avg abs. err = 8.8672e-09
               * avg rel. err = 1.57541e-09
                  -> in ULPs  = 0.020038
               * max abs. err = 4.76837e-07
                 (at x=54.7661)
               * max rel. err = 1.19194e-07
                 -> in ULPs   = 1
                 (at x=0.021)
            */
            using Float = Scalar;
            using UInt = scalar_t<int_array_t<Expr>>;

            const Expr inf(std::numeric_limits<Float>::infinity());

            Expr x(derived());

            /* Catch negative and NaN values */
            auto validMask = x >= Float(0);

            /* Cut off denormalized values (our frexp does not handle them) */
            if (std::is_same<Float, float>::value)
                x = max(x, Expr(memcpy_cast<Float>(UInt(0x00800000u))));
            else
                x = max(x, Expr(memcpy_cast<Float>(UInt(0x0010000000000000ull))));

            Expr e;
            std::tie(x, e) = frexp(x);

            auto ltInvSqrt2 = x < Float(0.707106781186547524);

            e -= Expr(Float(1.f)) & ltInvSqrt2;
            x += (x & ltInvSqrt2) - Expr(Float(1));

            Expr z = x*x;
            Expr y(Float(7.0376836292e-2));
            y = fmadd(y, x, Expr(Float(-1.1514610310e-1)));
            y = fmadd(y, x, Expr(Float(+1.1676998740e-1)));
            y = fmadd(y, x, Expr(Float(-1.2420140846e-1)));
            y = fmadd(y, x, Expr(Float(+1.4249322787e-1)));
            y = fmadd(y, x, Expr(Float(-1.6668057665e-1)));
            y = fmadd(y, x, Expr(Float(+2.0000714765e-1)));
            y = fmadd(y, x, Expr(Float(-2.4999993993e-1)));
            y = fmadd(y, x, Expr(Float(+3.3333331174e-1)));
            y *= x * z;

            y += Float(-2.12194440e-4) * e;
            y += Float(-0.5) * z;  /* y - 0.5 x^2 */
            z = x + y;             /* ... + x  */
            z += Float(0.693359375) * e;

            r = select(eq(derived(), inf), inf, z | ~validMask);
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = log(derived().coeff(i));
        }
        return r;
    }

    /// Multiply by integer power of 2
    auto ldexp_(const Derived &n) const {
        using Expr = expr_t<Derived>;

        Expr r;
        if (Approx) {
            r = derived() * reinterpret_array<Expr>(sli<23>(int_array_t<Expr>(n) + 0x7f));
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = ldexp(derived().coeff(i), n.coeff(i));
        }
        return r;
    }

    /// Break floating-point number into normalized fraction and power of 2
    auto frexp_() const {
        using Expr = expr_t<Derived>;
        Expr result_m, result_e;

        /// Caveat: does not handle denormals correctly
        if (Approx) {
            using Float = Scalar;
            using IntArray = int_array_t<Expr>;
            using Int = scalar_t<IntArray>;
            using IntMask = mask_t<IntArray>;

            const IntArray
                exponentMask(Int(0x7f800000u)),
                mantissaSignMask(Int(~0x7f800000u)),
                biasMinus1(Int(0x7e));

            IntArray x = reinterpret_array<IntArray>(derived());
            IntArray exponent_bits = x & exponentMask;

            /* Detect zero/inf/NaN */
            IntMask is_normal =
                reinterpret_array<IntMask>(neq(derived(), zero<Expr>())) &
                neq(exponent_bits, exponentMask);

            IntArray exponent_i = (sri<23>(exponent_bits)) - biasMinus1;
            IntArray mantissa =
                (x & mantissaSignMask) | IntArray(memcpy_cast<Int>(Float(.5f)));

            result_e = Expr(exponent_i & is_normal);
            result_m = reinterpret_array<Expr>(select(is_normal, mantissa, x));
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                std::tie(result_m.coeff(i), result_e.coeff(i)) = frexp(derived().coeff(i));
        }
        return std::make_pair(result_m, result_e);
    }

    auto pow_(const Derived &y) const {
        using Expr = expr_t<Derived>;

        Expr r;
        if (Approx) {
            r = exp(log(derived()) * y);
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = pow(derived().coeff(i), y.coeff(i));
        }
        return r;
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Hyperbolic and inverse hyperbolic functions
    // -----------------------------------------------------------------------

    auto sinh_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;

        if (Approx) {
            /*
             - sinh (in [-10, 10]):
               * avg abs. err = 2.67105e-05
               * avg rel. err = 4.18202e-08
                  -> in ULPs  = 0.499102
               * max abs. err = 0.000976562
                 (at x=-9.99998)
               * max rel. err = 1.65943e-05
                 -> in ULPs   = 178
                 (at x=-0.00998974)
            */

            Expr exp0 = exp(derived()),
                 exp1 = Float(1) / exp0;

            r = select(
                abs(derived()) < Float(1e-2), derived(),
                (exp0 - exp1) * Float(0.5)
            );
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = sinh(derived().coeff(i));
        }
        return r;
    }

    auto cosh_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
             - cosh (in [-10, 10]):
               * avg abs. err = 4.07615e-05
               * avg rel. err = 2.94041e-08
                  -> in ULPs  = 0.349255
               * max abs. err = 0.000976562
                 (at x=-10)
               * max rel. err = 2e-07
                 -> in ULPs   = 2
                 (at x=-9.7037)
            */

            Expr exp0 = exp(derived()),
                 exp1 = Float(1) / exp0;

            r = (exp0 + exp1) * Float(.5f);
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = cosh(derived().coeff(i));
        }
        return r;
    }

    auto sincosh_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;
        Expr s_out, c_out;

        if (Approx) {
            /*
             - sinh (in [-10, 10]):
               * avg abs. err = 2.67105e-05
               * avg rel. err = 4.18202e-08
                  -> in ULPs  = 0.499102
               * max abs. err = 0.000976562
                 (at x=-9.99998)
               * max rel. err = 1.65943e-05
                 -> in ULPs   = 178
                 (at x=-0.00998974)

             - cosh (in [-10, 10]):
               * avg abs. err = 4.07615e-05
               * avg rel. err = 2.94041e-08
                  -> in ULPs  = 0.349255
               * max abs. err = 0.000976562
                 (at x=-10)
               * max rel. err = 2e-07
                 -> in ULPs   = 2
                 (at x=-9.7037)
            */
            Expr exp0 = exp(derived()),
                 exp1 = rcp(exp0),
                 half = Expr(Float(.5));

            s_out = select(
                abs(derived()) < Float(1e-2), derived(),
                half * (exp0 - exp1)
            );

            c_out = half * (exp0 + exp1);
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i) {
                s_out.coeff(i) = sinh(derived().coeff(i));
                c_out.coeff(i) = cosh(derived().coeff(i));
            }
        }

        return std::make_pair(s_out, c_out);
    }

    auto tanh_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
             - tanh (in [-10, 10]):
               * avg abs. err = 3.81231e-08
               * avg rel. err = 5.9058e-08
                  -> in ULPs  = 0.866501
               * max abs. err = 3.32482e-07
                 (at x=-0.00998974)
               * max rel. err = 3.32835e-05
                 -> in ULPs   = 357
                 (at x=-0.00998974)

               TBD: correct behavior for +/- inf and nan
            */
            Expr exp0 = exp(derived()),
                 exp1 = rcp(exp0);

            r = select(
                abs(derived()) < Float(1e-2), derived(),
                (exp0 - exp1) / (exp0 + exp1)
            );
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = tanh(derived().coeff(i));
        }
        return r;
    }

    auto csch_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            Expr exp0 = exp(derived()),
                 exp1 = rcp(exp0);

            r = rcp(exp0 - exp1) * Expr(Float(2));
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = csch(derived().coeff(i));
        }
        return r;
    }

    auto sech_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            Expr exp0 = exp(derived()),
                 exp1 = rcp(exp0);

            r = rcp(exp0 + exp1) * Expr(Float(2));
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = sech(derived().coeff(i));
        }
        return r;
    }

    auto coth_() const {
        using Expr = expr_t<Derived>;

        Expr r;
        if (Approx) {
            Expr exp0 = exp(derived()),
                 exp1 = rcp(exp0);

            r = (exp0 + exp1) / (exp0 - exp1);
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = coth(derived().coeff(i));
        }
        return r;
    }

    auto asinh_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
             - asinh (in [-10, 10]):
               * avg abs. err = 8.51667e-07
               * avg rel. err = 3.46427e-07
                  -> in ULPs  = 4.00706
               * max abs. err = 1.3113e-05
                 (at x=-9.99982)
               * max rel. err = 1.65948e-05
                 -> in ULPs   = 178
                 (at x=-0.00998974)

               TBD: correct behavior for +/- inf and nan
            */
            Expr x = derived();
            r = select(
                abs(x) < Float(1e-2), x,
                log(x + sqrt(Expr(Float(1)) + x * x))
            );
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = asinh(derived().coeff(i));
        }
        return r;
    }

    auto acosh_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
             - acosh (in [1, 10]):
               * avg abs. err = 3.17789e-08
               * avg rel. err = 1.69285e-08
                  -> in ULPs  = 0.198501
               * max abs. err = 2.38419e-07
                 (at x=3.08437)
               * max rel. err = 1.35449e-05
                 -> in ULPs   = 123
                 (at x=1.00001)
            */
            const Expr one(Float(1));
            Expr x = derived();

            r = log(x + sqrt(x - one) * sqrt(x + one));
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = acosh(derived().coeff(i));
        }
        return r;
    }

    auto atanh_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
             - atanh (in [-0.999, 0.999]):
               * avg abs. err = 1.90389e-08
               * avg rel. err = 2.06572e-07
                  -> in ULPs  = 2.43416
               * max abs. err = 3.33413e-07
                 (at x=-0.00999898)
               * max rel. err = 3.33502e-05
                 -> in ULPs   = 358
                 (at x=-0.00999898)
            */
            const Expr one(Float(1));
            Expr x = derived();

            r = select(
                abs(x) < Float(1e-2), x,
                Expr(Float(.5f)) * log(
                    (one + x) / (one - x)
                )
            );
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = atanh(derived().coeff(i));
        }
        return r;
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Error function and its inverse
    // -----------------------------------------------------------------------

    auto erf_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        Expr r;
        if (Approx) {
            /*
             - erf (in [-1, 1]):
               * avg abs. err = 1.01113e-07
               * avg rel. err = 3.23606e-07
                  -> in ULPs  = 3.89448
               * max abs. err = 4.76837e-07
                 (at x=-0.0811131)
               * max rel. err = 5.22126e-06
                 -> in ULPs   = 64
                 (at x=-0.0811131)
             */

            // A&S formula 7.1.26
            Expr x_ = derived();
            Expr x = abs(x_), x2 = x_ * x_;
            Expr t = Expr(1) / fmadd(Expr(Float(0.3275911)), x, Expr(Float(1)));

            Expr y(Float(1.061405429));
            y = fmadd(y, t, Expr(Float(-1.453152027)));
            y = fmadd(y, t, Expr(Float(1.421413741)));
            y = fmadd(y, t, Expr(Float(-0.284496736)));
            y = fmadd(y, t, Expr(Float(0.254829592)));
            auto ev = exp(-x2);
            y *= t * ev;

            /* Switch between the A&S approximation and a Taylor series
               expansion around the origin */
            r = select(
                x > Expr(Float(0.08)),
                (Float(1) - y) | detail::sign_mask(x_),
                x_ * fmadd(x2, Expr(Float(-M_2_SQRTPI/3)),
                               Expr(Float( M_2_SQRTPI)))
            );
        } else {
            ENOKI_CHKSCALAR for (size_t i = 0; i < Derived::Size; ++i)
                r.coeff(i) = erf(derived().coeff(i));
        }
        return r;
    }

    auto erfi_() const {
        using Expr = expr_t<Derived>;
        using Float = Scalar;

        // Based on "Approximating the erfi function" by Mark Giles
        Expr x = derived();
        Expr w = -log((Float(1) - x) * (Float(1) + x));

        Expr w1 = w - Float(2.5);
        Expr w2 = sqrt(w) - Float(3);

        Expr p1(Float(2.81022636e-08));
        Expr p2(Float(-0.000200214257));
        p1 = fmadd(p1, w1, Expr(Float(3.43273939e-07)));
        p2 = fmadd(p2, w2, Expr(Float(0.000100950558)));
        p1 = fmadd(p1, w1, Expr(Float(-3.5233877e-06)));
        p2 = fmadd(p2, w2, Expr(Float(0.00134934322)));
        p1 = fmadd(p1, w1, Expr(Float(-4.39150654e-06)));
        p2 = fmadd(p2, w2, Expr(Float(-0.00367342844)));
        p1 = fmadd(p1, w1, Expr(Float(0.00021858087)));
        p2 = fmadd(p2, w2, Expr(Float(0.00573950773)));
        p1 = fmadd(p1, w1, Expr(Float(-0.00125372503)));
        p2 = fmadd(p2, w2, Expr(Float(-0.0076224613)));
        p1 = fmadd(p1, w1, Expr(Float(-0.00417768164)));
        p2 = fmadd(p2, w2, Expr(Float(0.00943887047)));
        p1 = fmadd(p1, w1, Expr(Float(0.246640727)));
        p2 = fmadd(p2, w2, Expr(Float(1.00167406)));
        p1 = fmadd(p1, w1, Expr(Float(1.50140941)));
        p2 = fmadd(p2, w2, Expr(Float(2.83297682)));

        return select(w < Float(5), p1, p2) * x;
    }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Component access
    // -----------------------------------------------------------------------

    ENOKI_INLINE const Value &x() const {
        static_assert(Derived::ActualSize >= 1,
                      "StaticArrayBase::x(): requires Size >= 1");
        return derived().coeff(0);
    }

    ENOKI_INLINE Value& x() {
        static_assert(Derived::ActualSize >= 1,
                      "StaticArrayBase::x(): requires Size >= 1");
        return derived().coeff(0);
    }

    ENOKI_INLINE const Value &y() const {
        static_assert(Derived::ActualSize >= 2,
                      "StaticArrayBase::y(): requires Size >= 2");
        return derived().coeff(1);
    }

    ENOKI_INLINE Value& y() {
        static_assert(Derived::ActualSize >= 2,
                      "StaticArrayBase::y(): requires Size >= 2");
        return derived().coeff(1);
    }

    ENOKI_INLINE const Value& z() const {
        static_assert(Derived::ActualSize >= 3,
                      "StaticArrayBase::z(): requires Size >= 3");
        return derived().coeff(2);
    }

    ENOKI_INLINE Value& z() {
        static_assert(Derived::ActualSize >= 3,
                      "StaticArrayBase::z(): requires Size >= 3");
        return derived().coeff(2);
    }

    ENOKI_INLINE const Value& w() const {
        static_assert(Derived::ActualSize >= 4,
                      "StaticArrayBase::w(): requires Size >= 4");
        return derived().coeff(3);
    }

    ENOKI_INLINE Value& w() {
        static_assert(Derived::ActualSize >= 4,
                      "StaticArrayBase::w(): requires Size >= 4");
        return derived().coeff(3);
    }

    ENOKI_INLINE Value *data() { return &derived().coeff(0); }
    ENOKI_INLINE const Value *data() const { return &derived().coeff(0); }

    //! @}
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //! @{ \name Other Methods
    // -----------------------------------------------------------------------

    /// Return the array size
    constexpr size_t size() const { return Derived::Size; }

    //! @}
    // -----------------------------------------------------------------------

};

NAMESPACE_BEGIN(detail)

template <typename Array, size_t N, typename... Indices,
          std::enable_if_t<sizeof...(Indices) == N, int> = 0>
std::ostream &print(std::ostream &os, const Array &a,
                    const std::array<size_t, N> &,
                    Indices... indices) {
    os << a.derived().coeff(indices...);
    return os;
}

template <typename Array, size_t N, typename... Indices,
          std::enable_if_t<sizeof...(Indices) != N, int> = 0>
std::ostream &print(std::ostream &os, const Array &a,
                    const std::array<size_t, N> &size,
                    Indices... indices) {
    constexpr size_t k = N - sizeof...(Indices) - 1;
    os << "[";
    for (size_t i = 0; i < size[k]; ++i) {
        print(os, a, size, i, indices...);
        if (i + 1 < size[k]) {
            if (k == 0) {
                os << ", ";
            } else {
                os << ",\n";
                for (size_t i = 0; i <= sizeof...(Indices); ++i)
                    os << " ";
            }
        }
    }
    os << "]";
    return os;
}

NAMESPACE_END(detail)

template <typename Value, typename Derived>
ENOKI_NOINLINE std::ostream &operator<<(std::ostream &os,
                                        const ArrayBase<Value, Derived> &a) {
    return detail::print(os, a, shape(a.derived()));
}

/// Macro to initialize uninitialized floating point arrays with NaNs in debug mode
#if defined(NDEBUG)
#define ENOKI_TRIVIAL_CONSTRUCTOR(Type_)                                       \
    template <typename T = Type_,                                              \
         std::enable_if_t<std::is_default_constructible<T>::value, int> = 0>   \
    ENOKI_INLINE StaticArrayImpl() { }
#else
#define ENOKI_TRIVIAL_CONSTRUCTOR(Type_)                                       \
    template <typename T = Type_,                                              \
         std::enable_if_t<std::is_floating_point<T>::value &&                  \
                          std::is_default_constructible<T>::value, int> = 0>   \
    ENOKI_INLINE StaticArrayImpl()                                             \
     : StaticArrayImpl(std::numeric_limits<scalar_t<T>>::quiet_NaN()) { } \
    template <typename T = Type_,                                              \
         std::enable_if_t<!std::is_floating_point<T>::value &&                 \
                          std::is_default_constructible<T>::value, int> = 0>   \
    ENOKI_INLINE StaticArrayImpl() { }
#endif

/// SFINAE macro for constructors that convert from another type
#define ENOKI_CONVERT(Type)                                                    \
    template <typename Type2, bool Approx2, RoundingMode Mode2,                \
              typename Derived2,                                               \
              std::enable_if_t<detail::is_same<Type2, Type>::value, int> = 0>  \
    ENOKI_INLINE StaticArrayImpl(                                              \
        const StaticArrayBase<Type2, Size, Approx2, Mode2, Derived2> &a)

/// SFINAE macro for constructors that reinterpret another type
#define ENOKI_REINTERPRET(Type)                                                \
    template <typename Type2, bool Approx2, RoundingMode Mode2,                \
              typename Derived2,                                               \
              std::enable_if_t<detail::is_same<Type2, Type>::value, int> = 0>  \
    ENOKI_INLINE StaticArrayImpl(                                              \
        const StaticArrayBase<Type2, Size, Approx2, Mode2, Derived2> &a,       \
        detail::reinterpret_flag)

/// SFINAE macro for constructors that reinterpret another type (K mask registers)
#define ENOKI_REINTERPRET_KMASK(Type, Size)                                    \
    template <typename Type2, bool Approx2, RoundingMode Mode2,                \
              typename Derived2,                                               \
              std::enable_if_t<detail::is_same<Type2, Type>::value, int> = 0>  \
    ENOKI_INLINE KMask(                                                        \
        const StaticArrayBase<Type2, Size, Approx2, Mode2, Derived2> &a,       \
        detail::reinterpret_flag)

/// SFINAE macro for strided operations (scatter, gather)
#define ENOKI_REQUIRE_INDEX(T, Index)                                          \
    template <                                                                 \
        size_t Stride, typename T,                                             \
        std::enable_if_t<std::is_integral<typename T::Value>::value &&         \
                         sizeof(typename T::Value) == sizeof(Index), int> = 0>

/// SFINAE macro for strided operations (prefetch)
#define ENOKI_REQUIRE_INDEX_PF(T, Index)                                       \
    template <                                                                 \
        size_t Stride, bool Write, size_t Level, typename T,                   \
        std::enable_if_t<std::is_integral<typename T::Value>::value &&         \
                         sizeof(typename T::Value) == sizeof(Index), int> = 0>

/// SFINAE macro for strided operations (transform)
#define ENOKI_REQUIRE_INDEX_TRANSFORM(T, Index)                                \
    template <                                                                 \
        size_t Stride, typename T, typename Func,                              \
        std::enable_if_t<std::is_integral<typename T::Value>::value &&         \
                         sizeof(typename T::Value) == sizeof(Index), int> = 0>

#define ENOKI_NATIVE_ARRAY(Value_, Size_, Approx_, Register, Mode)             \
    static constexpr bool Native = true;                                       \
    using Base = StaticArrayBase<Value_, Size_, Approx_, Mode, Derived>;       \
    using Arg = Derived;                                                       \
    using Expr = Derived;                                                      \
    using Base::operator=;                                                     \
    using typename Base::Value;                                                \
    using typename Base::Array1;                                               \
    using typename Base::Array2;                                               \
    using Base::Size;                                                          \
    using Base::ActualSize;                                                    \
    using Base::derived;                                                       \
    Register m;                                                                \
    ENOKI_TRIVIAL_CONSTRUCTOR(Value_)                                          \
    ENOKI_INLINE StaticArrayImpl(Register value) : m(value) { }                \
    StaticArrayImpl(const StaticArrayImpl &a) = default;                       \
    StaticArrayImpl &operator=(const StaticArrayImpl &a) = default;            \
    ENOKI_INLINE Value &coeff(size_t i) { return ((Value *) &m)[i]; }          \
    ENOKI_INLINE const Value &coeff(size_t i) const {                          \
        return ((const Value *) &m)[i];                                        \
    }                                                                          \
    template <typename Type2, typename Derived2, typename T = Derived,         \
              std::enable_if_t<std::is_assignable<Value_ &, Type2>::value &&   \
                               Derived2::Size == T::Size, int> = 0>            \
    ENOKI_INLINE StaticArrayImpl(const ArrayBase<Type2, Derived2> &a) {        \
        ENOKI_TRACK_SCALAR for (size_t i = 0; i < Derived2::Size; ++i)         \
            derived().coeff(i) = Value(a.derived().coeff(i));                  \
    }                                                                          \
    template <size_t Size2, bool Approx2, RoundingMode Mode2,                  \
              typename Derived2>                                               \
    ENOKI_INLINE StaticArrayImpl(                                              \
        const StaticArrayBase<bool, Size2, Approx2, Mode2, Derived2> &a,       \
        detail::reinterpret_flag) {                                            \
        static_assert(Derived::Size == Derived2::Size, "Size mismatch!");      \
        using Int = typename detail::type_chooser<sizeof(Value)>::Int;         \
        const Value on = memcpy_cast<Value>(Int(-1));                          \
        const Value off = memcpy_cast<Value>(Int(0));                          \
        ENOKI_TRACK_SCALAR for (size_t i = 0; i < Derived2::Size; ++i)         \
            coeff(i) = a.derived().coeff(i) ? on : off;                        \
    }                                                                          \
    template <                                                                 \
        typename T, std::enable_if_t<std::is_same<T, bool>::value, int> = 0,   \
        typename Int = typename detail::type_chooser<sizeof(Value)>::Int>      \
    ENOKI_INLINE StaticArrayImpl(T b)                                          \
        : StaticArrayImpl(b ? memcpy_cast<Value>(Int(-1))                      \
                            : memcpy_cast<Value>(Int(0))) { }

#define ENOKI_NATIVE_ARRAY_CLASSIC(Value_, Size_, Approx_, Register)           \
    ENOKI_NATIVE_ARRAY(Value_, Size_, Approx_, Register,                       \
                       RoundingMode::Default)                                  \
    using Mask = Derived;

NAMESPACE_END(enoki)
