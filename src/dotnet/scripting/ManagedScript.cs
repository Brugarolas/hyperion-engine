using System;
using System.IO;
using System.Runtime.InteropServices;

namespace Hyperion
{
    [Flags]
    public enum ManagedScriptState : uint
    {
        Uninitialized = 0x0,
        Compiled = 0x1,
        Dirty = 0x2,
        Processing = 0x4,
        Errored = 0x8
    }

    [StructLayout(LayoutKind.Sequential, Size = 2064)]
    public unsafe struct ManagedScript
    {
        private fixed byte path[1024];

        private fixed byte assemblyPath[1024];

        [MarshalAs(UnmanagedType.U4)]
        private uint state;

        [MarshalAs(UnmanagedType.U8)]
        public ulong lastModifiedTimestamp;

        public string Path
        {
            get
            {
                fixed (byte* p = path)
                {
                    return Marshal.PtrToStringAnsi((IntPtr)p);
                }
            }
            set
            {
                fixed (byte* p = path)
                {
                    byte[] bytes = System.Text.Encoding.ASCII.GetBytes(value);
                    Marshal.Copy(bytes, 0, (IntPtr)p, bytes.Length);
                }
            }
        }

        public string AssemblyPath
        {
            get
            {
                fixed (byte* p = assemblyPath)
                {
                    return Marshal.PtrToStringAnsi((IntPtr)p);
                }
            }
            set
            {
                fixed (byte* p = assemblyPath)
                {
                    byte[] bytes = System.Text.Encoding.ASCII.GetBytes(value);
                    Marshal.Copy(bytes, 0, (IntPtr)p, bytes.Length);
                }
            }
        }

        public ManagedScriptState State
        {
            get
            {
                return (ManagedScriptState)state;
            }
            set
            {
                state = (uint)value;
            }
        }

        public ulong LastModifiedTimestamp
        {
            get
            {
                return lastModifiedTimestamp;
            }
            set
            {
                lastModifiedTimestamp = value;
            }
        }
    }

    public class ManagedScriptWrapper
    {
        private IntPtr ptr;

        public ManagedScriptWrapper(IntPtr ptr)
        {
            this.ptr = ptr;
        }

        public bool IsValid
        {
            get
            {
                return ptr != IntPtr.Zero;
            }
        }

        public bool IsErrored
        {
            get
            {
                if (!IsValid)
                {
                    return false;
                }

                return (Get().State & ManagedScriptState.Errored) != 0;
            }
        }

        public bool IsDirty
        {
            get
            {
                if (!IsValid)
                {
                    return false;
                }

                return (Get().State & ManagedScriptState.Dirty) != 0;
            }
        }

        public bool IsProcessing
        {
            get
            {
                if (!IsValid)
                {
                    return false;
                }

                return (Get().State & ManagedScriptState.Processing) != 0;
            }
        }

        public unsafe ref ManagedScript Get()
        {
            if (!IsValid)
            {
                throw new InvalidOperationException("ManagedScriptWrapper is not initialized");
            }

            return ref System.Runtime.CompilerServices.Unsafe.AsRef<ManagedScript>(ptr.ToPointer());
        }

        public void UpdateState()
        {
            if (!IsValid || IsErrored)
            {
                return;
            }

            ref ManagedScript managedScript = ref Get();

            if (!File.Exists(managedScript.Path))
            {
                managedScript.State |= ManagedScriptState.Errored;

                return;
            }

            ulong lastModifiedTimestamp = (ulong)(new FileInfo(managedScript.Path).LastWriteTimeUtc - new DateTime(1970, 1, 1)).TotalSeconds;

            if (lastModifiedTimestamp > managedScript.LastModifiedTimestamp)
            {
                Console.WriteLine("File {0} has been modified at {1}. Previous timestamp was {2}", managedScript.Path, lastModifiedTimestamp, managedScript.LastModifiedTimestamp);

                managedScript.State |= ManagedScriptState.Dirty;
                managedScript.LastModifiedTimestamp = lastModifiedTimestamp;
            }
        }
    }
}