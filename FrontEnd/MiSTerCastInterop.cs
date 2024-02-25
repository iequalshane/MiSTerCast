using System;
using System.Runtime.InteropServices;

namespace MiSTerCast
{
    class MiSTerCastInterop
    {
        /*
        int ALIGMENT_CENTER = 0;
        int ALIGMENT_TOP_LEFT = 1;
        int ALIGMENT_TOP = 2;
        int ALIGMENT_TOP_RIGHT = 3;
        int ALIGMENT_RIGHT = 4;
        int ALIGMENT_BOTTOM_RIGHT = 5;
        int ALIGMENT_BOTTOM = 6;
        int ALIGMENT_BOTTOM_LEFT = 7;
        int ALIGMENT_LEFT = 8;

        int ROTATE_NONE = 0;
        int ROTATE_CCW =  1;
        int ROTATE_CW =   2;
        int ROTATE_180 =  3;
        */
        
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void LogDelegate(string message, bool error);

        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void CaptureImageDelegate(int width, int height, IntPtr buffer);

        [DllImport("MISTERCASTLIB.dll", EntryPoint = "Initialize", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool Initialize(LogDelegate logCallback, CaptureImageDelegate captureImageCallback);

        [DllImport("MISTERCASTLIB.dll", EntryPoint = "Shutdown", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool Shutdown();

        [DllImport("MISTERCASTLIB.dll", EntryPoint = "StartStream", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool StartStream(string targetIp);

        [DllImport("MISTERCASTLIB.dll", EntryPoint = "StopStream", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool StopStream();

        [DllImport("MISTERCASTLIB.dll", EntryPoint = "SetModeline", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool SetModeline(
            Double pclock,
            UInt16 hactive,
            UInt16 hbegin,
            UInt16 hend,
            UInt16 htotal,
            UInt16 vactive,
            UInt16 vbegin,
            UInt16 vend,
            UInt16 vtotal,
            bool interlace);

        [DllImport("MISTERCASTLIB.dll", EntryPoint = "SetSource", CallingConvention = CallingConvention.Cdecl)]
        public static extern bool SetSource(
            byte display,
            bool audio,
            bool preview,
            byte alignment,
            byte cropmode,
            UInt16 width,
            UInt16 height,
            Int16 xoffset,
            Int16 yoffset,
            byte rotation);
    }
}
