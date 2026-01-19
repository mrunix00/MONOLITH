#include <libs/Unity/src/unity.h>
#include <userspace/libs/libc/include/math.h>

/* fabs tests */
static void test_fabs_positive(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(5.0, fabs(5.0));
}

static void test_fabs_negative(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(5.0, fabs(-5.0));
}

static void test_fabs_zero(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, fabs(0.0));
}

/* floor tests */
static void test_floor_positive(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(3.0, floor(3.7));
}

static void test_floor_negative(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(-4.0, floor(-3.2));
}

static void test_floor_integer(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(5.0, floor(5.0));
}

/* ceil tests */
static void test_ceil_positive(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(4.0, ceil(3.2));
}

static void test_ceil_negative(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(-3.0, ceil(-3.7));
}

static void test_ceil_integer(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(5.0, ceil(5.0));
}

/* fmod tests */
static void test_fmod_positive(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.0, fmod(7.0, 3.0));
}

static void test_fmod_negative(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, -1.0, fmod(-7.0, 3.0));
}

static void test_fmod_fractional(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.5, fmod(5.5, 2.5));
}

static void test_fmod_zero_divisor(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, fmod(5.0, 0.0));
}

/* sqrt tests */
static void test_sqrt_perfect_square(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.0, sqrt(9.0));
}

static void test_sqrt_non_perfect(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.4142, sqrt(2.0));
}

static void test_sqrt_one(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.0, sqrt(1.0));
}

static void test_sqrt_zero(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, sqrt(0.0));
}

static void test_sqrt_negative(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, sqrt(-4.0));
}

/* pow tests */
static void test_pow_positive_exp(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 8.0, pow(2.0, 3.0));
}

static void test_pow_zero_exp(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(1.0, pow(5.0, 0.0));
}

static void test_pow_negative_exp(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.125, pow(2.0, -3.0));
}

static void test_pow_zero_base(void)
{
    TEST_ASSERT_EQUAL_DOUBLE(0.0, pow(0.0, 5.0));
}

static void test_pow_fractional_exp(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 2.0, pow(4.0, 0.5));
}

/* cos tests */
static void test_cos_zero(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 1.0, cos(0.0));
}

static void test_cos_pi(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.01, -1.0, cos(3.14159265));
}

static void test_cos_pi_over_2(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.0, cos(1.5707963));
}

static void test_cos_negative(void)
{
    /* cos is even function: cos(-x) = cos(x) */
    TEST_ASSERT_DOUBLE_WITHIN(0.01, cos(1.0), cos(-1.0));
}

/* acos tests */
static void test_acos_one(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, acos(1.0));
}

static void test_acos_zero(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1.5707963, acos(0.0));
}

static void test_acos_negative_one(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 3.14159265, acos(-1.0));
}

static void test_acos_half(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1.0471975, acos(0.5));
}

static void test_acos_clamps_above_one(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.0, acos(1.5));
}

static void test_acos_clamps_below_neg_one(void)
{
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 3.14159265, acos(-1.5));
}

void libc_math_tests(void)
{
    /* fabs tests */
    RUN_TEST(test_fabs_positive);
    RUN_TEST(test_fabs_negative);
    RUN_TEST(test_fabs_zero);

    /* floor tests */
    RUN_TEST(test_floor_positive);
    RUN_TEST(test_floor_negative);
    RUN_TEST(test_floor_integer);

    /* ceil tests */
    RUN_TEST(test_ceil_positive);
    RUN_TEST(test_ceil_negative);
    RUN_TEST(test_ceil_integer);

    /* fmod tests */
    RUN_TEST(test_fmod_positive);
    RUN_TEST(test_fmod_negative);
    RUN_TEST(test_fmod_fractional);
    RUN_TEST(test_fmod_zero_divisor);

    /* sqrt tests */
    RUN_TEST(test_sqrt_perfect_square);
    RUN_TEST(test_sqrt_non_perfect);
    RUN_TEST(test_sqrt_one);
    RUN_TEST(test_sqrt_zero);
    RUN_TEST(test_sqrt_negative);

    /* pow tests */
    RUN_TEST(test_pow_positive_exp);
    RUN_TEST(test_pow_zero_exp);
    RUN_TEST(test_pow_negative_exp);
    RUN_TEST(test_pow_zero_base);
    RUN_TEST(test_pow_fractional_exp);

    /* cos tests */
    RUN_TEST(test_cos_zero);
    RUN_TEST(test_cos_pi);
    RUN_TEST(test_cos_pi_over_2);
    RUN_TEST(test_cos_negative);

    /* acos tests */
    RUN_TEST(test_acos_one);
    RUN_TEST(test_acos_zero);
    RUN_TEST(test_acos_negative_one);
    RUN_TEST(test_acos_half);
    RUN_TEST(test_acos_clamps_above_one);
    RUN_TEST(test_acos_clamps_below_neg_one);
}
