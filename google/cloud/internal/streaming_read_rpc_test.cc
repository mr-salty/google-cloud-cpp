// Copyright 2021 Google LLC
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

#include "google/cloud/internal/streaming_read_rpc.h"
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string value;
};

using MockReturnType =
    std::unique_ptr<grpc::ClientReaderInterface<FakeResponse>>;

class MockReader : public grpc::ClientReaderInterface<FakeResponse> {
 public:
  MOCK_METHOD(bool, Read, (FakeResponse*), (override));
  MOCK_METHOD(grpc::Status, Finish, (), (override));

  // Unused. We define them (as they are pure virtual), but not as mocks.
  bool NextMessageSize(std::uint32_t*) override { return true; }
  void WaitForInitialMetadata() override {}
};

TEST(StreamingReadRpcImpl, SuccessfulStream) {
  auto mock = absl::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](FakeResponse* r) {
        r->value = "value-0";
        return true;
      })
      .WillOnce([](FakeResponse* r) {
        r->value = "value-1";
        return true;
      })
      .WillOnce([](FakeResponse* r) {
        r->value = "value-2";
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce(Return(grpc::Status::OK));

  StreamingReadRpcImpl<FakeResponse> impl(
      absl::make_unique<grpc::ClientContext>(), std::move(mock));
  std::vector<std::string> values;
  for (;;) {
    auto v = impl.Read();
    if (absl::holds_alternative<FakeResponse>(v)) {
      values.push_back(absl::get<FakeResponse>(std::move(v)).value);
      continue;
    }
    EXPECT_THAT(absl::get<Status>(std::move(v)), StatusIs(StatusCode::kOk));
    break;
  }
  EXPECT_THAT(values, ElementsAre("value-0", "value-1", "value-2"));
}

TEST(StreamingReadRpcImpl, EmptyStream) {
  auto mock = absl::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read).WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish).WillOnce(Return(grpc::Status::OK));

  StreamingReadRpcImpl<FakeResponse> impl(
      absl::make_unique<grpc::ClientContext>(), std::move(mock));
  auto v = impl.Read();
  ASSERT_FALSE(absl::holds_alternative<FakeResponse>(v));
  EXPECT_THAT(absl::get<Status>(std::move(v)), StatusIs(StatusCode::kOk));
}

TEST(StreamingReadRpcImpl, EmptyWithError) {
  auto mock = absl::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read).WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh")));

  StreamingReadRpcImpl<FakeResponse> impl(
      absl::make_unique<grpc::ClientContext>(), std::move(mock));
  auto v = impl.Read();
  ASSERT_FALSE(absl::holds_alternative<FakeResponse>(v));
  EXPECT_THAT(absl::get<Status>(std::move(v)),
              StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
}

TEST(StreamingReadRpcImpl, ErrorAfterData) {
  auto mock = absl::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](FakeResponse* r) {
        r->value = "test-value-0";
        return true;
      })
      .WillOnce(Return(false));
  EXPECT_CALL(*mock, Finish)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh")));

  StreamingReadRpcImpl<FakeResponse> impl(
      absl::make_unique<grpc::ClientContext>(), std::move(mock));
  std::vector<std::string> values;
  for (;;) {
    auto v = impl.Read();
    if (absl::holds_alternative<FakeResponse>(v)) {
      values.push_back(absl::get<FakeResponse>(std::move(v)).value);
      continue;
    }
    EXPECT_THAT(absl::get<Status>(std::move(v)),
                StatusIs(StatusCode::kPermissionDenied, "uh-oh"));
    break;
  }
  EXPECT_THAT(values, ElementsAre("test-value-0"));
}

TEST(StreamingReadRpcImpl, HandleUnfinished) {
  auto mock = absl::make_unique<MockReader>();
  EXPECT_CALL(*mock, Read)
      .WillOnce([](FakeResponse* r) {
        r->value = "value-0";
        return true;
      })
      .WillOnce([](FakeResponse* r) {
        r->value = "value-1";
        return true;
      });
  EXPECT_CALL(*mock, Finish)
      .WillOnce(
          Return(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, "uh-oh")));

  auto backend =
      std::make_shared<google::cloud::testing_util::CaptureLogLinesBackend>();
  auto id = google::cloud::LogSink::Instance().AddBackend(backend);

  {
    StreamingReadRpcImpl<FakeResponse> impl(
        absl::make_unique<grpc::ClientContext>(), std::move(mock));
    std::vector<std::string> values;
    values.push_back(absl::get<FakeResponse>(impl.Read()).value);
    values.push_back(absl::get<FakeResponse>(impl.Read()).value);
    EXPECT_THAT(values, ElementsAre("value-0", "value-1"));
  }
  EXPECT_THAT(backend->ClearLogLines(),
              Contains(AllOf(HasSubstr("unhandled error"), HasSubstr("status="),
                             HasSubstr("uh-oh"))));

  google::cloud::LogSink::Instance().RemoveBackend(id);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google
