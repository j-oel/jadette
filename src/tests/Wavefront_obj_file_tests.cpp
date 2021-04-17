// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch_tests.h"

#include "../Wavefront_obj_file.h"

 
using namespace std;
using namespace DirectX;
using namespace DirectX::PackedVector;

SCENARIO("The Obj parser works")
{
    vector<XMFLOAT4> input_vertices;
    vector<XMHALF4> input_normals;
    vector<XMHALF2> input_texture_coords;
    vector<XMHALF4> input_tangents;
    vector<XMHALF4> input_bitangents;
    vector<XMHALF4> input_colors;

    Vertices vertices;
    vector<int> indices;
    string material;

    auto collection = std::make_shared<Model_collection>();


    GIVEN("Some minimal Wavefront Obj data")
    {

        istringstream obj_data("v 1 2 9\n"
                               "vn 0 0 0\n"
                               "f 1//1");

        WHEN("the data has been parsed")
        {
            bool more_objects = read_obj_file(obj_data, vertices, indices, input_vertices,
                input_normals, input_texture_coords, input_tangents, input_bitangents,
                input_colors, material, &collection->materials, Obj_flip_v:: yes);

            THEN("the vertex is available")
            {
                REQUIRE(vertices.positions.size() == 1);
                
                auto position = XMLoadFloat4(&vertices.positions[0].position);
                
                REQUIRE(position.m128_f32[0] == 1.0f);
                REQUIRE(position.m128_f32[1] == 2.0f);
                REQUIRE(position.m128_f32[2] == 9.0f);
            }
        }
    }


    GIVEN("Some minimal Wavefront Obj data with texture coordinates")
    {

        istringstream obj_data("v 3 6 8\n"
                               "vn 0.3 0.1 0.7\n"
                               "vt 0.2 0.4\n"
                               "f 1/1/1");

        WHEN("the data has been parsed")
        {
            bool more_objects = read_obj_file(obj_data, vertices, indices, input_vertices,
                input_normals, input_texture_coords, input_tangents, input_bitangents,
                input_colors, material, &collection->materials, Obj_flip_v::yes);

            THEN("the vertex position is available")
            {
                REQUIRE(vertices.positions.size() == 1);

                auto position = XMLoadFloat4(&vertices.positions[0].position);

                REQUIRE(position.m128_f32[0] == 3.0f);
                REQUIRE(position.m128_f32[1] == 6.0f);
                REQUIRE(position.m128_f32[2] == 8.0f);
            }

            THEN("the normal is available")
            {
                REQUIRE(vertices.normals.size() == 1);

                auto normal = convert_half4_to_vector(vertices.normals[0].normal);

                REQUIRE(normal.m128_f32[0] == Approx(0.3f).epsilon(0.001));
                REQUIRE(normal.m128_f32[1] == Approx(0.1f).epsilon(0.001));
                REQUIRE(normal.m128_f32[2] == Approx(0.7f).epsilon(0.001));
            }
            
            THEN("the texture coordinates are available")
            {
                REQUIRE(vertices.positions.size() == 1);
                REQUIRE(vertices.normals.size() == 1);

                auto position = XMLoadFloat4(&vertices.positions[0].position);
                auto normal = convert_half4_to_vector(vertices.normals[0].normal);

                REQUIRE(position.m128_f32[3] == Approx(0.2f).epsilon(0.001));
                // obj files use an inverted v-axis
                REQUIRE(normal.m128_f32[3] == Approx(0.6f).epsilon(0.001));
            }
        }
    }


    GIVEN("Some minimal Wavefront Obj data with vertex colors")
    {

        istringstream obj_data("v 1 2 9 0.3 0.8 0.1\n"
                               "vn 0 0 0\n"
                               "f 1//1");

        WHEN("the data has been parsed")
        {
            bool more_objects = read_obj_file(obj_data, vertices, indices, input_vertices,
                input_normals, input_texture_coords, input_tangents, input_bitangents,
                input_colors, material, &collection->materials, Obj_flip_v::yes);

            THEN("the position is available")
            {
                REQUIRE(vertices.positions.size() == 1);

                auto position = XMLoadFloat4(&vertices.positions[0].position);

                REQUIRE(position.m128_f32[0] == 1.0f);
                REQUIRE(position.m128_f32[1] == 2.0f);
                REQUIRE(position.m128_f32[2] == 9.0f);
            }

            THEN("the vertex colors are available")
            {
                REQUIRE(vertices.colors.size() == 1);

                auto color = convert_half4_to_vector(vertices.colors[0].color);

                REQUIRE(color.m128_f32[0] == Approx(0.3f).epsilon(0.001));
                REQUIRE(color.m128_f32[1] == Approx(0.8f).epsilon(0.001));
                REQUIRE(color.m128_f32[2] == Approx(0.1f).epsilon(0.001));
            }
        }
    }


    GIVEN("Some minimal Wavefront Obj data with vertex colors with alpha component")
    {

        istringstream obj_data("v 1 2 9 0.3 0.8 0.1 0.5\n"
                               "vn 0 0 0\n"
                               "f 1//1");

        WHEN("the data has been parsed")
        {
            bool more_objects = read_obj_file(obj_data, vertices, indices, input_vertices,
                input_normals, input_texture_coords, input_tangents, input_bitangents,
                input_colors, material, &collection->materials, Obj_flip_v::yes);

            THEN("the position is available")
            {
                REQUIRE(vertices.positions.size() == 1);

                auto position = XMLoadFloat4(&vertices.positions[0].position);

                REQUIRE(position.m128_f32[0] == 1.0f);
                REQUIRE(position.m128_f32[1] == 2.0f);
                REQUIRE(position.m128_f32[2] == 9.0f);
            }

            THEN("the vertex colors are available")
            {
                REQUIRE(vertices.colors.size() == 1);

                auto color = convert_half4_to_vector(vertices.colors[0].color);

                REQUIRE(color.m128_f32[0] == Approx(0.3f).epsilon(0.001));
                REQUIRE(color.m128_f32[1] == Approx(0.8f).epsilon(0.001));
                REQUIRE(color.m128_f32[2] == Approx(0.1f).epsilon(0.001));
                REQUIRE(color.m128_f32[3] == Approx(0.5f).epsilon(0.001));
            }
        }
    }
}
