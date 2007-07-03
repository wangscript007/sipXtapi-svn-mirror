//  
// Copyright (C) 2007 SIPfoundry Inc. 
// Licensed by SIPfoundry under the LGPL license. 
//  
// Copyright (C) 2007 SIPez LLC. 
// Licensed to SIPfoundry under a Contributor Agreement. 
//  
// $$ 
////////////////////////////////////////////////////////////////////////////// 

// Author: Alexander Chemeris <Alexander DOT Chemeris AT SIPez DOT com>

#ifndef _MpDspUtils_h_
#define _MpDspUtils_h_

/// Switch between fixed and floating point math.
#define MP_FIXED_POINT
//#undef  MP_FIXED_POINT

// SYSTEM INCLUDES
// APPLICATION INCLUDES
#include <os/OsStatus.h>
#include <os/OsIntTypes.h>
#ifdef MP_FIXED_POINT // [
#  include <math.h>
#endif // MP_FIXED_POINT ]

// DEFINES
#define MPF_SIGN_BIT16  (1<<15)
#define MPF_SIGN_BIT32  (1<<31)

// MACROS
#ifdef MP_FIXED_POINT // [
#  define MPF_FLOAT(num, intLength, fracLength) (floor((num)*(float)(1<<(fracLength))))
#else  // MP_FIXED_POINT ][
#  define MPF_FLOAT(num, intLength, fracLength) (num)
#endif // MP_FIXED_POINT ]

#define MPF_STEP(intLength, fracLength)    MPF_FLOAT(1.0/(1<<(fracLength)), (intLength), (fracLength))
#define MPF_MAX(intLength, fracLength)     MPF_FLOAT((1<<(intLength))-1.0/(1<<(fracLength)), (intLength), (fracLength))

  /// Saturate \p val to [-maxval;maxval] range.
  /**
  *  @note This does not touch type of the data.
  *  @note DSPs are usually saturate to [-maxval-1;maxval] range, but this will
  *        lead to problems when negating negative saturated value. That is
  *        -(-maxval-1) may wrap to 0. So, we saturate to [-maxval;maxval] range.
  */
#define MPF_SATURATE(val, maxval) ((val)>=(maxval) ? (maxval) : (val)<-(maxval) ? -(maxval) : (val))
  /// Saturate \p val to fit 16-bit signed integer.
  /**
  *  @see MPF_SATURATE()
  */
#define MPF_SATURATE16(val) MPF_SATURATE(val, INT16_MAX)
  /// Saturate \p val to fit 32-bit signed integer. This does not touch type of the data.
  /**
  *  @see MPF_SATURATE()
  */
#define MPF_SATURATE32(val) MPF_SATURATE(val, INT32_MAX)

  /// Return 16 bit integer.
  /**
  *  In debug mode this will be used to check values. So, use it when appropriate.
  */
#define MPF_EXTRACRT16(val) ((int16_t)(val))

// EXTERNAL FUNCTIONS
// EXTERNAL VARIABLES
// CONSTANTS
// STRUCTS
// TYPEDEFS
// FORWARD DECLARATIONS

/**
*  @brief Class for generic DSP functions.
*
*  This is utils class, containing only static functions. It provide all DSP
*  processing primitives we need, and meant to be good optimized. In fact,
*  we want this to be only place, which will contain hardware specific
*  speed optimizations. If you could not find what you need here - feel free
*  to add it. But do not forget to add generic C-version of each function,
*  so it will be available if there is no optimized version.
*
*  <H3>Namig conventions.</H3>
*
*  If function ends with \c I it is in-place function. That is its destination
*  is one of source vectors (usually second). If function ends with \c Gain or 
*  \c Att and its source is integers, it is scaling function. That is it scale
*  source vector up or down (gain or attenuate, respectively) before operation.
*  Functions, where source vector is float never scale values, even with \c Gain
*  or \c Att suffix. Their \c Gain and \c Att versions are provided to simplify
*  writing of universal floating/fixed-point code.
*
*  <H3>Saturation.</H3>
*
*  All arithmetic functions saturate result if destination is integer value
*  (that is for 8-bit unsigned data type <tt>200+200=255</tt>). Functions do not
*  saturate result if destination is floating point.
*
*  <H3>Unittest coverage.</H3>
*
*  All functions should be covered by unittests. If you implemented optimized
*  version of some function, run unittests to check does your optimized version
*  behave as expected. If unittests does not cover all paths inside your
*  optimized function, add or modify unittests to cover all of them and check
*  does your function behave the same as original one. If optimized version
*  behave differently from original function and this is wanted behaviour,
*  this is different function in fact. And so you should give it different name.
*  When creating new function, do not forget to provide clean C implementation
*  for convenience.
*
*  @warning Please, keep all methods of this class static and stateless!
*/
class MpDspUtils
{
/* //////////////////////////////// PUBLIC //////////////////////////////// */
public:

/* ========================= Arithmetic Functions ========================= */
///@name Arithmetic Functions
//@{

     /// Return (a+b) saturated (16-bit).
   static inline
   int16_t add(int16_t a, int16_t b);

     /// Perform (a+=b) saturated (16-bit).
   static inline
   void add_I(int16_t &a, int16_t b);

     /// Return (a+b) saturated (32-bit).
   static inline
   int32_t add(int32_t a, int32_t b);

     /// Perform (a+=b) saturated (32-bit).
   static inline
   void add_I(int32_t &a, int32_t b);

#ifndef MP_FIXED_POINT // [

     /// Return (a+b) NOT saturated (float).
   static inline
   float add(float a, float b);

     /// Perform (a+=b) NOT saturated (float).
   static inline
   void add_I(float &a, float b);

#endif // !MP_FIXED_POINT ]

//@}

/* ================== Vector Logical and Shift Functions ================== */
///@name Vector Logical and Shift Functions
//@{


//@}

/* ====================== Vector Arithmetic Functions ===================== */
///@name Vector Arithmetic Functions
//@{

#ifdef MP_FIXED_POINT // [

     /// Add source vector to accumulator.
   static OsStatus add_I(const int16_t *pSrc1, int32_t *pSrc2Dst, int dataLength);

     /// Gain source vector by 2^src1ScaleFactor factor and add it to accumulator.
   static OsStatus add_IGain(const int16_t *pSrc1, int32_t *pSrc2Dst, int dataLength, unsigned src1ScaleFactor);

     /// Attenuate source vector by 2^src1ScaleFactor factor and add it to accumulator.
   static OsStatus add_IAtt(const int16_t *pSrc1, int32_t *pSrc2Dst, int dataLength, unsigned src1ScaleFactor);

     /// Multiply source vector by given \p val and add it to accumulator.
   static OsStatus addMul_I(const int16_t *pSrc1, int16_t val, int32_t *pSrc2Dst, int dataLength);

#else  // MP_FIXED_POINT ][

     /// Add source vector to accumulator.
   static OsStatus add_I(const int16_t *pSrc1, float *pSrc2Dst, int dataLength);

     /// Stub for fixed-point mode compatibility. It DOES NOT gain values.
   static inline OsStatus add_IGain(const int16_t *pSrc1, float *pSrc2Dst, int dataLength, unsigned src1ScaleFactor)
   {
      return add_I(pSrc1, pSrc2Dst, dataLength);
   }

     /// Stub for fixed-point mode compatibility. It DOES NOT attenuate values.
   static inline OsStatus add_IAtt(const int16_t *pSrc1, float *pSrc2Dst, int dataLength, unsigned src1ScaleFactor)
   {
      return add_I(pSrc1, pSrc2Dst, dataLength);
   }

     /// Multiply source vector by given \p val and add it to accumulator.
   static OsStatus addMul_I(const int16_t *pSrc1, float val, float *pSrc2Dst, int dataLength);

#endif // MP_FIXED_POINT ]

//@}

/* ====================== Vector Conversion Functions ===================== */
///@name Vector Conversion Functions
//@{

#ifdef MP_FIXED_POINT // [

     /// Convert vector of 32-bit integers to 16-bit integers.
   static OsStatus convert(const int32_t *pSrc, int16_t *pDst, int dataLength);

     /// Gain vector of 32-bit integers and convert them to 16-bit integers.
   static OsStatus convert_Gain(const int32_t *pSrc, int16_t *pDst, int dataLength, unsigned srcScaleFactor);

     /// Attenuate vector of 32-bit integers and convert them to 16-bit integers.
   static OsStatus convert_Att(const int32_t *pSrc, int16_t *pDst, int dataLength, unsigned srcScaleFactor);

#else  // MP_FIXED_POINT ][

     /// Convert type of source vector.
   static OsStatus convert(const float *pSrc, int16_t *pDst, int dataLength);

     /// Stub for fixed-point mode compatibility. It DOES NOT gains values.
   static inline OsStatus convert_Gain(const float *pSrc, int16_t *pDst, int dataLength, unsigned srcScaleFactor)
   {
      return convert(pSrc, pDst, dataLength);
   }

     /// Stub for fixed-point mode compatibility. It DOES NOT attenuate values.
   static inline OsStatus convert_Att(const float *pSrc, int16_t *pDst, int dataLength, unsigned srcScaleFactor)
   {
      return convert(pSrc, pDst, dataLength);
   }

#endif // MP_FIXED_POINT ]

//@}

};

/* ============================ INLINE METHODS ============================ */

#include <mp/MpDspUtilsSum.h>

#endif  // _MpDspUtils_h_
