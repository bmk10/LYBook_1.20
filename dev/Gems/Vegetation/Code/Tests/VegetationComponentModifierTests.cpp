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
#include "Vegetation_precompiled.h"

#include "VegetationTest.h"
#include "VegetationMocks.h"

#include <AzTest/AzTest.h>
#include <Tests/TestTypes.h>

#include <Source/Components/PositionModifierComponent.h>
#include <Source/Components/RotationModifierComponent.h>
#include <Source/Components/ScaleModifierComponent.h>
#include <Source/Components/SlopeAlignmentModifierComponent.h>

namespace UnitTest
{
    struct VegetationComponentModifierTests
        : public VegetationComponentTests
    {
        Vegetation::InstanceData m_instanceData;

        void RegisterComponentDescriptors() override
        {
            m_app.RegisterComponentDescriptor(MockShapeServiceComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(MockVegetationAreaServiceComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(MockMeshServiceComponent::CreateDescriptor());
        }
    };

    TEST_F(VegetationComponentModifierTests, PositionModifierComponent)
    {
        const auto crcMask = AZ_CRC("mock-mask", 0xfdf99e32);

        Vegetation::InstanceData vegInstance;
        vegInstance.m_position = AZ::Vector3(2.0f, 4.0f, 0.0f);

        MockGradientRequestHandler gradient;
        gradient.m_defaultValue = 0.99f;

        Vegetation::PositionModifierConfig config;
        config.m_autoSnapToSurface = false;
        config.m_rangeMinX = -0.3f;
        config.m_rangeMaxX = 0.3f;
        config.m_gradientSamplerX.m_gradientId = gradient.m_entity.GetId();

        config.m_rangeMinY = -0.3f;
        config.m_rangeMaxY = 0.3f;
        config.m_gradientSamplerY.m_gradientId = gradient.m_entity.GetId();

        config.m_rangeMinZ = 0.0f;
        config.m_rangeMaxZ = 0.0f;
        config.m_gradientSamplerZ.m_gradientId = gradient.m_entity.GetId();

        Vegetation::PositionModifierComponent* component = nullptr;
        auto entity = CreateEntity(config, &component, [](AZ::Entity* e)
        {
            e->CreateComponent<MockVegetationAreaServiceComponent>();
        });

        Vegetation::ModifierRequestBus::Event(entity->GetId(), &Vegetation::ModifierRequestBus::Events::Execute, vegInstance);

        EXPECT_TRUE(vegInstance.m_position.GetX().IsClose(2.294f));
        EXPECT_TRUE(vegInstance.m_position.GetY().IsClose(4.294f));
        EXPECT_TRUE(vegInstance.m_position.GetZ().IsClose(0.0f));

        // with the surface handler
        MockSurfaceHandler mockSurfaceHandler;
        mockSurfaceHandler.m_outPosition = AZ::Vector3(vegInstance.m_position.GetX(), vegInstance.m_position.GetY(), 6.0f);
        mockSurfaceHandler.m_outNormal = AZ::Vector3(0.0f, 0.0f, 1.0f);
        mockSurfaceHandler.m_outMasks[crcMask] = 1.0f;

        entity->Deactivate();
        config.m_autoSnapToSurface = true;
        component->ReadInConfig(&config);
        entity->Activate();

        Vegetation::ModifierRequestBus::Event(entity->GetId(), &Vegetation::ModifierRequestBus::Events::Execute, vegInstance);
        EXPECT_EQ(mockSurfaceHandler.m_outNormal, vegInstance.m_normal);
        EXPECT_EQ(mockSurfaceHandler.m_outMasks, vegInstance.m_masks);
    }

    TEST_F(VegetationComponentModifierTests, RotationModifierComponent)
    {
        m_instanceData.m_rotation = AZ::Quaternion::CreateIdentity();

        MockGradientRequestHandler gradientX;
        gradientX.m_defaultValue = 0.15f;

        MockGradientRequestHandler gradientY;
        gradientY.m_defaultValue = 0.25f;

        MockGradientRequestHandler gradientZ;
        gradientZ.m_defaultValue = 0.35f;

        Vegetation::RotationModifierConfig config;
        config.m_rangeMinX = -100.0f;
        config.m_rangeMaxX = 100.0f;
        config.m_gradientSamplerX.m_gradientId = gradientX.m_entity.GetId();

        config.m_rangeMinY = -80.0f;
        config.m_rangeMaxY = 80.0f;
        config.m_gradientSamplerY.m_gradientId = gradientY.m_entity.GetId();

        config.m_rangeMinZ = -180.0f;
        config.m_rangeMaxZ = 180.0f;
        config.m_gradientSamplerZ.m_gradientId = gradientZ.m_entity.GetId();

        Vegetation::RotationModifierComponent* component = nullptr;
        auto entity = CreateEntity(config, &component, [](AZ::Entity* e)
        {
            e->CreateComponent<MockVegetationAreaServiceComponent>();
        });

        Vegetation::ModifierRequestBus::Event(entity->GetId(), &Vegetation::ModifierRequestBus::Events::Execute, m_instanceData);

        EXPECT_TRUE(m_instanceData.m_rotation.GetW().IsClose(0.777f));
        EXPECT_TRUE(m_instanceData.m_rotation.GetX().IsClose(-0.353f));
        EXPECT_TRUE(m_instanceData.m_rotation.GetY().IsClose(-0.495f));
        EXPECT_TRUE(m_instanceData.m_rotation.GetZ().IsClose(-0.175f));
    }

    TEST_F(VegetationComponentModifierTests, ScaleModifierComponent)
    {
        MockGradientRequestHandler gradient;
        gradient.m_defaultValue = 0.1234f;

        Vegetation::ScaleModifierConfig config;
        config.m_gradientSampler.m_gradientId = gradient.m_entity.GetId();
        config.m_rangeMin = 0.1f;
        config.m_rangeMax = 0.9f;

        m_instanceData.m_scale = 1.0f;

        Vegetation::ScaleModifierComponent* component = nullptr;
        auto entity = CreateEntity(config, &component, [](AZ::Entity* e)
        {
            e->CreateComponent<MockVegetationAreaServiceComponent>();
        });

        Vegetation::ModifierRequestBus::Event(entity->GetId(), &Vegetation::ModifierRequestBus::Events::Execute, m_instanceData);

        EXPECT_TRUE(AZ::IsClose(m_instanceData.m_scale, 0.19872f, std::numeric_limits<decltype(m_instanceData.m_scale)>::epsilon()));
    }

    TEST_F(VegetationComponentModifierTests, SlopeAlignmentModifierComponent)
    {
        MockGradientRequestHandler gradient;
        gradient.m_defaultValue = 0.87654f;

        Vegetation::SlopeAlignmentModifierConfig config;
        config.m_gradientSampler.m_gradientId = gradient.m_entity.GetId();
        config.m_rangeMin = 0.1f;
        config.m_rangeMax = 0.9f;

        m_instanceData.m_normal = AZ::Vector3(0.12f, 0.34f, 0.56f);
        m_instanceData.m_alignment = AZ::Quaternion::CreateFromAxisAngle(AZ::Vector3::CreateAxisY(), AZ::DegToRad(42)).GetNormalized();

        Vegetation::SlopeAlignmentModifierComponent* component = nullptr;
        auto entity = CreateEntity(config, &component, [](AZ::Entity* e)
        {
            e->CreateComponent<MockVegetationAreaServiceComponent>();
        });

        Vegetation::ModifierRequestBus::Event(entity->GetId(), &Vegetation::ModifierRequestBus::Events::Execute, m_instanceData);

        EXPECT_TRUE(m_instanceData.m_alignment.GetX().IsClose(-0.1973f));
        EXPECT_TRUE(m_instanceData.m_alignment.GetY().IsClose( 0.0666f));
        EXPECT_TRUE(m_instanceData.m_alignment.GetZ().IsClose(-0.0134f));
        EXPECT_TRUE(m_instanceData.m_alignment.GetW().IsClose( 0.9779f));
    }
}

