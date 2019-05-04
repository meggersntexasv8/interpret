// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <code@koch.ninja>

#ifndef TRANSPARENT_ML_CORE_INTERNAL_H
#define TRANSPARENT_ML_CORE_INTERNAL_H

#include <inttypes.h>
#include <stddef.h> // size_t, ptrdiff_t

#ifndef NDEBUG
// enable output features for all debug builds everywhere in our library
#include <iostream> // cout
#include <stdio.h> // printf
#endif

#if defined(__clang__) || defined(__GNUC__) // compiler
#ifndef __has_builtin
#define __has_builtin(x) 0 // __has_builtin is supported in newer compilers.  On older compilers diable anything we would check with it
#endif // __has_builtin

#define LIKELY(b) __builtin_expect(static_cast<bool>(b), 1)
#define UNLIKELY(b) __builtin_expect(static_cast<bool>(b), 0)
#define PREDICTABLE(b) (b)

#if __has_builtin(__builtin_unpredictable)
#define UNPREDICTABLE(b) __builtin_unpredictable(b)
#else // __has_builtin(__builtin_unpredictable)
#define UNPREDICTABLE(b) (b)
#endif // __has_builtin(__builtin_unpredictable)

#define TML_INLINE inline
#elif defined(_MSC_VER) /* compiler type */
#define LIKELY(b) (b)
#define UNLIKELY(b) (b)
#define PREDICTABLE(b) (b)
#define UNPREDICTABLE(b) (b)
#define TML_INLINE __forceinline
#else // compiler
#error compiler not recognized
#endif // compiler

#if defined(__clang__) || defined(__GNUC__) // compiler
#elif defined(_MSC_VER) /* compiler type */
#pragma warning(push)
#pragma warning(disable: 4018) // signed/unsigned mismatch
#else // compiler
#error compiler not recognized
#endif // compiler

template<typename TTo, typename TFrom>
constexpr TML_INLINE bool IsNumberConvertable(TFrom number) {
   // TODO: this function won't work for some floats and doubles.  For example, if you had just a bit more than the maximum integer in a double, it might round down properly, but this function will say that it won't.

   // the general rules of conversion are as follows:
   // calling std::numeric_limits<?>::max() returns an item of that type
   // casting and comparing will never give us undefined behavior.  It can give us implementation defined behavior or unspecified behavior, which is legal.  Undefined behavior results from overflowing negative integers, but we don't add or subtract.
   // C/C++ uses value preserving instead of sign preserving.  Generally, if you have two integer numbers that you're comparing then if one type can be converted into the other with no loss in range then that the smaller range integer is converted into the bigger range integer
   // if one type can't cover the entire range of the other, then items are converted to UNSIGNED values.  This is probably the most dangerous thing for us to deal with

   static_assert(std::numeric_limits<TTo>::is_specialized, "TTo must be specialized");
   static_assert(std::numeric_limits<TFrom>::is_specialized, "TFrom must be specialized");

   static_assert(std::numeric_limits<TTo>::is_signed || 0 == std::numeric_limits<TTo>::lowest(), "min of an unsigned TTo value must be zero");
   static_assert(std::numeric_limits<TFrom>::is_signed || 0 == std::numeric_limits<TFrom>::lowest(), "min of an unsigned TFrom value must be zero");
   static_assert(0 <= std::numeric_limits<TTo>::max(), "TTo max must be positive");
   static_assert(0 <= std::numeric_limits<TFrom>::max(), "TFrom max must be positive");
   static_assert(std::numeric_limits<TTo>::is_signed != std::numeric_limits<TFrom>::is_signed || ((std::numeric_limits<TTo>::lowest() <= std::numeric_limits<TFrom>::lowest() && std::numeric_limits<TFrom>::max() <= std::numeric_limits<TTo>::max()) || (std::numeric_limits<TFrom>::lowest() <= std::numeric_limits<TTo>::lowest() && std::numeric_limits<TTo>::max() <= std::numeric_limits<TFrom>::max())), "types should entirely wrap their smaller types or be the same size");

   return std::numeric_limits<TTo>::is_signed ? (std::numeric_limits<TFrom>::is_signed ? (std::numeric_limits<TTo>::lowest() <= number && number <= std::numeric_limits<TTo>::max()) : (number <= std::numeric_limits<TTo>::max())) : (std::numeric_limits<TFrom>::is_signed ? (0 <= number && number <= std::numeric_limits<TTo>::max()) : (number <= std::numeric_limits<TTo>::max()));

   // C++11 is pretty limited for constexpr functions and requires everything to be in 1 line (above).  In C++14 though the below more readable code should be used.
   //if(std::numeric_limits<TTo>::is_signed) {
   //   if(std::numeric_limits<TFrom>::is_signed) {
   //      // To signed from signed
   //      // if both operands are the same size, then they should be the same type
   //      // if one operand is bigger, then both operands will be converted to that type and the result will not have unspecified behavior
   //      return std::numeric_limits<TTo>::lowest() <= number && number <= std::numeric_limits<TTo>::max();
   //   } else {
   //      // To signed from unsigned
   //      // if both operands are the same size, then max will be converted to the unsigned type, but that should be fine as max should fit
   //      // if one operand is bigger, then both operands will be converted to that type and the result will not have unspecified behavior
   //      return number <= std::numeric_limits<TTo>::max();
   //   }
   //} else {
   //   if(std::numeric_limits<TFrom>::is_signed) {
   //      // To unsigned from signed
   //      // the zero comparison is done signed.  If number is negative, then the results of the max comparison are unspecified, but we don't care because it's not undefined and any value true or false will lead to the same answer since the zero comparison was false.
   //      // For the max comparison, if both operands are the same size, then number will be converted to the unsigned type, which will be fine since we already checked that it wasn't zero
   //      // For the max comparison, if one operand is bigger, then both operands will be converted to that type and the result will not have unspecified behavior
   //      return 0 <= number && number <= std::numeric_limits<TTo>::max();
   //   } else {
   //      // To unsigned from unsigned
   //      // both are unsigned, so both will be upconverted to the biggest data type and then compared.  There is no undefined or unspecified behavior here
   //      return number <= std::numeric_limits<TTo>::max();
   //   }
   //}
}
#if defined(__clang__) || defined(__GNUC__) // compiler
#elif defined(_MSC_VER) /* compiler type */
#pragma warning(pop)
#else // compiler
#error compiler not recognized
#endif // compiler

enum AttributeTypeCore { OrdinalCore = 0, NominalCore = 1};

// there doesn't seem to be a reasonable upper bound for how high you can set the k_cCompilerOptimizedTargetStatesMax value.  The bottleneck seems to be that setting it too high increases compile time and module size
// this is how much the runtime speeds up if you compile it with hard coded vector sizes
// 200 => 2.65%
// 32  => 3.28%
// 16  => 5.12%
// 8   => 5.34%
// 4   => 8.31%
// TODO: increase this up to something like 16.  I have decreased it to 2 in order to make compiling more efficient, and so that I regularily test the runtime looped version of our code
// TODO: BUT DON'T CHANGE THIS VALUE FROM 2 UNTIL WE HAVE RESOLVED THE ISSUE OF Removing countCompilerClassificationTargetStates from the main template of SegmentedRegion (using a template on just the Add function would be ok)
constexpr ptrdiff_t k_cCompilerOptimizedTargetStatesMax = 3;
static_assert(2 <= k_cCompilerOptimizedTargetStatesMax, "we special case binary classification to have only 1 output.  If we remove the compile time optimization for the binary class state then we would output model files with two values instead of our special case 1");

typedef size_t StorageDataTypeCore;

// we get a signed/unsigned mismatch if we use size_t in SegmentedRegion because we use whole numbers there
typedef ptrdiff_t ActiveDataType;

constexpr ptrdiff_t k_Regression = -1;
constexpr ptrdiff_t k_DynamicClassification = 0;
constexpr TML_INLINE bool IsRegression(ptrdiff_t cCompilerClassificationTargetStates) {
   return k_Regression == cCompilerClassificationTargetStates;
}
constexpr TML_INLINE bool IsClassification(ptrdiff_t cCompilerClassificationTargetStates) {
   return 0 <= cCompilerClassificationTargetStates;
}
constexpr TML_INLINE bool IsBinaryClassification(ptrdiff_t cCompilerClassificationTargetStates) {
#ifdef TREAT_BINARY_AS_MULTICLASS
   return false;
#else //TREAT_BINARY_AS_MULTICLASS
   return 2 == cCompilerClassificationTargetStates;
#endif //TREAT_BINARY_AS_MULTICLASS
}

constexpr TML_INLINE size_t GetVectorLengthFlatCore(ptrdiff_t cTargetStates) {
   // this will work for anything except if countCompilerClassificationTargetStates is set to DYNAMIC_CLASSIFICATION which means we should have passed in the dynamic value since DYNAMIC_CLASSIFICATION is a constant that doesn't tell us anything about the real value
#ifdef TREAT_BINARY_AS_MULTICLASS
   return cTargetStates <= 1 ? static_cast<size_t>(1) : static_cast<size_t>(cTargetStates);
#else // TREAT_BINARY_AS_MULTICLASS
   return cTargetStates <= 2 ? static_cast<size_t>(1) : static_cast<size_t>(cTargetStates);
#endif // TREAT_BINARY_AS_MULTICLASS
}
constexpr TML_INLINE size_t GetVectorLengthFlatCore(size_t cTargetStates) {
   // this will work for anything except if countCompilerClassificationTargetStates is set to DYNAMIC_CLASSIFICATION which means we should have passed in the dynamic value since DYNAMIC_CLASSIFICATION is a constant that doesn't tell us anything about the real value
#ifdef TREAT_BINARY_AS_MULTICLASS
   return cTargetStates <= 1 ? static_cast<size_t>(1) : static_cast<size_t>(cTargetStates);
#else // TREAT_BINARY_AS_MULTICLASS
   return cTargetStates <= 2 ? static_cast<size_t>(1) : static_cast<size_t>(cTargetStates);
#endif // TREAT_BINARY_AS_MULTICLASS
}

// THIS NEEDS TO BE A MACRO AND NOT AN INLINE FUNCTION -> an inline function will cause all the parameters to get resolved before calling the function
// We want any arguments to our macro to not get resolved if they are not needed at compile time so that we do less work if it's not needed
// This will effectively turn the variable into a compile time constant if it can be resolved at compile time
// The caller can put pTargetAttribute->m_cStates inside the macro call and it will be optimize away if it isn't necessary
// having compile time counts of the target state should allow for loop elimination in most cases and the restoration of SIMD instructions in places where you couldn't do so with variable loop iterations
#define GET_VECTOR_LENGTH(MACRO_countCompilerClassificationTargetStates, MACRO_countRuntimeClassificationTargetStates) (k_DynamicClassification == (MACRO_countCompilerClassificationTargetStates) ? static_cast<size_t>(MACRO_countRuntimeClassificationTargetStates) : GetVectorLengthFlatCore(MACRO_countCompilerClassificationTargetStates))

// THIS NEEDS TO BE A MACRO AND NOT AN INLINE FUNCTION -> an inline function will cause all the parameters to get resolved before calling the function
// We want any arguments to our macro to not get resolved if they are not needed at compile time so that we do less work if it's not needed
// This will effectively turn the variable into a compile time constant if it can be resolved at compile time
// having compile time counts of the target state should allow for loop elimination in most cases and the restoration of SIMD instructions in places where you couldn't do so with variable loop iterations
// this macro is legal to use when the template is set to non-classification opterations, but it will return a number that will overflow any memory allocation if anyone tries to use the value
// this macro can always be used before calling GET_VECTOR_LENGTH if you want to make other section of the code that depend on cTargetStates compile out
// TODO: use this macro more
#define GET_COUNT_CLASSIFICATION_TARGET_STATES(MACRO_countCompilerClassificationTargetStates, MACRO_countRuntimeClassificationTargetStates) ((MACRO_countCompilerClassificationTargetStates) < 0 ? std::numeric_limits<size_t>::max() : (0 == (MACRO_countCompilerClassificationTargetStates) ? static_cast<size_t>(MACRO_countRuntimeClassificationTargetStates) : static_cast<size_t>(MACRO_countCompilerClassificationTargetStates)))

// THIS NEEDS TO BE A MACRO AND NOT AN INLINE FUNCTION -> an inline function will cause all the parameters to get resolved before calling the function
// We want any arguments to our macro to not get resolved if they are not needed at compile time so that we do less work if it's not needed
// This will effectively turn the variable into a compile time constant if it can be resolved at compile time
// having compile time counts of the target state should allow for loop elimination in most cases and the restoration of SIMD instructions in places where you couldn't do so with variable loop iterations
// TODO: use this macro more
#define GET_ATTRIBUTE_COMBINATION_DIMENSIONS(MACRO_countCompilerDimensions, MACRO_countRuntimeDimensions) ((MACRO_countCompilerDimensions) <= 0 ? static_cast<size_t>(MACRO_countRuntimeDimensions) : static_cast<size_t>(MACRO_countCompilerDimensions))

template<typename T>
constexpr size_t CountBitsRequiredCore(T cBitsMax) {
   // this is a bit inefficient when called in the runtime, but we don't call it anywhere that's important performance wise.
   return 0 == cBitsMax ? 0 : 1 + CountBitsRequiredCore<T>(cBitsMax / 2);
}
template<typename T>
constexpr size_t CountBitsRequiredPositiveMax() {
   return CountBitsRequiredCore(std::numeric_limits<T>::max());
}

constexpr size_t k_cBitsForSizeTCore = CountBitsRequiredPositiveMax<size_t>();
// it's impossible for us to have more than k_cDimensionsMax dimensions.  Even if we had the minimum number of states per variable (two), then we would have 2^N memory spaces at our binning step, and that would exceed our memory size if it's greater than the number of bits allowed in a size_t, so on a 64 bit machine, 64 dimensions is a hard maximum.  We can subtract one bit safely, since we know that the rest of our program takes some memory, denying the full 64 bits of memory available.  This extra bit is very helpful since we can then set the 64th bit without overflowing it inside loops and other places
// TODO : we should check at startup if there are equal to or less than these number of dimensions, otherwise return an error.  I don't think we want to wait until we try allocating the memory to discover that we can't do it
// TODO : we can restrict the dimensionatlity even more because BinnedBuckets aren't 1 byte, so we can see how many would fit into memory.  This isn't a big deal, but it could be nice if we generate static code to handle every possible valid dimension value
constexpr size_t k_cDimensionsMax = k_cBitsForSizeTCore - 1;
static_assert(k_cDimensionsMax < k_cBitsForSizeTCore, "reserve the highest bit for bit manipulation space");

constexpr size_t k_cBitsForStorageType = CountBitsRequiredPositiveMax<StorageDataTypeCore>();
constexpr size_t k_cCountItemsBitPackedMax = k_cBitsForStorageType; // if each item is a bit, then the number of items will equal the number of bits

constexpr TML_INLINE size_t GetCountItemsBitPacked(const size_t cBits) {
   return k_cBitsForStorageType / cBits;
}
constexpr TML_INLINE size_t GetCountBits(const size_t cItemsBitPacked) {
   return k_cBitsForStorageType / cItemsBitPacked;
}
constexpr TML_INLINE size_t GetNextCountItemsBitPacked(const size_t cItemsBitPackedPrev) {
   // for 64 bits, the progression is: 64,32,21,16, 12,10,9,8,7,6,5,4,3,2,1
   // for 32 bits, the progression is: 32,16,10,8,6,5,4,3,2,1 [which are all included in 64 bits]
   return k_cBitsForStorageType / ((k_cBitsForStorageType / cItemsBitPackedPrev) + 1);
}

// TODO : also check for places where to convert a size_t into a ptrdiff_t and check for overflow there throughout our code
// TODO : there are many places that could be overflowing multiplication.  We need to look for places where this might happen
constexpr TML_INLINE bool IsMultiplyError(size_t num1, size_t num2) {
   // algebraically, we want to know if this is true: std::numeric_limits<size_t>::max() + 1 <= num1 * num2
   // which can be turned into: (std::numeric_limits<size_t>::max() + 1 - num1) / num1 + 1 <= num2
   // which can be turned into: (std::numeric_limits<size_t>::max() + 1 - num1) / num1 < num2
   // which can be turned into: (std::numeric_limits<size_t>::max() - num1 + 1) / num1 < num2
   // which works if num1 == 1, but does not work if num1 is zero, so check for zero first

   // it will never overflow if num1 is zero
   return 0 != num1 && ((std::numeric_limits<size_t>::max() - num1 + 1) / num1 < num2);
}

// TODO : keep this constant, but make it global and compile out the costs... we want to document that it's possible and how, but we have tested it and found it's worse
static constexpr int k_iZeroResidual = -1;

#endif // TRANSPARENT_ML_CORE_INTERNAL_H
