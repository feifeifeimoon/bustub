//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// order_by_index_scan.cpp
//
// Identification: src/optimizer/order_by_index_scan.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <memory>

#include "binder/bound_order_by.h"
#include "catalog/catalog.h"
#include "catalog/column.h"
#include "catalog/schema.h"
#include "common/exception.h"
#include "common/macros.h"
#include "execution/expressions/column_value_expression.h"
#include "execution/expressions/constant_value_expression.h"
#include "execution/plans/abstract_plan.h"
#include "execution/plans/filter_plan.h"
#include "execution/plans/index_scan_plan.h"
#include "execution/plans/nested_loop_join_plan.h"
#include "execution/plans/projection_plan.h"
#include "execution/plans/seq_scan_plan.h"
#include "execution/plans/sort_plan.h"
#include "optimizer/optimizer.h"
#include "type/type_id.h"

namespace bustub {

/**
 * @brief optimize order by as index scan if there's an index on a table
 */
auto Optimizer::OptimizeOrderByAsIndexScan(const AbstractPlanNodeRef &plan) -> AbstractPlanNodeRef {
  std::vector<AbstractPlanNodeRef> children;
  for (const auto &child : plan->GetChildren()) {
    children.emplace_back(OptimizeOrderByAsIndexScan(child));
  }
  auto optimized_plan = plan->CloneWithChildren(std::move(children));

  if (optimized_plan->GetType() == PlanType::Sort) {
    const auto &sort_plan = dynamic_cast<const SortPlanNode &>(*optimized_plan);
    const auto &order_bys = sort_plan.GetOrderBy();

    std::vector<uint32_t> order_by_column_ids;
    for (const auto &[order_type, expr] : order_bys) {
      // Order type is asc or default
      if (!(order_type == OrderByType::ASC || order_type == OrderByType::DEFAULT)) {
        return optimized_plan;
      }

      // Order expression is a column value expression
      const auto *column_value_expr = dynamic_cast<ColumnValueExpression *>(expr.get());
      if (column_value_expr == nullptr) {
        return optimized_plan;
      }

      order_by_column_ids.push_back(column_value_expr->GetColIdx());
    }

    // Has exactly one child
    BUSTUB_ENSURE(optimized_plan->children_.size() == 1, "Sort with multiple children?? Impossible!");
    const auto &child_plan = optimized_plan->children_[0];

    if (child_plan->GetType() == PlanType::SeqScan) {
      const auto &seq_scan = dynamic_cast<const SeqScanPlanNode &>(*child_plan);
      const auto table_info = catalog_.GetTable(seq_scan.GetTableOid());
      const auto indices = catalog_.GetTableIndexes(table_info->name_);

      for (const auto &index : indices) {
        const auto &columns = index->index_->GetKeyAttrs();
        if (order_by_column_ids == columns) {
          return std::make_shared<IndexScanPlanNode>(optimized_plan->output_schema_, table_info->oid_,
                                                     index->index_oid_);
        }
      }
    }
  }

  return optimized_plan;
}

}  // namespace bustub
