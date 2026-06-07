#include "utils/minmea/minmea.h"
#include "unity.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

static const char *RMC_SENTENCE = "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62";
static const char *GGA_SENTENCE = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";

void setUp(void)
{
}

void tearDown(void)
{
}

static void test_minmea_checksum_accepts_body_with_or_without_dollar(void)
{
    TEST_ASSERT_EQUAL_HEX8(0x62, minmea_checksum("GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E"));
    TEST_ASSERT_EQUAL_HEX8(0x62, minmea_checksum(RMC_SENTENCE));
}

static void test_minmea_check_validates_strict_and_non_strict_sentences(void)
{
    TEST_ASSERT_TRUE(minmea_check(RMC_SENTENCE, true));
    TEST_ASSERT_FALSE(minmea_check("$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*63", true));
    TEST_ASSERT_TRUE(minmea_check("$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E", false));
    TEST_ASSERT_FALSE(minmea_check("$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E", true));
}

static void test_minmea_parse_rmc_exposes_expected_fix_fields(void)
{
    struct minmea_sentence_rmc frame;

    TEST_ASSERT_TRUE(minmea_parse_rmc(&frame, RMC_SENTENCE));
    TEST_ASSERT_EQUAL_STRING("GPRMC", frame.type.buf);
    TEST_ASSERT_TRUE(frame.valid);
    TEST_ASSERT_EQUAL_INT(8, frame.time.hours);
    TEST_ASSERT_EQUAL_INT(18, frame.time.minutes);
    TEST_ASSERT_EQUAL_INT(36, frame.time.seconds);
    TEST_ASSERT_EQUAL_INT(13, frame.date.day);
    TEST_ASSERT_EQUAL_INT(9, frame.date.month);
    TEST_ASSERT_EQUAL_INT(98, frame.date.year);
    TEST_ASSERT_EQUAL_INT32(-375165, frame.latitude.value);
    TEST_ASSERT_EQUAL_INT32(100, frame.latitude.scale);
    TEST_ASSERT_EQUAL_INT32(1450736, frame.longitude.value);
    TEST_ASSERT_EQUAL_INT32(100, frame.longitude.scale);
    TEST_ASSERT_EQUAL_INT32(0, frame.speed.value);
    TEST_ASSERT_EQUAL_INT32(10, frame.speed.scale);
    TEST_ASSERT_EQUAL_INT32(3600, frame.course.value);
    TEST_ASSERT_EQUAL_INT32(10, frame.course.scale);
}

static void test_minmea_parse_gga_exposes_expected_altitude_and_quality(void)
{
    struct minmea_sentence_gga frame;

    TEST_ASSERT_TRUE(minmea_parse_gga(&frame, GGA_SENTENCE));
    TEST_ASSERT_EQUAL_STRING("GPGGA", frame.type.buf);
    TEST_ASSERT_EQUAL_INT(12, frame.time.hours);
    TEST_ASSERT_EQUAL_INT(35, frame.time.minutes);
    TEST_ASSERT_EQUAL_INT(19, frame.time.seconds);
    TEST_ASSERT_EQUAL_INT32(4807038, frame.latitude.value);
    TEST_ASSERT_EQUAL_INT32(1000, frame.latitude.scale);
    TEST_ASSERT_EQUAL_INT32(1131000, frame.longitude.value);
    TEST_ASSERT_EQUAL_INT32(1000, frame.longitude.scale);
    TEST_ASSERT_EQUAL_INT(1, frame.fix_quality);
    TEST_ASSERT_EQUAL_INT(8, frame.satellites_tracked);
    TEST_ASSERT_EQUAL_INT32(9, frame.hdop.value);
    TEST_ASSERT_EQUAL_INT32(10, frame.hdop.scale);
    TEST_ASSERT_EQUAL_INT32(5454, frame.altitude.value);
    TEST_ASSERT_EQUAL_INT32(10, frame.altitude.scale);
    TEST_ASSERT_EQUAL_INT('M', frame.altitude_units);
}

static void test_minmea_getdatetime_converts_parsed_date_and_time(void)
{
    struct minmea_sentence_rmc frame;
    struct tm datetime;

    TEST_ASSERT_TRUE(minmea_parse_rmc(&frame, RMC_SENTENCE));
    TEST_ASSERT_EQUAL_INT(0, minmea_getdatetime(&datetime, &frame.date, &frame.time));
    TEST_ASSERT_EQUAL_INT(98, datetime.tm_year);
    TEST_ASSERT_EQUAL_INT(8, datetime.tm_mon);
    TEST_ASSERT_EQUAL_INT(13, datetime.tm_mday);
    TEST_ASSERT_EQUAL_INT(8, datetime.tm_hour);
    TEST_ASSERT_EQUAL_INT(18, datetime.tm_min);
    TEST_ASSERT_EQUAL_INT(36, datetime.tm_sec);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_minmea_checksum_accepts_body_with_or_without_dollar);
    RUN_TEST(test_minmea_check_validates_strict_and_non_strict_sentences);
    RUN_TEST(test_minmea_parse_rmc_exposes_expected_fix_fields);
    RUN_TEST(test_minmea_parse_gga_exposes_expected_altitude_and_quality);
    RUN_TEST(test_minmea_getdatetime_converts_parsed_date_and_time);
    return UNITY_END();
}
