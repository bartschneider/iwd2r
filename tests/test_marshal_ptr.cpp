#include "compat/marshal_ptr.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("marshal pointer handles round-trip within a pass", "[marshal_ptr]")
{
    iwd2r::ResetMarshalPtrs();
    int a = 1;
    int b = 2;
    CHECK(iwd2r::MarshalPtr(nullptr) == 0);
    CHECK(iwd2r::UnmarshalPtr(0) == nullptr);
    const uint32_t ha = iwd2r::MarshalPtr(&a);
    const uint32_t hb = iwd2r::MarshalPtr(&b);
    CHECK(ha != 0);
    CHECK(hb != 0);
    CHECK(ha != hb);
    CHECK(iwd2r::MarshalPtr(&a) == ha);
    CHECK(iwd2r::UnmarshalPtr(ha) == &a);
    CHECK(iwd2r::UnmarshalPtr(hb) == &b);
    CHECK(iwd2r::UnmarshalPtr(hb + 100) == nullptr);
    iwd2r::ResetMarshalPtrs();
    CHECK(iwd2r::UnmarshalPtr(ha) == nullptr);
    CHECK(iwd2r::MarshalPtr(&b) == 1);
}
