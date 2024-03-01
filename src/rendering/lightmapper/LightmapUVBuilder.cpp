#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#define HYP_XATLAS

#ifdef HYP_XATLAS
#include <xatlas.h>
#endif

namespace hyperion::v2 {

// LightmapUVMap

Bitmap<3> LightmapUVMap::ToBitmap() const
{
    AssertThrowMsg(uvs.Size() == width * height, "Invalid UV map size");

    Bitmap<3> bitmap(width, height);

    for (uint x = 0; x < width; x++) {
        for (uint y = 0; y < height; y++) {
            const uint index = x + y * width;

            const Vec3f color = {
                uvs[index].color.x,
                uvs[index].color.y,
                uvs[index].color.z
            };

            bitmap.SetPixel(x, y, { ubyte(color.x * 255.0f), ubyte(color.y * 255.0f), ubyte(color.z * 255.0f) });
        }
    }

    return bitmap;
}

Array<float> LightmapUVMap::ToFloatArray() const
{
    AssertThrowMsg(uvs.Size() == width * height, "Invalid UV map size");

    // RGBA32F format
    Array<float> float_array;
    float_array.Resize(width * height * 4);

    for (uint x = 0; x < width; x++) {
        for (uint y = 0; y < height; y++) {
            const uint index = (x + width) % width
                + (height - y + height) % height * width;

            float_array[index * 4 + 0] = uvs[index].color.x;
            float_array[index * 4 + 1] = uvs[index].color.y;
            float_array[index * 4 + 2] = uvs[index].color.z;
            float_array[index * 4 + 3] = uvs[index].color.w;

            // float_array[index * 4 + 0] = uvs[index].lightmap_uv.x;
            // float_array[index * 4 + 1] = uvs[index].lightmap_uv.y;
            // float_array[index * 4 + 2] = 0.0f;
            // float_array[index * 4 + 3] = 1.0f;
        }
    }

    return float_array;
}

// LightmapUVBuilder

LightmapUVBuilder::LightmapUVBuilder(const LightmapUVBuilderParams &params)
    : m_params(params)
{
    m_mesh_data.Resize(params.elements.Size());

    for (SizeType i = 0; i < params.elements.Size(); i++) {
        const LightmapEntity &element = params.elements[i];

        LightmapMeshData &lightmap_mesh_data = m_mesh_data[i];

        if (!element.mesh || !element.mesh->GetStreamedMeshData()) {
            continue;
        }

        const Handle<Mesh> &mesh = element.mesh;

        const RC<StreamedMeshData> &mesh_data = mesh->GetStreamedMeshData();
        AssertThrow(mesh_data != nullptr);

        auto ref = mesh_data->AcquireRef();

        lightmap_mesh_data.mesh_id = element.mesh.GetID();

        lightmap_mesh_data.transform = element.transform;

        lightmap_mesh_data.vertex_positions.Resize(ref->GetMeshData().vertices.Size() * 3);
        lightmap_mesh_data.vertex_normals.Resize(ref->GetMeshData().vertices.Size() * 3);
        lightmap_mesh_data.vertex_uvs.Resize(ref->GetMeshData().vertices.Size() * 2);

        lightmap_mesh_data.indices = ref->GetMeshData().indices;

        lightmap_mesh_data.lightmap_uvs.Resize(ref->GetMeshData().vertices.Size());

        for (SizeType i = 0; i < ref->GetMeshData().vertices.Size(); i++) {
            const Vec3f position = element.transform * ref->GetMeshData().vertices[i].GetPosition();
            const Vec3f normal = (element.transform.Inverted().Transpose() * Vec4f(ref->GetMeshData().vertices[i].GetNormal(), 0.0f)).GetXYZ();
            const Vec2f uv = ref->GetMeshData().vertices[i].GetTexCoord0();

            lightmap_mesh_data.vertex_positions[i * 3] = position.x;
            lightmap_mesh_data.vertex_positions[i * 3 + 1] = position.y;
            lightmap_mesh_data.vertex_positions[i * 3 + 2] = position.z;

            lightmap_mesh_data.vertex_normals[i * 3] = normal.x;
            lightmap_mesh_data.vertex_normals[i * 3 + 1] = normal.y;
            lightmap_mesh_data.vertex_normals[i * 3 + 2] = normal.z;

            lightmap_mesh_data.vertex_uvs[i * 2] = uv.x;
            lightmap_mesh_data.vertex_uvs[i * 2 + 1] = uv.y;
        }
    }
}


LightmapUVBuilder::Result LightmapUVBuilder::Build()
{
    if (!m_params.elements.Any()) {
        return { Result::RESULT_ERR, "No elements to build lightmap" };
    }

    LightmapUVMap uv_map;

#ifdef HYP_XATLAS
    xatlas::Atlas *atlas = xatlas::Create();

    for (SizeType i = 0; i < m_mesh_data.Size(); i++) {
        const LightmapMeshData &lightmap_mesh_data = m_mesh_data[i];
    
        xatlas::MeshDecl mesh_decl;
        mesh_decl.indexData = lightmap_mesh_data.indices.Data();
        mesh_decl.indexFormat = xatlas::IndexFormat::UInt32;
        mesh_decl.indexCount = uint32(lightmap_mesh_data.indices.Size());
        mesh_decl.vertexCount = uint32(lightmap_mesh_data.vertex_positions.Size() / 3);
        mesh_decl.vertexPositionData = lightmap_mesh_data.vertex_positions.Data();
        mesh_decl.vertexPositionStride = sizeof(float) * 3;
        mesh_decl.vertexNormalData = lightmap_mesh_data.vertex_normals.Data();
        mesh_decl.vertexNormalStride = sizeof(float) * 3;
        // mesh_decl.vertexUvData = lightmap_mesh_data.vertex_uvs.Data();
        // mesh_decl.vertexUvStride = sizeof(float) * 2;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, mesh_decl);

        if (error != xatlas::AddMeshError::Success) {
            xatlas::Destroy(atlas);

            DebugLog(LogType::Error, "Error adding mesh: %s\n", xatlas::StringForEnum(error));

            return { LightmapUVBuilder::Result::RESULT_ERR, "Error adding mesh" };
        }

        xatlas::AddMeshJoin(atlas);
    }

    xatlas::PackOptions pack_options { };
    // pack_options.resolution = 2048;
    pack_options.padding = 0;
    pack_options.texelsPerUnit = 32.0f;
    pack_options.bilinear = false;
    pack_options.blockAlign = true;
    // pack_options.bruteForce = true;
    // pack_options.rotateCharts = true;

    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas, pack_options);
    
    // write lightmap data
    uv_map.width = atlas->width;
    uv_map.height = atlas->height;
    uv_map.uvs.Resize(atlas->width * atlas->height);

    uv_map.tmp_bitmap = Bitmap<3>(atlas->width, atlas->height);

    for (uint mesh_index = 0; mesh_index < atlas->meshCount; mesh_index++) {
        AssertThrow(mesh_index < m_mesh_data.Size());

        const xatlas::Mesh &atlas_mesh = atlas->meshes[mesh_index];

        // for (uint chart_index = 0; chart_index < atlas_mesh.chartCount; chart_index++) {
        //     const xatlas::Chart &chart = atlas_mesh.chartArray[chart_index];

        //     for (uint face_index = 0; face_index < chart.faceCount; face_index++) {
        //         const uint32 face = chart.faceArray[face_index];
                
        //         FixedArray<Pair<uint, Vec2i>, 3> verts;

        //         for (uint i = 0; i < 3; i++) {
        //             const uint32 vertex_index = atlas_mesh.indexArray[face * 3 + i];

        //             const xatlas::Vertex &v = atlas_mesh.vertexArray[vertex_index];

        //             verts[i] = { v.xref, { int(v.uv[0]), int(v.uv[1]) } };
        //         }

        //         m_mesh_data[mesh_index].lightmap_uvs[verts[0].first * 2 + 0] = float(verts[0].second[0]) / float(atlas->width);
        //         m_mesh_data[mesh_index].lightmap_uvs[verts[0].first * 2 + 1] = float(verts[0].second[1]) / float(atlas->height);
        //         m_mesh_data[mesh_index].lightmap_uvs[verts[1].first * 2 + 0] = float(verts[1].second[0]) / float(atlas->width);
        //         m_mesh_data[mesh_index].lightmap_uvs[verts[1].first * 2 + 1] = float(verts[1].second[1]) / float(atlas->height);
        //         m_mesh_data[mesh_index].lightmap_uvs[verts[2].first * 2 + 0] = float(verts[2].second[0]) / float(atlas->width);
        //         m_mesh_data[mesh_index].lightmap_uvs[verts[2].first * 2 + 1] = float(verts[2].second[1]) / float(atlas->height);

        //         Vec2i pts[3] = { verts[0].second, verts[1].second, verts[2].second };

        //         ubyte random_color[3] = { ubyte(rand() % 256), ubyte(rand() % 256), ubyte(rand() % 256) };
        //         uv_map.tmp_bitmap.FillTriangle(pts[0], pts[1], pts[2], { random_color[0], random_color[1], random_color[2] });
        //         // uv_map.tmp_bitmap.DrawLine(pts[0].x, pts[0].y, pts[1].x, pts[1].y, { random_color[0], random_color[1], random_color[2] });
        //         // uv_map.tmp_bitmap.DrawLine(pts[1].x, pts[1].y, pts[2].x, pts[2].y, { random_color[0], random_color[1], random_color[2] });
        //         // uv_map.tmp_bitmap.DrawLine(pts[2].x, pts[2].y, pts[0].x, pts[0].y, { random_color[0], random_color[1], random_color[2] });
                
        //     }
        // }

        // continue;


        for (uint i = 0; i < atlas_mesh.indexCount; i += 3) {
            bool skip = false;
            int atlas_index = -1;
            FixedArray<Pair<uint, Vec2i>, 3> verts;

            for (uint j = 0; j < 3; j++) {
                // Get UV coordinates for each edge
                const xatlas::Vertex &v = atlas_mesh.vertexArray[atlas_mesh.indexArray[i + j]];

                if (v.atlasIndex == -1) {
                    skip = true;

                    break;
                }
                
                atlas_index = v.atlasIndex;

                verts[j] = { v.xref, { int(v.uv[0]), int(v.uv[1]) } };
            }

            if (skip) {
                continue;
            }
            

            // AssertThrowMsg(atlas_mesh.indexArray[i] < lightmap_uvs.Size() / 2, "Index out of bounds: %d >= %u", atlas_mesh.indexArray[i], lightmap_uvs.Size() / 2);

            m_mesh_data[mesh_index].lightmap_uvs[verts[0].first] = (Vec2f(verts[0].second) / Vec2f { float(atlas->width), float(atlas->height) });
            m_mesh_data[mesh_index].lightmap_uvs[verts[1].first] = (Vec2f(verts[1].second) / Vec2f { float(atlas->width), float(atlas->height) });
            m_mesh_data[mesh_index].lightmap_uvs[verts[2].first] = (Vec2f(verts[2].second) / Vec2f { float(atlas->width), float(atlas->height) });

            Vec2i pts[3] = { verts[0].second, verts[1].second, verts[2].second };

            Vec2i bboxmin(uv_map.width - 1,  uv_map.height - 1); 
            Vec2i bboxmax(0, 0); 
            Vec2i clamp(uv_map.width - 1, uv_map.height - 1);

            for (int i = 0; i < 3; i++) { 
                bboxmin.x = MathUtil::Max(0, MathUtil::Min(bboxmin.x, pts[i].x));
	            bboxmin.y = MathUtil::Max(0, MathUtil::Min(bboxmin.y, pts[i].y));

	            bboxmax.x = MathUtil::Min(clamp.x, MathUtil::Max(bboxmax.x, pts[i].x));
	            bboxmax.y = MathUtil::Min(clamp.y, MathUtil::Max(bboxmax.y, pts[i].y));
            }

            Vec2i P;
            for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) { 
                for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) { 
                    Vec3f bc_screen = MathUtil::CalculateBarycentricCoordinates(Vec2f(pts[0]), Vec2f(pts[1]), Vec2f(pts[2]), Vec2f(P));
                    if (bc_screen.x < 0 || bc_screen.y < 0 || bc_screen.z < 0) {
                        continue;
                    }

                    const Vec2f lightmap_uv {
                        float(P.x) / float(atlas->width),
                        float(P.y) / float(atlas->height)
                    };

                    const uint index = (P.x + atlas->width) % atlas->width
                        + (atlas->height - P.y + atlas->height) % atlas->height * atlas->width;

                    uv_map.uvs[index] = {
                        m_mesh_data[mesh_index].mesh_id,    // mesh_id
                        m_mesh_data[mesh_index].transform,  // transform
                        i / 3,                              // triangle_index
                        bc_screen,                          // barycentric_coords
                        lightmap_uv                         // lightmap_uv
                    };

                    auto mesh_to_uv_indices_it = uv_map.mesh_to_uv_indices.Find(m_mesh_data[mesh_index].mesh_id);

                    if (mesh_to_uv_indices_it == uv_map.mesh_to_uv_indices.End()) {
                        mesh_to_uv_indices_it = uv_map.mesh_to_uv_indices.Insert(m_mesh_data[mesh_index].mesh_id, { }).first;
                    }

                    mesh_to_uv_indices_it->second.PushBack(index);
                } 
            }
        }
    }
    
    // // temp
    // for (uint x = 0; x < atlas->width; x++) {
    //    for (uint y = 0; y <  atlas->height; y++) {
    //        const uint index = (x + atlas->width) % atlas->width
    //            + (atlas->height - y + atlas->height) % atlas->height * atlas->width;

    //     //    uv_map.tmp_bitmap.GetPixelAtIndex(index).SetRGBA(Vec4f(float(x) / float(uv_map.width), float(y) / float(uv_map.height), 0.0f, 1.0f));

    //        uv_map.uvs[index].lightmap_uv = Vec2f(float(x) / float(uv_map.width), float(y) / float(uv_map.height));
    //    }
    // }

    for (SizeType mesh_index = 0; mesh_index < m_mesh_data.Size(); mesh_index++) {
        const LightmapMeshData &lightmap_mesh_data = m_mesh_data[mesh_index];
        const LightmapEntity &element = m_params.elements[mesh_index];

        const Handle<Mesh> &mesh = element.mesh;
        AssertThrow(mesh.IsValid());

        auto ref = mesh->GetStreamedMeshData()->AcquireRef();

        MeshData new_mesh_data;
        // new_mesh_data.vertices = ref->GetMeshData().vertices;
        // new_mesh_data.indices = ref->GetMeshData().indices;

        // for (SizeType i = 0; i < new_mesh_data.indices.Size(); i++) {
        //     Vertex &vertex = new_mesh_data.vertices[new_mesh_data.indices[i]];
        //     const Vec2f &lightmap_uv = lightmap_mesh_data.lightmap_uvs[new_mesh_data.indices[i]];

        //     vertex.SetTexCoord1(lightmap_uv);
        // }

        new_mesh_data.vertices.Resize(atlas->meshes[mesh_index].vertexCount);
        new_mesh_data.indices.Resize(atlas->meshes[mesh_index].indexCount);

        for (uint j = 0; j < atlas->meshes[mesh_index].indexCount; j++) {
            new_mesh_data.indices[j] = atlas->meshes[mesh_index].indexArray[j];

            new_mesh_data.vertices[new_mesh_data.indices[j]] = ref->GetMeshData().vertices[atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].xref];
            new_mesh_data.vertices[new_mesh_data.indices[j]].texcoord1 = Vec2f {
                float(atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].uv[0]) / float(atlas->width),
                float(atlas->meshes[mesh_index].vertexArray[atlas->meshes[mesh_index].indexArray[j]].uv[1]) / float(atlas->height)
            };
        }

        Mesh::SetStreamedMeshData(mesh, StreamedMeshData::FromMeshData(new_mesh_data));
    }

    xatlas::Destroy(atlas);

    return {
        LightmapUVBuilder::Result::RESULT_OK,
        std::move(uv_map)
    };
#else
    return { LightmapUVBuilder::Result::RESULT_ERR, "No method to build lightmap" };
#endif
}

} // namespace hyperion::v2