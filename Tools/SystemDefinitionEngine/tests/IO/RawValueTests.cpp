/// @file RawValueTests.cpp
/// @brief Tests for RawValue type helpers: Integer/Number distinction, AsDouble coercion.

#include "SDE/IO/ModelLoader.h"

#include <gtest/gtest.h>

using namespace SDE;

TEST(RawValueTest, IntegerVariantIsInteger)
{
    RawValue rv;
    rv.data = static_cast<RawInteger>(42);
    EXPECT_TRUE(rv.IsInteger());
    EXPECT_TRUE(rv.IsNumber());
    EXPECT_FALSE(rv.IsText());
}

TEST(RawValueTest, NumberVariantIsNotInteger)
{
    RawValue rv;
    rv.data = static_cast<RawNumber>(3.14);
    EXPECT_FALSE(rv.IsInteger());
    EXPECT_TRUE(rv.IsNumber());
}

TEST(RawValueTest, AsDoubleConvertsInteger)
{
    RawValue rv;
    rv.data = static_cast<RawInteger>(7);
    EXPECT_DOUBLE_EQ(rv.AsDouble(), 7.0);
}

TEST(RawValueTest, AsDoubleReturnsNumber)
{
    RawValue rv;
    rv.data = static_cast<RawNumber>(2.5);
    EXPECT_DOUBLE_EQ(rv.AsDouble(), 2.5);
}

TEST(RawValueTest, AsIntegerReturnsExactValue)
{
    RawValue rv;
    rv.data = static_cast<RawInteger>(-100);
    EXPECT_EQ(rv.AsInteger(), -100);
}

TEST(RawValueTest, TextVariantIsText)
{
    RawValue rv;
    rv.data = RawText{"hello"};
    EXPECT_TRUE(rv.IsText());
    EXPECT_FALSE(rv.IsNumber());
}

TEST(RawValueTest, BoolVariantIsBool)
{
    RawValue rv;
    rv.data = static_cast<RawBool>(true);
    EXPECT_TRUE(rv.IsBool());
}

TEST(RawValueTest, NullVariantIsNull)
{
    RawValue rv;
    rv.data = RawNull{};
    EXPECT_TRUE(rv.IsNull());
    EXPECT_FALSE(rv.IsNumber());
}

TEST(RawValueTest, ArrayVariantIsArray)
{
    RawValue rv;
    rv.data = RawArray{};
    EXPECT_TRUE(rv.IsArray());
}

TEST(RawValueTest, ObjectVariantIsObject)
{
    RawValue rv;
    rv.data = RawObject{};
    EXPECT_TRUE(rv.IsObject());
}

TEST(RawValueTest, LocationDefaultsToZero)
{
    RawValue rv;
    EXPECT_EQ(rv.location.line, 0);
    EXPECT_EQ(rv.location.column, 0);
}

TEST(RawValueTest, LocationPreservedOnConstruction)
{
    RawValue rv;
    rv.location = {"data.json", 10, 5};
    rv.data     = static_cast<RawInteger>(99);
    EXPECT_EQ(rv.location.file, "data.json");
    EXPECT_EQ(rv.location.line, 10);
}
