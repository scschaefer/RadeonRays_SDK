#include "radeon_rays.h"
#include <GL/glew.h>
#include <GLUT/GLUT.h>
#include <cassert>
#include <iostream>
#include <memory>
#include "../tools/shader_manager.h"

using namespace RadeonRays;

namespace {
    float const g_vertices[] = {
        -1.f,-1.f,0.f,
        1.f,-1.f,0.f,
        0.f,1.f,0.f,
    };
    float const g_normals[] = {
        -1.f,-1.f,1.f,
        1.f,-1.f,1.f,
        0.f,1.f,1.f,
    };
    int const g_indices[] = { 0, 1, 2 };
    const int g_numfaceverts[] = { 3 };
    
    GLuint g_vertex_buffer, g_index_buffer;
    GLuint g_texture;
    int g_window_width = 640;
    int g_window_height = 480;
    std::unique_ptr<ShaderManager> g_shader_manager;
}

void InitGl()
{
    g_shader_manager.reset(new ShaderManager());
    glClearColor(0.0, 0.0, 0.0, 0.0);

    glCullFace(GL_FRONT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glGenBuffers(1, &g_vertex_buffer);
    glGenBuffers(1, &g_index_buffer);
    // create Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer);
    // fill data
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertices), g_vertices, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_indices), g_indices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glGenTextures(1, &g_texture);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_window_width, g_window_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void DrawScene()
{
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, g_window_width, g_window_height);

    glClear(GL_COLOR_BUFFER_BIT);
    GLuint program = g_shader_manager->GetProgram("simple");
    glUseProgram(program);

    GLuint texloc = glGetUniformLocation(program, "g_Texture");
    assert(texloc >= 0);
    glUniform1i(texloc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_index_buffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    GLuint position_attr = glGetAttribLocation(program, "inPosition");

    glVertexAttribPointer(position_attr, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(position_attr);

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glUseProgram(0);

    glFinish();
    glutSwapBuffers();
}

int main(int argc, char* argv[])
{
    // GLUT Window Initialization:
    glutInit(&argc, (char**)argv);
    glutInitWindowSize(640, 480);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutCreateWindow("Triangle");
#ifndef __APPLE__
    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::cout << "GLEW initialization failed\n";
        return -1;
    }
#endif

    InitGl();

    int nativeidx = -1;

    // Always use OpenCL
    IntersectionApi::SetPlatform(DeviceInfo::kOpenCL);

    for (auto idx = 0U; idx < IntersectionApi::GetDeviceCount(); ++idx)
    {
        DeviceInfo devinfo;
        IntersectionApi::GetDeviceInfo(idx, devinfo);

        if (devinfo.type == DeviceInfo::kGpu && nativeidx == -1)
        {
            nativeidx = idx;
        }
    }
    assert(nativeidx != -1);
    IntersectionApi* api = IntersectionApi::Create(nativeidx);

    Shape* shape = api->CreateMesh(g_vertices, 3, 3 * sizeof(float), g_indices, 0, g_numfaceverts, 1);
    assert(shape != nullptr);
    api->AttachShape(shape);

    // Rays
    ray rays[3];

    // Prepare the ray
    rays[0].o = float4(0.f, 0.f, -1.f, 1000.f);
    rays[0].d = float3(0.f, 0.f, 10.f);
    rays[1].o = float4(0.f, 0.5f, -10.f, 1000.f);
    rays[1].d = float3(0.f, 0.f, 1.f);
    rays[2].o = float4(0.4f, 0.f, -10.f, 1000.f);
    rays[2].d = float3(0.f, 0.f, 1.f);

    // Intersection and hit data
    Intersection isect[3];

    auto ray_buffer = api->CreateBuffer(3 * sizeof(ray), rays);
    auto isect_buffer = api->CreateBuffer(3 * sizeof(Intersection), nullptr);
    api->Commit();
    
    api->QueryIntersection(ray_buffer, 3, isect_buffer, nullptr, nullptr);
    Event* e = nullptr;
    Intersection* tmp = nullptr;
    api->MapBuffer(isect_buffer, kMapRead, 0, 3 * sizeof(Intersection), (void**)&tmp, &e);
    e->Wait();
    api->DeleteEvent(e);
    e = nullptr;
    
    isect[0] = tmp[0];
    isect[1] = tmp[1];
    isect[2] = tmp[2];
    std::vector<unsigned char> tex_data(g_window_width * g_window_height * 4);
    for (int i = 0; i < g_window_width * g_window_height; ++i)
    {
        tex_data[4 * i] = 255;
        tex_data[4 * i + 1] = 255;
        tex_data[4 * i + 2] = 255;
        tex_data[4 * i + 3] = 255;
    }

    for (int i = 0; i < 3; ++i)
    {
        if (isect[i].shapeid == kNullId)
            continue;
        float x = g_vertices[3] * isect[i].uvwt.x + g_vertices[6] * isect[i].uvwt.y + g_vertices[0] * (1 - isect[i].uvwt.x - isect[i].uvwt.y);
        float y = g_vertices[4] * isect[i].uvwt.x + g_vertices[7] * isect[i].uvwt.y + g_vertices[1] * (1 - isect[i].uvwt.x - isect[i].uvwt.y);

        int k = g_window_height * g_window_width * (y + 1) / 2 + g_window_width * (x + 1) / 2;
        tex_data[k*4] = 255;
        tex_data[k*4 + 1] = 0;
        tex_data[k*4 + 2] = 0;
    }

    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_window_width, g_window_height, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
    glBindTexture(GL_TEXTURE_2D, NULL);

    glutDisplayFunc(DrawScene);
    glutMainLoop(); //Start the main loop

    api->DetachShape(shape);
    api->DeleteShape(shape);
    IntersectionApi::Delete(api);

    return 0;
}