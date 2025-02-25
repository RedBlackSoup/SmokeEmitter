## SmokeEmitter

A GPU particle system implemented by DX12.

### Dependence:

- Windows 10 or newer.
- VS2022 & MSVC v143 toolsets.

### How to Build:

To build SomkeEmitter for Windows 10 or later, use the latest version of Visual Studio and the provided `SmokeEmitter.sln` solution file. By simply pressing F5, the application will be built and then start. There is an example image that you can see in the project.

![动画](./assets/%E5%8A%A8%E7%94%BB.gif)

### Usage

- Press keys to control the movement of camera.
  `W - forward` `S - backward` `A - left` `D - right`
  `E - upward` `Q - down`

  ![移动](./assets/%E7%A7%BB%E5%8A%A8.gif)

- Press direction keys(`up`, `down`, `left`, `right`) to control the direction of camera.

  ![视角](./assets/%E8%A7%86%E8%A7%92.gif)

- Click side bars to control parameters of the particle system.

  `Color` and `Size`:

  ![颜色大小](./assets/%E9%A2%9C%E8%89%B2%E5%A4%A7%E5%B0%8F.gif)

  `Emitter shape` is a cylinder, so three parameters correspond the `theta angle`, `radius` and `height`.

  ![形状](./assets/%E5%BD%A2%E7%8A%B6.gif)

  `velocity`, `velocity distribution`, `force` controls the movement of particles (`force` corresponds the acceleration)

  ![速度](./assets/%E9%80%9F%E5%BA%A6.gif)

  Other parameters like `life time` and `spawn rate` are the particle properties.

  ![生命周期](./assets/%E7%94%9F%E5%91%BD%E5%91%A8%E6%9C%9F.gif)

### Third party libraries

- ImGui: https://github.com/ocornut/imgui
- stb-images: https://github.com/nothings/stb