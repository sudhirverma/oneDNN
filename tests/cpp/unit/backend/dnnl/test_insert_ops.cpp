/*******************************************************************************
* Copyright 2022 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <algorithm>
#include <tuple>
#include <vector>

#include "gtest/gtest.h"

#include "interface/graph.hpp"
#include "interface/partition.hpp"

#include "backend/dnnl/dnnl_backend.hpp"
#include "backend/dnnl/dnnl_partition_impl.hpp"

#include "backend/dnnl/kernels/large_partition.hpp"
#include "backend/dnnl/op_executable.hpp"
#include "backend/dnnl/passes/constant_propagation.hpp"
#include "backend/dnnl/passes/insert_ops.hpp"
#include "backend/dnnl/passes/layout_propagation.hpp"
#include "backend/dnnl/passes/lower.hpp"
#include "backend/dnnl/passes/memory_planning.hpp"
#include "backend/dnnl/passes/transform.hpp"

#include "cpp/unit/unit_test_common.hpp"
#include "cpp/unit/utils.hpp"

using namespace dnnl::graph::impl;
using namespace dnnl::graph::impl::op_kind;
using namespace dnnl::graph::tests::unit::utils;

using op_ptr = std::shared_ptr<dnnl::graph::impl::op_t>;

TEST(InsertOps, InsertPermuteForOpOnlyRequireDataFormat) {
    impl::engine_t &g_eng = get_engine();
    dnnl::engine p_eng = impl::dnnl_impl::make_dnnl_engine(g_eng);
    size_t id = 0;

    auto op = std::make_shared<impl::op_t>(id++,
            static_cast<impl::op_kind_t>(dnnl_impl::op_kind::kDnnl_prelu),
            "prelu");

    op->set_attr<std::string>(impl::op_attr::data_format, "NXC");
    op->set_attr<bool>(impl::op_attr::per_channel_broadcast, true);

    impl::dims dims {1, 2, 2, 2};
    impl::dims wei_dims {1, 2, 2, 2};

    impl::logical_tensor_t src_lt
            = logical_tensor_init(id++, dims, impl::data_type::f32);
    impl::logical_tensor_t wei_lt
            = logical_tensor_init(id++, wei_dims, impl::data_type::f32);
    impl::logical_tensor_t dst_lt
            = logical_tensor_init(id++, dims, impl::data_type::f32);

    op->add_input(src_lt);
    op->add_input(wei_lt);
    op->add_output(dst_lt);
    UNUSED(p_eng);
    auto subgraph = std::make_shared<dnnl_impl::subgraph_t>(
            std::vector<std::shared_ptr<impl::op_t>> {op},
            /* reset_layout */ false);
    dnnl_impl::dnnl_backend::get_singleton();

    auto &prelu_op = subgraph->get_ops()[0];
    impl::logical_tensor_t in_lt1
            = logical_tensor_init(id++, dims, impl::data_type::f32);
    prelu_op->add_input(in_lt1);

    auto &mgr = subgraph->fusion_info_mgr_;
    auto key = mgr.init_info();
    prelu_op->set_attr<int64_t>(dnnl_impl::op_attr::fusion_info_key, key);
    auto &fusion_info = mgr.get_mutable_info(key);

    auto post_op = std::make_shared<impl::op_t>(id++,
            static_cast<impl::op_kind_t>(dnnl_impl::op_kind::dnnl_binary),
            "add");
    post_op->set_attr<int64_t>(dnnl_impl::op_attr::alg_kind,
            static_cast<int64_t>(
                    dnnl_impl::get_binary_alg_map().at(impl::op_kind::Add)));
    fusion_info.append_post_binary(post_op, {2});
    ASSERT_EQ(
            dnnl_impl::insert_permute_for_op_only_require_data_format(subgraph),
            impl::status::success);
}

TEST(InsertOps, InsertToGroupForReorder) {
    impl::engine_t &g_eng = get_engine();
    dnnl::engine p_eng = impl::dnnl_impl::make_dnnl_engine(g_eng);
    size_t id = 0;
    using item_type = std::tuple<impl::dims, impl::dims, impl::status_t>;
    std::vector<item_type> items {
            item_type(impl::dims {1, 2, 2, 2}, impl::dims {1, 2, 2, 2},
                    impl::status::success),
            item_type(impl::dims {1, 2, 2, 2}, impl::dims {1, 2, 2},
                    impl::status::unimplemented),
            item_type(impl::dims {1, 2, 2}, impl::dims {1, 2, 2, 2},
                    impl::status::invalid_shape),
            item_type(impl::dims {2, 2, 2}, impl::dims {1, 2, 2, 2},
                    impl::status::success),
            item_type(impl::dims {2}, impl::dims {1, 2, 2, 2},
                    impl::status::invalid_shape),
    };
    for (auto &item : items) {
        auto &in_dims = std::get<0>(item);
        auto &out_dims = std::get<1>(item);
        auto &status = std::get<2>(item);

        auto op = std::make_shared<impl::op_t>(id++,
                static_cast<impl::op_kind_t>(dnnl_impl::op_kind::dnnl_reorder),
                "reorder");
        impl::logical_tensor_t in_lt
                = logical_tensor_init(id++, in_dims, impl::data_type::f32);
        impl::logical_tensor_t out_lt
                = logical_tensor_init(id++, out_dims, impl::data_type::f32);
        op->add_input(in_lt);
        op->add_output(out_lt);
        UNUSED(p_eng);
        auto subgraph = std::make_shared<dnnl_impl::subgraph_t>(
                std::vector<std::shared_ptr<impl::op_t>> {op},
                /* reset_layout */ false);
        ASSERT_EQ(dnnl_impl::insert_to_group_for_reorder(subgraph), status);
    }
}
