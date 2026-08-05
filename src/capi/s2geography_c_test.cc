
#include "s2geography_c.h"

#include <gtest/gtest.h>
#include <s2/s2cell_id.h>

#include <cstring>
#include <limits>
#include <vector>

// This test file performs "is it plugged in" level checks for all C API
// functions. The goal is to ensure that:
// 1. All functions are exported and linkable
// 2. Basic happy-path usage works
// 3. Functions return expected values for simple cases

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(S2GeographyC, ErrorCreate) {
  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);
  ASSERT_NE(err, nullptr);
  S2GeogErrorDestroy(err);
}

TEST(S2GeographyC, ErrorGetMessage) {
  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  // Fresh error should have empty message
  const char* msg = S2GeogErrorGetMessage(err);
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(strlen(msg), 0);

  S2GeogErrorDestroy(err);
}

// ============================================================================
// Cell Functions Tests
// ============================================================================

TEST(S2GeographyC, LngLatToCellId) {
  // Test with a valid coordinate (New York City area)
  struct S2GeogVertex vertex;
  vertex.v[0] = -73.9857;  // longitude
  vertex.v[1] = 40.7484;   // latitude
  vertex.v[2] = 0;
  vertex.v[3] = 0;

  uint64_t cell_id = S2GeogLngLatToCellId(&vertex);

  // Should return a valid (non-sentinel) cell ID
  EXPECT_NE(cell_id, S2CellId::Sentinel().id());
}

TEST(S2GeographyC, LngLatToCellIdNaN) {
  // Test with NaN coordinates - should return sentinel
  struct S2GeogVertex vertex;
  vertex.v[0] = std::numeric_limits<double>::quiet_NaN();
  vertex.v[1] = 40.0;

  EXPECT_EQ(S2GeogLngLatToCellId(&vertex), S2CellId::Sentinel().id());

  // Should return sentinel cell ID for NaN input
  // S2CellId::Sentinel().id() is expected here
  // The actual sentinel value - just verify it's consistent
  struct S2GeogVertex vertex2;
  vertex2.v[0] = 0.0;
  vertex2.v[1] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(S2GeogLngLatToCellId(&vertex2), S2CellId::Sentinel().id());
}

// ============================================================================
// Geography Accessors Tests
// ============================================================================

TEST(S2GeographyC, GeogCreate) {
  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);
  ASSERT_NE(geog, nullptr);

  // Should be able to force prepare a fresh geography
  ASSERT_EQ(S2GeogForcePrepare(geog, nullptr), S2GEOGRAPHY_OK);

  S2GeogDestroy(geog);
}

TEST(S2GeographyC, MemUsed) {
  const char* wkt_point = "POINT (0 0)";

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  // MemUsed should return something reasonable for an empty geography
  size_t mem_empty = S2GeogMemUsed(geog);
  EXPECT_GT(mem_empty, 0);

  // Initialize with a point
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt_point, strlen(wkt_point),
                                     geog, nullptr),
            S2GEOGRAPHY_OK);

  // MemUsed should return something reasonable for a point
  size_t mem_point = S2GeogMemUsed(geog);
  EXPECT_GT(mem_point, 0);

  // After forcing the index to build, memory should increase
  ASSERT_EQ(S2GeogForcePrepare(geog, nullptr), S2GEOGRAPHY_OK);
  size_t mem_prepared = S2GeogMemUsed(geog);
  EXPECT_GT(mem_prepared, mem_point);

  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Geography Factory Tests
// ============================================================================

TEST(S2GeographyC, FactoryCreate) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);
  ASSERT_NE(factory, nullptr);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromWkbPoint) {
  // WKB for POINT(0 0) - little endian
  const uint8_t wkb_point[] = {
      0x01,                    // byte order: little endian
      0x01, 0x00, 0x00, 0x00,  // type: Point (1)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // x: 0.0
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // y: 0.0
  };

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  EXPECT_EQ(S2GeogFactoryInitFromWkbNonOwning(factory, wkb_point,
                                              sizeof(wkb_point), geog, err),
            S2GEOGRAPHY_OK);

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromInvalidWkb) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(S2GeogFactoryInitFromWkbNonOwning(factory, nullptr, 0, geog, err),
            EINVAL);
  EXPECT_STREQ(S2GeogErrorGetMessage(err),
               "Expected endian byte but found end of buffer at byte 0");

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromWktPoint) {
  const char* wkt_point = "POINT (0 0)";

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  EXPECT_EQ(S2GeogFactoryInitFromWkt(factory, wkt_point, strlen(wkt_point),
                                     geog, err),
            S2GEOGRAPHY_OK);

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, FactoryInitFromInvalidWkt) {
  const char* invalid_wkt = "NOT VALID WKT";

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  EXPECT_NE(S2GeogFactoryInitFromWkt(factory, invalid_wkt, strlen(invalid_wkt),
                                     geog, err),
            S2GEOGRAPHY_OK);

  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Rectangle Bounder Tests
// ============================================================================

TEST(S2GeographyC, RectBounderBound) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* wkt = "POINT (10 20)";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt, strlen(wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogRectBounder* bounder = nullptr;
  ASSERT_EQ(S2GeogRectBounderCreate(&bounder), S2GEOGRAPHY_OK);

  // Fresh bounder should be empty
  EXPECT_EQ(S2GeogRectBounderIsEmpty(bounder), 1);

  // Bound a point
  S2GeogRectBounderBound(bounder, geog, err);

  // Should no longer be empty
  EXPECT_EQ(S2GeogRectBounderIsEmpty(bounder), 0);

  struct S2GeogVertex lo, hi;
  EXPECT_EQ(S2GeogRectBounderFinish(bounder, &lo, &hi, err), S2GEOGRAPHY_OK);
  EXPECT_LE(lo.v[0], 10);
  EXPECT_GE(hi.v[0], 10);
  EXPECT_LE(lo.v[1], 20);
  EXPECT_GE(hi.v[1], 20);

  // If we clear, the bounder should be empty again
  S2GeogRectBounderClear(bounder);
  EXPECT_EQ(S2GeogRectBounderIsEmpty(bounder), 1);

  // Test ExpandByDistance
  S2GeogRectBounderBound(bounder, geog, err);
  S2GeogRectBounderExpandByDistance(bounder, 1000.0);  // 1km
  EXPECT_EQ(S2GeogRectBounderFinish(bounder, &lo, &hi, err), S2GEOGRAPHY_OK);
  // After expanding by 1km (~0.009 degrees at equator), bounds should be larger
  EXPECT_LT(lo.v[0], 10 - 0.008);
  EXPECT_GT(hi.v[0], 10 + 0.008);
  EXPECT_LT(lo.v[1], 20 - 0.008);
  EXPECT_GT(hi.v[1], 20 + 0.008);

  // Test ExpandByDistanceWithRadius with half Earth's radius
  // This should result in twice the angular expansion
  S2GeogRectBounderClear(bounder);
  S2GeogRectBounderBound(bounder, geog, err);
  double half_earth_radius = 6371000.0 / 2.0;  // Half of Earth's radius in m
  S2GeogRectBounderExpandByDistanceWithRadius(bounder, 1000.0,
                                              half_earth_radius);
  EXPECT_EQ(S2GeogRectBounderFinish(bounder, &lo, &hi, err), S2GEOGRAPHY_OK);
  // With half the radius, bounds should expand more
  EXPECT_LT(lo.v[0], 10 - 0.016);
  EXPECT_GT(hi.v[0], 10 + 0.016);
  EXPECT_LT(lo.v[1], 20 - 0.016);
  EXPECT_GT(hi.v[1], 20 + 0.016);

  // Test UpdateRect
  S2GeogRectBounderClear(bounder);
  S2GeogRectBounderUpdateRect(bounder, -10.0, -20.0, 30.0, 40.0);
  EXPECT_EQ(S2GeogRectBounderIsEmpty(bounder), 0);
  EXPECT_EQ(S2GeogRectBounderFinish(bounder, &lo, &hi, err), S2GEOGRAPHY_OK);
  EXPECT_DOUBLE_EQ(lo.v[0], -10.0);
  EXPECT_DOUBLE_EQ(lo.v[1], -20.0);
  EXPECT_DOUBLE_EQ(hi.v[0], 30.0);
  EXPECT_DOUBLE_EQ(hi.v[1], 40.0);

  S2GeogRectBounderDestroy(bounder);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Sedona UDF Interface Tests
// ============================================================================

TEST(S2GeographyC, NumKernels) { EXPECT_EQ(S2GeogNumKernels(), 36); }

TEST(S2GeographyC, InitKernelsInvalidFormat) {
  // Test with invalid format
  size_t num_kernels = S2GeogNumKernels();
  std::vector<char> buffer(num_kernels * 256);  // Oversized buffer

  // Invalid format (not S2GEOGRAPHY_KERNEL_FORMAT_SEDONA_UDF)
  S2GeogErrorCode code = S2GeogInitKernels(buffer.data(), buffer.size(), 999);
  EXPECT_NE(code, S2GEOGRAPHY_OK);
}

// ============================================================================
// Version Functions Tests
// ============================================================================

TEST(S2GeographyC, NanoarrowVersion) {
  const char* version = S2GeogNanoarrowVersion();
  ASSERT_NE(version, nullptr);
  // Should be a non-empty version string
  EXPECT_GT(strlen(version), 0);
  // Should contain at least one dot (like "0.5.0")
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, GeoArrowVersion) {
  const char* version = S2GeogGeoArrowVersion();
  ASSERT_NE(version, nullptr);
  EXPECT_GT(strlen(version), 0);
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, OpenSSLVersion) {
  const char* version = S2GeogOpenSSLVersion();
  ASSERT_NE(version, nullptr);
  EXPECT_GT(strlen(version), 0);
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, S2GeometryVersion) {
  const char* version = S2GeogS2GeometryVersion();
  ASSERT_NE(version, nullptr);
  EXPECT_GT(strlen(version), 0);
  EXPECT_NE(strchr(version, '.'), nullptr);
}

TEST(S2GeographyC, AbseilVersion) {
  const char* version = S2GeogAbseilVersion();
  ASSERT_NE(version, nullptr);
  // Could be a version string or "<live at head>"
  EXPECT_GT(strlen(version), 0);
}

// ============================================================================
// Binary Predicate Operations Tests (Parameterized)
// ============================================================================

struct BinaryPredicateParam {
  const char* name;
  int op_id;
  const char* lhs_wkt;
  const char* rhs_wkt;
  int64_t expected;
};

class BinaryPredicateTest
    : public ::testing::TestWithParam<BinaryPredicateParam> {};

TEST_P(BinaryPredicateTest, EvalGeogGeog) {
  const auto& p = GetParam();

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* lhs = nullptr;
  struct S2Geog* rhs = nullptr;
  ASSERT_EQ(S2GeogCreate(&lhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&rhs), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.lhs_wkt, strlen(p.lhs_wkt), lhs, err),
      S2GEOGRAPHY_OK);
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.rhs_wkt, strlen(p.rhs_wkt), rhs, err),
      S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, p.op_id), S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), p.name);
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_BOOL);

  ASSERT_EQ(S2GeogOpEvalGeogGeog(op, lhs, rhs, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpGetInt(op), p.expected);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(rhs);
  S2GeogDestroy(lhs);
  S2GeogFactoryDestroy(factory);
}

INSTANTIATE_TEST_SUITE_P(
    S2GeographyC, BinaryPredicateTest,
    ::testing::Values(BinaryPredicateParam{"intersects",
                                           S2GEOGRAPHY_OP_INTERSECTS,
                                           "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                                           "POINT (0.25 0.25)", 1},
                      BinaryPredicateParam{"disjoint", S2GEOGRAPHY_OP_DISJOINT,
                                           "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                                           "POINT (0.25 0.25)", 0},
                      BinaryPredicateParam{"contains", S2GEOGRAPHY_OP_CONTAINS,
                                           "POLYGON ((0 0, 2 0, 0 2, 0 0))",
                                           "POINT (0.25 0.25)", 1},
                      BinaryPredicateParam{"within", S2GEOGRAPHY_OP_WITHIN,
                                           "POINT (0.25 0.25)",
                                           "POLYGON ((0 0, 2 0, 0 2, 0 0))", 1},
                      BinaryPredicateParam{"equals", S2GEOGRAPHY_OP_EQUALS,
                                           "POLYGON ((0 0, 1 0, 0 1, 0 0))",
                                           "POLYGON ((1 0, 0 1, 0 0, 1 0))",
                                           1}),
    [](const ::testing::TestParamInfo<BinaryPredicateParam>& info) {
      return info.param.name;
    });

// ============================================================================
// DistanceWithin Operation Test
// ============================================================================

TEST(S2GeographyC, DistanceWithinOperation) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* lhs = nullptr;
  struct S2Geog* rhs = nullptr;
  ASSERT_EQ(S2GeogCreate(&lhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&rhs), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  // Points ~111km apart (1 degree latitude)
  const char* lhs_wkt = "POINT (0 0)";
  const char* rhs_wkt = "POINT (0 1)";
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, lhs_wkt, strlen(lhs_wkt), lhs, err),
      S2GEOGRAPHY_OK);
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, rhs_wkt, strlen(rhs_wkt), rhs, err),
      S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_DISTANCE_WITHIN),
            S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), "distance_within");
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_BOOL);

  // Distance is ~111195 meters, so 200000 meters should return true
  ASSERT_EQ(S2GeogOpEvalGeogGeogDouble(op, lhs, rhs, 200000.0, err),
            S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpGetInt(op), 1);

  // 50000 meters should return false
  ASSERT_EQ(S2GeogOpEvalGeogGeogDouble(op, lhs, rhs, 50000.0, err),
            S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpGetInt(op), 0);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(rhs);
  S2GeogDestroy(lhs);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Unary Double Operations Tests (Parameterized)
// ============================================================================

struct UnaryDoubleParam {
  const char* name;
  int op_id;
  const char* wkt;
  double expected;
};

class UnaryDoubleTest : public ::testing::TestWithParam<UnaryDoubleParam> {};

TEST_P(UnaryDoubleTest, EvalGeog) {
  const auto& p = GetParam();

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, p.wkt, strlen(p.wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, p.op_id), S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), p.name);
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_DOUBLE);

  ASSERT_EQ(S2GeogOpEvalGeog(op, geog, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);
  EXPECT_NEAR(S2GeogOpGetDouble(op), p.expected, p.expected * 1e-3);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

INSTANTIATE_TEST_SUITE_P(
    S2GeographyC, UnaryDoubleTest,
    ::testing::Values(UnaryDoubleParam{"area", S2GEOGRAPHY_OP_AREA,
                                       "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                                       1.23652e10},
                      UnaryDoubleParam{"perimeter", S2GEOGRAPHY_OP_PERIMETER,
                                       "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                                       444763.0},
                      UnaryDoubleParam{"length", S2GEOGRAPHY_OP_LENGTH,
                                       "LINESTRING (0 0, 1 0)", 111195.0}),
    [](const ::testing::TestParamInfo<UnaryDoubleParam>& info) {
      return info.param.name;
    });

// ============================================================================
// Binary Double Operations Tests (Parameterized)
// ============================================================================

struct BinaryDoubleParam {
  const char* name;
  int op_id;
  const char* lhs_wkt;
  const char* rhs_wkt;
  double expected;
};

class BinaryDoubleTest : public ::testing::TestWithParam<BinaryDoubleParam> {};

TEST_P(BinaryDoubleTest, EvalGeogGeog) {
  const auto& p = GetParam();

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* lhs = nullptr;
  struct S2Geog* rhs = nullptr;
  ASSERT_EQ(S2GeogCreate(&lhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&rhs), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.lhs_wkt, strlen(p.lhs_wkt), lhs, err),
      S2GEOGRAPHY_OK);
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.rhs_wkt, strlen(p.rhs_wkt), rhs, err),
      S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, p.op_id), S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), p.name);
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_DOUBLE);

  ASSERT_EQ(S2GeogOpEvalGeogGeog(op, lhs, rhs, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);
  EXPECT_NEAR(S2GeogOpGetDouble(op), p.expected, p.expected * 1e-3);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(rhs);
  S2GeogDestroy(lhs);
  S2GeogFactoryDestroy(factory);
}

INSTANTIATE_TEST_SUITE_P(
    S2GeographyC, BinaryDoubleTest,
    ::testing::Values(
        BinaryDoubleParam{"distance", S2GEOGRAPHY_OP_DISTANCE, "POINT (0 0)",
                          "POINT (0 1)", 111195.0},
        BinaryDoubleParam{"max_distance", S2GEOGRAPHY_OP_MAX_DISTANCE,
                          "POINT (0 0)", "LINESTRING (0 1, 0 2)", 222390.0},
        BinaryDoubleParam{"line_locate_point", S2GEOGRAPHY_OP_LINE_LOCATE_POINT,
                          "LINESTRING (0 0, 0 1)", "POINT (0 0.5)", 0.5}),
    [](const ::testing::TestParamInfo<BinaryDoubleParam>& info) {
      return info.param.name;
    });

// ============================================================================
// Null Output Channel Tests
// ============================================================================

TEST(S2GeographyC, OpHasResultDouble) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* point = nullptr;
  struct S2Geog* empty = nullptr;
  ASSERT_EQ(S2GeogCreate(&point), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&empty), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* point_wkt = "POINT (0 0)";
  const char* empty_wkt = "LINESTRING EMPTY";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, point_wkt, strlen(point_wkt),
                                     point, err),
            S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, empty_wkt, strlen(empty_wkt),
                                     empty, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_DISTANCE), S2GEOGRAPHY_OK);

  // The distance to an empty geography is null rather than an error or NaN
  ASSERT_EQ(S2GeogOpEvalGeogGeog(op, point, empty, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 0);

  // ...and the null state must not persist into the next evaluation
  ASSERT_EQ(S2GeogOpEvalGeogGeog(op, point, point, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);
  EXPECT_DOUBLE_EQ(S2GeogOpGetDouble(op), 0.0);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(empty);
  S2GeogDestroy(point);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Integer Output Operations Tests
// ============================================================================

TEST(S2GeographyC, OpCellIdFromPoint) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* point = nullptr;
  struct S2Geog* empty = nullptr;
  ASSERT_EQ(S2GeogCreate(&point), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&empty), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* point_wkt = "POINT (-73.9857 40.7484)";
  const char* empty_wkt = "LINESTRING EMPTY";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, point_wkt, strlen(point_wkt),
                                     point, err),
            S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, empty_wkt, strlen(empty_wkt),
                                     empty, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_CELL_ID_FROM_POINT),
            S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), "cell_id_from_point");
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_INT);

  // Should agree with the standalone cell ID function
  struct S2GeogVertex vertex;
  vertex.v[0] = -73.9857;
  vertex.v[1] = 40.7484;
  vertex.v[2] = 0;
  vertex.v[3] = 0;

  ASSERT_EQ(S2GeogOpEvalGeog(op, point, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);
  EXPECT_EQ(static_cast<uint64_t>(S2GeogOpGetInt(op)),
            S2GeogLngLatToCellId(&vertex));

  // An empty geography has no cell ID
  ASSERT_EQ(S2GeogOpEvalGeog(op, empty, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 0);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(empty);
  S2GeogDestroy(point);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, OpCoveringCellIds) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* wkt = "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt, strlen(wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_COVERING_CELL_IDS),
            S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), "covering_cell_ids");
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_INT);

  // The default covering uses at most 8 cells
  ASSERT_EQ(S2GeogOpEvalGeog(op, geog, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);

  size_t count = S2GeogOpGetIntCount(op);
  ASSERT_GT(count, 0);
  EXPECT_LE(count, 8);

  std::vector<int64_t> cell_ids(count);
  EXPECT_EQ(S2GeogOpGetInts(op, cell_ids.data(), cell_ids.size()), count);
  for (int64_t cell_id : cell_ids) {
    EXPECT_TRUE(S2CellId(static_cast<uint64_t>(cell_id)).is_valid());
  }

  // A smaller output copies only the values that fit
  EXPECT_EQ(S2GeogOpGetInts(op, cell_ids.data(), 1), 1);

  // An explicit level range and cell budget is respected
  ASSERT_EQ(S2GeogOpEvalGeogIntIntInt(op, geog, 0, 30, 4, err), S2GEOGRAPHY_OK);
  EXPECT_GT(S2GeogOpGetIntCount(op), 0);
  EXPECT_LE(S2GeogOpGetIntCount(op), 4);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Unary Geography Operations Tests (Parameterized)
// ============================================================================

struct UnaryGeographyParam {
  const char* name;
  int op_id;
  const char* wkt;
};

class UnaryGeographyTest
    : public ::testing::TestWithParam<UnaryGeographyParam> {};

TEST_P(UnaryGeographyTest, EvalGeog) {
  const auto& p = GetParam();

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  struct S2Geog* result = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&result), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, p.wkt, strlen(p.wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, p.op_id), S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), p.name);
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_GEOGRAPHY);

  ASSERT_EQ(S2GeogOpEvalGeog(op, geog, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);

  // The same output geography can be reused for multiple calls
  ASSERT_EQ(S2GeogOpGetGeog(op, result, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpGetGeog(op, result, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogForcePrepare(result, err), S2GEOGRAPHY_OK);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(result);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

INSTANTIATE_TEST_SUITE_P(
    S2GeographyC, UnaryGeographyTest,
    ::testing::Values(
        UnaryGeographyParam{"centroid", S2GEOGRAPHY_OP_CENTROID,
                            "MULTIPOINT (0 0, 0 2)"},
        UnaryGeographyParam{"convex_hull", S2GEOGRAPHY_OP_CONVEX_HULL,
                            "MULTIPOINT (0 0, 1 0, 1 1, 0 1)"},
        UnaryGeographyParam{"point_on_surface", S2GEOGRAPHY_OP_POINT_ON_SURFACE,
                            "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))"}),
    [](const ::testing::TestParamInfo<UnaryGeographyParam>& info) {
      return info.param.name;
    });

// ============================================================================
// Binary Geography Operations Tests (Parameterized)
// ============================================================================

struct BinaryGeographyParam {
  const char* name;
  int op_id;
  const char* lhs_wkt;
  const char* rhs_wkt;
};

class BinaryGeographyTest
    : public ::testing::TestWithParam<BinaryGeographyParam> {};

TEST_P(BinaryGeographyTest, EvalGeogGeog) {
  const auto& p = GetParam();

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* lhs = nullptr;
  struct S2Geog* rhs = nullptr;
  struct S2Geog* result = nullptr;
  ASSERT_EQ(S2GeogCreate(&lhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&rhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&result), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.lhs_wkt, strlen(p.lhs_wkt), lhs, err),
      S2GEOGRAPHY_OK);
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, p.rhs_wkt, strlen(p.rhs_wkt), rhs, err),
      S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, p.op_id), S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), p.name);
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_GEOGRAPHY);

  ASSERT_EQ(S2GeogOpEvalGeogGeog(op, lhs, rhs, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);

  ASSERT_EQ(S2GeogOpGetGeog(op, result, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogForcePrepare(result, err), S2GEOGRAPHY_OK);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(result);
  S2GeogDestroy(rhs);
  S2GeogDestroy(lhs);
  S2GeogFactoryDestroy(factory);
}

INSTANTIATE_TEST_SUITE_P(
    S2GeographyC, BinaryGeographyTest,
    ::testing::Values(
        BinaryGeographyParam{"intersection", S2GEOGRAPHY_OP_INTERSECTION,
                             "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                             "POLYGON ((0.5 0, 1.5 0, 1.5 1, 0.5 1, 0.5 0))"},
        BinaryGeographyParam{"union", S2GEOGRAPHY_OP_UNION,
                             "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                             "POLYGON ((0.5 0, 1.5 0, 1.5 1, 0.5 1, 0.5 0))"},
        BinaryGeographyParam{"difference", S2GEOGRAPHY_OP_DIFFERENCE,
                             "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                             "POLYGON ((0.5 0, 1.5 0, 1.5 1, 0.5 1, 0.5 0))"},
        BinaryGeographyParam{"sym_difference", S2GEOGRAPHY_OP_SYM_DIFFERENCE,
                             "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))",
                             "POLYGON ((0.5 0, 1.5 0, 1.5 1, 0.5 1, 0.5 0))"},
        BinaryGeographyParam{"closest_point", S2GEOGRAPHY_OP_CLOSEST_POINT,
                             "LINESTRING (0 0, 0 1)", "POINT (1 0.5)"},
        BinaryGeographyParam{"shortest_line", S2GEOGRAPHY_OP_SHORTEST_LINE,
                             "LINESTRING (0 0, 0 1)", "POINT (1 0.5)"},
        BinaryGeographyParam{"longest_line", S2GEOGRAPHY_OP_LONGEST_LINE,
                             "LINESTRING (0 0, 0 1)", "POINT (1 0.5)"}),
    [](const ::testing::TestParamInfo<BinaryGeographyParam>& info) {
      return info.param.name;
    });

// ============================================================================
// Geography + Double Operations Tests (Parameterized)
// ============================================================================

struct GeogDoubleGeographyParam {
  const char* name;
  int op_id;
  const char* wkt;
  double arg1;
};

class GeogDoubleGeographyTest
    : public ::testing::TestWithParam<GeogDoubleGeographyParam> {};

TEST_P(GeogDoubleGeographyTest, EvalGeogDouble) {
  const auto& p = GetParam();

  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  struct S2Geog* result = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&result), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, p.wkt, strlen(p.wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, p.op_id), S2GEOGRAPHY_OK);

  ASSERT_STREQ(S2GeogOpName(op), p.name);
  ASSERT_EQ(S2GeogOpOutputType(op), S2GEOGRAPHY_OUTPUT_TYPE_GEOGRAPHY);

  ASSERT_EQ(S2GeogOpEvalGeogDouble(op, geog, p.arg1, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogOpHasResult(op), 1);

  ASSERT_EQ(S2GeogOpGetGeog(op, result, err), S2GEOGRAPHY_OK);
  EXPECT_EQ(S2GeogForcePrepare(result, err), S2GEOGRAPHY_OK);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(result);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

INSTANTIATE_TEST_SUITE_P(
    S2GeographyC, GeogDoubleGeographyTest,
    ::testing::Values(
        GeogDoubleGeographyParam{"simplify", S2GEOGRAPHY_OP_SIMPLIFY,
                                 "LINESTRING (0 0, 0 0.5, 0 1)", 1000.0},
        GeogDoubleGeographyParam{"buffer", S2GEOGRAPHY_OP_BUFFER, "POINT (0 0)",
                                 1000.0},
        GeogDoubleGeographyParam{"reduce_precision",
                                 S2GEOGRAPHY_OP_REDUCE_PRECISION,
                                 "LINESTRING (0 0, 0 1)", 0.01},
        GeogDoubleGeographyParam{"segmentize", S2GEOGRAPHY_OP_SEGMENTIZE,
                                 "LINESTRING (0 0, 0 1)", 20000.0},
        GeogDoubleGeographyParam{"tessellate_geog",
                                 S2GEOGRAPHY_OP_TESSELLATE_GEOG,
                                 "LINESTRING (0 0, 10 10)", 1000.0},
        GeogDoubleGeographyParam{"tessellate_geom",
                                 S2GEOGRAPHY_OP_TESSELLATE_GEOM,
                                 "LINESTRING (0 0, 10 10)", 1000.0},
        GeogDoubleGeographyParam{"line_interpolate_point",
                                 S2GEOGRAPHY_OP_LINE_INTERPOLATE_POINT,
                                 "LINESTRING (0 0, 0 1)", 0.5}),
    [](const ::testing::TestParamInfo<GeogDoubleGeographyParam>& info) {
      return info.param.name;
    });

// ============================================================================
// Geography Output Tests
// ============================================================================

TEST(S2GeographyC, OpGetGeogRoundTrip) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* input = nullptr;
  struct S2Geog* other = nullptr;
  struct S2Geog* expected = nullptr;
  struct S2Geog* result = nullptr;
  ASSERT_EQ(S2GeogCreate(&input), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&other), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&expected), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&result), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* input_wkt = "MULTIPOINT (0 0, 0 2)";
  const char* other_wkt = "MULTIPOINT (10 0, 10 2)";
  const char* expected_wkt = "POINT (0 1)";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, input_wkt, strlen(input_wkt),
                                     input, err),
            S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, other_wkt, strlen(other_wkt),
                                     other, err),
            S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, expected_wkt,
                                     strlen(expected_wkt), expected, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* centroid = nullptr;
  struct S2GeogOp* distance = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&centroid, S2GEOGRAPHY_OP_CENTROID), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpCreate(&distance, S2GEOGRAPHY_OP_DISTANCE), S2GEOGRAPHY_OK);

  // The centroid of two points is the midpoint between them
  ASSERT_EQ(S2GeogOpEvalGeog(centroid, input, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpGetGeog(centroid, result, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpEvalGeogGeog(distance, result, expected, err),
            S2GEOGRAPHY_OK);
  EXPECT_NEAR(S2GeogOpGetDouble(distance), 0.0, 1e-6);

  // The output is a copy and survives the next evaluation of the operation
  ASSERT_EQ(S2GeogOpEvalGeog(centroid, other, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpEvalGeogGeog(distance, result, expected, err),
            S2GEOGRAPHY_OK);
  EXPECT_NEAR(S2GeogOpGetDouble(distance), 0.0, 1e-6);

  S2GeogOpDestroy(distance);
  S2GeogOpDestroy(centroid);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(result);
  S2GeogDestroy(expected);
  S2GeogDestroy(other);
  S2GeogDestroy(input);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, OpGetGeogIntersectionArea) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* lhs = nullptr;
  struct S2Geog* rhs = nullptr;
  struct S2Geog* result = nullptr;
  ASSERT_EQ(S2GeogCreate(&lhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&rhs), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&result), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* lhs_wkt = "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))";
  const char* rhs_wkt = "POLYGON ((0.5 0, 1.5 0, 1.5 1, 0.5 1, 0.5 0))";
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, lhs_wkt, strlen(lhs_wkt), lhs, err),
      S2GEOGRAPHY_OK);
  ASSERT_EQ(
      S2GeogFactoryInitFromWkt(factory, rhs_wkt, strlen(rhs_wkt), rhs, err),
      S2GEOGRAPHY_OK);

  struct S2GeogOp* intersection = nullptr;
  struct S2GeogOp* area = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&intersection, S2GEOGRAPHY_OP_INTERSECTION),
            S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpCreate(&area, S2GEOGRAPHY_OP_AREA), S2GEOGRAPHY_OK);

  // The overlap is half of the one degree square
  ASSERT_EQ(S2GeogOpEvalGeogGeog(intersection, lhs, rhs, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpGetGeog(intersection, result, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpEvalGeog(area, result, err), S2GEOGRAPHY_OK);
  EXPECT_NEAR(S2GeogOpGetDouble(area), 6.1826e9, 6.1826e7);

  S2GeogOpDestroy(area);
  S2GeogOpDestroy(intersection);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(result);
  S2GeogDestroy(rhs);
  S2GeogDestroy(lhs);
  S2GeogFactoryDestroy(factory);
}

TEST(S2GeographyC, OpGetGeogWrongOutputType) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  struct S2Geog* result = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&result), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* wkt = "POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt, strlen(wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_AREA), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpEvalGeog(op, geog, err), S2GEOGRAPHY_OK);

  EXPECT_EQ(S2GeogOpGetGeog(op, result, err), EINVAL);
  EXPECT_GT(strlen(S2GeogErrorGetMessage(err)), 0);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(result);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Buffer Operation Tests
// ============================================================================

TEST(S2GeographyC, OpBufferArities) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  struct S2Geog* result = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogCreate(&result), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* wkt = "POINT (0 0)";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt, strlen(wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* buffer = nullptr;
  struct S2GeogOp* area = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&buffer, S2GEOGRAPHY_OP_BUFFER), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpCreate(&area, S2GEOGRAPHY_OP_AREA), S2GEOGRAPHY_OK);

  // The default buffer approximates a 1km circle to within a few percent
  const double circle_area = 3.14159265358979323846 * 1000.0 * 1000.0;
  ASSERT_EQ(S2GeogOpEvalGeogDouble(buffer, geog, 1000.0, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpGetGeog(buffer, result, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpEvalGeog(area, result, err), S2GEOGRAPHY_OK);
  double area_default = S2GeogOpGetDouble(area);
  EXPECT_NEAR(area_default, circle_area, circle_area * 0.05);

  // Two segments per quadrant is a coarser approximation
  ASSERT_EQ(S2GeogOpEvalGeogDoubleInt(buffer, geog, 1000.0, 2, err),
            S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpGetGeog(buffer, result, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpEvalGeog(area, result, err), S2GEOGRAPHY_OK);
  double area_quad_segs = S2GeogOpGetDouble(area);
  EXPECT_NE(area_quad_segs, area_default);

  // ...and is exactly what the equivalent parameter string produces
  const char* params = "quad_segs=2";
  ASSERT_EQ(S2GeogOpEvalGeogDoubleString(buffer, geog, 1000.0, params,
                                         strlen(params), err),
            S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpGetGeog(buffer, result, err), S2GEOGRAPHY_OK);
  ASSERT_EQ(S2GeogOpEvalGeog(area, result, err), S2GEOGRAPHY_OK);
  EXPECT_DOUBLE_EQ(S2GeogOpGetDouble(area), area_quad_segs);

  S2GeogOpDestroy(area);
  S2GeogOpDestroy(buffer);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(result);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}

// ============================================================================
// Operator Creation Tests
// ============================================================================

TEST(S2GeographyC, OpCreateUnsupported) {
  struct S2GeogOp* op = nullptr;
  EXPECT_EQ(S2GeogOpCreate(&op, 0), ENOTSUP);
  EXPECT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_COVERING_CELL_IDS + 1), ENOTSUP);
}

TEST(S2GeographyC, OpEvalUnsupportedArity) {
  struct S2GeogFactory* factory = nullptr;
  ASSERT_EQ(S2GeogFactoryCreate(&factory), S2GEOGRAPHY_OK);

  struct S2Geog* geog = nullptr;
  ASSERT_EQ(S2GeogCreate(&geog), S2GEOGRAPHY_OK);

  struct S2GeogError* err = nullptr;
  ASSERT_EQ(S2GeogErrorCreate(&err), S2GEOGRAPHY_OK);

  const char* wkt = "POINT (0 0)";
  ASSERT_EQ(S2GeogFactoryInitFromWkt(factory, wkt, strlen(wkt), geog, err),
            S2GEOGRAPHY_OK);

  struct S2GeogOp* op = nullptr;
  ASSERT_EQ(S2GeogOpCreate(&op, S2GEOGRAPHY_OP_AREA), S2GEOGRAPHY_OK);

  // st_area takes exactly one geography
  EXPECT_EQ(S2GeogOpEvalGeogDouble(op, geog, 0.0, err), EINVAL);
  EXPECT_GT(strlen(S2GeogErrorGetMessage(err)), 0);

  S2GeogOpDestroy(op);
  S2GeogErrorDestroy(err);
  S2GeogDestroy(geog);
  S2GeogFactoryDestroy(factory);
}
