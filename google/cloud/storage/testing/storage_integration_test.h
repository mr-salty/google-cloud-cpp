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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/well_known_headers.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/status_or.h"
#include <gmock/gmock.h>
#include <cstddef>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
namespace testing {
/**
 * Common class for storage integration tests.
 */
class StorageIntegrationTest : public ::testing::Test {
 protected:
  // Get the number of files open in this process.
  static StatusOr<std::size_t> GetNumOpenFiles();

  // Normally called automatically, but if you derive from this class and
  // override `SetUp()`, ensure you call this `SetUp()` explicitly.
  void SetUp() override;

  // These accessors must only be used after `SetUp()`.
  Client client() const { return *client_; }
  std::string const& project_id() const { return project_id_; }
  std::string const& bucket_name() const { return bucket_name_; }
  std::string const& test_service_account() const {
    return test_service_account_;
  }
  std::string const& test_signing_service_account() const {
    return test_signing_service_account_;
  }
  google::cloud::internal::DefaultPRNG& generator() { return generator_; }

  std::string MakeRandomBucketName();
  std::string MakeRandomObjectName();
  std::string MakeRandomFilename();
  std::string MakeEntityName();

  static std::string LoremIpsum();

  EncryptionKeyData MakeEncryptionKeyData();

  static constexpr int kDefaultRandomLineCount = 1000;
  static constexpr int kDefaultLineSize = 200;

  void WriteRandomLines(std::ostream& upload, std::ostream& local,
                        int line_count = kDefaultRandomLineCount,
                        int line_size = kDefaultLineSize);

  std::string MakeRandomData(std::size_t desired_size);

  static bool UsingTestbench();

  // Tests should generally use the `Client` returned by `client()` but these
  // are supplied for tests that need to create multiple `Client`s or change
  // the retry policy.
  static StatusOr<Client> MakeIntegrationTestClient();
  static StatusOr<Client> MakeIntegrationTestClient(
      std::unique_ptr<RetryPolicy> retry_policy);

 private:
  static std::unique_ptr<BackoffPolicy> TestBackoffPolicy();
  static std::unique_ptr<RetryPolicy> TestRetryPolicy();

  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();
  StatusOr<Client> client_;
  std::string project_id_;
  std::string bucket_name_;
  std::string test_service_account_;
  std::string test_signing_service_account_;
};

/**
 * Common class for storage integration tests that use a HMAC Service Account.
 */
class StorageIntegrationTestWithHmacServiceAccount
    : public StorageIntegrationTest {
 protected:
  void SetUp() override;

  std::string const& service_account() const { return service_account_; }

 private:
  std::string service_account_;
};

/**
 * Tests that a callable reports permanent errors correctly.
 *
 * @param callable the function / code snippet under test. This is typically a
 *     lambda expression that exercises some code path expected to report
 *     a permanent failure.
 * @tparam Callable the type of @p callable.
 */
template <typename Callable>
void TestPermanentFailure(Callable&& callable) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(
      try { callable(); } catch (std::runtime_error const& ex) {
        EXPECT_THAT(ex.what(), ::testing::HasSubstr("Permanent error in"));
        throw;
      },
      std::runtime_error);
#else
  EXPECT_DEATH_IF_SUPPORTED(callable(), "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

/**
 * Count the number of *AccessControl entities with matching name and role.
 */
template <typename AccessControlResource>
typename std::vector<AccessControlResource>::difference_type
CountMatchingEntities(std::vector<AccessControlResource> const& acl,
                      AccessControlResource const& expected) {
  return std::count_if(
      acl.begin(), acl.end(), [&expected](AccessControlResource const& x) {
        return x.entity() == expected.entity() && x.role() == expected.role();
      });
}

}  // namespace testing
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_TESTING_STORAGE_INTEGRATION_TEST_H
