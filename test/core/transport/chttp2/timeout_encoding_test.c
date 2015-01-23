/*
 *
 * Copyright 2014, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "src/core/transport/chttp2/timeout_encoding.h"

#include <stdio.h>
#include <string.h>

#include <grpc/support/log.h>
#include <grpc/support/useful.h>
#include "test/core/util/test_config.h"

#define LOG_TEST() gpr_log(GPR_INFO, "%s", __FUNCTION__)

static void assert_encodes_as(gpr_timespec ts, const char *s) {
  char buffer[GRPC_CHTTP2_TIMEOUT_ENCODE_MIN_BUFSIZE];
  grpc_chttp2_encode_timeout(ts, buffer);
  gpr_log(GPR_INFO, "check '%s' == '%s'", buffer, s);
  GPR_ASSERT(0 == strcmp(buffer, s));
}

void test_encoding(void) {
  LOG_TEST();
  assert_encodes_as(gpr_time_from_micros(-1), "1n");
  assert_encodes_as(gpr_time_from_seconds(-10), "1n");
  assert_encodes_as(gpr_time_from_nanos(10), "10n");
  assert_encodes_as(gpr_time_from_nanos(999999999), "1S");
  assert_encodes_as(gpr_time_from_micros(1), "1u");
  assert_encodes_as(gpr_time_from_micros(10), "10u");
  assert_encodes_as(gpr_time_from_micros(100), "100u");
  assert_encodes_as(gpr_time_from_micros(890), "890u");
  assert_encodes_as(gpr_time_from_micros(900), "900u");
  assert_encodes_as(gpr_time_from_micros(901), "901u");
  assert_encodes_as(gpr_time_from_millis(1), "1m");
  assert_encodes_as(gpr_time_from_millis(2), "2m");
  assert_encodes_as(gpr_time_from_micros(10001), "10100u");
  assert_encodes_as(gpr_time_from_micros(999999), "1S");
  assert_encodes_as(gpr_time_from_millis(1000), "1S");
  assert_encodes_as(gpr_time_from_millis(2000), "2S");
  assert_encodes_as(gpr_time_from_millis(2500), "2500m");
  assert_encodes_as(gpr_time_from_millis(59900), "59900m");
  assert_encodes_as(gpr_time_from_seconds(50), "50S");
  assert_encodes_as(gpr_time_from_seconds(59), "59S");
  assert_encodes_as(gpr_time_from_seconds(60), "1M");
  assert_encodes_as(gpr_time_from_seconds(80), "80S");
  assert_encodes_as(gpr_time_from_seconds(90), "90S");
  assert_encodes_as(gpr_time_from_minutes(2), "2M");
  assert_encodes_as(gpr_time_from_minutes(20), "20M");
  assert_encodes_as(gpr_time_from_hours(1), "1H");
  assert_encodes_as(gpr_time_from_hours(10), "10H");
  assert_encodes_as(gpr_time_from_seconds(1000000000), "1000000000S");
}

static void assert_decodes_as(const char *buffer, gpr_timespec expected) {
  gpr_timespec got;
  gpr_log(GPR_INFO, "check decoding '%s'", buffer);
  GPR_ASSERT(1 == grpc_chttp2_decode_timeout(buffer, &got));
  GPR_ASSERT(0 == gpr_time_cmp(got, expected));
}

void decode_suite(char ext, gpr_timespec (*answer)(long x)) {
  long test_vals[] = {1,       12,       123,       1234,     12345,   123456,
                      1234567, 12345678, 123456789, 98765432, 9876543, 987654,
                      98765,   9876,     987,       98,       9};
  int i;
  char input[32];
  for (i = 0; i < GPR_ARRAY_SIZE(test_vals); i++) {
    sprintf(input, "%ld%c", test_vals[i], ext);
    assert_decodes_as(input, answer(test_vals[i]));
    sprintf(input, "   %ld%c", test_vals[i], ext);
    assert_decodes_as(input, answer(test_vals[i]));
    sprintf(input, "%ld %c", test_vals[i], ext);
    assert_decodes_as(input, answer(test_vals[i]));
    sprintf(input, "%ld %c  ", test_vals[i], ext);
    assert_decodes_as(input, answer(test_vals[i]));
  }
}

void test_decoding(void) {
  LOG_TEST();
  decode_suite('n', gpr_time_from_nanos);
  decode_suite('u', gpr_time_from_micros);
  decode_suite('m', gpr_time_from_millis);
  decode_suite('S', gpr_time_from_seconds);
  decode_suite('M', gpr_time_from_minutes);
  decode_suite('H', gpr_time_from_hours);
  assert_decodes_as("1000000000000000000000u", gpr_inf_future);
}

void test_decoding_fails(void) {
  gpr_timespec x;
  LOG_TEST();
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout(" ", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("x", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("1", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("1x", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("1ux", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("!", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("n1", &x));
  GPR_ASSERT(0 == grpc_chttp2_decode_timeout("-1u", &x));
}

int main(int argc, char **argv) {
  grpc_test_init(argc, argv);
  test_encoding();
  test_decoding();
  test_decoding_fails();
  return 0;
}
