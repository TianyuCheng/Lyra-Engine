#include "camera.h"
#include "uniform.h"

Uniform Uniform::create(float fovy, float aspect, const glm::vec3& eye)
{
    Uniform res = {};

    TestCamera camera;
    camera.proj = glm::perspective(fovy, aspect, 0.1f, 100.0f);
    camera.view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    auto& device = RHI::get_current_device();

    res.ubuffer = execute([&]() {
        auto desc               = GPUBufferDescriptor{};
        desc.label              = "uniform_buffer";
        desc.size               = sizeof(TestCamera);
        desc.usage              = GPUBufferUsage::UNIFORM | GPUBufferUsage::MAP_WRITE;
        desc.mapped_at_creation = true;
        return device.create_buffer(desc);
    });

    auto uniform  = res.ubuffer.get_mapped_range<TestCamera>();
    uniform.at(0) = camera;
    return res;
}
