// Copyright 2018 Google LLC
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
#include "google/cloud/status.h"
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

using ::google::cloud::storage::testing::TestPermanentFailure;
using ::testing::UnorderedElementsAre;

using ObjectIntegrationTest =
    ::google::cloud::storage::testing::StorageIntegrationTest;

TEST_F(ObjectIntegrationTest, FullPatch) {
  auto object_name = MakeRandomObjectName();
  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> original =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), Projection("full"));
  ASSERT_STATUS_OK(original);

  ObjectMetadata desired = *original;
  desired.mutable_acl().push_back(ObjectAccessControl()
                                      .set_entity("allAuthenticatedUsers")
                                      .set_role("READER"));
  if (original->cache_control() != "no-cache") {
    desired.set_cache_control("no-cache");
  } else {
    desired.set_cache_control("");
  }
  if (original->content_disposition() != "inline") {
    desired.set_content_disposition("inline");
  } else {
    desired.set_content_disposition("attachment; filename=test.txt");
  }
  if (original->content_encoding() != "identity") {
    desired.set_content_encoding("identity");
  } else {
    desired.set_content_encoding("");
  }
  // Use 'en' and 'fr' as test languages because they are known to be supported.
  // The server rejects private tags such as 'x-pig-latin'.
  if (original->content_language() != "en") {
    desired.set_content_language("en");
  } else {
    desired.set_content_language("fr");
  }
  if (original->content_type() != "application/octet-stream") {
    desired.set_content_type("application/octet-stream");
  } else {
    desired.set_content_type("application/text");
  }

  // We want to create a diff that modifies the metadata, so either erase or
  // insert a value for `test-label` depending on the initial state.
  if (original->has_metadata("test-label")) {
    desired.mutable_metadata().erase("test-label");
  } else {
    desired.mutable_metadata().emplace("test-label", "test-value");
  }

  StatusOr<ObjectMetadata> patched =
      client().PatchObject(bucket_name(), object_name, *original, desired);
  ASSERT_STATUS_OK(patched);

  // acl() - cannot compare for equality because many fields are updated with
  // unknown values (entity_id, etag, etc)
  EXPECT_EQ(1, std::count_if(patched->acl().begin(), patched->acl().end(),
                             [](ObjectAccessControl const& x) {
                               return x.entity() == "allAuthenticatedUsers";
                             }));

  EXPECT_EQ(desired.cache_control(), patched->cache_control());
  EXPECT_EQ(desired.content_disposition(), patched->content_disposition());
  EXPECT_EQ(desired.content_encoding(), patched->content_encoding());
  EXPECT_EQ(desired.content_language(), patched->content_language());
  EXPECT_EQ(desired.content_type(), patched->content_type());
  EXPECT_EQ(desired.metadata(), patched->metadata());

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, ListObjectsDelimiter) {
  if (UsingTestbench()) GTEST_SKIP();

  auto object_prefix = MakeRandomObjectName();
  client()
      .InsertObject(bucket_name(), object_prefix + "/foo", LoremIpsum(),
                    storage::IfGenerationMatch(0))
      .value();
  client()
      .InsertObject(bucket_name(), object_prefix + "/foo/bar", LoremIpsum(),
                    storage::IfGenerationMatch(0))
      .value();
  client()
      .InsertObject(bucket_name(), object_prefix + "/foo/baz", LoremIpsum(),
                    storage::IfGenerationMatch(0))
      .value();
  client()
      .InsertObject(bucket_name(), object_prefix + "/qux/quux", LoremIpsum(),
                    storage::IfGenerationMatch(0))
      .value();
  client()
      .InsertObject(bucket_name(), object_prefix + "/something", LoremIpsum(),
                    storage::IfGenerationMatch(0))
      .value();

  ListObjectsReader reader = client().ListObjects(
      bucket_name(), Prefix(object_prefix + "/"), Delimiter("/"));
  std::vector<std::string> actual;
  for (auto it = reader.begin(); it != reader.end(); ++it) {
    auto const& meta = it->value();
    EXPECT_EQ(bucket_name(), meta.bucket());
    actual.push_back(meta.name());
  }
  EXPECT_THAT(actual, UnorderedElementsAre(object_prefix + "/foo",
                                           object_prefix + "/something"));
  reader = client().ListObjects(bucket_name(), Prefix(object_prefix));
  for (auto& meta : reader) {
    ASSERT_STATUS_OK(meta);
    client().DeleteObject(bucket_name(), meta->name());
  }
}

TEST_F(ObjectIntegrationTest, BasicReadWrite) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name(), meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, EncryptedReadWrite) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();
  EncryptionKeyData key = MakeEncryptionKeyData();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, expected,
                            IfGenerationMatch(0), EncryptionKey(key));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name(), meta->bucket());
  ASSERT_TRUE(meta->has_customer_encryption());
  EXPECT_EQ("AES256", meta->customer_encryption().encryption_algorithm);
  EXPECT_EQ(key.sha256, meta->customer_encryption().key_sha256);

  // Create a iostream to read the object back.
  auto stream =
      client().ReadObject(bucket_name(), object_name, EncryptionKey(key));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, ReadNotFound) {
  auto object_name = MakeRandomObjectName();

  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  EXPECT_FALSE(stream.status().ok());
  EXPECT_FALSE(stream.IsOpen());
  EXPECT_EQ(StatusCode::kNotFound, stream.status().code())
      << "status=" << stream.status();
  EXPECT_TRUE(stream.bad());
}

TEST_F(ObjectIntegrationTest, StreamingWrite) {
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name(), meta.bucket());
  auto expected_str = expected.str();
  ASSERT_EQ(expected_str.size(), meta.size());

  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, StreamingWriteAutoClose) {
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::string expected = "A short string to test\n";

  {
    // Create the object, but only if it does not exist already.
    auto os =
        client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
    os << expected;
  }
  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  EXPECT_EQ(expected, actual);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, StreamingWriteEmpty) {
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ObjectMetadata meta = os.metadata().value();
  ASSERT_EQ(object_name, meta.name());
  ASSERT_EQ(bucket_name(), meta.bucket());
  ASSERT_EQ(0U, meta.size());

  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_TRUE(actual.empty());

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, XmlStreamingWrite) {
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os = client().WriteObject(bucket_name(), object_name,
                                 IfGenerationMatch(0), Fields(""));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  // When asking for an empty list of fields we should not expect any values:
  EXPECT_TRUE(meta.bucket().empty());
  EXPECT_TRUE(meta.name().empty());

  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(actual.empty());
  auto expected_str = expected.str();
  EXPECT_EQ(expected_str.size(), actual.size()) << " meta=" << meta;
  EXPECT_EQ(expected_str, actual);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, XmlReadWrite) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name(), meta->bucket());

  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, AccessControlCRUD) {
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto insert = client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                                      IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);

  auto entity_name = MakeEntityName();
  StatusOr<std::vector<ObjectAccessControl>> initial_acl =
      client().ListObjectAcl(bucket_name(), object_name);
  ASSERT_STATUS_OK(initial_acl);

  auto name_counter = [](std::string const& name,
                         std::vector<ObjectAccessControl> const& list) {
    auto name_matcher = [](std::string const& name) {
      return
          [name](ObjectAccessControl const& m) { return m.entity() == name; };
    };
    return std::count_if(list.begin(), list.end(), name_matcher(name));
  };
  ASSERT_EQ(0, name_counter(entity_name, *initial_acl))
      << "Test aborted. The entity <" << entity_name << "> already exists."
      << "This is unexpected as the test generates a random object name.";

  StatusOr<ObjectAccessControl> result = client().CreateObjectAcl(
      bucket_name(), object_name, entity_name, "OWNER");
  ASSERT_STATUS_OK(result);
  EXPECT_EQ("OWNER", result->role());
  auto current_acl = client().ListObjectAcl(bucket_name(), object_name);
  ASSERT_STATUS_OK(current_acl);
  // Search using the entity name returned by the request, because we use
  // 'project-editors-<project_id>' this different than the original entity
  // name, the server "translates" the project id to a project number.
  EXPECT_EQ(1, name_counter(result->entity(), *current_acl));

  auto get_result =
      client().GetObjectAcl(bucket_name(), object_name, entity_name);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(*get_result, *result);

  ObjectAccessControl new_acl = *get_result;
  new_acl.set_role("READER");
  auto updated_result =
      client().UpdateObjectAcl(bucket_name(), object_name, new_acl);
  ASSERT_STATUS_OK(updated_result);
  EXPECT_EQ("READER", updated_result->role());
  get_result = client().GetObjectAcl(bucket_name(), object_name, entity_name);
  EXPECT_EQ(*get_result, *updated_result);

  new_acl = *get_result;
  new_acl.set_role("OWNER");
  // Because this is a freshly created object, with a random name, we do not
  // worry about implementing optimistic concurrency control.
  get_result = client().PatchObjectAcl(bucket_name(), object_name, entity_name,
                                       *get_result, new_acl);
  ASSERT_STATUS_OK(get_result);
  EXPECT_EQ(get_result->role(), new_acl.role());

  // Remove an entity and verify it is no longer in the ACL.
  auto status =
      client().DeleteObjectAcl(bucket_name(), object_name, entity_name);
  ASSERT_STATUS_OK(status);
  current_acl = client().ListObjectAcl(bucket_name(), object_name);
  ASSERT_STATUS_OK(current_acl);
  EXPECT_EQ(0, name_counter(result->entity(), *current_acl));

  status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, WriteWithContentType) {
  auto object_name = MakeRandomObjectName();

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0),
                           ContentType("text/plain"));
  os.exceptions(std::ios_base::failbit);
  os << LoremIpsum();
  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name(), meta.bucket());
  EXPECT_EQ("text/plain", meta.content_type());

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, GetObjectMetadataFailure) {
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto meta = client().GetObjectMetadata(bucket_name(), object_name);
  EXPECT_FALSE(meta.ok()) << "value=" << meta.value();
}

TEST_F(ObjectIntegrationTest, StreamingWriteFailure) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name(), meta->bucket());

  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::badbit | std::ios_base::failbit);
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  testing_util::ExpectException<std::ios::failure>(
      [&] { os.Close(); },
      [&](std::ios::failure const&) {
        EXPECT_FALSE(os.metadata().ok());
        EXPECT_EQ(StatusCode::kFailedPrecondition,
                  os.metadata().status().code());
      },
      "" /* the message generated by the C++ runtime is unknown */);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_F(ObjectIntegrationTest, StreamingWriteFailureNoex) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name(), meta->bucket());

  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
  os << "Expected failure data:\n" << LoremIpsum();

  // This operation should fail because the object already exists.
  os.Close();
  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().ok());
  EXPECT_EQ(StatusCode::kFailedPrecondition, os.metadata().status().code());

  client().DeleteObject(bucket_name(), object_name);
}

TEST_F(ObjectIntegrationTest, ListObjectsFailure) {
  auto nonexistent_bucket_name = MakeRandomBucketName();

  ListObjectsReader reader =
      client().ListObjects(nonexistent_bucket_name, Versions(true));

  // This operation should fail because the bucket does not exist.
  TestPermanentFailure([&] {
    std::vector<ObjectMetadata> actual;
    for (auto&& o : reader) {
      actual.emplace_back(std::move(o).value());
    }
  });
}

TEST_F(ObjectIntegrationTest, DeleteObjectFailure) {
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_FALSE(status.ok());
}

TEST_F(ObjectIntegrationTest, UpdateObjectFailure) {
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto update =
      client().UpdateObject(bucket_name(), object_name, ObjectMetadata());
  EXPECT_FALSE(update.ok()) << "value=" << update.value();
}

TEST_F(ObjectIntegrationTest, PatchObjectFailure) {
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto patch = client().PatchObject(bucket_name(), object_name,
                                    ObjectMetadataPatchBuilder());
  EXPECT_FALSE(patch.ok()) << "value=" << patch.value();
}

TEST_F(ObjectIntegrationTest, ListAccessControlFailure) {
  auto object_name = MakeRandomObjectName();

  // This operation should fail because the source object does not exist.
  auto list = client().ListObjectAcl(bucket_name(), object_name);
  ASSERT_FALSE(list.ok()) << "list[0]=" << list.value().front();
}

TEST_F(ObjectIntegrationTest, CreateAccessControlFailure) {
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client().CreateObjectAcl(bucket_name(), object_name, entity_name,
                                      "READER");
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, GetAccessControlFailure) {
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client().GetObjectAcl(bucket_name(), object_name, entity_name);
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, UpdateAccessControlFailure) {
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client().UpdateObjectAcl(
      bucket_name(), object_name,
      ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, PatchAccessControlFailure) {
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto acl = client().PatchObjectAcl(
      bucket_name(), object_name, entity_name, ObjectAccessControl(),
      ObjectAccessControl().set_entity(entity_name).set_role("READER"));
  ASSERT_FALSE(acl.ok()) << "value=" << acl.value();
}

TEST_F(ObjectIntegrationTest, DeleteAccessControlFailure) {
  auto object_name = MakeRandomObjectName();
  auto entity_name = MakeEntityName();

  // This operation should fail because the source object does not exist.
  auto status =
      client().DeleteObjectAcl(bucket_name(), object_name, entity_name);
  ASSERT_FALSE(status.ok());
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
