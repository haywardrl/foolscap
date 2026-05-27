#include "app/render/rect.h"

#include <unity.h>

void setUp(void) {
}
void tearDown(void) {
}

static void test_zero_rect_is_empty(void) {
    TEST_ASSERT_TRUE(rect_is_empty((rect_t){0}));
}

static void test_zero_width_or_height_is_empty(void) {
    TEST_ASSERT_TRUE(rect_is_empty((rect_t){0, 0, 0, 5}));
    TEST_ASSERT_TRUE(rect_is_empty((rect_t){0, 0, 5, 0}));
}

static void test_positive_rect_is_not_empty(void) {
    TEST_ASSERT_FALSE(rect_is_empty((rect_t){1, 2, 3, 4}));
}

static void test_union_with_empty_is_identity(void) {
    rect_t r = {10, 20, 5, 5};
    rect_t u = rect_union(r, (rect_t){0});
    TEST_ASSERT_EQUAL_INT(10, u.x);
    TEST_ASSERT_EQUAL_INT(20, u.y);
    TEST_ASSERT_EQUAL_INT(5, u.w);
    TEST_ASSERT_EQUAL_INT(5, u.h);

    u = rect_union((rect_t){0}, r);
    TEST_ASSERT_EQUAL_INT(10, u.x);
    TEST_ASSERT_EQUAL_INT(5, u.w);
}

static void test_union_is_bounding_box(void) {
    // [0,0,2,2] and [10,10,2,2] -> [0,0,12,12]
    rect_t u = rect_union((rect_t){0, 0, 2, 2}, (rect_t){10, 10, 2, 2});
    TEST_ASSERT_EQUAL_INT(0, u.x);
    TEST_ASSERT_EQUAL_INT(0, u.y);
    TEST_ASSERT_EQUAL_INT(12, u.w);
    TEST_ASSERT_EQUAL_INT(12, u.h);
}

static void test_union_of_overlapping(void) {
    rect_t u = rect_union((rect_t){0, 0, 5, 5}, (rect_t){3, 3, 5, 5});
    TEST_ASSERT_EQUAL_INT(0, u.x);
    TEST_ASSERT_EQUAL_INT(0, u.y);
    TEST_ASSERT_EQUAL_INT(8, u.w);
    TEST_ASSERT_EQUAL_INT(8, u.h);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_zero_rect_is_empty);
    RUN_TEST(test_zero_width_or_height_is_empty);
    RUN_TEST(test_positive_rect_is_not_empty);
    RUN_TEST(test_union_with_empty_is_identity);
    RUN_TEST(test_union_is_bounding_box);
    RUN_TEST(test_union_of_overlapping);
    return UNITY_END();
}
