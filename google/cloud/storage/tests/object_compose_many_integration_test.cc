// Copyright 2020 Google LLC
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
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/log.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/expect_exception.h"
#include <gmock/gmock.h>
#include <sys/types.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ObjectComposeManyIntegrationTest =
    ::google::cloud::storage::testing::StorageIntegrationTest;

TEST_F(ObjectComposeManyIntegrationTest, ComposeMany) {
  auto prefix = CreateRandomPrefixName();
  std::string const dest_object_name = prefix + ".dest";

  std::vector<ComposeSourceObject> source_objs;
  std::string expected;
  for (int i = 0; i != 33; ++i) {
    std::string const object_name = prefix + ".src-" + std::to_string(i);
    std::string content = std::to_string(i);
    expected += content;
    StatusOr<ObjectMetadata> insert_meta = client().InsertObject(
        bucket_name(), object_name, std::move(content), IfGenerationMatch(0));
    ASSERT_STATUS_OK(insert_meta);
    source_objs.emplace_back(ComposeSourceObject{
        std::move(object_name), insert_meta->generation(), {}});
  }

  auto client_lv = client();  // ComposeMany requires an `lvalue`.
  auto res = ComposeMany(client_lv, bucket_name(), std::move(source_objs),
                         prefix, dest_object_name, false);

  ASSERT_STATUS_OK(res);
  EXPECT_EQ(dest_object_name, res->name());

  auto stream = client().ReadObject(bucket_name(), dest_object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto deletion_status = client().DeleteObject(
      bucket_name(), dest_object_name, IfGenerationMatch(res->generation()));
  ASSERT_STATUS_OK(deletion_status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
