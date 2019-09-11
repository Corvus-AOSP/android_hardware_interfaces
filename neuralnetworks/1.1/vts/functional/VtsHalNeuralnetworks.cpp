/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "neuralnetworks_hidl_hal_test"

#include "VtsHalNeuralnetworks.h"
#include "1.0/Callbacks.h"
#include "1.0/Utils.h"
#include "GeneratedTestHarness.h"
#include "TestHarness.h"

#include <android-base/logging.h>

namespace android::hardware::neuralnetworks::V1_1::vts::functional {

using V1_0::ErrorStatus;
using V1_0::IPreparedModel;
using V1_0::Request;
using V1_0::implementation::PreparedModelCallback;

void createPreparedModel(const sp<IDevice>& device, const Model& model,
                         sp<IPreparedModel>* preparedModel) {
    ASSERT_NE(nullptr, preparedModel);
    *preparedModel = nullptr;

    // see if service can handle model
    bool fullySupportsModel = false;
    const Return<void> supportedCall = device->getSupportedOperations_1_1(
            model, [&fullySupportsModel](ErrorStatus status, const hidl_vec<bool>& supported) {
                ASSERT_EQ(ErrorStatus::NONE, status);
                ASSERT_NE(0ul, supported.size());
                fullySupportsModel = std::all_of(supported.begin(), supported.end(),
                                                 [](bool valid) { return valid; });
            });
    ASSERT_TRUE(supportedCall.isOk());

    // launch prepare model
    const sp<PreparedModelCallback> preparedModelCallback = new PreparedModelCallback();
    const Return<ErrorStatus> prepareLaunchStatus = device->prepareModel_1_1(
            model, ExecutionPreference::FAST_SINGLE_ANSWER, preparedModelCallback);
    ASSERT_TRUE(prepareLaunchStatus.isOk());
    ASSERT_EQ(ErrorStatus::NONE, static_cast<ErrorStatus>(prepareLaunchStatus));

    // retrieve prepared model
    preparedModelCallback->wait();
    const ErrorStatus prepareReturnStatus = preparedModelCallback->getStatus();
    *preparedModel = preparedModelCallback->getPreparedModel();

    // The getSupportedOperations_1_1 call returns a list of operations that are
    // guaranteed not to fail if prepareModel_1_1 is called, and
    // 'fullySupportsModel' is true i.f.f. the entire model is guaranteed.
    // If a driver has any doubt that it can prepare an operation, it must
    // return false. So here, if a driver isn't sure if it can support an
    // operation, but reports that it successfully prepared the model, the test
    // can continue.
    if (!fullySupportsModel && prepareReturnStatus != ErrorStatus::NONE) {
        ASSERT_EQ(nullptr, preparedModel->get());
        LOG(INFO) << "NN VTS: Early termination of test because vendor service cannot prepare "
                     "model that it does not support.";
        std::cout << "[          ]   Early termination of test because vendor service cannot "
                     "prepare model that it does not support."
                  << std::endl;
        GTEST_SKIP();
    }
    ASSERT_EQ(ErrorStatus::NONE, prepareReturnStatus);
    ASSERT_NE(nullptr, preparedModel->get());
}

// A class for test environment setup
NeuralnetworksHidlEnvironment* NeuralnetworksHidlEnvironment::getInstance() {
    // This has to return a "new" object because it is freed inside
    // testing::AddGlobalTestEnvironment when the gtest is being torn down
    static NeuralnetworksHidlEnvironment* instance = new NeuralnetworksHidlEnvironment();
    return instance;
}

void NeuralnetworksHidlEnvironment::registerTestServices() {
    registerTestService<IDevice>();
}

// The main test class for NEURALNETWORK HIDL HAL.
void NeuralnetworksHidlTest::SetUp() {
    testing::VtsHalHidlTargetTestBase::SetUp();

#ifdef PRESUBMIT_NOT_VTS
    const std::string name =
            NeuralnetworksHidlEnvironment::getInstance()->getServiceName<IDevice>();
    const std::string sampleDriver = "sample-";
    if (kDevice == nullptr && name.substr(0, sampleDriver.size()) == sampleDriver) {
        GTEST_SKIP();
    }
#endif  // PRESUBMIT_NOT_VTS

    ASSERT_NE(nullptr, kDevice.get());
}

// Forward declaration from ValidateModel.cpp
void validateModel(const sp<IDevice>& device, const Model& model);
// Forward declaration from ValidateRequest.cpp
void validateRequest(const sp<V1_0::IPreparedModel>& preparedModel, const V1_0::Request& request);

void validateEverything(const sp<IDevice>& device, const Model& model, const Request& request) {
    validateModel(device, model);

    // Create IPreparedModel.
    sp<IPreparedModel> preparedModel;
    createPreparedModel(device, model, &preparedModel);
    if (preparedModel == nullptr) return;

    validateRequest(preparedModel, request);
}

TEST_P(ValidationTest, Test) {
    const Model model = createModel(kTestModel);
    const Request request = createRequest(kTestModel);
    ASSERT_FALSE(kTestModel.expectFailure);
    validateEverything(kDevice, model, request);
}

INSTANTIATE_GENERATED_TEST(ValidationTest, [](const test_helper::TestModel&) { return true; });

}  // namespace android::hardware::neuralnetworks::V1_1::vts::functional

using android::hardware::neuralnetworks::V1_1::vts::functional::NeuralnetworksHidlEnvironment;

int main(int argc, char** argv) {
    testing::AddGlobalTestEnvironment(NeuralnetworksHidlEnvironment::getInstance());
    testing::InitGoogleTest(&argc, argv);
    NeuralnetworksHidlEnvironment::getInstance()->init(&argc, argv);

    int status = RUN_ALL_TESTS();
    return status;
}
