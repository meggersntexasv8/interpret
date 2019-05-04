// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <ebm@koch.ninja>

#ifndef MULTI_DIMENSIONAL_TRAINING_H
#define MULTI_DIMENSIONAL_TRAINING_H

#include <type_traits> // std::is_pod
#include <assert.h>
#include <stddef.h> // size_t, ptrdiff_t

#include "EbmInternal.h" // TML_INLINE
#include "SegmentedRegion.h"
#include "EbmStatistics.h"
#include "CachedThreadResources.h"
#include "AttributeInternal.h"
#include "SamplingWithReplacement.h"
#include "BinnedBucket.h"

// TODO: in this file handle errors from SetCountDivisions, EnsureValueCapacity


#ifndef NDEBUG

static void PrintBinary(size_t term) {
   bool isZero = false;
   size_t bit = static_cast<size_t>(1) << (k_cBitsForSizeTCore - 1);
   do {
      if(0 == (term & bit)) {
         if(isZero) {
            printf("0");
         } else {
            printf(" ");
         }
      } else {
         isZero = true;
         printf("1");
      }
      bit >>= 1;
   } while(0 != bit);
}

template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
void GetTotalsDebugSlow(const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const AttributeCombinationCore * const pAttributeCombination, const size_t * const aiStart, const size_t * const aiLast, const size_t cTargetStates, BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pRet) {
   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
   size_t aiDimensions[k_cDimensionsMax];

   size_t iBin = 0;
   size_t valueMultipleInitialize = 1;
   for(size_t iDimensionInitialize = 0; iDimensionInitialize < cDimensions; ++iDimensionInitialize) {
      size_t cStates = pAttributeCombination->m_AttributeCombinationEntry[iDimensionInitialize].m_pAttribute->m_cStates;
      iBin += aiStart[iDimensionInitialize] * valueMultipleInitialize;
      valueMultipleInitialize *= cStates;

      assert(aiStart[iDimensionInitialize] < cStates);
      assert(aiLast[iDimensionInitialize] < cStates);
      assert(aiStart[iDimensionInitialize] <= aiLast[iDimensionInitialize]);

      aiDimensions[iDimensionInitialize] = aiStart[iDimensionInitialize];
   }

   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);
   pRet->template Zero<countCompilerClassificationTargetStates>(cTargetStates);

   while(true) {
      const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aBinnedBuckets, iBin);

      pRet->template Add<countCompilerClassificationTargetStates>(*pBinnedBucket, cTargetStates);

      size_t iDimension = 0;
      size_t valueMultipleLoop = 1;
      while(aiDimensions[iDimension] == aiLast[iDimension]) {
         assert(aiStart[iDimension] <= aiLast[iDimension]);
         iBin -= (aiLast[iDimension] - aiStart[iDimension]) * valueMultipleLoop;

         size_t cStates = pAttributeCombination->m_AttributeCombinationEntry[iDimension].m_pAttribute->m_cStates;
         valueMultipleLoop *= cStates;

         aiDimensions[iDimension] = aiStart[iDimension];
         ++iDimension;
         if(iDimension == cDimensions) {
            return;
         }
      }
      ++aiDimensions[iDimension];
      iBin += valueMultipleLoop;
   }
}

template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
void CompareTotalsDebug(const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const AttributeCombinationCore * const pAttributeCombination, const size_t * const aiPoint, const size_t directionVector, const size_t cTargetStates, BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pRet) {
   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);

   size_t aiStart[k_cDimensionsMax];
   size_t aiLast[k_cDimensionsMax];
   size_t directionVectorDestroy = directionVector;
   for(size_t iDimensionDebug = 0; iDimensionDebug < pAttributeCombination->m_cAttributes; ++iDimensionDebug) {
      size_t cStates = pAttributeCombination->m_AttributeCombinationEntry[iDimensionDebug].m_pAttribute->m_cStates;
      if(UNPREDICTABLE(0 != (1 & directionVectorDestroy))) {
         aiStart[iDimensionDebug] = aiPoint[iDimensionDebug] + 1;
         aiLast[iDimensionDebug] = cStates - 1;
      } else {
         aiStart[iDimensionDebug] = 0;
         aiLast[iDimensionDebug] = aiPoint[iDimensionDebug];
      }
      directionVectorDestroy >>= 1;
   }

   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pComparison = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesPerBinnedBucket));
   if(nullptr == pComparison) {
      exit(1);
   }
   GetTotalsDebugSlow<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, aiLast, cTargetStates, pComparison);
   assert(pRet->cCasesInBucket == pComparison->cCasesInBucket);

   free(pComparison);
}

#endif // NDEBUG


//struct CurrentIndexAndCountStates {
//   size_t iCur;
//   // copy cStates to our local stack since we'll be referring to them often and our stack is more compact in cache and less all over the place AND not shared between CPUs
//   size_t cStates;
//};
//
//template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
//void BuildFastTotals(BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const size_t cTargetStates, const AttributeCombinationCore * const pAttributeCombination) {
//   // TODO: sort our N-dimensional combinations at program startup so that the longest dimension is first!  That way we can more efficiently walk through contiguous memory better in this function!
//
//   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
//   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates));
//
//#ifndef NDEBUG
//   // make a copy of the original binned buckets for debugging purposes
//   size_t cTotalBucketsDebug = 1;
//   for(size_t iDimensionDebug = 0; iDimensionDebug < pAttributeCombination->m_cAttributes; ++iDimensionDebug) {
//      cTotalBucketsDebug *= pAttributeCombination->m_AttributeCombinationEntry[iDimensionDebug].m_pAttribute->m_cStates;
//   }
//   const size_t cBytesBufferDebug = cTotalBucketsDebug * cBytesPerBinnedBucket;
//   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesBufferDebug + cBytesPerBinnedBucket));
//   if(nullptr == aBinnedBucketsDebugCopy) {
//      exit(1);
//   }
//   memcpy(aBinnedBucketsDebugCopy, aBinnedBuckets, cBytesBufferDebug);
//
//   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pDebugBucket = GetBinnedBucketByIndex<IsRegression(IsRegression(countCompilerClassificationTargetStates))>(cBytesPerBinnedBucket, aBinnedBucketsDebugCopy, cTotalBucketsDebug);
//#endif // NDEBUG
//
//   assert(0 < cDimensions);
//
//   CurrentIndexAndCountStates currentIndexAndCountStates[k_cDimensionsMax];
//   const CurrentIndexAndCountStates * const pCurrentIndexAndCountStatesEnd = &currentIndexAndCountStates[cDimensions];
//   const AttributeCombinationCore::AttributeCombinationEntry * pAttributeCombinationEntry = &pAttributeCombination->m_AttributeCombinationEntry[0];
//   for(CurrentIndexAndCountStates * pCurrentIndexAndCountStatesInitialize = currentIndexAndCountStates; pCurrentIndexAndCountStatesEnd != pCurrentIndexAndCountStatesInitialize; ++pCurrentIndexAndCountStatesInitialize, ++pAttributeCombinationEntry) {
//      pCurrentIndexAndCountStatesInitialize->iCur = 0;
//      assert(2 <= pAttributeCombinationEntry->m_pAttribute->m_cStates);
//      pCurrentIndexAndCountStatesInitialize->cStates = pAttributeCombinationEntry->m_pAttribute->m_cStates;
//   }
//
//   static_assert(k_cDimensionsMax < k_cBitsForSizeT, "reserve the highest bit for bit manipulation space");
//   assert(cDimensions < k_cBitsForSizeT);
//   const size_t permuteVectorEnd = static_cast<size_t>(1) << cDimensions;
//   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pBinnedBucket = aBinnedBuckets;
//
//   goto skip_intro;
//
//   CurrentIndexAndCountStates * pCurrentIndexAndCountStates;
//   size_t iBucket;
//   while(true) {
//      pCurrentIndexAndCountStates->iCur = iBucket;
//      // we're walking through all buckets, so just move to the next one in the flat array, with the knoledge that we'll figure out it's multi-dimenional index below
//      pBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucket, 1);
//
//   skip_intro:
//
//      // TODO : I think this code below can be made more efficient by storing the sum of all the items in the 0th dimension where we don't subtract the 0th dimension then when we go to sum up the next set we can eliminate half the work!
//
//      size_t permuteVector = 1;
//      do {
//         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTargetBinnedBucket = pBinnedBucket;
//         bool bPositive = false;
//         size_t permuteVectorDestroy = permuteVector;
//         ptrdiff_t multiplyDimension = -1;
//         pCurrentIndexAndCountStates = &currentIndexAndCountStates[0];
//         do {
//            if(0 != (1 & permuteVectorDestroy)) {
//               if(0 == pCurrentIndexAndCountStates->iCur) {
//                  goto skip_combination;
//               }
//               pTargetBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pTargetBinnedBucket, multiplyDimension);
//               bPositive = !bPositive;
//            }
//            // TODO: can we eliminate the multiplication by storing the multiples instead of hte cStates?
//            multiplyDimension *= pCurrentIndexAndCountStates->cStates;
//            ++pCurrentIndexAndCountStates;
//            permuteVectorDestroy >>= 1;
//         } while(0 != permuteVectorDestroy);
//         if(bPositive) {
//            pBinnedBucket->Add(*pTargetBinnedBucket, cTargetStates);
//         } else {
//            pBinnedBucket->Subtract(*pTargetBinnedBucket, cTargetStates);
//         }
//      skip_combination:
//         ++permuteVector;
//      } while(permuteVectorEnd != permuteVector);
//
//#ifndef NDEBUG
//      size_t aiStart[k_cDimensionsMax];
//      size_t aiLast[k_cDimensionsMax];
//      for(size_t iDebugDimension = 0; iDebugDimension < cDimensions; ++iDebugDimension) {
//         aiStart[iDebugDimension] = 0;
//         aiLast[iDebugDimension] = currentIndexAndCountStates[iDebugDimension].iCur;
//      }
//      GetTotalsDebugSlow<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBucketsDebugCopy, pAttributeCombination, aiStart, aiLast, cTargetStates, pDebugBucket);
//      assert(pDebugBucket->cCasesInBucket == pBinnedBucket->cCasesInBucket);
//#endif
//
//      pCurrentIndexAndCountStates = &currentIndexAndCountStates[0];
//      while(true) {
//         iBucket = pCurrentIndexAndCountStates->iCur + 1;
//         assert(iBucket <= pCurrentIndexAndCountStates->cStates);
//         if(iBucket != pCurrentIndexAndCountStates->cStates) {
//            break;
//         }
//         pCurrentIndexAndCountStates->iCur = 0;
//         ++pCurrentIndexAndCountStates;
//         if(pCurrentIndexAndCountStatesEnd == pCurrentIndexAndCountStates) {
//#ifndef NDEBUG
//            free(aBinnedBucketsDebugCopy);
//#endif
//            return;
//         }
//      }
//   }
//}
//





//struct CurrentIndexAndCountStates {
//   ptrdiff_t multipliedIndexCur;
//   ptrdiff_t multipleTotal;
//};
//
//template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
//void BuildFastTotals(BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const size_t cTargetStates, const AttributeCombinationCore * const pAttributeCombination) {
//   // TODO: sort our N-dimensional combinations at program startup so that the longest dimension is first!  That way we can more efficiently walk through contiguous memory better in this function!
//
//   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
//   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates));
//
//#ifndef NDEBUG
//   // make a copy of the original binned buckets for debugging purposes
//   size_t cTotalBucketsDebug = 1;
//   for(size_t iDimensionDebug = 0; iDimensionDebug < pAttributeCombination->m_cAttributes; ++iDimensionDebug) {
//      cTotalBucketsDebug *= pAttributeCombination->m_AttributeCombinationEntry[iDimensionDebug].m_pAttribute->m_cStates;
//   }
//   const size_t cBytesBufferDebug = cTotalBucketsDebug * cBytesPerBinnedBucket;
//   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesBufferDebug + cBytesPerBinnedBucket));
//   if(nullptr == aBinnedBucketsDebugCopy) {
//      exit(1);
//   }
//   memcpy(aBinnedBucketsDebugCopy, aBinnedBuckets, cBytesBufferDebug);
//
//   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pDebugBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aBinnedBucketsDebugCopy, cTotalBucketsDebug);
//#endif // NDEBUG
//
//   assert(0 < cDimensions);
//
//   CurrentIndexAndCountStates currentIndexAndCountStates[k_cDimensionsMax];
//   const CurrentIndexAndCountStates * const pCurrentIndexAndCountStatesEnd = &currentIndexAndCountStates[cDimensions];
//   const AttributeCombinationCore::AttributeCombinationEntry * pAttributeCombinationEntry = &pAttributeCombination->m_AttributeCombinationEntry[0];
//   ptrdiff_t multipleTotalInitialize = -1;
//   for(CurrentIndexAndCountStates * pCurrentIndexAndCountStatesInitialize = currentIndexAndCountStates; pCurrentIndexAndCountStatesEnd != pCurrentIndexAndCountStatesInitialize; ++pCurrentIndexAndCountStatesInitialize, ++pAttributeCombinationEntry) {
//      pCurrentIndexAndCountStatesInitialize->multipliedIndexCur = 0;
//      assert(2 <= pAttributeCombinationEntry->m_pAttribute->m_cStates);
//      multipleTotalInitialize *= static_cast<ptrdiff_t>(pAttributeCombinationEntry->m_pAttribute->m_cStates);
//      pCurrentIndexAndCountStatesInitialize->multipleTotal = multipleTotalInitialize;
//   }
//
//   static_assert(k_cDimensionsMax < k_cBitsForSizeT, "reserve the highest bit for bit manipulation space");
//   assert(cDimensions < k_cBitsForSizeT);
//   const size_t permuteVectorEnd = static_cast<size_t>(1) << cDimensions;
//   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pBinnedBucket = aBinnedBuckets;
//
//   goto skip_intro;
//
//   CurrentIndexAndCountStates * pCurrentIndexAndCountStates;
//   ptrdiff_t multipliedIndexCur;
//   while(true) {
//      pCurrentIndexAndCountStates->multipliedIndexCur = multipliedIndexCur;
//      // we're walking through all buckets, so just move to the next one in the flat array, with the knoledge that we'll figure out it's multi-dimenional index below
//      pBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucket, 1);
//
//   skip_intro:
//
//      // TODO : I think this code below can be made more efficient by storing the sum of all the items in the 0th dimension where we don't subtract the 0th dimension then when we go to sum up the next set we can eliminate half the work!
//
//      size_t permuteVector = 1;
//      do {
//         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTargetBinnedBucket = pBinnedBucket;
//         bool bPositive = false;
//         size_t permuteVectorDestroy = permuteVector;
//         ptrdiff_t multipleTotal = -1;
//         pCurrentIndexAndCountStates = &currentIndexAndCountStates[0];
//         do {
//            if(0 != (1 & permuteVectorDestroy)) {
//               // even though our index is multiplied by the total states until this point, we only care about the zero state, and zero multiplied by anything is zero
//               if(0 == pCurrentIndexAndCountStates->multipliedIndexCur) {
//                  goto skip_combination;
//               }
//               pTargetBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pTargetBinnedBucket, multipleTotal);
//               bPositive = !bPositive;
//            }
//            multipleTotal = pCurrentIndexAndCountStates->multipleTotal;
//            ++pCurrentIndexAndCountStates;
//            permuteVectorDestroy >>= 1;
//         } while(0 != permuteVectorDestroy);
//         if(bPositive) {
//            pBinnedBucket->Add(*pTargetBinnedBucket, cTargetStates);
//         } else {
//            pBinnedBucket->Subtract(*pTargetBinnedBucket, cTargetStates);
//         }
//      skip_combination:
//         ++permuteVector;
//      } while(permuteVectorEnd != permuteVector);
//
//#ifndef NDEBUG
//      size_t aiStart[k_cDimensionsMax];
//      size_t aiLast[k_cDimensionsMax];
//      ptrdiff_t multipleTotalDebug = -1;
//      for(size_t iDebugDimension = 0; iDebugDimension < cDimensions; ++iDebugDimension) {
//         aiStart[iDebugDimension] = 0;
//         aiLast[iDebugDimension] = static_cast<size_t>(currentIndexAndCountStates[iDebugDimension].multipliedIndexCur / multipleTotalDebug);
//         multipleTotalDebug = currentIndexAndCountStates[iDebugDimension].multipleTotal;
//      }
//      GetTotalsDebugSlow<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBucketsDebugCopy, pAttributeCombination, aiStart, aiLast, cTargetStates, pDebugBucket);
//      assert(pDebugBucket->cCasesInBucket == pBinnedBucket->cCasesInBucket);
//#endif
//
//      pCurrentIndexAndCountStates = &currentIndexAndCountStates[0];
//      ptrdiff_t multipleTotal = -1;
//      while(true) {
//         multipliedIndexCur = pCurrentIndexAndCountStates->multipliedIndexCur + multipleTotal;
//         multipleTotal = pCurrentIndexAndCountStates->multipleTotal;
//         if(multipliedIndexCur != multipleTotal) {
//            break;
//         }
//         pCurrentIndexAndCountStates->multipliedIndexCur = 0;
//         ++pCurrentIndexAndCountStates;
//         if(pCurrentIndexAndCountStatesEnd == pCurrentIndexAndCountStates) {
//#ifndef NDEBUG
//            free(aBinnedBucketsDebugCopy);
//#endif
//            return;
//         }
//      }
//   }
//}
//








template<bool bRegression>
struct FastTotalState2 {
   BinnedBucket<bRegression> * pDimensionalCur;
   BinnedBucket<bRegression> * pDimensionalWrap;
   BinnedBucket<bRegression> * pDimensionalFirst;
   size_t iCur;
   size_t cStates;
};

template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
void BuildFastTotals(BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const size_t cTargetStates, const AttributeCombinationCore * const pAttributeCombination, size_t cTotalBuckets
#ifndef NDEBUG
   , const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy, const unsigned char * const aBinnedBucketsEndDebug
#endif // NDEBUG
) {
   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);

   FastTotalState2<IsRegression(countCompilerClassificationTargetStates)> fastTotalState[k_cDimensionsMax];
   const FastTotalState2<IsRegression(countCompilerClassificationTargetStates)> * const pFastTotalStateEnd = &fastTotalState[cDimensions];
   {
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pDimensionalBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aBinnedBuckets, cTotalBuckets);

      FastTotalState2<IsRegression(countCompilerClassificationTargetStates)> * pFastTotalStateInitialize = fastTotalState;
      const AttributeCombinationCore::AttributeCombinationEntry * pAttributeCombinationEntry = &pAttributeCombination->m_AttributeCombinationEntry[0];
      size_t multiply = 1;
      assert(0 < cDimensions);
      do {
         ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pDimensionalBucket, aBinnedBucketsEndDebug);

         size_t cStates = pAttributeCombinationEntry->m_pAttribute->m_cStates;
         assert(2 <= cStates);

         pFastTotalStateInitialize->iCur = 0;
         pFastTotalStateInitialize->cStates = cStates;

         pFastTotalStateInitialize->pDimensionalFirst = pDimensionalBucket;
         pFastTotalStateInitialize->pDimensionalCur = pDimensionalBucket;
         pDimensionalBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pDimensionalBucket, multiply);

#ifndef NDEBUG
         ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pDimensionalBucket, -1), aBinnedBucketsEndDebug);
         for(BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pDimensionalCur = pFastTotalStateInitialize->pDimensionalCur; pDimensionalBucket != pDimensionalCur; pDimensionalCur = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pDimensionalCur, 1)) {
            pDimensionalCur->template AssertZero<countCompilerClassificationTargetStates>(cTargetStates);
         }
#endif // NDEBUG

         // TODO : we don't need either the first or the wrap values since they are the next ones in the list.. we may need to populate one item past the end and make the list one larger
         pFastTotalStateInitialize->pDimensionalWrap = pDimensionalBucket;

         multiply *= cStates;

         ++pAttributeCombinationEntry;
         ++pFastTotalStateInitialize;
      } while(LIKELY(pFastTotalStateEnd != pFastTotalStateInitialize));
   }

#ifndef NDEBUG
   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pDebugBucket = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesPerBinnedBucket));
#endif //NDEBUG

   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pBinnedBucket = aBinnedBuckets;

   while(true) {
      ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pBinnedBucket, aBinnedBucketsEndDebug);

      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pAddPrev = pBinnedBucket;
      for(ptrdiff_t iDimension = cDimensions - 1; 0 <= iDimension ; --iDimension) {
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pAddTo = fastTotalState[iDimension].pDimensionalCur;
         pAddTo->template Add<countCompilerClassificationTargetStates>(*pAddPrev, cTargetStates);
         pAddPrev = pAddTo;
         pAddTo = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pAddTo, 1);
         if(pAddTo == fastTotalState[iDimension].pDimensionalWrap) {
            pAddTo = fastTotalState[iDimension].pDimensionalFirst;
         }
         fastTotalState[iDimension].pDimensionalCur = pAddTo;
      }
      pBinnedBucket->template Copy<countCompilerClassificationTargetStates>(*pAddPrev, cTargetStates);

#ifndef NDEBUG
      size_t aiStart[k_cDimensionsMax];
      size_t aiLast[k_cDimensionsMax];
      for(size_t iDebugDimension = 0; iDebugDimension < cDimensions; ++iDebugDimension) {
         aiStart[iDebugDimension] = 0;
         aiLast[iDebugDimension] = fastTotalState[iDebugDimension].iCur;
      }
      GetTotalsDebugSlow<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBucketsDebugCopy, pAttributeCombination, aiStart, aiLast, cTargetStates, pDebugBucket);
      assert(pDebugBucket->cCasesInBucket == pBinnedBucket->cCasesInBucket);
#endif // NDEBUG

      // we're walking through all buckets, so just move to the next one in the flat array, with the knoledge that we'll figure out it's multi-dimenional index below
      pBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucket, 1);

      FastTotalState2<IsRegression(countCompilerClassificationTargetStates)> * pFastTotalState = &fastTotalState[0];
      while(true) {
         ++pFastTotalState->iCur;
         if(LIKELY(pFastTotalState->cStates != pFastTotalState->iCur)) {
            break;
         }
         pFastTotalState->iCur = 0;

         assert(pFastTotalState->pDimensionalFirst == pFastTotalState->pDimensionalCur);
         memset(pFastTotalState->pDimensionalFirst, 0, reinterpret_cast<char *>(pFastTotalState->pDimensionalWrap) - reinterpret_cast<char *>(pFastTotalState->pDimensionalFirst));

         ++pFastTotalState;

         if(UNLIKELY(pFastTotalStateEnd == pFastTotalState)) {
#ifndef NDEBUG
            free(pDebugBucket);
#endif // NDEBUG
            return;
         }
      }
   }
}


struct CurrentIndexAndCountStates {
   ptrdiff_t multipliedIndexCur;
   ptrdiff_t multipleTotal;
};

// TODO : ALL OF THE BELOW!
//- D is the number of dimensions
//- N is the number of cases per dimension(assume all dimensions have the same number of cases for simplicity)
//- when we construct the initial D - dimensional totals, our current algorithm is N^D * 2 ^ (D - 1).We can probably reduce this to N^D * D with a lot of side memory that records the cost of going each direction
//- the above algorithm gives us small slices of work, so it probably can't help us make the next step of calculating the total regional space from a point to a corner any faster since the slices only give us 1 step away and we'd have to iterate through all the slices to get a larger region
//- we currently have one N^D memory region which allows us to calculate the total from any point to any corner in at worst 2 ^ D operations.If we had 2 ^ D memory spaces and were willing to construct them, then we could calculate the total from any point to any corner in 1 operation.If we made a second total region which had the totals from any point to the(1, 1, ..., 1, 1) corner, then we could calculate any point to corer in sqrt(2 ^ D), which is A LOT BETTER and it only takes twice as much memory.For an 8 dimensional space we would need 16 operations instead of 256!
//- to implement an algorithm that uses the(0, 0, ..., 0, 0) totals volume and the(1, 1, ..., 1, 1) volume, just see whether the input vector has more zeros or 1's and then choose the end point that is closest.
//- we can calculate the total from any arbitrary start and end point(instead of just a point to a corner) if we set the end point as the end and iterate through ALL permutations of all #'s of bits.  There doesn't seem to be any simplification that allows us to handle less than the full combinatoral exploration, even if we constructed a totals for each possible 2 ^ D corner
//- if we succeed(with extra memory) to turn the totals construction algorithm into a N^D*D algorithm, then we might be able to use that to calculate the totals dynamically at the same time that we sweep the splitting space for splits.The simplest sweep would be to look at each region from a point to each corner and choose the best split that isolates one of those corners instead of splitting at different poiints in each dimension.If we did the simplest possible thing, then our algorithm would be 2 ^ D*N^D*D OR(2 * N) ^ D*D.If we wanted the more complicated splits, then we might need to first build a totals so that we could determine the "tube totals" and then we could sweep the tube and have the costs on both sides of the split
//- IMEDIATE TASKS :
//- get point to corner working for N - dimensional to(0, 0, ..., 0, 0)
//- get splitting working for N - dimensional
//- have a look at our final dimensionality.Is the totals calculation the bottleneck, or the point to corner totals function ?
//- I think I understand the costs of all implementations of point to corner computation, so don't implement the (1,1,...,1,1) to point algorithm yet.. try implementing the more optimized totals calculation (with more memory).  After we have the optimized totals calculation, then try to re-do the splitting code to do splitting at the same time as totals calculation.  If that isn't better than our existing stuff, then optimzie the point to corner calculation code
//- implement a function that calcualtes the total of any volume using just the(0, 0, ..., 0, 0) totals ..as a debugging function.We might use this for trying out more complicated splits where we allow 2 splits on some axies

// TODO: build a pair and triple specific version of this function.  For pairs we can get ride of the pPrevious and just use the actual cell at (-1,-1) from our current cell, and we can use two loops with everything in memory [look at code above from before we incoporated the previous totals].  Triples would also benefit from pulling things out since we have low iterations of the inner loop and we can access indicies directly without additional add/subtract/bit operations.  Beyond triples, the combinatorial choices start to explode, so we should probably use this general N-dimensional code.
// TODO: after we build pair and triple specific versions of this function, we don't need to have a compiler countCompilerDimensions, since the compiler won't really be able to simpify the loops that are exploding in dimensionality
template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
void BuildFastTotalsZeroMemoryIncrease(BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const size_t cTargetStates, const AttributeCombinationCore * const pAttributeCombination
#ifndef NDEBUG
   , const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy, const unsigned char * const aBinnedBucketsEndDebug
#endif // NDEBUG
) {
   // TODO: sort our N-dimensional combinations at program startup so that the longest dimension is first!  That way we can more efficiently walk through contiguous memory better in this function!

   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);

   CurrentIndexAndCountStates currentIndexAndCountStates[k_cDimensionsMax];
   const CurrentIndexAndCountStates * const pCurrentIndexAndCountStatesEnd = &currentIndexAndCountStates[cDimensions];
   ptrdiff_t multipleTotalInitialize = -1;
   {
      CurrentIndexAndCountStates * pCurrentIndexAndCountStatesInitialize = currentIndexAndCountStates;
      const AttributeCombinationCore::AttributeCombinationEntry * pAttributeCombinationEntry = &pAttributeCombination->m_AttributeCombinationEntry[0];
      assert(0 < cDimensions);
      do {
         pCurrentIndexAndCountStatesInitialize->multipliedIndexCur = 0;
         assert(2 <= pAttributeCombinationEntry->m_pAttribute->m_cStates);
         multipleTotalInitialize *= static_cast<ptrdiff_t>(pAttributeCombinationEntry->m_pAttribute->m_cStates);
         pCurrentIndexAndCountStatesInitialize->multipleTotal = multipleTotalInitialize;
         ++pAttributeCombinationEntry;
         ++pCurrentIndexAndCountStatesInitialize;
      } while(LIKELY(pCurrentIndexAndCountStatesEnd != pCurrentIndexAndCountStatesInitialize));
   }

   // TODO: If we have a compiler cVectorLength, we could put the pPrevious object into our stack since it would have a defined size.  We could then eliminate having to access it through a pointer and we'd just access through the stack pointer
   // TODO: can we put BinnedBucket object onto the stack in other places too?
   // we reserved 1 extra space for these when we binned our buckets
   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pPrevious = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aBinnedBuckets, -multipleTotalInitialize);
   ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pPrevious, aBinnedBucketsEndDebug);

#ifndef NDEBUG
   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pDebugBucket = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesPerBinnedBucket));
   pPrevious->AssertZero();
#endif //NDEBUG

   static_assert(k_cDimensionsMax < k_cBitsForSizeTCore, "reserve the highest bit for bit manipulation space");
   assert(cDimensions < k_cBitsForSizeTCore);
   assert(2 <= cDimensions);
   const size_t permuteVectorEnd = static_cast<size_t>(1) << (cDimensions - 1);
   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pBinnedBucket = aBinnedBuckets;
   
   ptrdiff_t multipliedIndexCur0 = 0;
   const ptrdiff_t multipleTotal0 = currentIndexAndCountStates[0].multipleTotal;

   goto skip_intro;

   CurrentIndexAndCountStates * pCurrentIndexAndCountStates;
   ptrdiff_t multipliedIndexCur;
   while(true) {
      pCurrentIndexAndCountStates->multipliedIndexCur = multipliedIndexCur;

   skip_intro:
      
      // TODO: We're currently reducing the work by a factor of 2 by keeping the pPrevious values.  I think I could reduce the work by annohter factor of 2 if I maintained a 1 dimensional array of previous values for the 2nd dimension.  I think I could reduce by annohter factor of 2 by maintaininng a two dimensional space of previous values, etc..  At the end I think I can remove the combinatorial treatment by adding about the same order of memory as our existing totals space, which is a great tradeoff because then we can figure out a cell by looping N times for N dimensions instead of 2^N!
      //       After we're solved that, I think I can use the resulting intermediate work to avoid the 2^N work in the region totals function that uses our work (this is speculative)
      //       I think instead of storing the totals in the N^D space, I'll end up storing the previous values for the 1st dimension, or maybe I need to keep both.  Or maybe I can eliminate a huge amount of memory in the last dimension by doing a tiny bit of extra work.  I don't know yet.
      //       
      // TODO: before doing the above, I think I want to take what I have and extract a 2-dimensional and 3-dimensional specializations since these don't need the extra complexity.  Especially for 2-D where I don't even need to keep the previous value

      ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pBinnedBucket, aBinnedBucketsEndDebug);

      const size_t cCasesInBucket = pBinnedBucket->cCasesInBucket + pPrevious->cCasesInBucket;
      pBinnedBucket->cCasesInBucket = cCasesInBucket;
      pPrevious->cCasesInBucket = cCasesInBucket;
      for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
         const FractionalDataType sumResidualError = pBinnedBucket->aPredictionStatistics[iVector].sumResidualError + pPrevious->aPredictionStatistics[iVector].sumResidualError;
         pBinnedBucket->aPredictionStatistics[iVector].sumResidualError = sumResidualError;
         pPrevious->aPredictionStatistics[iVector].sumResidualError = sumResidualError;

         if(IsClassification(countCompilerClassificationTargetStates)) {
            const FractionalDataType sumDenominator = pBinnedBucket->aPredictionStatistics[iVector].GetSumDenominator() + pPrevious->aPredictionStatistics[iVector].GetSumDenominator();
            pBinnedBucket->aPredictionStatistics[iVector].SetSumDenominator(sumDenominator);
            pPrevious->aPredictionStatistics[iVector].SetSumDenominator(sumDenominator);
         }
      }

      size_t permuteVector = 1;
      do {
         ptrdiff_t offsetPointer = 0;
         unsigned int evenOdd = 0;
         size_t permuteVectorDestroy = permuteVector;
         // skip the first one since we preserve the total from the previous run instead of adding all the -1 values
         const CurrentIndexAndCountStates * pCurrentIndexAndCountStatesLoop = &currentIndexAndCountStates[1];
         assert(0 != permuteVectorDestroy);
         do {
            // even though our index is multiplied by the total states until this point, we only care about the zero state, and zero multiplied by anything is zero
            if(UNLIKELY(0 != ((0 == pCurrentIndexAndCountStatesLoop->multipliedIndexCur ? 1 : 0) & permuteVectorDestroy))) {
               goto skip_combination;
            }
            offsetPointer = UNPREDICTABLE(0 != (1 & permuteVectorDestroy)) ? pCurrentIndexAndCountStatesLoop[-1].multipleTotal + offsetPointer : offsetPointer;
            evenOdd ^= permuteVectorDestroy; // flip least significant bit if the dimension bit is set
            ++pCurrentIndexAndCountStatesLoop;
            permuteVectorDestroy >>= 1;
            // this (0 != permuteVectorDestroy) condition is somewhat unpredictable because for low dimensions or for low permutations it exits after just a few loops
            // it might be tempting to try and eliminate the loop by templating it and hardcoding the number of iterations based on the number of dimensions, but that would probably
            // be a bad choice because we can exit this loop early when the permutation number is low, and on average that eliminates more than half of the loop iterations
            // the cost of a branch misprediction is probably equal to one complete loop above, but we're reducing it by more than that, and keeping the code more compact by not 
            // exploding the amount of code based on the number of possible dimensions
         } while(LIKELY(0 != permuteVectorDestroy));
         ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucket, offsetPointer), aBinnedBucketsEndDebug);
         if(UNPREDICTABLE(0 != (1 & evenOdd))) {
            pBinnedBucket->Add(*GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucket, offsetPointer), cTargetStates);
         } else {
            pBinnedBucket->Subtract(*GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucket, offsetPointer), cTargetStates);
         }
      skip_combination:
         ++permuteVector;
      } while(LIKELY(permuteVectorEnd != permuteVector));

#ifndef NDEBUG
      size_t aiStart[k_cDimensionsMax];
      size_t aiLast[k_cDimensionsMax];
      ptrdiff_t multipleTotalDebug = -1;
      for(size_t iDebugDimension = 0; iDebugDimension < cDimensions; ++iDebugDimension) {
         aiStart[iDebugDimension] = 0;
         aiLast[iDebugDimension] = static_cast<size_t>((0 == iDebugDimension ? multipliedIndexCur0 : currentIndexAndCountStates[iDebugDimension].multipliedIndexCur) / multipleTotalDebug);
         multipleTotalDebug = currentIndexAndCountStates[iDebugDimension].multipleTotal;
      }
      GetTotalsDebugSlow<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBucketsDebugCopy, pAttributeCombination, aiStart, aiLast, cTargetStates, pDebugBucket);
      assert(pDebugBucket->cCasesInBucket == pBinnedBucket->cCasesInBucket);
#endif // NDEBUG

      // we're walking through all buckets, so just move to the next one in the flat array, with the knoledge that we'll figure out it's multi-dimenional index below
      pBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucket, 1);

      // TODO: we are putting storage that would exist in our array from the innermost loop into registers (multipliedIndexCur0 & multipleTotal0).  We can probably do this in many other places as well that use this pattern of indexing via an array

      --multipliedIndexCur0;
      if(LIKELY(multipliedIndexCur0 != multipleTotal0)) {
         goto skip_intro;
      }

      pPrevious->Zero(cTargetStates);
      multipliedIndexCur0 = 0;
      pCurrentIndexAndCountStates = &currentIndexAndCountStates[1];
      ptrdiff_t multipleTotal = multipleTotal0;
      while(true) {
         multipliedIndexCur = pCurrentIndexAndCountStates->multipliedIndexCur + multipleTotal;
         multipleTotal = pCurrentIndexAndCountStates->multipleTotal;
         if(LIKELY(multipliedIndexCur != multipleTotal)) {
            break;
         }

         pCurrentIndexAndCountStates->multipliedIndexCur = 0;
         ++pCurrentIndexAndCountStates;
         if(UNLIKELY(pCurrentIndexAndCountStatesEnd == pCurrentIndexAndCountStates)) {
#ifndef NDEBUG
            free(pDebugBucket);
#endif // NDEBUG
            return;
         }
      }
   }
}



struct TotalsDimension {
   size_t cIncrement;
   size_t cLast;
};

template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
void GetTotals(const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const AttributeCombinationCore * const pAttributeCombination, const size_t * const aiPoint, const size_t directionVector, const size_t cTargetStates, BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pRet
#ifndef NDEBUG
   , const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy, const unsigned char * const aBinnedBucketsEndDebug
#endif // NDEBUG
) {
   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);

   static_assert(k_cDimensionsMax < k_cBitsForSizeTCore, "reserve the highest bit for bit manipulation space");
   assert(cDimensions < k_cBitsForSizeTCore);
   assert(2 <= cDimensions);

   size_t multipleTotalInitialize = 1;
   size_t startingOffset = 0;
   const AttributeCombinationCore::AttributeCombinationEntry * pAttributeCombinationEntry = &pAttributeCombination->m_AttributeCombinationEntry[0];
   const AttributeCombinationCore::AttributeCombinationEntry * const pAttributeCombinationEntryEnd = &pAttributeCombination->m_AttributeCombinationEntry[cDimensions];
   const size_t * piPointInitialize = aiPoint;

   if(0 == directionVector) {
      // we would require a check in our inner loop below to handle the case of zero AttributeCombinationEntry items, so let's handle it separetly here instead
      assert(0 < cDimensions);
      do {
         size_t cStates = pAttributeCombinationEntry->m_pAttribute->m_cStates;
         startingOffset += multipleTotalInitialize * (*piPointInitialize);
         multipleTotalInitialize *= cStates;
         ++pAttributeCombinationEntry;
         ++piPointInitialize;
      } while(LIKELY(pAttributeCombinationEntryEnd != pAttributeCombinationEntry));
      const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aBinnedBuckets, startingOffset);
      // TODO : re-enable this check once we use the same memory region
      //ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pRet, aBinnedBucketsEndDebug);
      ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pBinnedBucket, aBinnedBucketsEndDebug);
      pRet->template Copy<countCompilerClassificationTargetStates>(*pBinnedBucket, cTargetStates);
      return;
   }

   //this is a fast way of determining the number of bits (see if the are faster algorithms.. CPU hardware or expoential shifting potentially).  We may use it in the future if we're trying to decide whether to go from (0,0,...,0,0) or (1,1,...,1,1)
   //unsigned int cBits = 0;
   //{
   //   size_t directionVectorDestroy = directionVector;
   //   while(directionVectorDestroy) {
   //      directionVectorDestroy &= (directionVectorDestroy - 1);
   //      ++cBits;
   //   }
   //}

   TotalsDimension totalsDimension[k_cDimensionsMax];
   TotalsDimension * pTotalsDimensionEnd = totalsDimension;
   {
      size_t directionVectorDestroy = directionVector;
      assert(0 < cDimensions);
      do {
         size_t cStates = pAttributeCombinationEntry->m_pAttribute->m_cStates;
         if(UNPREDICTABLE(0 != (1 & directionVectorDestroy))) {
            size_t cLast = multipleTotalInitialize * (cStates - 1);
            pTotalsDimensionEnd->cIncrement = multipleTotalInitialize * (*piPointInitialize);
            pTotalsDimensionEnd->cLast = cLast;
            multipleTotalInitialize += cLast;
            ++pTotalsDimensionEnd;
         } else {
            startingOffset += multipleTotalInitialize * (*piPointInitialize);
            multipleTotalInitialize *= cStates;
         }
         ++pAttributeCombinationEntry;
         ++piPointInitialize;
         directionVectorDestroy >>= 1;
      } while(LIKELY(pAttributeCombinationEntryEnd != pAttributeCombinationEntry));
   }
   const unsigned int cAllBits = static_cast<unsigned int>(pTotalsDimensionEnd - totalsDimension);
   assert(cAllBits < k_cBitsForSizeTCore);

   pRet->template Zero<countCompilerClassificationTargetStates>(cTargetStates);

   size_t permuteVector = 0;
   do {
      ptrdiff_t offsetPointer = startingOffset;
      size_t evenOdd = cAllBits;
      size_t permuteVectorDestroy = permuteVector;
      const TotalsDimension * pTotalsDimensionLoop = &totalsDimension[0];
      do {
         evenOdd ^= permuteVectorDestroy; // flip least significant bit if the dimension bit is set
         offsetPointer += *(UNPREDICTABLE(0 != (1 & permuteVectorDestroy)) ? &pTotalsDimensionLoop->cLast : &pTotalsDimensionLoop->cIncrement);
         permuteVectorDestroy >>= 1;
         ++pTotalsDimensionLoop;
         // TODO : this (pTotalsDimensionEnd != pTotalsDimensionLoop) condition is somewhat unpredictable since the number of dimensions is small.  Since the number of iterations will remain constant, we can use templates to move this check out of both loop to the completely non-looped outer body and then we eliminate a bunch of unpredictable branches AND a bunch of adds and a lot of other stuff.  If we allow ourselves to come at the vector from either size (0,0,...,0,0) or (1,1,...,1,1) then we only need to hardcode 63/2 loops.
      } while(LIKELY(pTotalsDimensionEnd != pTotalsDimensionLoop));
      const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pBinnedBucket = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aBinnedBuckets, offsetPointer);
      if(UNPREDICTABLE(0 != (1 & evenOdd))) {
         // TODO : re-enable this check once we use the same memory region
         //ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pRet, aBinnedBucketsEndDebug);
         ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pBinnedBucket, aBinnedBucketsEndDebug);
         pRet->template Subtract<countCompilerClassificationTargetStates>(*pBinnedBucket, cTargetStates);
      } else {
         // TODO : re-enable this check once we use the same memory region
         //ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pRet, aBinnedBucketsEndDebug);
         ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pBinnedBucket, aBinnedBucketsEndDebug);
         pRet->template Add<countCompilerClassificationTargetStates>(*pBinnedBucket, cTargetStates);
      }
      ++permuteVector;
   } while(LIKELY(0 == (permuteVector >> cAllBits)));

#ifndef NDEBUG
   CompareTotalsDebug<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBucketsDebugCopy, pAttributeCombination, aiPoint, directionVector, cTargetStates, pRet);
#endif // NDEBUG
}

template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
FractionalDataType SweepMultiDiemensional(BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets, const AttributeCombinationCore * const pAttributeCombination, size_t * aiPoint, const size_t directionVectorLow, unsigned int iDimensionSweep, const size_t cTargetStates, BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pBinnedBucketBestAndTemp, size_t * piBestCut
#ifndef NDEBUG
   , const BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy, const unsigned char * const aBinnedBucketsEndDebug
#endif // NDEBUG
) {
   // TODO : optimize this function

   assert(iDimensionSweep < pAttributeCombination->m_cAttributes);
   assert(0 == (directionVectorLow & (static_cast<size_t>(1) << iDimensionSweep)));

   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);
   const size_t cBytesPerTwoBinnedBuckets = cBytesPerBinnedBucket << 1;

   size_t * const piPoint = &aiPoint[iDimensionSweep];
   *piPoint = 0;
   size_t directionVectorHigh = directionVectorLow | static_cast<size_t>(1) << iDimensionSweep;

   size_t cStates = pAttributeCombination->m_AttributeCombinationEntry[iDimensionSweep].m_pAttribute->m_cStates;

   size_t iBestCut = 0;

   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pTotalsLow = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucketBestAndTemp, 2);
   // TODO : re-enable this check once we use the same memory region
   //ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pTotalsLow, aBinnedBucketsEndDebug);

   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const pTotalsHigh = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucketBestAndTemp, 3);
   // TODO : re-enable this check once we use the same memory region
   //ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, pTotalsHigh, aBinnedBucketsEndDebug);

   FractionalDataType bestSplit = -std::numeric_limits<FractionalDataType>::infinity();
   for(size_t iState = 0; iState < cStates - 1; ++iState) {
      *piPoint = iState;

      GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiPoint, directionVectorLow, cTargetStates, pTotalsLow
#ifndef NDEBUG
         , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
      );

      GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiPoint, directionVectorHigh, cTargetStates, pTotalsHigh
#ifndef NDEBUG
         , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
      );

      FractionalDataType splittingScore = 0;
      for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
         splittingScore += 0 == pTotalsLow->cCasesInBucket ? 0 : ComputeNodeSplittingScore(pTotalsLow->aPredictionStatistics[iVector].sumResidualError, pTotalsLow->cCasesInBucket);
         splittingScore += 0 == pTotalsHigh->cCasesInBucket ? 0 : ComputeNodeSplittingScore(pTotalsHigh->aPredictionStatistics[iVector].sumResidualError, pTotalsHigh->cCasesInBucket);
         assert(0 <= splittingScore);
      }
      assert(0 <= splittingScore);

      if(bestSplit < splittingScore) {
         bestSplit = splittingScore;
         iBestCut = iState;

         // TODO : re-enable these checks once we use the same memory region
         //ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pBinnedBucketBestAndTemp, 1), aBinnedBucketsEndDebug);
         //ASSERT_BINNED_BUCKET_OK(cBytesPerBinnedBucket, GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, pTotalsLow, 1), aBinnedBucketsEndDebug);
         memcpy(pBinnedBucketBestAndTemp, pTotalsLow, cBytesPerTwoBinnedBuckets);
      }
   }
   *piBestCut = iBestCut;
   return bestSplit;
}

// TODO: consider adding controls to disallow cuts that would leave too few cases in a region
// TODO: it probably makes more sense to drop the denominator while looking for higher dimensional splits and then go back to our original binned data to retrieve the denominator.  Our binned data is small, so it isn't like having to iterate through all the cases again would incur significant costs, and those costs are probably outweighed by having a more compact representation
template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
bool TrainMultiDimensional(CachedTrainingThreadResources<IsRegression(countCompilerClassificationTargetStates)> * const pCachedThreadResources, SamplingMethod const * const pTrainingSet, const AttributeCombinationCore * const pAttributeCombination, SegmentedRegionCore<ActiveDataType, FractionalDataType> * const pSmallChangeToModelOverwriteSingleSamplingSet, const size_t cTargetStates) {
   // TODO : I'm reserving a single bucket for the first dimension, but I'll probably get rid of that and just use the space in the largest original binned bucket space, so do I really need to start from 1 here?
   size_t cTotalBuckets = 1;
   size_t cTotalBucketsMainSpace = 1;
   for(size_t iDimension = 0; iDimension < pAttributeCombination->m_cAttributes; ++iDimension) {
      cTotalBucketsMainSpace *= pAttributeCombination->m_AttributeCombinationEntry[iDimension].m_pAttribute->m_cStates;
      cTotalBuckets += cTotalBucketsMainSpace;
   }

   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);
   const size_t cBytesBuffer = cTotalBuckets * cBytesPerBinnedBucket;

   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(pCachedThreadResources->GetThreadByteBuffer1(cBytesBuffer));
   if(UNLIKELY(nullptr == aBinnedBuckets)) {
      return true;
   }
   // !!! VERY IMPORTANT: zero our one extra bucket for BuildFastTotals to use for multi-dimensional !!!!
   memset(aBinnedBuckets, 0, cBytesBuffer);

#ifndef NDEBUG
   const unsigned char * const aBinnedBucketsEndDebug = reinterpret_cast<unsigned char *>(aBinnedBuckets) + cBytesBuffer;
#endif // NDEBUG

   RecursiveBinDataSetTraining<countCompilerClassificationTargetStates, 2>::Recursive(pAttributeCombination->m_cAttributes, aBinnedBuckets, pAttributeCombination, pTrainingSet, cTargetStates
#ifndef NDEBUG
      , aBinnedBucketsEndDebug
#endif // NDEBUG
   );

   // TODO : BELOW HERE AND IN MANY OTHER PLACES IN OUR CODE WE MULTIPLY OUR DIMENSIONS BY # OF STATES, BUT DON'T CHECK IF THEY OVERFLOW.  THAT WOULD BE A MEMORY OVERFLOW!
#ifndef NDEBUG
   // make a copy of the original binned buckets for debugging purposes
   size_t cTotalBucketsDebug = 1;
   for(size_t iDimensionDebug = 0; iDimensionDebug < pAttributeCombination->m_cAttributes; ++iDimensionDebug) {
      cTotalBucketsDebug *= pAttributeCombination->m_AttributeCombinationEntry[iDimensionDebug].m_pAttribute->m_cStates;
   }
   const size_t cBytesBufferDebug = cTotalBucketsDebug * cBytesPerBinnedBucket;
   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesBufferDebug));
   if(nullptr == aBinnedBucketsDebugCopy) {
      exit(1);
   }
   memcpy(aBinnedBucketsDebugCopy, aBinnedBuckets, cBytesBufferDebug);
#endif // NDEBUG

   BuildFastTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, cTargetStates, pAttributeCombination, cTotalBucketsMainSpace
#ifndef NDEBUG
      , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
   );

   // TODO: we can just re-generate this code 63 times and eliminate the dynamic cDimensions value.  We can also do this in several other places like for SegmentedRegion and other critical places
   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);


   //permutation0
   //gain_permute0
   //  divs0
   //  gain0
   //    divs00
   //    gain00
   //      divs000
   //      gain000
   //      divs001
   //      gain001
   //    divs01
   //    gain01
   //      divs010
   //      gain010
   //      divs011
   //      gain011
   //  divs1
   //  gain1
   //    divs10
   //    gain10
   //      divs100
   //      gain100
   //      divs101
   //      gain101
   //    divs11
   //    gain11
   //      divs110
   //      gain110
   //      divs111
   //      gain111
   //---------------------------
   //permutation1
   //gain_permute1
   //  divs0
   //  gain0
   //    divs00
   //    gain00
   //      divs000
   //      gain000
   //      divs001
   //      gain001
   //    divs01
   //    gain01
   //      divs010
   //      gain010
   //      divs011
   //      gain011
   //  divs1
   //  gain1
   //    divs10
   //    gain10
   //      divs100
   //      gain100
   //      divs101
   //      gain101
   //    divs11
   //    gain11
   //      divs110
   //      gain110
   //      divs111
   //      gain111       *


   //size_t aiDimensionPermutation[k_cDimensionsMax];
   //for(unsigned int iDimensionInitialize = 0; iDimensionInitialize < cDimensions; ++iDimensionInitialize) {
   //   aiDimensionPermutation[iDimensionInitialize] = iDimensionInitialize;
   //}
   //size_t aiDimensionPermutationBest[k_cDimensionsMax];

   //// TODO this is a fixed length that we should make variable!
   //size_t aTODOSplits[1000000];
   //size_t aTODOSplitsBest[1000000];

   //do {
   //   size_t aiDimensions[k_cDimensionsMax];
   //   memset(aiDimensions, 0, sizeof(aiDimensions[0]) * cDimensions));
   //   while(true) {


   //      assert(0 == iDimension);
   //      while(true) {
   //         ++aiDimension[iDimension];
   //         if(aiDimension[iDimension] != pAttributeCombinations->m_AttributeCombinationEntry[aiDimensionPermutation[iDimension]].m_pAttribute->m_cStates) {
   //            break;
   //         }
   //         aiDimension[iDimension] = 0;
   //         ++iDimension;
   //         if(iDimension == cDimensions) {
   //            goto move_next_permutation;
   //         }
   //      }
   //   }
   //   move_next_permutation:
   //} while(std::next_permutation(aiDimensionPermutation, &aiDimensionPermutation[cDimensions]));






   size_t aiStart[k_cDimensionsMax];

   if(2 == cDimensions) {
      // TODO: we're fixed at max 1000 buckets here, but obviously this should be dynamically set
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * aDynamicBinnedBuckets = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesPerBinnedBucket * 1000));
      // TODO : check everything that uses aDynamicBinnedBuckets if they stay within memory
      //BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * aDynamicBinnedBucketsEnd = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 1000);

      FractionalDataType splittingScore;

      size_t cStatesDimension1 = pAttributeCombination->m_AttributeCombinationEntry[0].m_pAttribute->m_cStates;
      size_t cStatesDimension2 = pAttributeCombination->m_AttributeCombinationEntry[1].m_pAttribute->m_cStates;

      FractionalDataType bestSplittingScoreFirst = -std::numeric_limits<FractionalDataType>::infinity();

      size_t cutFirst1Best;
      size_t cutFirst1LowBest;
      size_t cutFirst1HighBest;

      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1LowLowBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 0);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1LowHighBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 1);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1HighLowBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 2);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1HighHighBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 3);

      for(size_t iState1 = 0; iState1 < cStatesDimension1 - 1; ++iState1) {
         aiStart[0] = iState1;

         splittingScore = 0;

         size_t cutSecond1LowBest;
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2LowLowBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 4);
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2LowHighBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 5);
         splittingScore += SweepMultiDiemensional<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x0, 1, cTargetStates, pTotals2LowLowBest, &cutSecond1LowBest
#ifndef NDEBUG
            , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
            );

         size_t cutSecond1HighBest;
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2HighLowBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 8);
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2HighHighBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 9);
         splittingScore += SweepMultiDiemensional<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x1, 1, cTargetStates, pTotals2HighLowBest, &cutSecond1HighBest
#ifndef NDEBUG
            , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
            );

         if(bestSplittingScoreFirst < splittingScore) {
            bestSplittingScoreFirst = splittingScore;
            cutFirst1Best = iState1;
            cutFirst1LowBest = cutSecond1LowBest;
            cutFirst1HighBest = cutSecond1HighBest;

            pTotals1LowLowBest->template Copy<countCompilerClassificationTargetStates>(*pTotals2LowLowBest, cTargetStates);
            pTotals1LowHighBest->template Copy<countCompilerClassificationTargetStates>(*pTotals2LowHighBest, cTargetStates);
            pTotals1HighLowBest->template Copy<countCompilerClassificationTargetStates>(*pTotals2HighLowBest, cTargetStates);
            pTotals1HighHighBest->template Copy<countCompilerClassificationTargetStates>(*pTotals2HighHighBest, cTargetStates);
         }
      }

      bool bCutFirst2 = false;

      size_t cutFirst2Best;
      size_t cutFirst2LowBest;
      size_t cutFirst2HighBest;

      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2LowLowBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 12);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2LowHighBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 13);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2HighLowBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 14);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals2HighHighBest = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 15);

      for(size_t iState2 = 0; iState2 < cStatesDimension2 - 1; ++iState2) {
         aiStart[1] = iState2;

         splittingScore = 0;

         size_t cutSecond2LowBest;
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1LowLowBestInner = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 16);
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1LowHighBestInner = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 17);
         splittingScore += SweepMultiDiemensional<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x0, 0, cTargetStates, pTotals1LowLowBestInner, &cutSecond2LowBest
#ifndef NDEBUG
            , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
            );

         size_t cutSecond2HighBest;
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1HighLowBestInner = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 20);
         BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotals1HighHighBestInner = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 21);
         splittingScore += SweepMultiDiemensional<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x2, 0, cTargetStates, pTotals1HighLowBestInner, &cutSecond2HighBest
#ifndef NDEBUG
            , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
            );

         if(bestSplittingScoreFirst < splittingScore) {
            bestSplittingScoreFirst = splittingScore;
            cutFirst2Best = iState2;
            cutFirst2LowBest = cutSecond2LowBest;
            cutFirst2HighBest = cutSecond2HighBest;

            pTotals2LowLowBest->template Copy<countCompilerClassificationTargetStates>(*pTotals1LowLowBestInner, cTargetStates);
            pTotals2LowHighBest->template Copy<countCompilerClassificationTargetStates>(*pTotals1LowHighBestInner, cTargetStates);
            pTotals2HighLowBest->template Copy<countCompilerClassificationTargetStates>(*pTotals1HighLowBestInner, cTargetStates);
            pTotals2HighHighBest->template Copy<countCompilerClassificationTargetStates>(*pTotals1HighHighBestInner, cTargetStates);

            bCutFirst2 = true;
         }
      }

      if(bCutFirst2) {
         pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(1, 1);
         pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = cutFirst2Best;

         if(cutFirst2LowBest < cutFirst2HighBest) {
            pSmallChangeToModelOverwriteSingleSamplingSet->EnsureValueCapacity(cVectorLength * 6);

            pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(0, 2);
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = cutFirst2LowBest;
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[1] = cutFirst2HighBest;
         } else if(cutFirst2HighBest < cutFirst2LowBest) {
            pSmallChangeToModelOverwriteSingleSamplingSet->EnsureValueCapacity(cVectorLength * 6);

            pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(0, 2);
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = cutFirst2HighBest;
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[1] = cutFirst2LowBest;
         } else {
            pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(0, 1);

            pSmallChangeToModelOverwriteSingleSamplingSet->EnsureValueCapacity(cVectorLength * 4);
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = cutFirst2LowBest;
         }

         for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
            FractionalDataType predictionLowLow;
            FractionalDataType predictionLowHigh;
            FractionalDataType predictionHighLow;
            FractionalDataType predictionHighHigh;

            if(IsRegression(countCompilerClassificationTargetStates)) {
               // regression
               predictionLowLow = 0 == pTotals2LowLowBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals2LowLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals2LowLowBest->cCasesInBucket);
               predictionLowHigh = 0 == pTotals2LowHighBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals2LowHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals2LowHighBest->cCasesInBucket);
               predictionHighLow = 0 == pTotals2HighLowBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals2HighLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals2HighLowBest->cCasesInBucket);
               predictionHighHigh = 0 == pTotals2HighHighBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals2HighHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals2HighHighBest->cCasesInBucket);
            } else {
               // classification
               assert(IsClassification(countCompilerClassificationTargetStates));
               predictionLowLow = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals2LowLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals2LowLowBest->aPredictionStatistics[iVector].GetSumDenominator());
               predictionLowHigh = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals2LowHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals2LowHighBest->aPredictionStatistics[iVector].GetSumDenominator());
               predictionHighLow = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals2HighLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals2HighLowBest->aPredictionStatistics[iVector].GetSumDenominator());
               predictionHighHigh = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals2HighHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals2HighHighBest->aPredictionStatistics[iVector].GetSumDenominator());
            }

            if(cutFirst2LowBest < cutFirst2HighBest) {
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[4 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[5 * cVectorLength + iVector] = predictionHighHigh;
            } else if(cutFirst2HighBest < cutFirst2LowBest) {
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[4 * cVectorLength + iVector] = predictionHighHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[5 * cVectorLength + iVector] = predictionHighHigh;
            } else {
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionHighHigh;
            }
         }
      } else {
         pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(0, 1);
         pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = cutFirst1Best;

         if(cutFirst1LowBest < cutFirst1HighBest) {
            pSmallChangeToModelOverwriteSingleSamplingSet->EnsureValueCapacity(cVectorLength * 6);

            pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(1, 2);
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = cutFirst1LowBest;
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[1] = cutFirst1HighBest;
         } else if(cutFirst1HighBest < cutFirst1LowBest) {
            pSmallChangeToModelOverwriteSingleSamplingSet->EnsureValueCapacity(cVectorLength * 6);

            pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(1, 2);
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = cutFirst1HighBest;
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[1] = cutFirst1LowBest;
         } else {
            pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(1, 1);

            pSmallChangeToModelOverwriteSingleSamplingSet->EnsureValueCapacity(cVectorLength * 4);
            pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = cutFirst1LowBest;
         }

         for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
            FractionalDataType predictionLowLow;
            FractionalDataType predictionLowHigh;
            FractionalDataType predictionHighLow;
            FractionalDataType predictionHighHigh;

            if(IsRegression(countCompilerClassificationTargetStates)) {
               // regression
               predictionLowLow = 0 == pTotals1LowLowBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals1LowLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals1LowLowBest->cCasesInBucket);
               predictionLowHigh = 0 == pTotals1LowHighBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals1LowHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals1LowHighBest->cCasesInBucket);
               predictionHighLow = 0 == pTotals1HighLowBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals1HighLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals1HighLowBest->cCasesInBucket);
               predictionHighHigh = 0 == pTotals1HighHighBest->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotals1HighHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals1HighHighBest->cCasesInBucket);
            } else {
               assert(IsClassification(countCompilerClassificationTargetStates));
               // classification
               predictionLowLow = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals1LowLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals1LowLowBest->aPredictionStatistics[iVector].GetSumDenominator());
               predictionLowHigh = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals1LowHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals1LowHighBest->aPredictionStatistics[iVector].GetSumDenominator());
               predictionHighLow = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals1HighLowBest->aPredictionStatistics[iVector].sumResidualError, pTotals1HighLowBest->aPredictionStatistics[iVector].GetSumDenominator());
               predictionHighHigh = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotals1HighHighBest->aPredictionStatistics[iVector].sumResidualError, pTotals1HighHighBest->aPredictionStatistics[iVector].GetSumDenominator());
            }

            if(cutFirst1LowBest < cutFirst1HighBest) {
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[4 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[5 * cVectorLength + iVector] = predictionHighHigh;
            } else if(cutFirst1HighBest < cutFirst1LowBest) {
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionHighHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[4 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[5 * cVectorLength + iVector] = predictionHighHigh;
            } else {
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionLowLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionHighLow;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionLowHigh;
               pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionHighHigh;
            }
         }
      }

      free(aDynamicBinnedBuckets);

   } else {
      // TODO: handle this better
      //printf("error, only supports pairs");
      exit(1);
   }

#ifndef NDEBUG
   free(aBinnedBucketsDebugCopy);
#endif // NDEBUG

   return false;
}

//template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
//bool TrainMultiDimensionalPaulAlgorithm(CachedThreadResources<IsRegression(countCompilerClassificationTargetStates)> * const pCachedThreadResources, const AttributeInternal * const pTargetAttribute, SamplingMethod const * const pTrainingSet, const AttributeCombinationCore * const pAttributeCombination, SegmentedRegion<ActiveDataType, FractionalDataType> * const pSmallChangeToModelOverwriteSingleSamplingSet) {
//   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets = BinDataSet<countCompilerClassificationTargetStates>(pCachedThreadResources, pAttributeCombination, pTrainingSet, pTargetAttribute);
//   if(UNLIKELY(nullptr == aBinnedBuckets)) {
//      return true;
//   }
//
//   BuildFastTotals(aBinnedBuckets, pTargetAttribute, pAttributeCombination);
//
//   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);
//   const size_t cTargetStates = pTargetAttribute->m_cStates;
//   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
//   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);
//
//   size_t aiStart[k_cDimensionsMax];
//   size_t aiLast[k_cDimensionsMax];
//
//   if(2 == cDimensions) {
//      // TODO: we're fixed at max 1000 buckets here, but obviously this should be dynamically set
//      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * aDynamicBinnedBuckets = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesPerBinnedBucket * 1000));
//
//      size_t cStatesDimension1 = pAttributeCombination->m_AttributeCombinationEntry[0].m_pAttribute->m_cStates;
//      size_t cStatesDimension2 = pAttributeCombination->m_AttributeCombinationEntry[1].m_pAttribute->m_cStates;
//
//      FractionalDataType bestSplittingScore = -std::numeric_limits<FractionalDataType>::infinity();
//
//      pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(0, 1);
//      pSmallChangeToModelOverwriteSingleSamplingSet->SetCountDivisions(1, 1);
//      pSmallChangeToModelOverwriteSingleSamplingSet->EnsureValueCapacity(cVectorLength * 4);
//
//      for(size_t iState1 = 0; iState1 < cStatesDimension1 - 1; ++iState1) {
//         for(size_t iState2 = 0; iState2 < cStatesDimension2 - 1; ++iState2) {
//            FractionalDataType splittingScore;
//
//            BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsLowLow = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 0);
//            BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsHighLow = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 1);
//            BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsLowHigh = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 2);
//            BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsHighHigh = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 3);
//
//            BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsTarget = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 4);
//            BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsOther = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 5);
//
//            aiStart[0] = 0;
//            aiStart[1] = 0;
//            aiLast[0] = iState1;
//            aiLast[1] = iState2;
//            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, aiLast, cTargetStates, pTotalsLowLow);
//
//            aiStart[0] = iState1 + 1;
//            aiStart[1] = 0;
//            aiLast[0] = cStatesDimension1 - 1;
//            aiLast[1] = iState2;
//            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, aiLast, cTargetStates, pTotalsHighLow);
//
//            aiStart[0] = 0;
//            aiStart[1] = iState2 + 1;
//            aiLast[0] = iState1;
//            aiLast[1] = cStatesDimension2 - 1;
//            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, aiLast, cTargetStates, pTotalsLowHigh);
//
//            aiStart[0] = iState1 + 1;
//            aiStart[1] = iState2 + 1;
//            aiLast[0] = cStatesDimension1 - 1;
//            aiLast[1] = cStatesDimension2 - 1;
//            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, aiLast, cTargetStates, pTotalsHighHigh);
//
//            // LOW LOW
//            pTotalsTarget->Zero(cTargetStates);
//            pTotalsOther->Zero(cTargetStates);
//
//            // MODIFY HERE
//            pTotalsTarget->Add(*pTotalsLowLow, cTargetStates);
//            pTotalsOther->Add(*pTotalsHighLow, cTargetStates);
//            pTotalsOther->Add(*pTotalsLowHigh, cTargetStates);
//            pTotalsOther->Add(*pTotalsHighHigh, cTargetStates);
//            
//            splittingScore = CalculateRegionSplittingScore<countCompilerClassificationTargetStates, countCompilerDimensions>(pTotalsTarget, pTotalsOther, cTargetStates);
//            if(bestSplittingScore < splittingScore) {
//               bestSplittingScore = splittingScore;
//
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = iState1;
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = iState2;
//
//               for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
//                  FractionalDataType predictionTarget;
//                  FractionalDataType predictionOther;
//
//                  if(IS_REGRESSION(countCompilerClassificationTargetStates)) {
//                     // regression
//                     predictionTarget = 0 == pTotalsTarget->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->cCasesInBucket);
//                     predictionOther = 0 == pTotalsOther->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->cCasesInBucket);
//                  } else {
//                     assert(IS_CLASSIFICATION(countCompilerClassificationTargetStates));
//                     // classification
//                     predictionTarget = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->aPredictionStatistics[iVector].GetSumDenominator());
//                     predictionOther = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->aPredictionStatistics[iVector].GetSumDenominator());
//                  }
//
//                  // MODIFY HERE
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionTarget;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionOther;
//               }
//            }
//
//
//
//
//            // HIGH LOW
//            pTotalsTarget->Zero(cTargetStates);
//            pTotalsOther->Zero(cTargetStates);
//
//            // MODIFY HERE
//            pTotalsOther->Add(*pTotalsLowLow, cTargetStates);
//            pTotalsTarget->Add(*pTotalsHighLow, cTargetStates);
//            pTotalsOther->Add(*pTotalsLowHigh, cTargetStates);
//            pTotalsOther->Add(*pTotalsHighHigh, cTargetStates);
//
//            splittingScore = CalculateRegionSplittingScore<countCompilerClassificationTargetStates, countCompilerDimensions>(pTotalsTarget, pTotalsOther, cTargetStates);
//            if(bestSplittingScore < splittingScore) {
//               bestSplittingScore = splittingScore;
//
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = iState1;
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = iState2;
//
//               for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
//                  FractionalDataType predictionTarget;
//                  FractionalDataType predictionOther;
//
//                  if(IS_REGRESSION(countCompilerClassificationTargetStates)) {
//                     // regression
//                     predictionTarget = 0 == pTotalsTarget->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->cCasesInBucket);
//                     predictionOther = 0 == pTotalsOther->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->cCasesInBucket);
//                  } else {
//                     assert(IS_CLASSIFICATION(countCompilerClassificationTargetStates));
//                     // classification
//                     predictionTarget = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->aPredictionStatistics[iVector].GetSumDenominator());
//                     predictionOther = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->aPredictionStatistics[iVector].GetSumDenominator());
//                  }
//
//                  // MODIFY HERE
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionTarget;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionOther;
//               }
//            }
//
//
//
//
//            // LOW HIGH
//            pTotalsTarget->Zero(cTargetStates);
//            pTotalsOther->Zero(cTargetStates);
//
//            // MODIFY HERE
//            pTotalsOther->Add(*pTotalsLowLow, cTargetStates);
//            pTotalsOther->Add(*pTotalsHighLow, cTargetStates);
//            pTotalsTarget->Add(*pTotalsLowHigh, cTargetStates);
//            pTotalsOther->Add(*pTotalsHighHigh, cTargetStates);
//
//            splittingScore = CalculateRegionSplittingScore<countCompilerClassificationTargetStates, countCompilerDimensions>(pTotalsTarget, pTotalsOther, cTargetStates);
//            if(bestSplittingScore < splittingScore) {
//               bestSplittingScore = splittingScore;
//
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = iState1;
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = iState2;
//
//               for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
//                  FractionalDataType predictionTarget;
//                  FractionalDataType predictionOther;
//
//                  if(IS_REGRESSION(countCompilerClassificationTargetStates)) {
//                     // regression
//                     predictionTarget = 0 == pTotalsTarget->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->cCasesInBucket);
//                     predictionOther = 0 == pTotalsOther->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->cCasesInBucket);
//                  } else {
//                     assert(IS_CLASSIFICATION(countCompilerClassificationTargetStates));
//                     // classification
//                     predictionTarget = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->aPredictionStatistics[iVector].GetSumDenominator());
//                     predictionOther = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->aPredictionStatistics[iVector].GetSumDenominator());
//                  }
//
//                  // MODIFY HERE
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionTarget;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionOther;
//               }
//            }
//
//
//
//            // HIGH HIGH
//            pTotalsTarget->Zero(cTargetStates);
//            pTotalsOther->Zero(cTargetStates);
//
//            // MODIFY HERE
//            pTotalsOther->Add(*pTotalsLowLow, cTargetStates);
//            pTotalsOther->Add(*pTotalsHighLow, cTargetStates);
//            pTotalsOther->Add(*pTotalsLowHigh, cTargetStates);
//            pTotalsTarget->Add(*pTotalsHighHigh, cTargetStates);
//
//            splittingScore = CalculateRegionSplittingScore<countCompilerClassificationTargetStates, countCompilerDimensions>(pTotalsTarget, pTotalsOther, cTargetStates);
//            if(bestSplittingScore < splittingScore) {
//               bestSplittingScore = splittingScore;
//
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(0)[0] = iState1;
//               pSmallChangeToModelOverwriteSingleSamplingSet->GetDivisionPointer(1)[0] = iState2;
//
//               for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
//                  FractionalDataType predictionTarget;
//                  FractionalDataType predictionOther;
//
//                  if(IS_REGRESSION(countCompilerClassificationTargetStates)) {
//                     // regression
//                     predictionTarget = 0 == pTotalsTarget->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->cCasesInBucket);
//                     predictionOther = 0 == pTotalsOther->cCasesInBucket ? 0 : ComputeSmallChangeInRegressionPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->cCasesInBucket);
//                  } else {
//                     assert(IS_CLASSIFICATION(countCompilerClassificationTargetStates));
//                     // classification
//                     predictionTarget = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsTarget->aPredictionStatistics[iVector].sumResidualError, pTotalsTarget->aPredictionStatistics[iVector].GetSumDenominator());
//                     predictionOther = ComputeSmallChangeInClassificationLogOddPredictionForOneSegment(pTotalsOther->aPredictionStatistics[iVector].sumResidualError, pTotalsOther->aPredictionStatistics[iVector].GetSumDenominator());
//                  }
//
//                  // MODIFY HERE
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[0 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[1 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[2 * cVectorLength + iVector] = predictionOther;
//                  pSmallChangeToModelOverwriteSingleSamplingSet->GetValuePointer()[3 * cVectorLength + iVector] = predictionTarget;
//               }
//            }
//
//
//
//
//
//
//         }
//      }
//
//      free(aDynamicBinnedBuckets);
//   } else {
//      printf("error, only supports pairs");
//      exit(1);
//   }
//   return false;
//}



template<ptrdiff_t countCompilerClassificationTargetStates, size_t countCompilerDimensions>
bool CalculateInteractionScore(CachedInteractionThreadResources * const pCachedThreadResources, DataSetInternalCore * pDataSet, const AttributeCombinationCore * const pAttributeCombination, FractionalDataType * const pInteractionScoreReturn) {
   // TODO : I'm reserving a single bucket for the first dimension, but I'll probably get rid of that and just use the space in the largest original binned bucket space, so do I really need to start from 1 here?
   size_t cTotalBuckets = 1;
   size_t cTotalBucketsMainSpace = 1;
   for(size_t iDimension = 0; iDimension < pAttributeCombination->m_cAttributes; ++iDimension) {
      cTotalBucketsMainSpace *= pAttributeCombination->m_AttributeCombinationEntry[iDimension].m_pAttribute->m_cStates;
      cTotalBuckets += cTotalBucketsMainSpace;
   }

   const size_t cTargetStates = pDataSet->m_pAttributeSet->m_cTargetStates;
   const size_t cVectorLength = GET_VECTOR_LENGTH(countCompilerClassificationTargetStates, cTargetStates);
   const size_t cBytesPerBinnedBucket = GetBinnedBucketSize<IsRegression(countCompilerClassificationTargetStates)>(cVectorLength);
   const size_t cBytesBuffer = cTotalBuckets * cBytesPerBinnedBucket;

   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBuckets = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(pCachedThreadResources->GetThreadByteBuffer1(cBytesBuffer));
   if(UNLIKELY(nullptr == aBinnedBuckets)) {
      return true;
   }
   // !!! VERY IMPORTANT: zero our one extra bucket for BuildFastTotals to use for multi-dimensional !!!!
   memset(aBinnedBuckets, 0, cBytesBuffer);

#ifndef NDEBUG
   const unsigned char * const aBinnedBucketsEndDebug = reinterpret_cast<unsigned char *>(aBinnedBuckets) + cBytesBuffer;
#endif // NDEBUG

   // TODO: we don't seem to use the denmoninator in PredictionStatistics, so we could remove that variable for classification
   
   BinDataSet<countCompilerClassificationTargetStates>(aBinnedBuckets, pAttributeCombination, pDataSet, cTargetStates
#ifndef NDEBUG
      , aBinnedBucketsEndDebug
#endif // NDEBUG
      );

   // TODO : BELOW HERE AND IN MANY OTHER PLACES IN OUR CODE WE MULTIPLY OUR DIMENSIONS BY # OF STATES, BUT DON'T CHECK IF THEY OVERFLOW.  THAT WOULD BE A MEMORY OVERFLOW!
#ifndef NDEBUG
   // make a copy of the original binned buckets for debugging purposes
   size_t cTotalBucketsDebug = 1;
   for(size_t iDimensionDebug = 0; iDimensionDebug < pAttributeCombination->m_cAttributes; ++iDimensionDebug) {
      cTotalBucketsDebug *= pAttributeCombination->m_AttributeCombinationEntry[iDimensionDebug].m_pAttribute->m_cStates;
   }
   const size_t cBytesBufferDebug = cTotalBucketsDebug * cBytesPerBinnedBucket;
   BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * const aBinnedBucketsDebugCopy = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesBufferDebug));
   if(nullptr == aBinnedBucketsDebugCopy) {
      exit(1);
   }
   memcpy(aBinnedBucketsDebugCopy, aBinnedBuckets, cBytesBufferDebug);
#endif // NDEBUG

   BuildFastTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, cTargetStates, pAttributeCombination, cTotalBucketsMainSpace
#ifndef NDEBUG
      , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
      );

   // TODO: we can just re-generate this code 63 times and eliminate the dynamic cDimensions value.  We can also do this in several other places like for SegmentedRegion and other critical places
   const size_t cDimensions = GET_ATTRIBUTE_COMBINATION_DIMENSIONS(countCompilerDimensions, pAttributeCombination->m_cAttributes);

   size_t aiStart[k_cDimensionsMax];

   if(2 == cDimensions) {
      // TODO: we're fixed at max 1000 buckets here, but obviously this should be dynamically set
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * aDynamicBinnedBuckets = static_cast<BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> *>(malloc(cBytesPerBinnedBucket * 4));
      // TODO : check everything that uses aDynamicBinnedBuckets if they stay within memory
      //BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * aDynamicBinnedBucketsEnd = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 1000);

      size_t cStatesDimension1 = pAttributeCombination->m_AttributeCombinationEntry[0].m_pAttribute->m_cStates;
      size_t cStatesDimension2 = pAttributeCombination->m_AttributeCombinationEntry[1].m_pAttribute->m_cStates;

      FractionalDataType bestSplittingScore = -std::numeric_limits<FractionalDataType>::infinity();

      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsLowLow = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 0);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsLowHigh = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 1);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsHighLow = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 2);
      BinnedBucket<IsRegression(countCompilerClassificationTargetStates)> * pTotalsHighHigh = GetBinnedBucketByIndex<IsRegression(countCompilerClassificationTargetStates)>(cBytesPerBinnedBucket, aDynamicBinnedBuckets, 3);

      for(size_t iState1 = 0; iState1 < cStatesDimension1 - 1; ++iState1) {
         aiStart[0] = iState1;
         for(size_t iState2 = 0; iState2 < cStatesDimension2 - 1; ++iState2) {
            aiStart[1] = iState2;

            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x00, cTargetStates, pTotalsLowLow
#ifndef NDEBUG
               , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
               );

            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x02, cTargetStates, pTotalsLowHigh
#ifndef NDEBUG
               , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
               );

            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x01, cTargetStates, pTotalsHighLow
#ifndef NDEBUG
               , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
               );

            GetTotals<countCompilerClassificationTargetStates, countCompilerDimensions>(aBinnedBuckets, pAttributeCombination, aiStart, 0x03, cTargetStates, pTotalsHighHigh
#ifndef NDEBUG
               , aBinnedBucketsDebugCopy, aBinnedBucketsEndDebug
#endif // NDEBUG
               );

            FractionalDataType splittingScore = 0;
            for(size_t iVector = 0; iVector < cVectorLength; ++iVector) {
               splittingScore += 0 == pTotalsLowLow->cCasesInBucket ? 0 : ComputeNodeSplittingScore(pTotalsLowLow->aPredictionStatistics[iVector].sumResidualError, pTotalsLowLow->cCasesInBucket);
               splittingScore += 0 == pTotalsLowHigh->cCasesInBucket ? 0 : ComputeNodeSplittingScore(pTotalsLowHigh->aPredictionStatistics[iVector].sumResidualError, pTotalsLowHigh->cCasesInBucket);
               splittingScore += 0 == pTotalsHighLow->cCasesInBucket ? 0 : ComputeNodeSplittingScore(pTotalsHighLow->aPredictionStatistics[iVector].sumResidualError, pTotalsHighLow->cCasesInBucket);
               splittingScore += 0 == pTotalsHighHigh->cCasesInBucket ? 0 : ComputeNodeSplittingScore(pTotalsHighHigh->aPredictionStatistics[iVector].sumResidualError, pTotalsHighHigh->cCasesInBucket);
               assert(0 <= splittingScore);
            }
            assert(0 <= splittingScore);

            if(bestSplittingScore < splittingScore) {
               bestSplittingScore = splittingScore;
            }
         }
      }

      *pInteractionScoreReturn = bestSplittingScore;
   } else {
      // TODO: handle this better
      //printf("error, only supports pairs");
      exit(1);
      return true;
   }

#ifndef NDEBUG
   free(aBinnedBucketsDebugCopy);
#endif // NDEBUG

   return false;
}

#endif // MULTI_DIMENSIONAL_TRAINING_H
