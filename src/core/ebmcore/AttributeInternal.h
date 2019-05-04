// Copyright (c) 2018 Microsoft Corporation
// Licensed under the MIT license.
// Author: Paul Koch <code@koch.ninja>

#ifndef ATTRIBUTE_INTERNAL_H
#define ATTRIBUTE_INTERNAL_H

#include <stddef.h> // size_t, ptrdiff_t

#include "EbmInternal.h" // TML_INLINE

enum AttributeTypeCore;

// AttributeInternal is a class internal to our library.  Our public interface will not have a "Attribute" POD that we can use for C interop since everything will be a specific type of attribute like OrdinalAttribute (POD)
class AttributeInternalCore final {
public:
   const size_t m_cStates;
   const size_t m_iAttributeData;
   // TODO : implement feature to handle m_attributeType
   const AttributeTypeCore m_attributeType;
   // TODO : implement feature to handle m_bMissing
   const bool m_bMissing;

   TML_INLINE AttributeInternalCore(const size_t cStates, const size_t iAttributeData, const AttributeTypeCore attributeType, const bool bMissing)
      : m_cStates(cStates)
      , m_iAttributeData(iAttributeData)
      , m_attributeType(attributeType)
      , m_bMissing(bMissing) {
   }
};

#endif // ATTRIBUTE_INTERNAL_H
