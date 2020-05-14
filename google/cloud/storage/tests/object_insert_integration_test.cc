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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::CountMatchingEntities;

class ObjectInsertIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  ObjectInsertIntegrationTest()
      : application_credentials_("GOOGLE_APPLICATION_CREDENTIALS", {}) {}

  void SetUp() override {
    if (!UsingTestbench()) {
      // This test was chosen (more or less arbitrarily) to validate that both
      // P12 and JSON credentials are usable in production. The positives for
      // this test are (1) it is relatively short (less than 60 seconds), (2) it
      // actually performs multiple operations against production.
      std::string const key_file_envvar = GetParam();
      auto value =
          google::cloud::internal::GetEnv(key_file_envvar.c_str()).value_or("");
      ASSERT_FALSE(value.empty()) << " expected ${" << key_file_envvar
                                  << "} to be set and be not empty";
      google::cloud::internal::SetEnv("GOOGLE_APPLICATION_CREDENTIALS", value);
    }
    google::cloud::storage::testing::StorageIntegrationTest::SetUp();
  }

  ::google::cloud::testing_util::ScopedEnvironment application_credentials_;
};

std::string GetLinesWith(testing_util::CaptureLogLinesBackend const& backend,
                         std::string const& text) {
  std::string msg;
  for (auto const& l : backend.log_lines) {
    if (l.find(text) == std::string::npos) continue;
    msg += l;
    msg += "\n";
  }
  return msg;
}

TEST_P(ObjectInsertIntegrationTest, SimpleInsertWithNonUrlSafeName) {
  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0),
      DisableCrc32cChecksum(true), DisableMD5Hash(true));
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

TEST_P(ObjectInsertIntegrationTest, XmlInsertWithNonUrlSafeName) {
  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

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

TEST_P(ObjectInsertIntegrationTest, MultipartInsertWithNonUrlSafeName) {
  auto object_name = "name-+-&-=- -%-" + MakeRandomObjectName();

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

TEST_P(ObjectInsertIntegrationTest, InsertWithMD5) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0),
      MD5HashValue("96HF9K981B+JfoQuTVnyCg=="));
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

TEST_P(ObjectInsertIntegrationTest, InsertWithComputedMD5) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0),
      MD5HashValue(ComputeMD5Hash(expected)));
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

TEST_P(ObjectInsertIntegrationTest, XmlInsertWithMD5) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0), Fields(""),
      MD5HashValue("96HF9K981B+JfoQuTVnyCg=="));
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

TEST_P(ObjectInsertIntegrationTest, InsertWithMetadata) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0),
      WithObjectMetadata(ObjectMetadata()
                             .upsert_metadata("test-key", "test-value")
                             .set_content_type("text/plain")));
  ASSERT_STATUS_OK(meta);
  EXPECT_EQ(object_name, meta->name());
  EXPECT_EQ(bucket_name(), meta->bucket());
  EXPECT_TRUE(meta->has_metadata("test-key"));
  EXPECT_EQ("test-value", meta->metadata("test-key"));
  EXPECT_EQ("text/plain", meta->content_type());

  // Create a iostream to read the object back.
  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(expected, actual);

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclAuthenticatedRead) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::AuthenticatedRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclBucketOwnerFullControl) {
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client().GetBucketMetadata(bucket_name(), Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerFullControl(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclBucketOwnerRead) {
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client().GetBucketMetadata(bucket_name(), Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);

  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclPrivate) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::Private(), Projection::Full());
  ASSERT_STATUS_OK(meta);

  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclProjectPrivate) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::ProjectPrivate(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertPredefinedAclPublicRead) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> meta = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::PublicRead(), Projection::Full());
  ASSERT_STATUS_OK(meta);
  EXPECT_LT(
      0, CountMatchingEntities(
             meta->acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclAuthenticatedRead) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::AuthenticatedRead(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta = client().GetObjectMetadata(
      bucket_name(), object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity("allAuthenticatedUsers")
                                         .set_role("READER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest,
       XmlInsertPredefinedAclBucketOwnerFullControl) {
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client().GetBucketMetadata(bucket_name(), Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerFullControl(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta = client().GetObjectMetadata(
      bucket_name(), object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("OWNER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclBucketOwnerRead) {
  auto object_name = MakeRandomObjectName();

  StatusOr<BucketMetadata> bucket =
      client().GetBucketMetadata(bucket_name(), Projection::Full());
  ASSERT_STATUS_OK(bucket);
  ASSERT_TRUE(bucket->has_owner());
  std::string owner = bucket->owner().entity;

  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::BucketOwnerRead(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta = client().GetObjectMetadata(
      bucket_name(), object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  EXPECT_LT(0, CountMatchingEntities(
                   meta->acl(),
                   ObjectAccessControl().set_entity(owner).set_role("READER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclPrivate) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::Private(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta = client().GetObjectMetadata(
      bucket_name(), object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclProjectPrivate) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::ProjectPrivate(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta = client().GetObjectMetadata(
      bucket_name(), object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  ASSERT_TRUE(meta->has_owner());
  EXPECT_LT(0, CountMatchingEntities(meta->acl(),
                                     ObjectAccessControl()
                                         .set_entity(meta->owner().entity)
                                         .set_role("OWNER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, XmlInsertPredefinedAclPublicRead) {
  auto object_name = MakeRandomObjectName();

  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      PredefinedAcl::PublicRead(), Fields(""));
  ASSERT_STATUS_OK(insert);

  StatusOr<ObjectMetadata> meta = client().GetObjectMetadata(
      bucket_name(), object_name, Projection::Full());
  ASSERT_STATUS_OK(meta);
  EXPECT_LT(
      0, CountMatchingEntities(
             meta->acl(),
             ObjectAccessControl().set_entity("allUsers").set_role("READER")))
      << *meta;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/**
 * @test Verify that `QuotaUser` inserts the correct query parameter.
 *
 * Testing for `QuotaUser` is less straightforward that most other parameters.
 * This parameter typically has no effect, so we simply verify that the
 * parameter appears in the request, and that the parameter is not rejected by
 * the server.  To verify that the parameter appears in the request we rely
 * on the logging facilities in the library, which is ugly to do.
 */
TEST_P(ObjectInsertIntegrationTest, InsertWithQuotaUser) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(opts);
  Client client((*std::move(opts))
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name(), object_name, LoremIpsum(),
                          IfGenerationMatch(0), QuotaUser("test-quota-user"));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto predicate = [this](std::string const& line) {
    return line.find(" POST ") != std::string::npos &&
           line.find("/b/" + bucket_name() + "/o") != std::string::npos &&
           line.find("quotaUser=test-quota-user") != std::string::npos;
  };
  auto count = std::count_if(backend->log_lines.begin(),
                             backend->log_lines.end(), predicate);
  EXPECT_LT(0, count) << ", logs matching POST="
                      << GetLinesWith(*backend, " POST ") << "\n";

  auto status = client.DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/**
 * @test Verify that `userIp` inserts the correct query parameter.
 *
 * Testing for `userIp` is less straightforward that most other parameters.
 * This parameter typically has no effect, so we simply verify that the
 * parameter appears in the request, and that the parameter is not rejected by
 * the server.  To verify that the parameter appears in the request we rely
 * on the logging facilities in the library, which is ugly to do.
 */
TEST_P(ObjectInsertIntegrationTest, InsertWithUserIp) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(opts);
  Client client((*std::move(opts))
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name(), object_name, LoremIpsum(),
                          IfGenerationMatch(0), UserIp("127.0.0.1"));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto predicate = [this](std::string const& line) {
    return line.find(" POST ") != std::string::npos &&
           line.find("/b/" + bucket_name() + "/o") != std::string::npos &&
           line.find("userIp=127.0.0.1") != std::string::npos;
  };
  auto count = std::count_if(backend->log_lines.begin(),
                             backend->log_lines.end(), predicate);
  EXPECT_LT(0, count) << ", logs matching POST="
                      << GetLinesWith(*backend, " POST ") << "\n";

  auto status = client.DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/**
 * @test Verify that `userIp` inserts a query parameter.
 *
 * Testing for `userIp` is less straightforward that most other parameters.
 * This parameter typically has no effect, so we simply verify that the
 * parameter appears in the request, and that the parameter is not rejected by
 * the server.  To verify that the parameter appears in the request we rely
 * on the logging facilities in the library, which is ugly to do.
 */
TEST_P(ObjectInsertIntegrationTest, InsertWithUserIpBlank) {
  auto opts = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(opts);
  Client client((*std::move(opts))
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  // Make sure at least one connection was created before we run the test, the
  // IP address can only be obtained once the first request to a given endpoint
  // is completed.
  {
    auto seed_object_name = MakeRandomObjectName();
    auto insert =
        client.InsertObject(bucket_name(), seed_object_name, LoremIpsum());
    ASSERT_STATUS_OK(insert);
    auto status = client.DeleteObject(bucket_name(), seed_object_name);
    ASSERT_STATUS_OK(status);
  }

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name(), object_name, LoremIpsum(),
                          IfGenerationMatch(0), UserIp(""));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto predicate = [this](std::string const& line) {
    return line.find(" POST ") != std::string::npos &&
           line.find("/b/" + bucket_name() + "/o") != std::string::npos &&
           line.find("userIp=") != std::string::npos;
  };
  auto count = std::count_if(backend->log_lines.begin(),
                             backend->log_lines.end(), predicate);
  EXPECT_LT(0, count) << ", logs matching POST="
                      << GetLinesWith(*backend, " POST ") << "\n";

  auto status = client.DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertWithContentType) {
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), ContentType("text/plain"));
  ASSERT_STATUS_OK(meta);

  EXPECT_EQ("text/plain", meta->content_type());

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertFailure) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);
  EXPECT_EQ(object_name, insert->name());
  EXPECT_EQ(bucket_name(), insert->bucket());

  // This operation should fail because the object already exists.
  StatusOr<ObjectMetadata> failure = client().InsertObject(
      bucket_name(), object_name, expected, IfGenerationMatch(0));
  EXPECT_FALSE(failure.ok()) << "metadata=" << *failure;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

TEST_P(ObjectInsertIntegrationTest, InsertXmlFailure) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already.
  StatusOr<ObjectMetadata> insert = client().InsertObject(
      bucket_name(), object_name, expected, Fields(""), IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert);

  EXPECT_EQ(object_name, insert->name());
  EXPECT_EQ(bucket_name(), insert->bucket());

  // This operation should fail because the object already exists.
  StatusOr<ObjectMetadata> failure = client().InsertObject(
      bucket_name(), object_name, expected, Fields(""), IfGenerationMatch(0));
  EXPECT_FALSE(failure.ok()) << "metadata=" << *failure;

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

INSTANTIATE_TEST_SUITE_P(
    ObjectInsertWithJsonCredentialsTest, ObjectInsertIntegrationTest,
    ::testing::Values("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_JSON"));
INSTANTIATE_TEST_SUITE_P(
    ObjectInsertWithP12CredentialsTest, ObjectInsertIntegrationTest,
    ::testing::Values("GOOGLE_CLOUD_CPP_STORAGE_TEST_KEY_FILE_P12"));

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
