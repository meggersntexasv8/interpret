// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <code@koch.ninja>

#ifndef DATA_SET_INTERNAL_H
#define DATA_SET_INTERNAL_H

#include <stddef.h> // size_t, ptrdiff_t

#include "ebmcore.h" // FractionalDataType
#include "EbmInternal.h" // TML_INLINE
#include "Logging.h" // EBM_ASSERT & LOG
#include "Feature.h"

// TODO: rename this to DataSetByFeature
class DataSetByFeature final {
   const FractionalDataType * const m_aResidualErrors;
   const StorageDataTypeCore * const * const m_aaInputData;
   const size_t m_cCases;
   const size_t m_cFeatures;

public:

   DataSetByFeature(const bool bRegression, const size_t cFeatures, const Feature * const aFeatures, const size_t cCases, const IntegerDataType * const aInputDataFrom, const void * const aTargetData, const FractionalDataType * const aPredictionScores, const size_t cTargetStates);
   ~DataSetByFeature();

   TML_INLINE bool IsError() const {
      return nullptr == m_aResidualErrors || 0 != m_cFeatures && nullptr == m_aaInputData;
   }

   TML_INLINE const FractionalDataType * GetResidualPointer() const {
      EBM_ASSERT(nullptr != m_aResidualErrors);
      return m_aResidualErrors;
   }
   // TODO: we can change this to take the m_iInputData value directly, which we get from the user! (this also applies to the other dataset)
   // TODO: rename this to GetInputDataPointer
   TML_INLINE const StorageDataTypeCore * GetDataPointer(const Feature * const pFeature) const {
      EBM_ASSERT(nullptr != pFeature);
      EBM_ASSERT(pFeature->m_iFeatureData < m_cFeatures);
      EBM_ASSERT(nullptr != m_aaInputData);
      return m_aaInputData[pFeature->m_iFeatureData];
   }
   TML_INLINE size_t GetCountCases() const {
      return m_cCases;
   }
   TML_INLINE size_t GetCountFeatures() const {
      return m_cFeatures;
   }
};

#endif // DATA_SET_INTERNAL_H
