WebGPU surface handling for SDL2
================================

The spec for upcomming [WebGPU](https://www.w3.org/TR/webgpu) API defines C
and C++ headers, so that it can be used also out side of Javascript. Currently
there is at least 2 implementations that can be used from native C or C++
code, [Dawn](https://dawn.googlesource.com/dawn) from Google and
[wgpu](https://github.com/gfx-rs/wgpu) from Mozilla.

This library provides a function `SDL_Webgpu_CreateSurface` to create a
WebGPU surface for a window created with SDL.

