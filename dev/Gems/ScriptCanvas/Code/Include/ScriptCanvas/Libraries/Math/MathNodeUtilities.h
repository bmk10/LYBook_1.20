/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once



#include <AzCore/Math/Vector4.h>
#include <ScriptCanvas/Core/NodeFunctionGeneric.h>
#include <ScriptCanvas/Data/NumericData.h>

namespace ScriptCanvas
{
    namespace MathNodeUtilities
    {
        AZ_INLINE void DefaultAxisLength(Node& node) { Node::SetDefaultValuesByIndex<0>::_(node, Data::One()); }

        template<int t_Index>
        AZ_INLINE void DefaultToleranceSIMD(Node& node) { Node::SetDefaultValuesByIndex<t_Index>::_(node, Data::ToleranceSIMD()); }

        template<int t_Index>
        AZ_INLINE void DefaultToleranceEpsilon(Node& node) { Node::SetDefaultValuesByIndex<t_Index>::_(node, Data::ToleranceEpsilon()); }
        
        Data::NumberType GetRandom(Data::NumberType lhs, Data::NumberType rhs);

        AZ::s64 GetRandom(AZ::s64 lhs, AZ::s64 rhs); 
                                
        template<typename NumberType>
        AZ_INLINE NumberType GetRandomIntegral(NumberType lhs, NumberType rhs)
        {
            return aznumeric_caster(GetRandom(aznumeric_cast<AZ::s64>(lhs), aznumeric_cast<AZ::s64>(rhs)));
        }

        template<>
        AZ_INLINE Data::NumberType GetRandomIntegral<Data::NumberType>(Data::NumberType lhs, Data::NumberType rhs)
        {
            return GetRandom(lhs, rhs);
        }

        template<typename NumberType>
        AZ_INLINE NumberType GetRandomReal(NumberType lhs, NumberType rhs)
        {
            return aznumeric_caster(GetRandom(aznumeric_cast<Data::NumberType>(lhs), aznumeric_cast<Data::NumberType>(rhs)));
        }

        template<>
        AZ_INLINE Data::NumberType GetRandomReal<Data::NumberType>(Data::NumberType lhs, Data::NumberType rhs)
        {
            return GetRandom(lhs, rhs);
        }

        void RandomEngineInit();
        void RandomEngineReset();
    } // namespace MathNodeUtilities

} // namespace ScriptCanvas

