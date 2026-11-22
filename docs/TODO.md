# Pending tasks

**[Documentation](README.md)** · **[Roadmap](ROADMAP.md)** ·
**[Project board](https://github.com/users/mikepadiernos/projects/3)**

## mpv integration (#563)

[#563](https://github.com/mikepadiernos/plasma-opus-file-manager/issues/563)
is in Development on `feature/M23-integrations`. The external **Open with mpv**
action is implemented for selected media, with explicit errors for remote
locations, a missing executable or launch failure.

Embedded playback is not implemented. KDE's MpvQt uses Qt Quick's OpenGL
renderer, changes the process numeric locale and can terminate the application
when mpv initialization fails. Before selecting an implementation, verify a
safe libmpv rendering and lifecycle approach for Wayland and X11, including
failure handling and builds without the optional library. Implement backend
selection, audio/video controls, preview and gallery behavior, and cleanup;
test autoplay, seeking, errors, remote temporary-file lifetime, and repeated
open/close before moving the issue to Review.
