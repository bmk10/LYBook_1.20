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

#include "TestTypes.h"

#include <AzCore/Component/TransformBus.h>
#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzCore/Module/Module.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Module/Environment.h>

using namespace AZ;

namespace UnitTest
{
    class DLL
        : public AllocatorsFixture
    {
    public:
        void LoadModule()
        {
            m_handle = DynamicModuleHandle::Create("AZCoreTestDLL");
            bool isLoaded = m_handle->Load(true);
            ASSERT_TRUE(isLoaded) << "Could not load required test module: " << m_handle->GetFilename().c_str(); // failed to load the DLL, please check the output paths

            auto createModule = m_handle->GetFunction<CreateModuleClassFunction>(CreateModuleClassFunctionName);
            // if this fails, we cannot continue as we will just nullptr exception
            ASSERT_NE(nullptr, createModule) << "Unable to find create module function in module: " << CreateModuleClassFunctionName;
            m_module = createModule();
            ASSERT_NE(nullptr, m_module);
        }

        void UnloadModule()
        {
            auto destroyModule = m_handle->GetFunction<DestroyModuleClassFunction>(DestroyModuleClassFunctionName);
            ASSERT_NE(nullptr, destroyModule) << "Could not find the destroy function in the module: " << DestroyModuleClassFunctionName;
            destroyModule(m_module);

            m_handle->Unload();
            m_handle.reset();
        }

        AZStd::unique_ptr<DynamicModuleHandle> m_handle;
        Module* m_module = nullptr;
    };

    class TransformHandler
        : public TransformNotificationBus::Handler
    {
    public:
        int m_numEBusCalls = 0;

        void OnParentChanged(EntityId oldParent, EntityId newParent) override
        {
            EXPECT_FALSE(oldParent.IsValid());
            void* systemAllocatorAddress = (void*)(u64)newParent;
            azfree(systemAllocatorAddress); // free memory allocated in a different module, this should be fine as we share environment/allocators
            ++m_numEBusCalls;
        }
    };

    TEST_F(DLL, CrossModuleBusHandler)
    {
        TransformHandler transformHandler;

        LoadModule();

        AZ::SerializeContext serializeContext;
        m_module->Reflect(&serializeContext);

        using DoTests = void(*)();
        DoTests runTests = m_handle->GetFunction<DoTests>("DoTests");
        EXPECT_NE(nullptr, runTests);

        // use the transform bus to verify that we can call EBus messages across modules
        transformHandler.BusConnect(EntityId());

        EXPECT_EQ(0, transformHandler.m_numEBusCalls);

        runTests();

        EXPECT_EQ(1, transformHandler.m_numEBusCalls);

        transformHandler.BusDisconnect(EntityId());

        UnloadModule();
    }

    TEST_F(DLL, CreateVariableFromModuleAndMain)
    {
        LoadModule();

        const char* envVariableName = "My Variable";
        EnvironmentVariable<UnitTest::DLLTestVirtualClass> envVariable;

        // create owned environment variable (variable which uses vtable so it can't exist when the module is unloaded.
        typedef void(*CreateDLLVar)(const char*);
        CreateDLLVar createDLLVar = m_handle->GetFunction<CreateDLLVar>("CreateDLLTestVirtualClass");
        createDLLVar(envVariableName);

        envVariable = AZ::Environment::FindVariable<UnitTest::DLLTestVirtualClass>(envVariableName);
        EXPECT_TRUE(envVariable);
        EXPECT_TRUE(envVariable.IsConstructed());
        EXPECT_EQ(1, envVariable->m_data);

        UnloadModule();

        // the variable is owned by the module (due to the vtable reference), once the module
        // is unloaded the variable should be destroyed, but still valid
        EXPECT_TRUE(envVariable);                    // variable should be valid
        EXPECT_FALSE(envVariable.IsConstructed());   // but destroyed

        //////////////////////////////////////////////////////////////////////////
        // load the module and see if we recreate our variable
        LoadModule();

        // create variable
        createDLLVar = m_handle->GetFunction<CreateDLLVar>("CreateDLLTestVirtualClass");
        createDLLVar(envVariableName);

        envVariable = AZ::Environment::FindVariable<UnitTest::DLLTestVirtualClass>(envVariableName);
        EXPECT_TRUE(envVariable.IsConstructed()); // createDLLVar should construct the variable if already there
        EXPECT_EQ(1, envVariable->m_data);

        UnloadModule();

        //////////////////////////////////////////////////////////////////////////
        // Since the variable is valid till the last reference is gone, we have the option
        // to recreate the variable from a different module
        EXPECT_TRUE(envVariable);                    // variable should be valid
        EXPECT_FALSE(envVariable.IsConstructed());   // but destroyed

        envVariable.Construct(); // since the variable is destroyed, we can create it from a different module, the new module will be owner
        EXPECT_TRUE(envVariable.IsConstructed()); // createDLLVar should construct the variable if already there
        EXPECT_EQ(1, envVariable->m_data);
    }

    TEST_F(DLL, CreateEnvironmentVariableThreadRace)
    {
        const int numThreads = 64;
        int values[numThreads] = { 0 };
        AZStd::thread threads[numThreads];
        AZ::EnvironmentVariable<int> envVar;
        for (int threadIdx = 0; threadIdx < numThreads; ++threadIdx)
        {
            auto threadFunc = [&values, &envVar, threadIdx]()
            {
                envVar = AZ::Environment::CreateVariable<int>("CreateEnvironmentVariableThreadRace", threadIdx);
                values[threadIdx] = *envVar;
            };

            threads[threadIdx] = AZStd::thread(threadFunc);
        }

        for (auto& thread : threads)
        {
            thread.join();
        }

        AZStd::unordered_set<int> uniqueValues;
        for (const int& value : values)
        {
            uniqueValues.insert(value);
        }

        EXPECT_EQ(1, uniqueValues.size());
    }

    TEST_F(DLL, LoadFailure)
    {
        auto handle = DynamicModuleHandle::Create("Not_a_DLL");
        bool isLoaded = handle->Load(true);
        EXPECT_FALSE(isLoaded);

        bool isUnloaded = handle->Unload();
        EXPECT_FALSE(isUnloaded);
    }

    TEST_F(DLL, LoadModuleTwice)
    {
        auto handle = DynamicModuleHandle::Create("AZCoreTestDLL");
        bool isLoaded = handle->Load(true);
        EXPECT_TRUE(isLoaded);
        EXPECT_TRUE(handle->IsLoaded());

        auto secondHandle = DynamicModuleHandle::Create("AZCoreTestDLL");
        isLoaded = secondHandle->Load(true);
        EXPECT_TRUE(isLoaded);
        EXPECT_TRUE(handle->IsLoaded());
        EXPECT_TRUE(secondHandle->IsLoaded());

        bool isUnloaded = handle->Unload();
        EXPECT_TRUE(isUnloaded);
        EXPECT_FALSE(handle->IsLoaded());
        EXPECT_TRUE(secondHandle->IsLoaded());

        isUnloaded = secondHandle->Unload();
        EXPECT_TRUE(isUnloaded);
        EXPECT_FALSE(handle->IsLoaded());
        EXPECT_FALSE(secondHandle->IsLoaded());
    }
}
