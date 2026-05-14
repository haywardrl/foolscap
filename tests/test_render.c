#include "app/render/font.h"

#include <unity.h>

void setUp(void) {
}
void tearDown(void) {
}

static void test_create_glyph_t_returns_not_null(void) {
    font_glyph_t *glyph;
    TEST_ASSERT_EQUAL(16, sizeof(*glyph));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_create_glyph_t_returns_not_null);
    return UNITY_END();
}
