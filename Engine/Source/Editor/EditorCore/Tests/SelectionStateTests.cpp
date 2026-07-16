/// @file SelectionStateTests.cpp
/// @brief Unit tests for editor selection filtering and history helpers.

#include "SagaEditor/Selection/SelectionFilter.h"
#include "SagaEditor/Selection/SelectionHistory.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{

using SagaEditor::SelectableId;
using SagaEditor::SelectionFilter;
using SagaEditor::SelectionHistory;

TEST(EditorSelectionFilterTest, AcceptsEveryIdWhenPredicateIsEmpty)
{
    SelectionFilter filter;
    const std::vector<SelectableId> ids{4, 2, 8, 1};

    EXPECT_EQ(filter.Filter(ids), ids);
    EXPECT_TRUE(filter.Passes(42));
}

TEST(EditorSelectionFilterTest, AppliesPredicateWithoutReorderingInput)
{
    SelectionFilter filter;
    filter.SetPredicate(
        [](SelectableId id)
        {
            return (id % 2) == 0;
        });

    const std::vector<SelectableId> ids{7, 2, 4, 5, 8};
    const std::vector<SelectableId> expected{2, 4, 8};

    EXPECT_EQ(filter.Filter(ids), expected);
    EXPECT_FALSE(filter.Passes(7));
    EXPECT_TRUE(filter.Passes(8));
}

TEST(EditorSelectionHistoryTest, EmptyHistoryIsStableAndNonNavigable)
{
    SelectionHistory history;

    EXPECT_FALSE(history.CanGoBack());
    EXPECT_FALSE(history.CanGoForward());
    EXPECT_TRUE(history.GoBack().empty());
    EXPECT_TRUE(history.GoForward().empty());
}

TEST(EditorSelectionHistoryTest, NavigatesBackAndForwardThroughSnapshots)
{
    SelectionHistory history;
    history.Push({1});
    history.Push({2, 3});
    history.Push({4});

    EXPECT_TRUE(history.CanGoBack());
    EXPECT_EQ(history.GoBack(), (std::vector<SelectableId>{2, 3}));
    EXPECT_EQ(history.GoBack(), (std::vector<SelectableId>{1}));
    EXPECT_FALSE(history.CanGoBack());

    EXPECT_TRUE(history.CanGoForward());
    EXPECT_EQ(history.GoForward(), (std::vector<SelectableId>{2, 3}));
    EXPECT_EQ(history.GoForward(), (std::vector<SelectableId>{4}));
    EXPECT_FALSE(history.CanGoForward());
}

TEST(EditorSelectionHistoryTest, PushAfterBackDiscardsForwardBranch)
{
    SelectionHistory history;
    history.Push({1});
    history.Push({2});
    history.Push({3});

    EXPECT_EQ(history.GoBack(), (std::vector<SelectableId>{2}));
    history.Push({9});

    EXPECT_FALSE(history.CanGoForward());
    EXPECT_EQ(history.GoBack(), (std::vector<SelectableId>{2}));
    EXPECT_EQ(history.GoForward(), (std::vector<SelectableId>{9}));
}

TEST(EditorSelectionHistoryTest, DuplicateCurrentSelectionDoesNotCreateEntry)
{
    SelectionHistory history;
    history.Push({1, 2});
    history.Push({1, 2});

    EXPECT_FALSE(history.CanGoBack());
    EXPECT_FALSE(history.CanGoForward());
    EXPECT_EQ(history.GoBack(), (std::vector<SelectableId>{1, 2}));
}

} // namespace
