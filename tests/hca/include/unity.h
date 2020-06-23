#ifndef _UNITY_H_
#define _UNITY_H_

#include <stdio.h>

#define TEST_GROUP(_g_)
#define TEST_SETUP(_s_) static void _ ## _s_ ## _setup(void)
#define TEST_TEAR_DOWN(_t_) static void _ ## _t_ ## _teardown(void)

#define TEST(_g_, _f_) void _g_ ## _ ## _f_ ## _test(void)
#define TEST_GROUP_RUNNER(_n_) void test ## _n_(void)

#define RUN_TEST_CASE(_g_, _f_) \
   extern void _g_ ## _ ## _f_ ## _test(void); \
   _g_ ## _ ## _f_ ## _test()

#define RUN_GROUP(_n_)  \
   test ## _n_();

extern void __attribute__((noreturn)) metal_shutdown(int code);

#define TEST_ASSERT_TRUE(_c_) \
   if (!(_c_)) { \
      printf("%s[%d] result: %d\n", __func__, __LINE__, !!(_c_)); \
      metal_shutdown(1); \
   } //else { \
      //printf("%s: OK\n", __func__); \
   //}

#endif // _UNITY_H_
