#include "common.h"

#include "hexutil/basics/hexgrid.h"

#define BOOST_TEST_MODULE HexgridTest
#include <boost/test/included/unit_test.hpp>


namespace hex {

struct Fixture {
};

BOOST_FIXTURE_TEST_SUITE(hexgrid_test, Fixture)

void check_neighbours(Point point, Point *expected_neighbours) {
    PointNeighbours all_neighbours = get_neighbours(point);
    for (int i = 0; i < 6; i++) {
        Point neighbour = get_neighbour(point, i);
        BOOST_CHECK_EQUAL(neighbour, expected_neighbours[i]);
        BOOST_CHECK_EQUAL(all_neighbours[i], expected_neighbours[i]);
        int dir = get_direction(point, neighbour);
        BOOST_CHECK_EQUAL(dir, i);
        int dist = distance_between(point, neighbour);
        BOOST_CHECK_EQUAL(dist, 1);
    }
}

BOOST_AUTO_TEST_CASE(test_neighbours_even) {
    Point even_point(6,6);
    Point expected_even_neighbours[] = { Point(6,5), Point(7,5), Point(7,6), Point(6,7), Point(5,6), Point(5,5) };
    check_neighbours(even_point, expected_even_neighbours);
}

BOOST_AUTO_TEST_CASE(test_neighbours_odd) {
    Point odd_point(7,7);
    Point expected_odd_neighbours[] = { Point(7,6), Point(8,7), Point(8,8), Point(7,8), Point(6,8), Point(6,7) };
    check_neighbours(odd_point, expected_odd_neighbours);
}

BOOST_AUTO_TEST_CASE(test_get_circle_even) {
    Point even_point(6,6);
    int radius = 3;
    int expected_num_scanlines = radius*2 + 1;
    int expected_scanlines[] = { 1, 3, 3, 3, 3, 2, 0 };

    std::vector<int> scanlines = get_circle_scanlines(even_point, radius);
    BOOST_CHECK_EQUAL(scanlines.size(), expected_num_scanlines);
    for (int i = 0; i < expected_num_scanlines; i++) {
        BOOST_CHECK_EQUAL(scanlines[i], expected_scanlines[i]);
    }
}

BOOST_AUTO_TEST_CASE(test_get_circle_odd) {
    Point odd_point(7,7);
    int radius = 3;
    int expected_num_scanlines = radius*2 + 1;
    int expected_scanlines[] = { 0, 2, 3, 3, 3, 3, 1 };

    std::vector<int> scanlines = get_circle_scanlines(odd_point, radius);
    BOOST_CHECK_EQUAL(scanlines.size(), expected_num_scanlines);
    for (int i = 0; i < expected_num_scanlines; i++) {
        BOOST_CHECK_EQUAL(scanlines[i], expected_scanlines[i]);
    }
}

BOOST_AUTO_TEST_CASE(test_pixel_to_point) {
    int x_spacing = 16;
    int y_spacing = 16;
    int slope_width = 4;
    int slope_height = 8;
    Point point = pixel_to_point(0, 0, x_spacing, y_spacing, slope_width, slope_height);
    BOOST_CHECK_EQUAL(point, Point(-1, -1));
    point = pixel_to_point(2, 0, x_spacing, y_spacing, slope_width, slope_height);
    BOOST_CHECK_EQUAL(point, Point(-1, -1));
    point = pixel_to_point(4, 0, x_spacing, y_spacing, slope_width, slope_height);
    BOOST_CHECK_EQUAL(point, Point(0, 0));
    point = pixel_to_point(11, 0, x_spacing, y_spacing, slope_width, slope_height);
    BOOST_CHECK_EQUAL(point, Point(0, 0));
}

BOOST_AUTO_TEST_CASE(test_circle_iterator_zero_radius) {
    hexgrid_circle circle(Point(7,21), 0);
    auto iter = circle.begin();
    BOOST_CHECK_EQUAL(*iter, Point(7, 21));
    BOOST_CHECK_EQUAL(iter == circle.end(), false);
    BOOST_CHECK_EQUAL(iter != circle.end(), true);
    iter++;
    BOOST_CHECK_EQUAL(iter == circle.end(), true);
    BOOST_CHECK_EQUAL(iter != circle.end(), false);
}

BOOST_AUTO_TEST_CASE(test_circle_iterator_big) {
    hexgrid_circle circle(Point(7,21), 2);
    std::vector<Point> points;
    int max = 20;
    for (auto iter = circle.begin(); iter != circle.end() && max > 0; iter++, max--) {
        points.push_back(*iter);
    }
    std::vector<Point> expected_points = get_circle_points(Point(7,21), 2, 100, 100);
    BOOST_CHECK_EQUAL(points, expected_points);
}

BOOST_AUTO_TEST_SUITE_END()

};
