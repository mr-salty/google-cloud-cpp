// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/openssl_util.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using CurlSignBlobIntegrationTest =
    ::google::cloud::storage::testing::StorageIntegrationTest;

TEST_F(CurlSignBlobIntegrationTest, Simple) {
  auto encoded = Base64Encode(LoremIpsum());

  SignBlobRequest request(test_signing_service_account(), encoded, {});

  StatusOr<SignBlobResponse> response =
      client().raw_client()->SignBlob(request);
  ASSERT_STATUS_OK(response);

  EXPECT_FALSE(response->key_id.empty());
  EXPECT_FALSE(response->signed_blob.empty());

  auto decoded = Base64Decode(response->signed_blob);
  EXPECT_FALSE(decoded.empty());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
