// SPDX-License-Identifier: GPL-3.0-only
// This file is part of Jadette.
// Copyright (C) 2021 Joel Jansson
// Distributed under GNU General Public License v3.0
// See gpl-3.0.txt or <https://www.gnu.org/licenses/>


#include "pch_tests.h"

#include "../Scene_file.h"
#include "../Scene_components.h"
#include "../util.h"
#include "../dx12_util.h"


using namespace std;
using namespace DirectX;


ComPtr<ID3D12Device> create_device()
{
    UINT dxgi_factory_flags = 0;
    ComPtr<IDXGIFactory5> dxgi_factory;
    throw_if_failed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&dxgi_factory)));
    ComPtr<IDXGIAdapter1> adapter = nullptr;

    ComPtr<ID3D12Device> device;

    for (UINT i = 0; dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 adapter_description;
        adapter->GetDesc1(&adapter_description);

        if (!(adapter_description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device))))
            break;
    }

    return device;
}


SCENARIO("The scene parser works")
{
    auto device = create_device();

    ComPtr<ID3D12CommandAllocator> allocator;
    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&allocator)));

    auto command_list = create_command_list(device, allocator);

    ComPtr<ID3D12DescriptorHeap> texture_descriptor_heap;
    const int textures_count = 1;
    create_texture_descriptor_heap(device, texture_descriptor_heap, textures_count);

    Scene_components sc;


    GIVEN("Some minimal scene file data")
    {

        istringstream scene_data("#comment test\n"
                                 "model cube cube\n"
                                 "object name static cube none 0 0 0 1");

        WHEN("the data has been parsed")
        {
            int texture_index = 0;
            read_scene_file_stream(scene_data, sc, device, *command_list.Get(), texture_index,
                texture_descriptor_heap, 0);


            THEN("the object is available")
            {
                REQUIRE(sc.graphical_objects.size() == 1);
                REQUIRE(sc.static_objects.size() == 1);
            }

            THEN("the other vectors are empty")
            {
                REQUIRE(sc.dynamic_objects.empty());
                REQUIRE(sc.flying_objects.empty());
                REQUIRE(sc.rotating_objects.empty());
                REQUIRE(sc.transparent_objects.empty());
                REQUIRE(sc.alpha_cut_out_objects.empty());
                REQUIRE(sc.two_sided_objects.empty());
            }
        }
    }

    GIVEN("Some slightly more complex scene file data")
    {

        istringstream scene_data("#comment test\n"
                                 "model cube cube\n"
                                 "object name static cube none 94 2 76 4\n"
                                 "object name2 dynamic cube none -31 14 -5 8\n");

        WHEN("the data has been parsed")
        {
            int texture_index = 0;
            read_scene_file_stream(scene_data, sc, device, *command_list.Get(), texture_index,
                texture_descriptor_heap, 0);


            THEN("the objects are available")
            {
                REQUIRE(sc.graphical_objects.size() == 2);
                REQUIRE(sc.static_objects.size() == 1);
                REQUIRE(sc.dynamic_objects.size() == 1);
            }

            THEN("the other vectors are empty")
            {
                REQUIRE(sc.flying_objects.empty());
                REQUIRE(sc.rotating_objects.empty());
                REQUIRE(sc.transparent_objects.empty());
                REQUIRE(sc.alpha_cut_out_objects.empty());
                REQUIRE(sc.two_sided_objects.empty());
            }
        }
    }

    GIVEN("Some even more complex scene file data")
    {
        istringstream scene_data("#comment test\n"
                                 "model cube cube\n"
                                 "object name static cube none 94 2 76 4\n"
                                 "object name2 dynamic cube none -31 14 -5 8\n"
                                 "fly name2 10 0 2 0 -1 0 20\n"
                                 "model the_plane plane\n"
                                 "ambient 0.3 0.2 0.32\n"
                                 "light 0 0 5 9 100 2 1.8 200 2.5 150 0.1 0.2 0.78 1\n"
                                 "light 2 1 8 9 10 2 3 31 0.4 15 0.7 0.25 0.3 1\n"
                                 "array dynamic the_plane none -20 10 33 5 4 10 4 127 48 17\n"
                                 "fly arrayobject30 -2 7 2 -1 0 0 400\n"
                                 "rotate arrayobject2\n"
                                 "view 100 -85 70 -50 8 99\n");

        WHEN("the data has been parsed")
        {
            int texture_index = 0;
            read_scene_file_stream(scene_data, sc, device, *command_list.Get(), texture_index,
                texture_descriptor_heap, 0);


            THEN("the objects are available")
            {
                REQUIRE(sc.graphical_objects.size() == 202);
                REQUIRE(sc.static_objects.size() == 1);
                REQUIRE(sc.dynamic_objects.size() == 201);
                REQUIRE(sc.flying_objects.size() == 2);
                REQUIRE(sc.rotating_objects.size() == 1);
            }

            THEN("the other vectors are empty.")
            {
                REQUIRE(sc.transparent_objects.empty());
                REQUIRE(sc.alpha_cut_out_objects.empty());
                REQUIRE(sc.two_sided_objects.empty());
            }

            THEN("some of the static model transform data is correct")
            {
                REQUIRE(sc.static_model_transforms.size() == 202);

                XMFLOAT4 translation;
                XMStoreFloat4(&translation, 
                    convert_half4_to_vector(sc.static_model_transforms[0].translation));
                REQUIRE(translation.x == 94);
                REQUIRE(translation.y == 2);
                REQUIRE(translation.z == 76);
                REQUIRE(translation.w == 4); // Scale

                XMFLOAT4 rotation;
                XMStoreFloat4(&rotation,
                    convert_half4_to_vector(sc.static_model_transforms[0].rotation));
                REQUIRE(rotation.x == 0);
                REQUIRE(rotation.y == 0);
                REQUIRE(rotation.z == 0);
                REQUIRE(rotation.w == 1);
            }

            THEN("some of the dynamic model transform data is correct")
            {
                REQUIRE(sc.dynamic_model_transforms.size() == 201);

                XMFLOAT4 translation;
                XMStoreFloat4(&translation,
                    convert_half4_to_vector(sc.dynamic_model_transforms[0].translation));
                REQUIRE(translation.x == -31);
                REQUIRE(translation.y == 14);
                REQUIRE(translation.z == -5);
                REQUIRE(translation.w == 8); // Scale

                XMFLOAT4 rotation;
                XMStoreFloat4(&rotation,
                    convert_half4_to_vector(sc.dynamic_model_transforms[0].rotation));
                REQUIRE(rotation.x == 0);
                REQUIRE(rotation.y == 0);
                REQUIRE(rotation.z == 0);
                REQUIRE(rotation.w == 1);
            }

            THEN("the ambient light data is correct")
            {
                REQUIRE(sc.ambient_light.x == 0.3f);
                REQUIRE(sc.ambient_light.y == 0.2f);
                REQUIRE(sc.ambient_light.z == 0.32f);
                REQUIRE(sc.ambient_light.w == 1.0f);
            }

            THEN("the view data is correct")
            {
                REQUIRE(sc.initial_view_position.x == 100.0f);
                REQUIRE(sc.initial_view_position.y == -85.0f);
                REQUIRE(sc.initial_view_position.z == 70.0f);

                REQUIRE(sc.initial_view_focus_point.x == -50.0f);
                REQUIRE(sc.initial_view_focus_point.y == 8.0f);
                REQUIRE(sc.initial_view_focus_point.z == 99.0f);
            }

            THEN("the light data is correct")
            {
                REQUIRE(sc.lights.size() == 2);

                REQUIRE(sc.lights[0].color.x == 0.1f);
                REQUIRE(sc.lights[0].color.y == 0.2f);
                REQUIRE(sc.lights[0].color.z == 0.78f);

                REQUIRE(sc.lights[0].diffuse_intensity == 1.8f);
                REQUIRE(sc.lights[0].diffuse_reach == 200.0f);

                REQUIRE(sc.lights[0].specular_intensity == 2.5f);
                REQUIRE(sc.lights[0].specular_reach == 150.0f);

                REQUIRE(sc.lights[0].position.x == 0.0f);
                REQUIRE(sc.lights[0].position.y == 0.0f);
                REQUIRE(sc.lights[0].position.z == 5.0f);

                REQUIRE(sc.lights[0].focus_point.x == 9.0f);
                REQUIRE(sc.lights[0].focus_point.y == 100.0f);
                REQUIRE(sc.lights[0].focus_point.z == 2.0f);


                REQUIRE(sc.lights[1].color.x == 0.7f);
                REQUIRE(sc.lights[1].color.y == 0.25f);
                REQUIRE(sc.lights[1].color.z == 0.3f);

                REQUIRE(sc.lights[1].diffuse_intensity == 3.0f);
                REQUIRE(sc.lights[1].diffuse_reach == 31.0);

                REQUIRE(sc.lights[1].specular_intensity == 0.4f);
                REQUIRE(sc.lights[1].specular_reach == 15.0f);

                REQUIRE(sc.lights[1].position.x == 2.0f);
                REQUIRE(sc.lights[1].position.y == 1.0f);
                REQUIRE(sc.lights[1].position.z == 8.0f);

                REQUIRE(sc.lights[1].focus_point.x == 9.0f);
                REQUIRE(sc.lights[1].focus_point.y == 10.0f);
                REQUIRE(sc.lights[1].focus_point.z == 2.0f);
            }
        }
    }

    GIVEN("Some scene file data that has an invalid texture reference")
    {

        istringstream scene_data("model cube cube\n"
                                 "object name static cube pattern 94 2 76 4\n");

        WHEN("the data is parsed")
        {
            THEN("the correct exception is thrown")
            {
                int texture_index = 0;
                REQUIRE_THROWS_AS(read_scene_file_stream(scene_data, sc, device,
                    *command_list.Get(), texture_index, texture_descriptor_heap, 0),
                    Texture_not_defined);
            }
        }
    }

    GIVEN("Some scene file data that has an invalid model reference")
    {

        istringstream scene_data("model cube cube\n"
                                 "object name static sphere none 94 2 76 4\n");

        WHEN("the data is parsed")
        {
            THEN("the correct exception is thrown")
            {
                int texture_index = 0;
                REQUIRE_THROWS_AS(read_scene_file_stream(scene_data, sc, device,
                    *command_list.Get(), texture_index, texture_descriptor_heap, 0),
                    Model_not_defined);
            }
        }
    }

    GIVEN("Some scene file data that has an invalid object reference")
    {

        istringstream scene_data("model cube cube\n"
                                 "object name static cube none 94 2 76 4\n"
                                 "fly plane");

        WHEN("the data is parsed")
        {
            THEN("the correct exception is thrown")
            {
                int texture_index = 0;
                REQUIRE_THROWS_AS(read_scene_file_stream(scene_data, sc, device,
                    *command_list.Get(), texture_index, texture_descriptor_heap, 0),
                    Object_not_defined);
            }
        }
    }

    GIVEN("Some scene file data that references a non-existing model file")
    {
        istringstream scene_data("model model_file a_file_that_doesnt_exist.obj\n"
                                 "object name static model_file none 94 2 76 4\n");

        WHEN("the data is parsed")
        {
            THEN("the correct exception is thrown")
            {
                int texture_index = 0;
                REQUIRE_THROWS_AS(read_scene_file_stream(scene_data, sc, device,
                    *command_list.Get(), texture_index, texture_descriptor_heap, 0),
                    File_open_error);
            }
        }
    }

    GIVEN("Some scene file data that references a non-existing texture file")
    {
        istringstream scene_data("model cube cube\n"
                                 "texture pattern a_file_that_doesnt_exist.dds\n"
                                 "object name static cube pattern 94 2 76 4\n");

        WHEN("the data is parsed")
        {
            THEN("the correct exception is thrown")
            {
                int texture_index = 0;
                REQUIRE_THROWS_AS(read_scene_file_stream(scene_data, sc, device,
                    *command_list.Get(), texture_index, texture_descriptor_heap, 0),
                    File_open_error);
            }
        }
    }

    GIVEN("Some scene file data that contains some non-existing command")
    {
        istringstream scene_data("mesh cube cube\n"
                                 "object name static cube none 94 2 76 4\n");

        WHEN("the data is parsed")
        {
            THEN("the correct exception is thrown")
            {
                int texture_index = 0;
                REQUIRE_THROWS_AS(read_scene_file_stream(scene_data, sc, device,
                    *command_list.Get(), texture_index, texture_descriptor_heap, 0),
                    Read_error);
            }
        }
    }

}
