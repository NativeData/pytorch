#include <gtest/gtest.h>

#include <test/cpp/jit/test_utils.h>
#include <sstream>

#include <torch/csrc/jit/serialization/export.h>
#include <torch/csrc/jit/serialization/import.h>
#include <torch/csrc/jit/serialization/import_source.h>
#include <torch/torch.h>

#include "caffe2/serialize/istream_adapter.h"

namespace torch {
namespace jit {

TEST(SerializationTest, ExtraFilesHookPreference) {
  // Tests that an extra file written explicitly has precedence over
  //   extra files written by a hook
  // TODO: test for the warning, too
  const auto script = R"JIT(
    def forward(self):
        x = torch.rand(5, 5)
        x = x.mm(x)
        return x
  )JIT";

  auto module =
      std::make_shared<Module>("Module", std::make_shared<CompilationUnit>());
  module->define(script);
  std::ostringstream oss;
  std::unordered_map<std::string, std::string> extra_files;
  extra_files["metadata.json"] = "abc";
  SetExportModuleExtraFilesHook([](const Module&) -> ExtraFilesMap {
    return {{"metadata.json", "def"}};
  });
  module->save(oss, extra_files);
  SetExportModuleExtraFilesHook(nullptr);

  std::istringstream iss(oss.str());
  caffe2::serialize::IStreamAdapter adapter{&iss};
  std::unordered_map<std::string, std::string> loaded_extra_files;
  loaded_extra_files["metadata.json"] = "";
  auto loaded_module = torch::jit::load(iss, torch::kCPU, loaded_extra_files);
  ASSERT_EQ(loaded_extra_files["metadata.json"], "abc");
}

TEST(SerializationTest, ExtraFileHooksNoSecret) {
  // no secrets
  std::stringstream ss;
  {
    Module m("__torch__.m");
    ExtraFilesMap extra;
    extra["metadata.json"] = "abc";
    m.save(ss, extra);
  }
  ss.seekg(0);
  {
    ExtraFilesMap extra;
    extra["metadata.json"] = "";
    extra["secret.json"] = "";
    jit::load(ss, c10::nullopt, extra);
    ASSERT_EQ(extra["metadata.json"], "abc");
    ASSERT_EQ(extra["secret.json"], "");
  }
}

TEST(SerializationTest, ExtraFileHooksWithSecret) {
  std::stringstream ss;
  {
    SetExportModuleExtraFilesHook([](const Module&) -> ExtraFilesMap {
      return {{"secret.json", "topsecret"}};
    });
    Module m("__torch__.m");
    ExtraFilesMap extra;
    extra["metadata.json"] = "abc";
    m.save(ss, extra);
    SetExportModuleExtraFilesHook(nullptr);
  }
  ss.seekg(0);
  {
    ExtraFilesMap extra;
    extra["metadata.json"] = "";
    extra["secret.json"] = "";
    jit::load(ss, c10::nullopt, extra);
    ASSERT_EQ(extra["metadata.json"], "abc");
    ASSERT_EQ(extra["secret.json"], "topsecret");
  }
}

TEST(SerializationTest, TypeTags) {
  auto list = c10::List<c10::List<int64_t>>();
  list.push_back(c10::List<int64_t>({1, 2, 3}));
  list.push_back(c10::List<int64_t>({4, 5, 6}));
  auto dict = c10::Dict<std::string, at::Tensor>();
  dict.insert("Hello", torch::ones({2, 2}));
  auto dict_list = c10::List<c10::Dict<std::string, at::Tensor>>();
  for (size_t i = 0; i < 5; i++) {
    auto another_dict = c10::Dict<std::string, at::Tensor>();
    another_dict.insert("Hello" + std::to_string(i), torch::ones({2, 2}));
    dict_list.push_back(another_dict);
  }
  auto tuple = std::tuple<int, std::string>(2, "hi");
  struct TestItem {
    IValue value;
    TypePtr expected_type;
  };
  std::vector<TestItem> items = {
      {list, ListType::create(ListType::create(IntType::get()))},
      {2, IntType::get()},
      {dict, DictType::create(StringType::get(), TensorType::get())},
      {dict_list,
       ListType::create(
           DictType::create(StringType::get(), TensorType::get()))},
      {tuple, TupleType::create({IntType::get(), StringType::get()})}};
  // NOLINTNEXTLINE(performance-for-range-copy)
  for (auto item : items) {
    auto bytes = torch::pickle_save(item.value);
    auto loaded = torch::pickle_load(bytes);
    ASSERT_TRUE(loaded.type()->isSubtypeOf(item.expected_type));
    ASSERT_TRUE(item.expected_type->isSubtypeOf(loaded.type()));
  }
}

TEST(SerializationTest, TestJitStream_CUDA) {
  torch::jit::Module model;
  std::vector<torch::jit::IValue> inputs;
  // Deserialize the ScriptModule from a file using torch::jit::load().
  // Load the scripted model. This should have been generated by tests_setup.py
  // Refer: TorchSaveJitStream_CUDA in test/cpp/jit/tests_setup.py
  model = torch::jit::load("saved_stream_model.pt");

  auto output = model.forward(inputs);
  auto list_of_elements = output.toTuple()->elements();
  auto is_stream_s = list_of_elements[0].toBool();

  // a,b: These are the two input tensors
  // c: This is output tensor generated by the operation torch.cat(a,b)
  auto a = list_of_elements[1].toTensor();
  auto b = list_of_elements[2].toTensor();
  auto c = list_of_elements[3].toTensor();
  // op: this is used to verify if the cat operation produced the same results
  // as that on the GPU with torch.cat
  auto op = at::cat({a, b}, 0);

  // Check if the stream is set
  ASSERT_TRUE(is_stream_s);
  // Check if the sizes of the outputs (op and c) is same on the GPU and CPU
  ASSERT_EQ(op.sizes(), c.sizes());
  // Check if both the output tensors are equal
  ASSERT_TRUE(op.equal(c));
}
} // namespace jit
} // namespace torch
