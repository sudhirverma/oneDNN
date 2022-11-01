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
#include <memory>

#include "interface/c_types_map.hpp"

#include "backend/dnnl/common.hpp"
#include "backend/dnnl/internal_attrs.hpp"
#include "backend/dnnl/layout_propagator.hpp"

#include "gtest/gtest.h"

#include "cpp/unit/backend/dnnl/dnnl_test_common.hpp"
#include "cpp/unit/unit_test_common.hpp"
#include "cpp/unit/utils.hpp"

namespace imp = dnnl::graph::impl;
namespace utils = dnnl::graph::tests::unit::utils;
namespace dnnl_impl = dnnl::graph::impl::dnnl_impl;

TEST(LayoutPropagator, LayoutPropagatorForPermute) {
    auto op = std::make_shared<impl::op_t>(0, impl::op_kind::Wildcard, "op");
    op->set_attr<std::vector<int64_t>>(dnnl_impl::op_attr::permutation, {1, 0});
    auto lt_in = utils::logical_tensor_init(
            0, {1, 1}, impl::data_type::f32, impl::layout_type::any);
    auto lt_out = utils::logical_tensor_init(1, {1, 1}, impl::data_type::f32);

    op->add_input(lt_in);
    op->add_output(lt_out);

    impl::engine_t &eng = get_engine();
    dnnl::engine p_engine = dnnl_impl::make_dnnl_engine(eng);
    dnnl_impl::fusion_info_mgr_t mgr;
    dnnl_impl::pd_cache_t pd_cache;
    auto sg = std::make_shared<dnnl_impl::subgraph_t>(
            std::vector<std::shared_ptr<impl::op_t>> {op}, p_engine,
            impl::fpmath_mode::any, false, false);
    dnnl_impl::subgraph_rewriter_t rewriter {sg};
    ASSERT_EQ(dnnl_impl::layout_propagator_for_permute(
                      op, p_engine, mgr, pd_cache, rewriter),
            impl::status::success);
}

TEST(LayoutPropagator, LayoutPropagatorForReorder) {
    auto op = std::make_shared<impl::op_t>(0, impl::op_kind::Wildcard, "op");
    op->set_attr<std::vector<int64_t>>(dnnl_impl::op_attr::permutation, {1, 0});
    auto lt_in = utils::logical_tensor_init(
            0, {1, 2}, impl::data_type::f32, impl::layout_type::any);
    auto lt_out = utils::logical_tensor_init(
            1, {1, 2}, impl::data_type::f32, impl::layout_type::strided);

    op->add_input(lt_in);
    op->add_output(lt_out);

    impl::engine_t &eng = get_engine();
    dnnl::engine p_engine = dnnl_impl::make_dnnl_engine(eng);
    dnnl_impl::fusion_info_mgr_t mgr;
    dnnl_impl::pd_cache_t pd_cache;
    auto sg = std::make_shared<dnnl_impl::subgraph_t>(
            std::vector<std::shared_ptr<impl::op_t>> {op}, p_engine,
            impl::fpmath_mode::any, false, false);
    dnnl_impl::subgraph_rewriter_t rewriter {sg};
    ASSERT_EQ(layout_propagator_for_reorder(
                      op, p_engine, mgr, pd_cache, rewriter),
            impl::status::success);
}

TEST(LayoutPropagator, LayoutPropagatorForSum) {
    impl::engine_t &eng = get_engine();
    dnnl::engine p_engine = dnnl_impl::make_dnnl_engine(eng);
    dnnl_impl::fusion_info_mgr_t mgr;
    dnnl_impl::pd_cache_t pd_cache;
    {
        auto op = std::make_shared<impl::op_t>(
                0, impl::op_kind::Wildcard, "op");
        auto lt_in = utils::logical_tensor_init(
                1, {1, 2}, impl::data_type::f32, impl::layout_type::strided);
        auto lt_out = utils::logical_tensor_init(
                2, {1, 2}, impl::data_type::f32, impl::layout_type::any);

        op->add_input(lt_in);
        op->add_output(lt_out);
        auto sg = std::make_shared<dnnl_impl::subgraph_t>(
                std::vector<std::shared_ptr<impl::op_t>> {op}, p_engine,
                impl::fpmath_mode::any, false, false);
        dnnl_impl::subgraph_rewriter_t rewriter {sg};
        ASSERT_EQ(layout_propagator_for_sum(
                          op, p_engine, mgr, pd_cache, rewriter),
                impl::status::success);
    }

    {
        auto op = std::make_shared<impl::op_t>(
                0, impl::op_kind::Wildcard, "op");
        auto lt_in = utils::logical_tensor_init(
                0, {1, 2}, impl::data_type::f32, impl::layout_type::any);
        auto lt_out = utils::logical_tensor_init(
                1, {1, 2}, impl::data_type::f32, impl::layout_type::strided);

        op->add_input(lt_in);
        op->add_output(lt_out);
        auto sg = std::make_shared<dnnl_impl::subgraph_t>(
                std::vector<std::shared_ptr<impl::op_t>> {op}, p_engine,
                impl::fpmath_mode::any, false, false);
        dnnl_impl::subgraph_rewriter_t rewriter {sg};
#ifndef NDEBUG
        ASSERT_DEATH(layout_propagator_for_sum(
                             op, p_engine, mgr, pd_cache, rewriter),
                "input format of sum primitive cannot be any.");
#endif
    }
}

TEST(LayoutPropagator, LayoutPropagatorForSubZps) {
    impl::engine_t &eng = get_engine();
    dnnl::engine p_engine = dnnl_impl::make_dnnl_engine(eng);
    dnnl_impl::fusion_info_mgr_t mgr;
    dnnl_impl::pd_cache_t pd_cache;
    auto op = std::make_shared<impl::op_t>(0, impl::op_kind::Wildcard, "op");
    auto sg = std::make_shared<dnnl_impl::subgraph_t>(
            std::vector<std::shared_ptr<impl::op_t>> {op}, p_engine,
            impl::fpmath_mode::any, false, false);
    dnnl_impl::subgraph_rewriter_t rewriter {sg};
#ifndef NDEBUG
    EXPECT_DEATH(dnnl_impl::layout_propagator_for_sub_zps(
                         op, p_engine, mgr, pd_cache, rewriter),
            "");
#else
    ASSERT_EQ(dnnl_impl::layout_propagator_for_sub_zps(
                      op, p_engine, mgr, pd_cache, rewriter),
            impl::status::invalid_op);
#endif
}

TEST(LayoutPropagator, LayoutPropagatorForAddZps) {
    impl::engine_t &eng = get_engine();
    dnnl::engine p_engine = dnnl_impl::make_dnnl_engine(eng);
    dnnl_impl::fusion_info_mgr_t mgr;
    dnnl_impl::pd_cache_t pd_cache;
    auto op = std::make_shared<impl::op_t>(0, impl::op_kind::Wildcard, "op");
    auto sg = std::make_shared<dnnl_impl::subgraph_t>(
            std::vector<std::shared_ptr<impl::op_t>> {op}, p_engine,
            impl::fpmath_mode::any, false, false);
    dnnl_impl::subgraph_rewriter_t rewriter {sg};
#ifndef NDEBUG
    EXPECT_DEATH(dnnl_impl::layout_propagator_for_add_zps(
                         op, p_engine, mgr, pd_cache, rewriter),
            "");
#else
    ASSERT_EQ(dnnl_impl::layout_propagator_for_add_zps(
                      op, p_engine, mgr, pd_cache, rewriter),
            impl::status::invalid_op);
#endif
}
