using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Mesh : IDisposable
    {
        private ManagedHandle handle;

        public Mesh(List<Vertex> vertices, List<uint> indices)
        {
            var verticesArray = vertices.ToArray();
            var indicesArray = indices.ToArray();
            
            handle = Mesh_Create(verticesArray, (uint)vertices.Count, indicesArray, (uint)indices.Count);
        }

        public Mesh(ManagedHandle handle)
        {
            this.handle = handle;
            this.handle.IncRef(Mesh_GetTypeID());
        }

        public void Dispose()
        {
            handle.DecRef(Mesh_GetTypeID());
        }

        public void Init()
        {
            Mesh_Init(handle);
        }

        public ManagedHandle Handle
        {
            get
            {
                return handle;
            }
        }

        public uint ID
        {
            get
            {
                return handle.id;
            }
        }

        public BoundingBox AABB
        {
            get
            {
                return Mesh_GetAABB(handle);
            }
        }

        [DllImport("libhyperion", EntryPoint = "Mesh_GetTypeID")]
        private static extern TypeID Mesh_GetTypeID();

        [DllImport("libhyperion", EntryPoint = "Mesh_Create")]
        private static extern ManagedHandle Mesh_Create(Vertex[] vertices, uint vertexCount, uint[] indices, uint indexCount);

        [DllImport("libhyperion", EntryPoint = "Mesh_Init")]
        private static extern void Mesh_Init(ManagedHandle mesh);

        [DllImport("libhyperion", EntryPoint = "Mesh_GetAABB")]
        private static extern BoundingBox Mesh_GetAABB(ManagedHandle mesh);
    }
}