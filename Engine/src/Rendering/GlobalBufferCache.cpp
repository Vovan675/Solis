#include "pch.h"
#include "GlobalBufferCache.h"
#include "Core/Variables.h"
#include "RHI/DynamicRHI.h"

void GlobalBufferCache::shutdown()
{
    global_vertex_buffer = nullptr;
    global_index_buffer = nullptr;
    global_meshlet_vertex_buffer = nullptr;
    global_meshlet_triangle_buffer = nullptr;
}

uint64_t GlobalBufferCache::addVertexBufferData(Engine::Vertex *vertices, uint32_t count)
{
    if (!global_vertex_buffer)
    {
        current_vertex_buffer_max_size = 1'000'000;
        current_vertex_buffer_size = 0;
    }

    if (!global_vertex_buffer || current_vertex_buffer_size + count > current_vertex_buffer_max_size)
    {
        uint64_t new_max_size = std::max(current_vertex_buffer_size + count, current_vertex_buffer_max_size * 3);

        BufferUsage additional_usage = BufferUsage::SHADER_READ_BUFFER;
        if (engine_ray_tracing)
            additional_usage |= BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;

        BufferDescription desc;
        desc.size = sizeof(Engine::Vertex) * new_max_size;
        desc.use_staging_buffer = true;
        desc.usage = BufferUsage::VERTEX_BUFFER | additional_usage;
        desc.storage_stride = sizeof(uint32_t);
        desc.alignment = 16;

        RHIBufferRef new_vertex_buffer = gDynamicRHI->createBuffer(desc);

        if (global_vertex_buffer && current_vertex_buffer_size > 0)
            gDynamicRHI->getCmdList()->copyBuffer(global_vertex_buffer, new_vertex_buffer, 0, 0, current_vertex_buffer_size * sizeof(Engine::Vertex));

        global_vertex_buffer = new_vertex_buffer;
        current_vertex_buffer_max_size = new_max_size;
    }

    BufferDescription staging_buffer_desc;
    staging_buffer_desc.size = sizeof(Engine::Vertex) * count;
    staging_buffer_desc.usage = BufferUsage::STAGING_BUFFER;
    RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_desc);

    staging_buffer->fill(vertices);

    gDynamicRHI->getCmdList()->copyBuffer(staging_buffer, global_vertex_buffer, 0, current_vertex_buffer_size * sizeof(Engine::Vertex), staging_buffer->getSize());

    uint64_t prev_vertex_buffer_size = current_vertex_buffer_size;
    current_vertex_buffer_size += count;
    return prev_vertex_buffer_size;
}

uint64_t GlobalBufferCache::addIndexBufferData(uint32_t *indices, uint32_t count)
{
    if (!global_index_buffer)
    {
        current_index_buffer_max_size = 3'000'000;
        current_index_buffer_size = 0;
    }

    if (!global_index_buffer || current_index_buffer_size + count > current_index_buffer_max_size)
    {
        uint64_t new_max_size = std::max(current_index_buffer_size + count, current_index_buffer_max_size * 3);

        BufferUsage additional_usage = BufferUsage::SHADER_READ_BUFFER;
        if (engine_ray_tracing)
            additional_usage |= BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_BUFFER;

        BufferDescription desc;
        desc.size = sizeof(uint32_t) * new_max_size;
        desc.use_staging_buffer = true;
        desc.usage = BufferUsage::INDEX_BUFFER | additional_usage;
        desc.storage_stride = sizeof(uint32_t);

        RHIBufferRef new_index_buffer = gDynamicRHI->createBuffer(desc);

        if (global_index_buffer && current_index_buffer_size > 0)
            gDynamicRHI->getCmdList()->copyBuffer(global_index_buffer, new_index_buffer, 0, 0, current_index_buffer_size * sizeof(uint32_t));

        // Replace the old buffer with the new one
        global_index_buffer = new_index_buffer;
        current_index_buffer_max_size = new_max_size;
    }

    BufferDescription staging_buffer_desc;
    staging_buffer_desc.size = sizeof(uint32_t) * count;
    staging_buffer_desc.usage = BufferUsage::STAGING_BUFFER;
    RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_desc);

    staging_buffer->fill(indices);

    gDynamicRHI->getCmdList()->copyBuffer(staging_buffer, global_index_buffer, 0, current_index_buffer_size * sizeof(uint32_t), staging_buffer->getSize());

    uint64_t prev_index_buffer_size = current_index_buffer_size;
    current_index_buffer_size += count;
    return prev_index_buffer_size;
}

uint64_t GlobalBufferCache::addMeshletVertexData(uint32_t *data, uint32_t count)
{
    if (!global_meshlet_vertex_buffer)
    {
        current_meshlet_vertex_buffer_max_size = 1'000'000;
        current_meshlet_vertex_buffer_size = 0;
    }

    if (!global_meshlet_vertex_buffer || current_meshlet_vertex_buffer_size + count > current_meshlet_vertex_buffer_max_size)
    {
        uint64_t new_max_size = std::max(current_meshlet_vertex_buffer_size + count, current_meshlet_vertex_buffer_max_size * 3);

        BufferDescription desc;
        desc.size = sizeof(uint32_t) * new_max_size;
        desc.use_staging_buffer = true;
        desc.usage = BufferUsage::VERTEX_BUFFER | BufferUsage::SHADER_READ_BUFFER;
        desc.storage_stride = sizeof(uint32_t);

        RHIBufferRef new_buffer = gDynamicRHI->createBuffer(desc);

        if (global_meshlet_vertex_buffer && current_meshlet_vertex_buffer_size > 0)
            gDynamicRHI->getCmdList()->copyBuffer(global_meshlet_vertex_buffer, new_buffer, 0, 0, current_meshlet_vertex_buffer_size * sizeof(uint32_t));

        global_meshlet_vertex_buffer = new_buffer;
        current_meshlet_vertex_buffer_max_size = new_max_size;
    }

    BufferDescription staging_buffer_desc;
    staging_buffer_desc.size = sizeof(uint32_t) * count;
    staging_buffer_desc.usage = BufferUsage::STAGING_BUFFER;
    RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_desc);

    staging_buffer->fill(data);

    gDynamicRHI->getCmdList()->copyBuffer(staging_buffer, global_meshlet_vertex_buffer, 0, current_meshlet_vertex_buffer_size * sizeof(uint32_t), staging_buffer->getSize());

    uint64_t prev_size = current_meshlet_vertex_buffer_size;
    current_meshlet_vertex_buffer_size += count;
    return prev_size;
}

uint64_t GlobalBufferCache::addMeshletTriangleData(uint32_t *data, uint32_t count)
{
    if (!global_meshlet_triangle_buffer)
    {
        current_meshlet_triangle_buffer_max_size = 3'000'000;
        current_meshlet_triangle_buffer_size = 0;
    }

    if (!global_meshlet_triangle_buffer || current_meshlet_triangle_buffer_size + count > current_meshlet_triangle_buffer_max_size)
    {
        uint64_t new_max_size = std::max(current_meshlet_triangle_buffer_size + count, current_meshlet_triangle_buffer_max_size * 3);

        BufferDescription desc;
        desc.size = sizeof(uint32_t) * new_max_size;
        desc.use_staging_buffer = true;
        desc.usage = BufferUsage::INDEX_BUFFER | BufferUsage::SHADER_READ_BUFFER;
        desc.storage_stride = sizeof(uint32_t);

        RHIBufferRef new_buffer = gDynamicRHI->createBuffer(desc);

        if (global_meshlet_triangle_buffer && current_meshlet_triangle_buffer_size > 0)
            gDynamicRHI->getCmdList()->copyBuffer(global_meshlet_triangle_buffer, new_buffer, 0, 0, current_meshlet_triangle_buffer_size * sizeof(uint32_t));

        global_meshlet_triangle_buffer = new_buffer;
        current_meshlet_triangle_buffer_max_size = new_max_size;
    }

    BufferDescription staging_buffer_desc;
    staging_buffer_desc.size = sizeof(uint32_t) * count;
    staging_buffer_desc.usage = BufferUsage::STAGING_BUFFER;
    RHIBufferRef staging_buffer = gDynamicRHI->createBuffer(staging_buffer_desc);

    staging_buffer->fill(data);

    gDynamicRHI->getCmdList()->copyBuffer(staging_buffer, global_meshlet_triangle_buffer, 0, current_meshlet_triangle_buffer_size * sizeof(uint32_t), staging_buffer->getSize());

    uint64_t prev_size = current_meshlet_triangle_buffer_size;
    current_meshlet_triangle_buffer_size += count;
    return prev_size;
}
