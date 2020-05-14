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
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/log.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/expect_exception.h"
#include <gmock/gmock.h>
#include <regex>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::testing::HasSubstr;

using ObjectHashIntegrationTest =
    ::google::cloud::storage::testing::StorageIntegrationTest;

/// @test Verify that MD5 hashes are computed by default.
TEST_F(ObjectHashIntegrationTest, DefaultMD5HashXML) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name(), object_name, LoremIpsum(),
                          IfGenerationMatch(0), Fields(""));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto count =
      std::count_if(backend->log_lines.begin(), backend->log_lines.end(),
                    [](std::string const& line) {
                      return line.rfind("x-goog-hash: md5=", 0) == 0;
                    });
  EXPECT_EQ(1, count);

  auto status = client.DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that MD5 hashes are computed by default.
TEST_F(ObjectHashIntegrationTest, DefaultMD5HashJSON) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [](std::string const& line) {
        // This is a big indirect, we detect if the upload changed to
        // multipart/related, and if so, we assume the hash value is being used.
        // Unfortunately I (@coryan) cannot think of a way to examine the upload
        // contents.
        return line.rfind("content-type: multipart/related; boundary=", 0) == 0;
      });
  EXPECT_EQ(1, count);

  if (insert_meta->has_metadata("x_testbench_upload")) {
    // When running against the testbench, we have some more information to
    // verify the right upload type and contents were sent.
    EXPECT_EQ("multipart", insert_meta->metadata("x_testbench_upload"));
    ASSERT_TRUE(insert_meta->has_metadata("x_testbench_md5"));
    auto expected_md5 = ComputeMD5Hash(LoremIpsum());
    EXPECT_EQ(expected_md5, insert_meta->metadata("x_testbench_md5"));
  }

  auto status = client.DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that `DisableMD5Hash` actually disables the header.
TEST_F(ObjectHashIntegrationTest, DisableMD5HashXML) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta = client.InsertObject(
      bucket_name(), object_name, LoremIpsum(), IfGenerationMatch(0),
      DisableMD5Hash(true), Fields(""));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto count =
      std::count_if(backend->log_lines.begin(), backend->log_lines.end(),
                    [](std::string const& line) {
                      return line.rfind("x-goog-hash: md5=", 0) == 0;
                    });
  EXPECT_EQ(0, count);

  auto status = client.DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that `DisableMD5Hash` actually disables the payload.
TEST_F(ObjectHashIntegrationTest, DisableMD5HashJSON) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  Client client((*client_options)
                    .set_enable_raw_client_tracing(true)
                    .set_enable_http_tracing(true));
  auto object_name = MakeRandomObjectName();

  auto backend = std::make_shared<testing_util::CaptureLogLinesBackend>();
  auto id = LogSink::Instance().AddBackend(backend);
  StatusOr<ObjectMetadata> insert_meta =
      client.InsertObject(bucket_name(), object_name, LoremIpsum(),
                          IfGenerationMatch(0), DisableMD5Hash(true));
  ASSERT_STATUS_OK(insert_meta);

  LogSink::Instance().RemoveBackend(id);

  auto count = std::count_if(
      backend->log_lines.begin(), backend->log_lines.end(),
      [](std::string const& line) {
        // This is a big indirect, we detect if the upload changed to
        // multipart/related, and if so, we assume the hash value is being used.
        // Unfortunately I (@coryan) cannot think of a way to examine the upload
        // contents.
        return line.rfind("content-type: multipart/related; boundary=", 0) == 0;
      });
  EXPECT_EQ(0, count);

  if (insert_meta->has_metadata("x_testbench_upload")) {
    // When running against the testbench, we have some more information to
    // verify the right upload type and contents were sent.
    EXPECT_EQ("simple", insert_meta->metadata("x_testbench_upload"));
    ASSERT_FALSE(insert_meta->has_metadata("x_testbench_md5"));
  }

  auto status = client.DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that MD5 hashes are computed by default on downloads.
TEST_F(ObjectHashIntegrationTest, DefaultMD5StreamingReadXML) {
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client().ReadObject(bucket_name(), object_name);
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->md5_hash()));

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that MD5 hashes are computed by default on downloads.
TEST_F(ObjectHashIntegrationTest, DefaultMD5StreamingReadJSON) {
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client().ReadObject(bucket_name(), object_name,
                                    IfMetagenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_EQ(stream.received_hash(), stream.computed_hash());
  EXPECT_THAT(stream.received_hash(), HasSubstr(meta->md5_hash()));

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that hashes and checksums can be disabled on downloads.
TEST_F(ObjectHashIntegrationTest, DisableHashesStreamingReadXML) {
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream =
      client().ReadObject(bucket_name(), object_name, DisableMD5Hash(true),
                          DisableCrc32cChecksum(true));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_TRUE(stream.computed_hash().empty());
  EXPECT_TRUE(stream.received_hash().empty());

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that hashes and checksums can be disabled on downloads.
TEST_F(ObjectHashIntegrationTest, DisableHashesStreamingReadJSON) {
  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client().ReadObject(
      bucket_name(), object_name, DisableMD5Hash(true),
      DisableCrc32cChecksum(true), IfMetagenerationNotMatch(0));
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  ASSERT_FALSE(stream.IsOpen());
  ASSERT_FALSE(actual.empty());

  EXPECT_TRUE(stream.computed_hash().empty());
  EXPECT_TRUE(stream.received_hash().empty());

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that MD5 hashes are computed by default on uploads.
TEST_F(ObjectHashIntegrationTest, DefaultMD5StreamingWriteJSON) {
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  auto expected_md5hash = ComputeMD5Hash(expected.str());

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_md5hash));

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify MD5 hash value before upload.
TEST_F(ObjectHashIntegrationTest, VerifyValidMD5StreamingWriteJSON) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();
  auto expected_md5hash = ComputeMD5Hash(expected);

  // Create the object, but only if it does not exist already.
  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0),
                           MD5HashValue(expected_md5hash));

  os.exceptions(std::ios_base::failbit);
  os << expected;
  os.Close();

  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(os.received_hash(), os.computed_hash());
  EXPECT_THAT(os.received_hash(), HasSubstr(expected_md5hash));

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify invalid MD5 hash value before upload.
TEST_F(ObjectHashIntegrationTest, InvalidMD5StreamingWriteJSON) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already. Dummy MD5HasValue
  // is passed during WriteObject.
  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0),
                           MD5HashValue("AAAAAAAAAA+AAAAAAAAAAA=="));

  os.exceptions(std::ios_base::failbit);
  os << expected;
  os.Close();

  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().status().ok());
}

/// @test Verify MD5 hashe before upload.
TEST_F(ObjectHashIntegrationTest, InvalidMD5StreamingWriteXML) {
  auto object_name = MakeRandomObjectName();

  std::string expected = LoremIpsum();

  // Create the object, but only if it does not exist already. Dummy MD5HasValue
  // is passed during WriteObject.
  auto os = client().WriteObject(bucket_name(), object_name,
                                 IfGenerationMatch(0), Projection::Full(),
                                 MD5HashValue("AAAAAAAAAA+AAAAAAAAAAA=="));
  os.exceptions(std::ios_base::failbit);
  os << expected;
  os.Close();

  EXPECT_TRUE(os.bad());
  EXPECT_FALSE(os.metadata().status().ok());
}

/// @test Verify that hashes and checksums can be disabled in uploads.
TEST_F(ObjectHashIntegrationTest, DisableHashesStreamingWriteJSON) {
  auto object_name = MakeRandomObjectName();

  // Create the object, but only if it does not exist already.
  auto os =
      client().WriteObject(bucket_name(), object_name, IfGenerationMatch(0),
                           DisableMD5Hash(true), DisableCrc32cChecksum(true));
  os.exceptions(std::ios_base::failbit);
  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;
  WriteRandomLines(os, expected);

  os.Close();
  ObjectMetadata meta = os.metadata().value();
  EXPECT_TRUE(os.received_hash().empty());
  EXPECT_TRUE(os.computed_hash().empty());

  auto status = client().DeleteObject(bucket_name(), object_name);
  ASSERT_STATUS_OK(status);
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectHashIntegrationTest, MismatchedMD5StreamingReadXML) {
  // This test is disabled when not using the testbench as it relies on the
  // testbench to inject faults.
  if (!UsingTestbench()) GTEST_SKIP();

  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client().ReadObject(
      bucket_name(), object_name, DisableCrc32cChecksum(true),
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        std::string actual(std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{});
      } catch (HashMismatchError const& ex) {
        EXPECT_NE(ex.received_hash(), ex.computed_hash());
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->md5_hash());
  EXPECT_FALSE(stream.status().ok());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  auto status = client().DeleteObject(bucket_name(), object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectHashIntegrationTest, MismatchedMD5StreamingReadJSON) {
  // This test is disabled when not using the testbench as it relies on the
  // testbench to inject faults.
  if (!UsingTestbench()) GTEST_SKIP();

  auto object_name = MakeRandomObjectName();

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, LoremIpsum(),
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client().ReadObject(
      bucket_name(), object_name, DisableCrc32cChecksum(true),
      IfMetagenerationNotMatch(0),
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try {
        std::string actual(std::istreambuf_iterator<char>{stream},
                           std::istreambuf_iterator<char>{});
      } catch (HashMismatchError const& ex) {
        EXPECT_NE(ex.received_hash(), ex.computed_hash());
        EXPECT_THAT(ex.what(), HasSubstr("mismatched hashes"));
        throw;
      },
      HashMismatchError);
#else
  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_FALSE(stream.received_hash().empty());
  EXPECT_FALSE(stream.computed_hash().empty());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

  auto status = client().DeleteObject(bucket_name(), object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that MD5 hash mismatches are reported when using .read().
TEST_F(ObjectHashIntegrationTest, MismatchedMD5StreamingReadXMLRead) {
  // This test is disabled when not using the testbench as it relies on the
  // testbench to inject faults.
  if (!UsingTestbench()) GTEST_SKIP();

  auto object_name = MakeRandomObjectName();
  auto contents = MakeRandomData(1024 * 1024);

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, contents,
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client().ReadObject(
      bucket_name(), object_name, DisableCrc32cChecksum(true),
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

  // Create a buffer large enough to hold the results and read pas EOF.
  std::vector<char> buffer(2 * contents.size());
  stream.read(buffer.data(), buffer.size());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_TRUE(stream.bad());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_EQ(StatusCode::kDataLoss, stream.status().code());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->md5_hash());

  auto status = client().DeleteObject(bucket_name(), object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that MD5 hash mismatches are reported when using .read().
TEST_F(ObjectHashIntegrationTest, MismatchedMD5StreamingReadJSONRead) {
  // This test is disabled when not using the testbench as it relies on the
  // testbench to inject faults.
  if (!UsingTestbench()) GTEST_SKIP();

  auto object_name = MakeRandomObjectName();
  auto contents = MakeRandomData(1024 * 1024);

  // Create an object and a stream to read it back.
  StatusOr<ObjectMetadata> meta =
      client().InsertObject(bucket_name(), object_name, contents,
                            IfGenerationMatch(0), Projection::Full());
  ASSERT_STATUS_OK(meta);

  auto stream = client().ReadObject(
      bucket_name(), object_name, DisableCrc32cChecksum(true),
      IfMetagenerationNotMatch(0),
      CustomHeader("x-goog-testbench-instructions", "return-corrupted-data"));

  // Create a buffer large enough to hold the results and read pas EOF.
  std::vector<char> buffer(2 * contents.size());
  stream.read(buffer.data(), buffer.size());
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_TRUE(stream.bad());
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_EQ(StatusCode::kDataLoss, stream.status().code());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());
  EXPECT_EQ(stream.received_hash(), meta->md5_hash());

  auto status = client().DeleteObject(bucket_name(), object_name);
  EXPECT_STATUS_OK(status);
}

/// @test Verify that MD5 hash mismatches are reported by default on downloads.
TEST_F(ObjectHashIntegrationTest, MismatchedMD5StreamingWriteJSON) {
  // This test is disabled when not using the testbench as it relies on the
  // testbench to inject faults.
  if (!UsingTestbench()) GTEST_SKIP();

  auto object_name = MakeRandomObjectName();

  // Create a stream to upload an object.
  ObjectWriteStream stream =
      client().WriteObject(bucket_name(), object_name,
                           DisableCrc32cChecksum(true), IfGenerationMatch(0),
                           CustomHeader("x-goog-testbench-instructions",
                                        "inject-upload-data-error"));
  stream << LoremIpsum() << "\n";
  stream << LoremIpsum();

  stream.Close();
  EXPECT_TRUE(stream.bad());
  EXPECT_STATUS_OK(stream.metadata());
  EXPECT_NE(stream.received_hash(), stream.computed_hash());

  auto status = client().DeleteObject(bucket_name(), object_name);
  EXPECT_STATUS_OK(status);
}

}  // anonymous namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
