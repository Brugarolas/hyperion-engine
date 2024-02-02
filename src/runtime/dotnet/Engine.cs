using System;
using System.Runtime.InteropServices;

namespace Hyperion
{
    public class Engine
    {
        private IntPtr ptr;

        private World world;

        public Engine(IntPtr ptr)
        {
            this.ptr = ptr;

            this.world = new World(Engine_GetWorld(this.ptr));
        }

        public World World
        {
            get
            {
                return world;
            }
        }

        [DllImport("libhyperion", EntryPoint = "Engine_GetInstance")]
        private static extern IntPtr Engine_GetInstance();

        [DllImport("libhyperion", EntryPoint = "Engine_GetWorld")]
        private static extern IntPtr Engine_GetWorld(IntPtr enginePtr);

        private static Engine? instance = null;

        public static Engine Instance
        {
            get
            {
                if (instance == null)
                {
                    instance = new Engine(Engine_GetInstance());
                }

                return instance;
            }
        }
    }
}