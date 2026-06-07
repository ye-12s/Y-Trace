#include "map/ytrace_map_tiles.h"
#include "unity.h"

#include <stddef.h>
#include <stdint.h>

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_ytrace_map_tiles_metadata_constants_are_consistent(void)
{
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_WIDTH_PX * YTRACE_MAP_TILE_HEIGHT_PX * YTRACE_MAP_TILE_COUNT,
                           YTRACE_MAP_TILE_PIXEL_BYTES);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_PALETTE_COLOR_COUNT * sizeof(uint16_t),
                           YTRACE_MAP_TILE_PALETTE_BYTES);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_PIXEL_BYTES + YTRACE_MAP_TILE_PALETTE_BYTES,
                           YTRACE_MAP_TILE_TOTAL_BYTES);
}

static void test_ytrace_map_tiles_resource_matches_public_metadata(void)
{
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_WIDTH_PX, ytrace_map_tiles.width_px);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_HEIGHT_PX, ytrace_map_tiles.height_px);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_COUNT, ytrace_map_tiles.tile_count);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_PALETTE_COLOR_COUNT, ytrace_map_tiles.palette_color_count);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_BYTES_PER_PIXEL, ytrace_map_tiles.bytes_per_pixel);
    TEST_ASSERT_EQUAL_INT(YTRACE_MAP_TILE_FORMAT_INDEX8_RGB565, ytrace_map_tiles.format);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_PIXEL_BYTES, ytrace_map_tiles.pixel_bytes);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_PALETTE_BYTES, ytrace_map_tiles.palette_bytes);
    TEST_ASSERT_EQUAL_UINT(YTRACE_MAP_TILE_TOTAL_BYTES, ytrace_map_tiles.total_bytes);
    TEST_ASSERT_EQUAL_PTR(ytrace_map_tiles_pixels, ytrace_map_tiles.pixels);
    TEST_ASSERT_EQUAL_PTR(ytrace_map_tiles_palette, ytrace_map_tiles.palette_rgb565);
}

static void test_ytrace_map_tile_pixels_returns_expected_offsets(void)
{
    const size_t tile_bytes = YTRACE_MAP_TILE_WIDTH_PX * YTRACE_MAP_TILE_HEIGHT_PX;

    TEST_ASSERT_EQUAL_PTR(ytrace_map_tiles_pixels, ytrace_map_tile_pixels(0));
    TEST_ASSERT_EQUAL_PTR(&ytrace_map_tiles_pixels[(YTRACE_MAP_TILE_COUNT - 1U) * tile_bytes],
                          ytrace_map_tile_pixels(YTRACE_MAP_TILE_COUNT - 1U));
}

static void test_ytrace_map_tile_pixels_rejects_out_of_range_index(void)
{
    TEST_ASSERT_NULL(ytrace_map_tile_pixels(YTRACE_MAP_TILE_COUNT));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ytrace_map_tiles_metadata_constants_are_consistent);
    RUN_TEST(test_ytrace_map_tiles_resource_matches_public_metadata);
    RUN_TEST(test_ytrace_map_tile_pixels_returns_expected_offsets);
    RUN_TEST(test_ytrace_map_tile_pixels_rejects_out_of_range_index);
    return UNITY_END();
}
