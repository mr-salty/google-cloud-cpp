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

#include "google/cloud/storage/internal/curl_client.h"
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

using CurlResumableUploadIntegrationTest =
    ::google::cloud::storage::testing::StorageIntegrationTest;

TEST_F(CurlResumableUploadIntegrationTest, Simple) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto curl_client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name(), object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      curl_client->CreateResumableSession(request);

  ASSERT_STATUS_OK(session);

  std::string const contents = LoremIpsum();
  StatusOr<ResumableUploadResponse> response =
      (*session)->UploadFinalChunk(contents, contents.size());

  ASSERT_STATUS_OK(response);
  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name(), metadata.bucket());
  EXPECT_EQ(contents.size(), metadata.size());

  auto status = curl_client->DeleteObject(
      DeleteObjectRequest(bucket_name(), object_name));
  ASSERT_STATUS_OK(status);
}

TEST_F(CurlResumableUploadIntegrationTest, WithReset) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto curl_client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name(), object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      curl_client->CreateResumableSession(request);

  ASSERT_STATUS_OK(session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');
  StatusOr<ResumableUploadResponse> response =
      (*session)->UploadChunk(contents);
  ASSERT_STATUS_OK(response.status());

  response = (*session)->ResetSession();
  ASSERT_STATUS_OK(response);

  response = (*session)->UploadFinalChunk(contents, 2 * contents.size());
  ASSERT_STATUS_OK(response);

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name(), metadata.bucket());
  EXPECT_EQ(2 * contents.size(), metadata.size());

  auto status = curl_client->DeleteObject(
      DeleteObjectRequest(bucket_name(), object_name));
  ASSERT_STATUS_OK(status);
}

TEST_F(CurlResumableUploadIntegrationTest, Restore) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto curl_client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name(), object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> old_session;
  old_session = curl_client->CreateResumableSession(request);

  ASSERT_STATUS_OK(old_session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');

  StatusOr<ResumableUploadResponse> response =
      (*old_session)->UploadChunk(contents);
  ASSERT_STATUS_OK(response.status());

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      curl_client->RestoreResumableSession((*old_session)->session_id());
  EXPECT_EQ(contents.size(), (*session)->next_expected_byte());
  old_session->reset();

  response = (*session)->UploadChunk(contents);
  ASSERT_STATUS_OK(response);

  response = (*session)->UploadFinalChunk(contents, 3 * contents.size());
  ASSERT_STATUS_OK(response);

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name(), metadata.bucket());
  EXPECT_EQ(3 * contents.size(), metadata.size());

  auto status = curl_client->DeleteObject(
      DeleteObjectRequest(bucket_name(), object_name));
  ASSERT_STATUS_OK(status);
}

TEST_F(CurlResumableUploadIntegrationTest, EmptyTrailer) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto curl_client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name(), object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      curl_client->CreateResumableSession(request);

  ASSERT_STATUS_OK(session);

  std::string const contents(UploadChunkRequest::kChunkSizeQuantum, '0');
  // Send 2 chunks sized to be round quantums.
  StatusOr<ResumableUploadResponse> response =
      (*session)->UploadChunk(contents);
  ASSERT_STATUS_OK(response.status());
  response = (*session)->UploadChunk(contents);
  ASSERT_STATUS_OK(response.status());

  // Consider a streaming upload where the application flushes before closing
  // the stream *and* the flush sends all the data remaining in the stream.
  // This can happen naturally when the upload is a round multiple of the
  // upload quantum. In this case the stream is terminated by sending an empty
  // chunk at the end, with the size of the previous chunks as an indication
  // of "done".
  response = (*session)->UploadFinalChunk(std::string{}, 2 * contents.size());
  ASSERT_STATUS_OK(response.status());

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name(), metadata.bucket());
  EXPECT_EQ(2 * contents.size(), metadata.size());

  auto status = curl_client->DeleteObject(
      DeleteObjectRequest(bucket_name(), object_name));
  ASSERT_STATUS_OK(status);
}

TEST_F(CurlResumableUploadIntegrationTest, Empty) {
  auto client_options = ClientOptions::CreateDefaultClientOptions();
  ASSERT_STATUS_OK(client_options);
  auto curl_client = CurlClient::Create(*client_options);
  auto object_name = MakeRandomObjectName();

  ResumableUploadRequest request(bucket_name(), object_name);
  request.set_multiple_options(IfGenerationMatch(0));

  StatusOr<std::unique_ptr<ResumableUploadSession>> session =
      curl_client->CreateResumableSession(request);

  ASSERT_STATUS_OK(session);

  auto response = (*session)->UploadFinalChunk(std::string{}, 0);
  ASSERT_STATUS_OK(response.status());

  EXPECT_TRUE(response->payload.has_value());
  auto metadata = *response->payload;
  EXPECT_EQ(object_name, metadata.name());
  EXPECT_EQ(bucket_name(), metadata.bucket());
  EXPECT_EQ(0, metadata.size());

  auto status = curl_client->DeleteObject(
      DeleteObjectRequest(bucket_name(), object_name));
  ASSERT_STATUS_OK(status);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
