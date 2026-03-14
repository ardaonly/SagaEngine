# Thirdparty Dependencies

SagaEngine requires a few external libraries:

- **Boost 1.90.0** – header-only, used for `asio`
- **Qt 6.10.2** – Widgets, Core, Gui
- **Diligent Framework** – GraphicsEngineOpenGL / Vulkan
- **RmlUi** – UI library
- **FreeType** – font rendering
- **googletest/googlemock** – for tests
- **pqxx**
- **hiredis**
- **redis++**

> Place libraries in `Thirdparty/source` or `Thirdparty/prebuilt` before building.
> The build system isn't working well right now; it will be rewritten and improved.