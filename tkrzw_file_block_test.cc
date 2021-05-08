/*************************************************************************************************
 * Tests for tkrzw_file_block.h
 *
 * Copyright 2020 Google LLC
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License.  You may obtain a copy of the License at
 *     https://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.  See the License for the specific language governing permissions
 * and limitations under the License.
 *************************************************************************************************/

#include "tkrzw_sys_config.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "tkrzw_file.h"
#include "tkrzw_file_block.h"
#include "tkrzw_file_test_common.h"
#include "tkrzw_file_util.h"
#include "tkrzw_lib_common.h"

using namespace testing;

// Main routine
int main(int argc, char** argv) {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

template <class FILEIMPL>
class BlockFileTest : public CommonFileTest {
 protected:
  void BasicTest(FILEIMPL* file);
  void DirectIOTest(FILEIMPL* file);
};

template <class FILEIMPL>
void BlockFileTest<FILEIMPL>::BasicTest(FILEIMPL* file) {
  tkrzw::TemporaryDirectory tmp_dir(true, "tkrzw-");
  const std::string file_path = tmp_dir.MakeUniquePath();
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            tkrzw::WriteFile(file_path, "012345678901234567890123456789"));
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file->SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Open(file_path, true, tkrzw::File::OPEN_DEFAULT));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->SetHeadBuffer(6));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(0, "ab", 2));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(3, "cde", 3));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(5, "EFG", 3));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(7, "gh", 2));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(9, "ij", 2));
  char* aligned = static_cast<char*>(tkrzw::xmallocaligned(8, 8));
  std::memset(aligned, 'Z', 8);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(16, aligned, 8));
  tkrzw::xfreealigned(aligned);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(24, "xx", 2));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(30, "zz", 2));
  char buf[256];
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(0, buf, 4));
  EXPECT_EQ("ab2c", std::string_view(buf, 4));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(4, buf, 4));
  EXPECT_EQ("dEFg", std::string_view(buf, 4));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(6, buf, 4));
  EXPECT_EQ("Fghi", std::string_view(buf, 4));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(8, buf, 4));
  EXPECT_EQ("hij1", std::string_view(buf, 4));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(8, buf, 8));
  EXPECT_EQ("hij12345", std::string_view(buf, 8));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(12, buf, 8));
  EXPECT_EQ("2345ZZZZ", std::string_view(buf, 8));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(0, buf, 32));
  EXPECT_EQ("ab2cdEFghij12345ZZZZZZZZxx6789zz", std::string_view(buf, 32));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(0, buf + 1, 32));
  EXPECT_EQ("ab2cdEFghij12345ZZZZZZZZxx6789zz", std::string_view(buf + 1, 32));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(0, buf + 1, 32));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(31, buf, 1));
  EXPECT_EQ("z", std::string_view(buf, 1));
  EXPECT_EQ(tkrzw::Status::INFEASIBLE_ERROR, file->Read(24, buf, 32));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Close());
  std::string content;
  EXPECT_EQ(tkrzw::Status::SUCCESS, tkrzw::ReadFile(file_path, &content));
  EXPECT_EQ("ab2cdEFghij12345ZZZZZZZZxx6789zz", content);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Open(file_path, true, tkrzw::File::OPEN_TRUNCATE));
  int64_t off = -1;
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Append("012", 3, &off));
  EXPECT_EQ(0, off);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Append("34", 2, &off));
  EXPECT_EQ(3, off);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Append("5678901", 7, &off));
  EXPECT_EQ(5, off);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Append("2345", 4, &off));
  EXPECT_EQ(12, off);
  aligned = static_cast<char*>(tkrzw::xmallocaligned(16, 8));
  std::memset(aligned, 'X', 8);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Append(aligned, 8, &off));
  EXPECT_EQ(16, off);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Append(aligned, 4, &off));
  EXPECT_EQ(24, off);
  tkrzw::xfreealigned(aligned);
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Close());
  EXPECT_EQ(tkrzw::Status::SUCCESS, tkrzw::ReadFile(file_path, &content));
  EXPECT_EQ("0123456789012345XXXXXXXXXXXX", content);
  EXPECT_EQ(28, tkrzw::GetFileSize(file_path));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Open(file_path, false));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(0, buf, 28));
  EXPECT_EQ("0123456789012345XXXXXXXXXXXX", std::string_view(buf, 28));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(6, buf, 10));
  EXPECT_EQ("6789012345", std::string_view(buf, 10));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(14, buf, 10));
  EXPECT_EQ("45XXXXXXXX", std::string_view(buf, 10));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(16, buf, 12));
  EXPECT_EQ("XXXXXXXXXXXX", std::string_view(buf, 12));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Close());
}

template <class FILEIMPL>
void BlockFileTest<FILEIMPL>::DirectIOTest(FILEIMPL* file) {
  tkrzw::TemporaryDirectory tmp_dir(true, "tkrzw-");
  const std::string file_path = tmp_dir.MakeUniquePath();
  constexpr int64_t block_size = 512;
  constexpr int64_t head_buffer_size = block_size * 10;
  constexpr int32_t file_size = block_size * 200;
  constexpr int32_t max_record_size = 1024;
  char* write_buf = static_cast<char*>(tkrzw::xmallocaligned(512, max_record_size));
  char* read_buf = static_cast<char*>(tkrzw::xmallocaligned(512, max_record_size));
  std::mt19937 mt(1);
  std::uniform_int_distribution<int32_t> size_dist(1, max_record_size);
  std::uniform_int_distribution<int32_t> append_dist(0, 1);
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            tkrzw::WriteFile(file_path, "012345678901234567890123456789"));
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file->SetAccessStrategy(block_size, tkrzw::BlockParallelFile::ACCESS_DIRECT));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Open(file_path, true, tkrzw::File::OPEN_TRUNCATE));
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->SetHeadBuffer(head_buffer_size));
  int64_t pos = 0;
  while (pos < file_size + max_record_size * 2) {
    const int32_t record_size = size_dist(mt);
    for (int32_t i = 0; i < record_size; i++) {
      write_buf[i] = 'a' + (pos + i) % 26;
    }
    if (append_dist(mt) == 0) {
      EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(pos, write_buf, record_size));
    } else {
      EXPECT_EQ(tkrzw::Status::SUCCESS, file->Append(write_buf, record_size));
    }
    pos += record_size;
  }
  pos = 0;
  while (pos < file_size + max_record_size) {
    const int32_t record_size = size_dist(mt);
    for (int32_t i = 0; i < record_size; i++) {
      write_buf[i] = 'A' + (pos + i) % 26;
    }
    EXPECT_EQ(tkrzw::Status::SUCCESS, file->Write(pos, write_buf, record_size));
    pos += record_size + size_dist(mt);
  }
  pos = 0;
  while (pos < file_size + max_record_size) {
    const int32_t record_size = size_dist(mt);
    for (int32_t i = 0; i < record_size; i++) {
      write_buf[i] = 'A' + (pos + i) % 26;
    }
    EXPECT_EQ(tkrzw::Status::SUCCESS, file->Read(pos, read_buf, record_size));
    EXPECT_EQ(tkrzw::StrLowerCase(std::string_view(write_buf, record_size)),
              tkrzw::StrLowerCase(std::string_view(read_buf, record_size)));
    pos += record_size;
  }
  EXPECT_EQ(tkrzw::Status::SUCCESS, file->Close());
  tkrzw::xfreealigned(read_buf);
  tkrzw::xfreealigned(write_buf);
}

class BlockParallelFileTest : public BlockFileTest<tkrzw::BlockParallelFile> {};

TEST_F(BlockParallelFileTest, Attributes) {
  tkrzw::BlockParallelFile file;
  EXPECT_FALSE(file.IsMemoryMapping());
  EXPECT_FALSE(file.IsAtomic());
}

TEST_F(BlockParallelFileTest, Basic) {
  tkrzw::BlockParallelFile file;
  BasicTest(&file);
}

TEST_F(BlockParallelFileTest, DirectIO) {
  tkrzw::BlockParallelFile file;
  DirectIOTest(&file);
}

TEST_F(BlockParallelFileTest, EmptyFile) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  EmptyFileTest(&file);
}

TEST_F(BlockParallelFileTest, SimpleRead) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  SimpleReadTest(&file);
}

TEST_F(BlockParallelFileTest, SimpleWrite) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  SimpleWriteTest(&file);
}

TEST_F(BlockParallelFileTest, ReallocWrite) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  ReallocWriteTest(&file);
}

TEST_F(BlockParallelFileTest, ImplicitClose) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  ImplicitCloseTest(&file);
}

TEST_F(BlockParallelFileTest, OpenOptions) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  OpenOptionsTest(&file);
}

TEST_F(BlockParallelFileTest, OrderedThread) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(100, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  OrderedThreadTest(&file);
}

TEST_F(BlockParallelFileTest, RandomThread) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(32, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  RandomThreadTest(&file);
}

TEST_F(BlockParallelFileTest, FileReader) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  FileReaderTest(&file);
}

TEST_F(BlockParallelFileTest, FlatRecord) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(16, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  FlatRecordTest(&file);
}

TEST_F(BlockParallelFileTest, Rename) {
  tkrzw::BlockParallelFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockParallelFile::ACCESS_DEFAULT));
  RenameTest(&file);
}

class BlockAtomicFileTest : public BlockFileTest<tkrzw::BlockAtomicFile> {};

TEST_F(BlockAtomicFileTest, Attributes) {
  tkrzw::BlockAtomicFile file;
  EXPECT_FALSE(file.IsMemoryMapping());
  EXPECT_TRUE(file.IsAtomic());
}

TEST_F(BlockAtomicFileTest, Basic) {
  tkrzw::BlockAtomicFile file;
  BasicTest(&file);
}

TEST_F(BlockAtomicFileTest, DirectIO) {
  tkrzw::BlockAtomicFile file;
  DirectIOTest(&file);
}

TEST_F(BlockAtomicFileTest, EmptyFile) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  EmptyFileTest(&file);
}

TEST_F(BlockAtomicFileTest, SimpleRead) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  SimpleReadTest(&file);
}

TEST_F(BlockAtomicFileTest, SimpleWrite) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  SimpleWriteTest(&file);
}

TEST_F(BlockAtomicFileTest, ReallocWrite) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  ReallocWriteTest(&file);
}

TEST_F(BlockAtomicFileTest, ImplicitClose) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  ImplicitCloseTest(&file);
}

TEST_F(BlockAtomicFileTest, OpenOptions) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  OpenOptionsTest(&file);
}

TEST_F(BlockAtomicFileTest, OrderedThread) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(100, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  OrderedThreadTest(&file);
}

TEST_F(BlockAtomicFileTest, RandomThread) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(32, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  RandomThreadTest(&file);
}

TEST_F(BlockAtomicFileTest, FileReader) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  FileReaderTest(&file);
}

TEST_F(BlockAtomicFileTest, FlatRecord) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(16, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  FlatRecordTest(&file);
}

TEST_F(BlockAtomicFileTest, Rename) {
  tkrzw::BlockAtomicFile file;
  EXPECT_EQ(tkrzw::Status::SUCCESS,
            file.SetAccessStrategy(8, tkrzw::BlockAtomicFile::ACCESS_DEFAULT));
  RenameTest(&file);
}

// END OF FILE