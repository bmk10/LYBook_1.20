/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <AzCore/Component/Component.h>
#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>

namespace GradientSignal
{
    //! Builder to process images 
    class EditorImageBuilderWorker
        : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(EditorImageBuilderWorker, AZ::SystemAllocator, 0);

        EditorImageBuilderWorker();
        ~EditorImageBuilderWorker();

        //! Asset Builder Callback Functions
        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response);
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response);

        //////////////////////////////////////////////////////////////////////////
        //!AssetBuilderSDK::AssetBuilderCommandBus interface
        void ShutDown() override; // if you get this you must fail all existing jobs and return.
        //////////////////////////////////////////////////////////////////////////

        static AZ::Uuid GetUUID();

    private:
        bool m_isShuttingDown = false;
    };

    //! EditorImageBuilderPluginComponent is to handle the lifecycle of ImageBuilder module.
    class EditorImageBuilderPluginComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(EditorImageBuilderPluginComponent, "{BF60FBB2-E124-4CB9-91CD-E6E640424C99}");
        static void Reflect(AZ::ReflectContext* context);

        EditorImageBuilderPluginComponent(); // avoid initialization here.

        //////////////////////////////////////////////////////////////////////////
        // AZ::Component
        virtual void Init(); // create objects, allocate memory and initialize yourself without reaching out to the outside world
        virtual void Activate(); // reach out to the outside world and connect up to what you need to, register things, etc.
        virtual void Deactivate(); // unregister things, disconnect from the outside world
        //////////////////////////////////////////////////////////////////////////

        virtual ~EditorImageBuilderPluginComponent(); // free memory an uninitialize yourself.

    private:
        EditorImageBuilderWorker m_imageBuilder;
    };

}// namespace GradientSignal
