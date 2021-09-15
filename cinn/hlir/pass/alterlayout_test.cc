#include <gtest/gtest.h>

#include <memory>

#include "cinn/cinn.h"
#include "cinn/frontend/syntax.h"
#include "cinn/hlir/framework/graph.h"
#include "cinn/hlir/framework/graph_compiler.h"
#include "cinn/hlir/framework/pass.h"
#include "cinn/hlir/op/use_ops.h"
#include "cinn/hlir/pass/use_pass.h"

DEFINE_string(model_dir, "", "");

namespace cinn {
namespace frontend {

using hlir::framework::Scope;
using utils::Join;

std::unique_ptr<Program> CreateAddProgram() {
  const int M = 32;
  const int N = 24;

  Placeholder a(Float(32), {M, N});
  Placeholder b(Float(32), {M, N});
  std::unique_ptr<Program> program(new Program);

  auto c = program->add(a, b);
  auto d = program->add(a, c);

  program->SetInputs({a, b});
  program->Validate();

  return program;
}

void SetRandData(const hlir::framework::Tensor& tensor, Target target) {
  auto* data = tensor->mutable_data<float>(target);
  for (size_t j = 0; j < tensor->shape().numel(); j++) {
    data[j] = (rand() * 1.f) / RAND_MAX;  // All random data
  }
}

TEST(conv, conv) {
  Placeholder A(Float(32), {1, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder C(Float(32), {1, 64, 112, 112}, "C");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  auto c = program.conv2d(A, B, attrs);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);

  runtime_program->Execute();
}

TEST(conv_relu_conv, conv_relu_conv) {
  Placeholder A(Float(32), {1, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder C(Float(32), {1, 64, 112, 112}, "C");
  Placeholder D(Float(32), {64, 64, 7, 7}, "D");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  auto c = program.conv2d(A, B, attrs);
  auto d = program.relu(c);
  auto e = program.conv2d(d, D, attrs);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B, D});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");
  scope->Var<hlir::framework::Tensor>("D");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  auto D1 = scope->GetTensor("D");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);
  SetRandData(D1, target);

  runtime_program->Execute();
}

TEST(conv_add_conv, conv_add_conv) {
  Placeholder A(Float(32), {1, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder C(Float(32), {64}, "C");
  Placeholder D(Float(32), {64, 64, 7, 7}, "D");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  auto c = program.conv2d(A, B, attrs);
  auto d = program.elementwise_add(c, C, 1);
  auto e = program.conv2d(d, D, attrs);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B, D});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");
  scope->Var<hlir::framework::Tensor>("D");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  auto D1 = scope->GetTensor("D");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);
  SetRandData(D1, target);

  runtime_program->Execute();
}

TEST(conv_bn_conv, conv_bn_conv) {
  Placeholder A(Float(32), {1, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder D(Float(32), {64, 64, 7, 7}, "D");

  Placeholder Scale(Float(32), {64}, "Scale");
  Placeholder Bias(Float(32), {64}, "Bias");
  Placeholder Mean(Float(32), {64}, "Mean");
  Placeholder Variance(Float(32), {64}, "Variance");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  std::unordered_map<std::string, Program::attr_t> attrs1;
  attrs1["epsilon"] = (float)0.001;

  auto c = program.conv2d(A, B, attrs);
  auto d = program.batchnorm(c, Scale, Bias, Mean, Variance, attrs1);
  auto e = program.conv2d(d, D, attrs);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B, D});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");
  scope->Var<hlir::framework::Tensor>("D");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  auto D1 = scope->GetTensor("D");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);
  SetRandData(D1, target);

  runtime_program->Execute();
}

TEST(conv_pool2d_conv, conv_pool2d_conv) {
  Placeholder A(Float(32), {1, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder C(Float(32), {1, 64, 112, 112}, "C");
  Placeholder D(Float(32), {64, 64, 7, 7}, "D");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  std::unordered_map<std::string, Program::attr_t> attrs2;
  attrs2["stride_size"]  = std::vector<int>({2, 2});
  attrs2["padding_size"] = std::vector<int>({1, 1, 1, 1});
  attrs2["kernel_size"]  = std::vector<int>({3, 3});
  std::string pool_type  = "max";
  attrs2["pool_type"]    = pool_type;

  auto c = program.conv2d(A, B, attrs);
  auto d = program.pool2d(c, attrs2);
  auto e = program.conv2d(d, D, attrs);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B, D});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");
  scope->Var<hlir::framework::Tensor>("D");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  auto D1 = scope->GetTensor("D");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);
  SetRandData(D1, target);

  runtime_program->Execute();
}

TEST(conv_softmax_conv, conv_softmax_conv) {
  Placeholder A(Float(32), {1, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder D(Float(32), {64, 64, 7, 7}, "D");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  std::unordered_map<std::string, Program::attr_t> attrs1;
  attrs1["axis"] = (int)-1;

  auto c = program.conv2d(A, B, attrs);
  auto d = program.softmax(c, attrs1);
  auto e = program.conv2d(d, D, attrs);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B, D});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");
  scope->Var<hlir::framework::Tensor>("D");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  auto D1 = scope->GetTensor("D");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);
  SetRandData(D1, target);

  runtime_program->Execute();
}

TEST(conv_sigmoid_conv, conv_sigmoid_conv) {
  Placeholder A(Float(32), {1, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder D(Float(32), {64, 64, 7, 7}, "D");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  auto c = program.conv2d(A, B, attrs);
  auto d = program.sigmoid(c);
  auto e = program.conv2d(d, D, attrs);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B, D});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");
  scope->Var<hlir::framework::Tensor>("D");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  auto D1 = scope->GetTensor("D");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);
  SetRandData(D1, target);

  runtime_program->Execute();
}

TEST(conv_mul_conv, conv_mul_conv) {
  Placeholder A(Float(32), {3, 3, 224, 224}, "A");
  Placeholder B(Float(32), {64, 3, 7, 7}, "B");
  Placeholder C(Float(32), {1, 64, 112, 112}, "C");
  Placeholder D(Float(32), {64, 64, 7, 7}, "D");

  Program program;
  std::unordered_map<std::string, Program::attr_t> attrs;
  attrs["stride"]        = std::vector<int>({2, 2});
  attrs["dilation"]      = std::vector<int>({1, 1});
  attrs["padding"]       = std::vector<int>({3, 3});
  std::string src_layout = "NCHW";
  attrs["data_format"]   = src_layout;

  std::unordered_map<std::string, Program::attr_t> attrs1;
  attrs1["axis"] = (int)-1;

  auto c = program.conv2d(A, B, attrs);
  auto d = program.mul(c, C, 1, 1);
  auto e = program.softmax(d, attrs1);

  Target target = common::DefaultHostTarget();
  program.SetInputs({A, B, D});
  program.Validate();
  LOG(INFO) << "Program:\n" << program;
  auto graph = std::make_shared<hlir::framework::Graph>(program, target);

  hlir::framework::ApplyPass(graph.get(), "InferShape");
  hlir::framework::ApplyPass(graph.get(), "AlterLayout");
  auto scope = BuildScope(target, graph);
  LOG(INFO) << "graph:\n" << graph->Visualize();

  hlir::framework::GraphCompiler gc(target, scope, graph);
  auto runtime_program = gc.Build();

  scope->Var<hlir::framework::Tensor>("A");
  scope->Var<hlir::framework::Tensor>("B");
  scope->Var<hlir::framework::Tensor>("C");
  scope->Var<hlir::framework::Tensor>("D");

  auto A1 = scope->GetTensor("A");
  auto B1 = scope->GetTensor("B");
  auto C1 = scope->GetTensor("C");
  auto D1 = scope->GetTensor("D");
  SetRandData(A1, target);
  SetRandData(B1, target);
  SetRandData(C1, target);
  SetRandData(D1, target);

  runtime_program->Execute();
}

}  // namespace frontend
}  // namespace cinn
