// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <code@koch.ninja>

#include "PrecompiledHeader.h"

#include <assert.h>
#include <string.h> // memset
#include <stdlib.h> // malloc, realloc, free
#include <stddef.h> // size_t, ptrdiff_t

#include "ebmcore.h" // FractionalDataType
#include "EbmInternal.h" // AttributeTypeCore
#include "AttributeInternal.h"
#include "AttributeCombinationInternal.h"
#include "DataSetByAttributeCombination.h"

#define INVALID_POINTER (reinterpret_cast<void *>(~static_cast<size_t>(0)))

TML_INLINE static FractionalDataType * ConstructResidualErrors(const size_t cCases, const size_t cVectorLength) {
   assert(0 < cCases);
   assert(0 < cVectorLength);

   if(IsMultiplyError(cCases, cVectorLength)) {
      return nullptr;
   }

   const size_t cElements = cCases * cVectorLength;

   if(IsMultiplyError(sizeof(FractionalDataType), cElements)) {
      return nullptr;
   }

   const size_t cBytes = sizeof(FractionalDataType) * cElements;
   FractionalDataType * aResidualErrors = static_cast<FractionalDataType *>(malloc(cBytes));
   return aResidualErrors;
}

TML_INLINE static FractionalDataType * ConstructPredictionScores(const size_t cCases, const size_t cVectorLength, const FractionalDataType * const aPredictionScoresFrom) {
   assert(0 < cCases);
   assert(0 < cVectorLength);

   if(IsMultiplyError(cCases, cVectorLength)) {
      return nullptr;
   }

   const size_t cElements = cCases * cVectorLength;

   if(IsMultiplyError(sizeof(FractionalDataType), cElements)) {
      return nullptr;
   }

   const size_t cBytes = sizeof(FractionalDataType) * cElements;
   FractionalDataType * const aPredictionScoresTo = static_cast<FractionalDataType *>(malloc(cBytes));
   if(nullptr == aPredictionScoresTo) {
      return nullptr;
   }

   if(nullptr == aPredictionScoresFrom) {
      memset(aPredictionScoresTo, 0, cBytes);
   } else {
      memcpy(aPredictionScoresTo, aPredictionScoresFrom, cBytes);
   }
   return aPredictionScoresTo;
}

TML_INLINE static const StorageDataTypeCore * ConstructTargetData(const size_t cCases, const IntegerDataType * const aTargets) {
   assert(0 < cCases);
   assert(nullptr != aTargets);

   const size_t cTargetDataBytes = sizeof(StorageDataTypeCore);
   if(IsMultiplyError(cTargetDataBytes, cCases)) {
      return nullptr;
   }
   StorageDataTypeCore * const aTargetData = static_cast<StorageDataTypeCore *>(malloc(cTargetDataBytes * cCases));

   const IntegerDataType * pTargetFrom = static_cast<const IntegerDataType *>(aTargets);
   const IntegerDataType * const pTargetFromEnd = pTargetFrom + cCases;
   StorageDataTypeCore * pTargetTo = static_cast<StorageDataTypeCore *>(aTargetData);
   do {
      const IntegerDataType data = *pTargetFrom;
      assert(0 <= data);
      assert((IsNumberConvertable<StorageDataTypeCore, IntegerDataType>(data)));
      *pTargetTo = static_cast<StorageDataTypeCore>(data);
      ++pTargetTo;
      ++pTargetFrom;
   } while(pTargetFromEnd != pTargetFrom);
   return aTargetData;
}

struct InputDataPointerAndCountStates {
   const IntegerDataType * m_pInputData;
   size_t m_cStates;
};

TML_INLINE static const StorageDataTypeCore * const * ConstructInputData(const size_t cAttributeCombinations, const AttributeCombinationCore * const * const apAttributeCombination, const size_t cCases, const IntegerDataType * const aInputDataFrom) {
   assert(0 < cAttributeCombinations);
   assert(nullptr != apAttributeCombination);
   assert(0 < cCases);
   assert(nullptr != aInputDataFrom);

   const size_t cBytesMemory = sizeof(void *) * cAttributeCombinations;
   StorageDataTypeCore ** const aaInputDataTo = static_cast<StorageDataTypeCore **>(malloc(cBytesMemory));
   if(nullptr == aaInputDataTo) {
      return nullptr;
   }

   StorageDataTypeCore ** paInputDataTo = aaInputDataTo;
   const AttributeCombinationCore * const * ppAttributeCombination = apAttributeCombination;
   const AttributeCombinationCore * const * const ppAttributeCombinationEnd = apAttributeCombination + cAttributeCombinations;
   do {
      const AttributeCombinationCore * const pAttributeCombination = *ppAttributeCombination;
      const size_t cItemsPerBitPackDataUnit = pAttributeCombination->m_cItemsPerBitPackDataUnit;
      const size_t cBitsPerItemMax = GetCountBits(cItemsPerBitPackDataUnit);
      assert(0 < cCases);
      const size_t cDataUnits = (cCases - 1) / cItemsPerBitPackDataUnit + 1; // this can't overflow or underflow

      if(IsMultiplyError(sizeof(StorageDataTypeCore), cDataUnits)) {
         goto free_all;
      }
      const size_t cBytesData = sizeof(StorageDataTypeCore) * cDataUnits;
      StorageDataTypeCore * pInputDataTo = static_cast<StorageDataTypeCore *>(malloc(cBytesData));
      if(nullptr == pInputDataTo) {
         goto free_all;
      }
      *paInputDataTo = pInputDataTo;
      ++paInputDataTo;

      // stop on the last byte in our array AND then do one special last loop with less or equal iterations to the normal loop
      const StorageDataTypeCore * const pInputDataToLast = reinterpret_cast<const StorageDataTypeCore *>(reinterpret_cast<const char *>(pInputDataTo) + cBytesData) - 1;

      const AttributeCombinationCore::AttributeCombinationEntry * pAttributeCombinationEntry = &pAttributeCombination->m_AttributeCombinationEntry[0];
      InputDataPointerAndCountStates dimensionInfo[k_cDimensionsMax];
      InputDataPointerAndCountStates * pDimensionInfo = &dimensionInfo[0];
      const InputDataPointerAndCountStates * const pDimensionInfoEnd = &dimensionInfo[pAttributeCombination->m_cAttributes];
      do {
         const AttributeInternalCore * const pAttribute = pAttributeCombinationEntry->m_pAttribute;
         pDimensionInfo->m_pInputData = &aInputDataFrom[pAttribute->m_iAttributeData * cCases];
         pDimensionInfo->m_cStates = pAttribute->m_cStates;
         ++pAttributeCombinationEntry;
         ++pDimensionInfo;
      } while(pDimensionInfoEnd != pDimensionInfo);

      // THIS IS NOT A CONSTANT FOR A REASON.. WE CHANGE IT ON OUR LAST ITERATION
      // if we ever template this function on cItemsPerBitPackDataUnit, then we'd want
      // to make this a constant so that the compiler could reason about it an eliminate loops
      // as it is, it isn't a constant, so the compiler would not be able to figure out that most
      // of the time it is a constant
      size_t shiftEnd = cBitsPerItemMax * cItemsPerBitPackDataUnit;
      while(pInputDataTo < pInputDataToLast) {
      one_last_loop:;
         size_t bits = 0;
         size_t shift = 0;
         do {
            size_t tensorMultiple = 1;
            size_t tensorIndex = 0;
            pDimensionInfo = &dimensionInfo[0];
            do {
               const IntegerDataType * pInputData = pDimensionInfo->m_pInputData;
               const IntegerDataType inputData = *pInputData;
               pDimensionInfo->m_pInputData = pInputData + 1;

               assert(0 <= inputData);
               assert((IsNumberConvertable<size_t, IntegerDataType>(inputData))); // data must be lower than cTargetStates and cTargetStates fits into a size_t which we checked earlier
               assert(static_cast<size_t>(inputData) < pDimensionInfo->m_cStates);

               tensorIndex += tensorMultiple * static_cast<size_t>(inputData);
               tensorMultiple *= pDimensionInfo->m_cStates;

               ++pDimensionInfo;
            } while(pDimensionInfoEnd != pDimensionInfo);
            // put our first item in the least significant bits.  We do this so that later when
            // unpacking the indexes, we can just AND our mask with the bitfield to get the index and in subsequent loops
            // we can just shift down.  This eliminates one extra shift that we'd otherwise need to make if the first
            // item was in the MSB
            bits |= tensorIndex << shift;
            shift += cBitsPerItemMax;
         } while(shiftEnd != shift);
         assert((IsNumberConvertable<StorageDataTypeCore, size_t>(bits)));
         *pInputDataTo = static_cast<StorageDataTypeCore>(bits);
         ++pInputDataTo;
      }

      if(pInputDataTo == pInputDataToLast) {
         // if this is the first time we've exited the loop, then re-enter it to do our last loop, but reduce the number of times we do the inner loop
         shiftEnd = cBitsPerItemMax * ((cCases - 1) % cItemsPerBitPackDataUnit + 1);
         goto one_last_loop;
      }

      ++ppAttributeCombination;
   } while(ppAttributeCombinationEnd != ppAttributeCombination);

   return aaInputDataTo;

free_all:
   while(aaInputDataTo != paInputDataTo) {
      --paInputDataTo;
      free(*paInputDataTo);
   }
   free(aaInputDataTo);
   return nullptr;
}

DataSetAttributeCombination::DataSetAttributeCombination(const bool bAllocateResidualErrors, const bool bAllocatePredictionScores, const bool bAllocateTargetData, const size_t cAttributeCombinations, const AttributeCombinationCore * const * const apAttributeCombination, const size_t cCases, const IntegerDataType * const aInputDataFrom, const void * const aTargets, const FractionalDataType * const aPredictionScoresFrom, const size_t cVectorLength)
   : m_aResidualErrors(bAllocateResidualErrors ? ConstructResidualErrors(cCases, cVectorLength) : static_cast<FractionalDataType *>(INVALID_POINTER))
   , m_aPredictionScores(bAllocatePredictionScores ? ConstructPredictionScores(cCases, cVectorLength, aPredictionScoresFrom) : static_cast<FractionalDataType *>(INVALID_POINTER))
   , m_aTargetData(bAllocateTargetData ? ConstructTargetData(cCases, static_cast<const IntegerDataType *>(aTargets)) : static_cast<const StorageDataTypeCore *>(INVALID_POINTER))
   , m_aaInputData(ConstructInputData(cAttributeCombinations, apAttributeCombination, cCases, aInputDataFrom))
   , m_cCases(cCases)
   , m_cAttributeCombinations(cAttributeCombinations) {

   assert(0 < cCases);
   assert(0 < cAttributeCombinations);
}

DataSetAttributeCombination::~DataSetAttributeCombination() {
   if(INVALID_POINTER != m_aResidualErrors) {
      free(m_aResidualErrors);
   }
   if(INVALID_POINTER != m_aPredictionScores) {
      free(m_aPredictionScores);
   }
   if(INVALID_POINTER != m_aTargetData) {
      free(const_cast<StorageDataTypeCore *>(m_aTargetData));
   }
   if(nullptr != m_aaInputData) {
      assert(0 < m_cAttributeCombinations);
      const StorageDataTypeCore * const * paInputData = m_aaInputData;
      const StorageDataTypeCore * const * const paInputDataEnd = m_aaInputData + m_cAttributeCombinations;
      do {
         assert(nullptr != *paInputData);
         free(const_cast<StorageDataTypeCore *>(*paInputData));
         ++paInputData;
      } while(paInputDataEnd != paInputData);
      free(const_cast<StorageDataTypeCore **>(m_aaInputData));
   }
}
