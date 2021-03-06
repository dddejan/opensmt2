//
// Created by Martin Blicha on 2019-01-05.
//

#include <gtest/gtest.h>
#include <lrasolver/Polynomial.h>

TEST(Polynomial_test, test_AddTerm){
    Polynomial poly;
    LVRef var {10};
    poly.addTerm(var, 1);
    EXPECT_EQ(poly.getCoeff(var), 1);
}

TEST(Polynomial_test, test_Merge){
    Polynomial poly1;
    Polynomial poly2;
    LVRef x {10};
    LVRef y {20};
    LVRef z {30};
    poly1.addTerm(x, 1);
    poly1.addTerm(y, -3);
    poly2.addTerm(z, 1);
    poly2.addTerm(y, 3);
    auto res = poly1.merge(poly2, 1);
    ASSERT_EQ(res.added.size(),1);
    EXPECT_EQ(res.added[0], z);
    ASSERT_EQ(res.removed.size(),1);
    EXPECT_EQ(res.removed[0], y);
    ASSERT_TRUE(poly1.contains(x));
    ASSERT_TRUE(poly1.contains(z));
    EXPECT_FALSE(poly1.contains(y));
    EXPECT_EQ(poly1.getCoeff(x), 1);
    EXPECT_EQ(poly1.getCoeff(z), 1);
}

TEST(Polynomial_test, test_Merge2){
    Polynomial poly1;
    Polynomial poly2;
    LVRef x1 {4772};
    LVRef x2 {4776};
    LVRef y1 {2604};
    LVRef y2 {4624};
    poly1.addTerm(x1, 1);
    poly1.addTerm(x2, -1);
    poly2.addTerm(y1, -1);
    poly2.addTerm(y2, 1);
    auto res = poly1.merge(poly2, 1);
    ASSERT_EQ(res.added.size(),2);
    ASSERT_EQ(res.removed.size(),0);
    ASSERT_TRUE(poly1.contains(x1));
    ASSERT_TRUE(poly1.contains(x2));
    ASSERT_TRUE(poly1.contains(y1));
    ASSERT_TRUE(poly1.contains(y2));
}

TEST(Polynomial_test, test_Merge3){
    Polynomial poly1;
    Polynomial poly2;
    LVRef x1 {4772};
    LVRef x2 {4776};
    LVRef y1 {2604};
    LVRef y2 {4624};
    poly1.addTerm(x1, 1);
    poly1.addTerm(x2, -2);
    poly2.addTerm(x2, 1);
    auto res = poly1.merge(poly2, 2);
    ASSERT_EQ(res.added.size(),0);
    ASSERT_EQ(res.removed.size(),1);
    EXPECT_EQ(res.removed[0], x2);
    ASSERT_TRUE(poly1.contains(x1));
    ASSERT_TRUE(!poly1.contains(x2));
}