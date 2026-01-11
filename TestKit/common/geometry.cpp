#include "./vertex.h"
#include "./geometry.h"

Geometry Geometry::create_fullscreen()
{
    Geometry geom = {};

    auto& device = RHI::get_current_device();

    geom.vbuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "vertex_buffer";
        desc.size               = sizeof(Vertex) * 6;
        desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    geom.ibuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "index_buffer";
        desc.size               = sizeof(uint32_t) * 6;
        desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    auto vertices = geom.vbuffer.get_mapped_range<Vertex>();
    auto indices  = geom.ibuffer.get_mapped_range<uint>();

    // positions
    vertices.at(0).position = {-1.0f, -1.0f, 0.0f};
    vertices.at(1).position = {+1.0f, -1.0f, 0.0f};
    vertices.at(2).position = {+1.0f, +1.0f, 0.0f};
    vertices.at(3).position = {-1.0f, -1.0f, 0.0f};
    vertices.at(4).position = {-1.0f, +1.0f, 0.0f};
    vertices.at(5).position = {+1.0f, +1.0f, 0.0f};

    // normals
    vertices.at(0).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(1).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(2).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(3).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(4).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(5).normal = {0.0f, 0.0f, 1.0f};

    // colors
    vertices.at(0).color = {0.0f, 0.0f, 0.0f};
    vertices.at(1).color = {1.0f, 0.0f, 0.0f};
    vertices.at(2).color = {1.0f, 1.0f, 0.0f};
    vertices.at(3).color = {0.0f, 0.0f, 0.0f};
    vertices.at(4).color = {1.0f, 0.0f, 1.0f};
    vertices.at(5).color = {1.0f, 1.0f, 0.0f};

    // uvs
    vertices.at(0).uv = {0.0f, 0.0f};
    vertices.at(1).uv = {0.0f, 1.0f};
    vertices.at(2).uv = {1.0f, 1.0f};
    vertices.at(3).uv = {0.0f, 0.0f};
    vertices.at(4).uv = {1.0f, 0.0f};
    vertices.at(5).uv = {1.0f, 1.0f};

    // indices
    indices.at(0) = 0;
    indices.at(1) = 1;
    indices.at(2) = 2;
    indices.at(3) = 3;
    indices.at(4) = 4;
    indices.at(5) = 5;

    return geom;
}

Geometry Geometry::create_triangle()
{
    Geometry geom = {};

    auto& device = RHI::get_current_device();

    geom.vbuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "vertex_buffer";
        desc.size               = sizeof(Vertex) * 3;
        desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    geom.ibuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "index_buffer";
        desc.size               = sizeof(uint32_t) * 3;
        desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    auto vertices = geom.vbuffer.get_mapped_range<Vertex>();
    auto indices  = geom.ibuffer.get_mapped_range<uint>();

    // positions
    vertices.at(0).position = {0.0f, 0.0f, 0.0f};
    vertices.at(1).position = {1.0f, 0.0f, 0.0f};
    vertices.at(2).position = {0.0f, 1.0f, 0.0f};

    // normals
    vertices.at(0).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(1).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(2).normal = {0.0f, 0.0f, 1.0f};

    // colors
    vertices.at(0).color = {1.0f, 0.0f, 0.0f};
    vertices.at(1).color = {0.0f, 1.0f, 0.0f};
    vertices.at(2).color = {0.0f, 0.0f, 1.0f};

    // uvs
    vertices.at(0).uv = {0.0f, 0.0f};
    vertices.at(1).uv = {0.0f, 1.0f};
    vertices.at(2).uv = {1.0f, 0.0f};

    // indices
    indices.at(0) = 0;
    indices.at(1) = 1;
    indices.at(2) = 2;

    return geom;
}

Geometry Geometry::create_overlapping_triangles()
{
    Geometry geom = {};

    auto& device = RHI::get_current_device();

    geom.vbuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "vertex_buffer";
        desc.size               = sizeof(Vertex) * 6;
        desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    geom.ibuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "index_buffer";
        desc.size               = sizeof(uint32_t) * 6;
        desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    auto vertices = geom.vbuffer.get_mapped_range<Vertex>();
    auto indices  = geom.ibuffer.get_mapped_range<uint>();

    // positions (tri 1)
    vertices.at(0).position = {0.0f, 0.0f, 0.0f};
    vertices.at(1).position = {1.0f, 0.0f, 0.0f};
    vertices.at(2).position = {0.0f, 1.0f, 0.0f};

    // normals (tri 1)
    vertices.at(0).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(1).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(2).normal = {0.0f, 0.0f, 1.0f};

    // colors (tri 1)
    vertices.at(0).color = {1.0f, 0.0f, 1.0f};
    vertices.at(1).color = {1.0f, 0.0f, 1.0f};
    vertices.at(2).color = {1.0f, 0.0f, 1.0f};

    // uvs (tri 1)
    vertices.at(0).uv = {0.0f, 0.0f};
    vertices.at(1).uv = {0.0f, 1.0f};
    vertices.at(2).uv = {1.0f, 0.0f};

    // positions (tri 2)
    vertices.at(3).position = {0.0f - 0.25f, 0.0f - 0.25f, -1.0f};
    vertices.at(4).position = {1.0f - 0.25f, 0.0f - 0.25f, -1.0f};
    vertices.at(5).position = {0.0f - 0.25f, 1.0f - 0.25f, -1.0f};

    // normals (tri 2)
    vertices.at(3).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(4).normal = {0.0f, 0.0f, 1.0f};
    vertices.at(5).normal = {0.0f, 0.0f, 1.0f};

    // colors (tri 2)
    vertices.at(3).color = {0.0f, 1.0f, 1.0f};
    vertices.at(4).color = {0.0f, 1.0f, 1.0f};
    vertices.at(5).color = {0.0f, 1.0f, 1.0f};

    // uvs (tri 2)
    vertices.at(3).uv = {0.0f, 0.0f};
    vertices.at(4).uv = {0.0f, 1.0f};
    vertices.at(5).uv = {1.0f, 0.0f};

    // indices
    indices.at(0) = 0;
    indices.at(1) = 1;
    indices.at(2) = 2;
    indices.at(3) = 3;
    indices.at(4) = 4;
    indices.at(5) = 5;

    return geom;
}

Geometry Geometry::create_cube()
{
    Geometry geom = {};

    auto& device = RHI::get_current_device();

    geom.vbuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "vertex_buffer";
        desc.size               = sizeof(Vertex) * 24;
        desc.usage              = GPUBufferUsage::VERTEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    geom.ibuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "index_buffer";
        desc.size               = sizeof(uint32_t) * 36;
        desc.usage              = GPUBufferUsage::INDEX | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    auto vertices = geom.vbuffer.get_mapped_range<Vertex>();
    auto indices  = geom.ibuffer.get_mapped_range<uint>();

    // front face
    vertices.at(0).position = {-1.0f, -1.0f, 1.0f};
    vertices.at(1).position = {1.0f, -1.0f, 1.0f};
    vertices.at(2).position = {1.0f, 1.0f, 1.0f};
    vertices.at(3).position = {-1.0f, 1.0f, 1.0f};
    vertices.at(0).normal   = {0.0f, 0.0f, 1.0f};
    vertices.at(1).normal   = {0.0f, 0.0f, 1.0f};
    vertices.at(2).normal   = {0.0f, 0.0f, 1.0f};
    vertices.at(3).normal   = {0.0f, 0.0f, 1.0f};
    vertices.at(0).uv       = {0.0f, 0.0f};
    vertices.at(1).uv       = {1.0f, 0.0f};
    vertices.at(2).uv       = {1.0f, 1.0f};
    vertices.at(3).uv       = {0.0f, 1.0f};

    // back face
    vertices.at(4).position = {-1.0f, -1.0f, -1.0f};
    vertices.at(5).position = {-1.0f, 1.0f, -1.0f};
    vertices.at(6).position = {1.0f, 1.0f, -1.0f};
    vertices.at(7).position = {1.0f, -1.0f, -1.0f};
    vertices.at(4).normal   = {0.0f, 0.0f, -1.0f};
    vertices.at(5).normal   = {0.0f, 0.0f, -1.0f};
    vertices.at(6).normal   = {0.0f, 0.0f, -1.0f};
    vertices.at(7).normal   = {0.0f, 0.0f, -1.0f};
    vertices.at(4).uv       = {1.0f, 0.0f};
    vertices.at(5).uv       = {1.0f, 1.0f};
    vertices.at(6).uv       = {0.0f, 1.0f};
    vertices.at(7).uv       = {0.0f, 0.0f};

    // top face
    vertices.at(8).position  = {-1.0f, 1.0f, -1.0f};
    vertices.at(9).position  = {-1.0f, 1.0f, 1.0f};
    vertices.at(10).position = {1.0f, 1.0f, 1.0f};
    vertices.at(11).position = {1.0f, 1.0f, -1.0f};
    vertices.at(8).normal    = {0.0f, 1.0f, 0.0f};
    vertices.at(9).normal    = {0.0f, 1.0f, 0.0f};
    vertices.at(10).normal   = {0.0f, 1.0f, 0.0f};
    vertices.at(11).normal   = {0.0f, 1.0f, 0.0f};
    vertices.at(8).uv        = {0.0f, 1.0f};
    vertices.at(9).uv        = {0.0f, 0.0f};
    vertices.at(10).uv       = {1.0f, 0.0f};
    vertices.at(11).uv       = {1.0f, 1.0f};

    // bottom face
    vertices.at(12).position = {-1.0f, -1.0f, -1.0f};
    vertices.at(13).position = {1.0f, -1.0f, -1.0f};
    vertices.at(14).position = {1.0f, -1.0f, 1.0f};
    vertices.at(15).position = {-1.0f, -1.0f, 1.0f};
    vertices.at(12).normal   = {0.0f, -1.0f, 0.0f};
    vertices.at(13).normal   = {0.0f, -1.0f, 0.0f};
    vertices.at(14).normal   = {0.0f, -1.0f, 0.0f};
    vertices.at(15).normal   = {0.0f, -1.0f, 0.0f};
    vertices.at(12).uv       = {1.0f, 1.0f};
    vertices.at(13).uv       = {0.0f, 1.0f};
    vertices.at(14).uv       = {0.0f, 0.0f};
    vertices.at(15).uv       = {1.0f, 0.0f};

    // right face
    vertices.at(16).position = {1.0f, -1.0f, -1.0f};
    vertices.at(17).position = {1.0f, 1.0f, -1.0f};
    vertices.at(18).position = {1.0f, 1.0f, 1.0f};
    vertices.at(19).position = {1.0f, -1.0f, 1.0f};
    vertices.at(16).normal   = {1.0f, 0.0f, 0.0f};
    vertices.at(17).normal   = {1.0f, 0.0f, 0.0f};
    vertices.at(18).normal   = {1.0f, 0.0f, 0.0f};
    vertices.at(19).normal   = {1.0f, 0.0f, 0.0f};
    vertices.at(16).uv       = {1.0f, 0.0f};
    vertices.at(17).uv       = {1.0f, 1.0f};
    vertices.at(18).uv       = {0.0f, 1.0f};
    vertices.at(19).uv       = {0.0f, 0.0f};

    // left face
    vertices.at(20).position = {-1.0f, -1.0f, -1.0f};
    vertices.at(21).position = {-1.0f, -1.0f, 1.0f};
    vertices.at(22).position = {-1.0f, 1.0f, 1.0f};
    vertices.at(23).position = {-1.0f, 1.0f, -1.0f};
    vertices.at(20).normal   = {-1.0f, 0.0f, 0.0f};
    vertices.at(21).normal   = {-1.0f, 0.0f, 0.0f};
    vertices.at(22).normal   = {-1.0f, 0.0f, 0.0f};
    vertices.at(23).normal   = {-1.0f, 0.0f, 0.0f};
    vertices.at(20).uv       = {0.0f, 0.0f};
    vertices.at(21).uv       = {1.0f, 0.0f};
    vertices.at(22).uv       = {1.0f, 1.0f};
    vertices.at(23).uv       = {0.0f, 1.0f};

    // set colors to white for all vertices
    for (int i = 0; i < 24; ++i) {
        vertices.at(i).color = {1.0f, 1.0f, 1.0f};
    }

    // indices
    uint* idx = indices.data;
    for (int i = 0; i < 6; ++i) {
        idx[0] = i * 4 + 0;
        idx[1] = i * 4 + 1;
        idx[2] = i * 4 + 2;
        idx[3] = i * 4 + 0;
        idx[4] = i * 4 + 2;
        idx[5] = i * 4 + 3;
        idx += 6;
    }

    return geom;
}
