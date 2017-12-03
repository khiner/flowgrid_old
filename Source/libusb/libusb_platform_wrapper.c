#ifdef _WIN32

# include "core.c"
# include "descriptor.c"
# include "hotplug.c"
# include "io.c"
# include "strerror.c"
# include "sync.c"

# include "os/poll_windows.c"
# include "os/threads_windows.c"
# include "os/windows_winusb.c"
# include "os/windows_nt_common.c"

#elif defined(__APPLE__)

#else
// On linux, we link directly to the system library installed through
// apt-get / pacman or any other package manages
#endif
