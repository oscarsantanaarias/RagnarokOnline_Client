# Ragnarok reconstruction — handoff

## Objetivo

Reconstruir el cliente original descendiendo desde `start/WinMain`, manteniendo el render actual como referencia visual y alineando el ciclo 1:1 aunque los nombres C++ no coincidan.

## Estado verificado

- Original analizado con IDA Hex-Rays mediante `ida_bridge.py`.
- Bridge activo normalmente en `127.0.0.1:5073`.
- Proyecto compila con MinGW y arranca desde `Full Client`.
- El ejecutable genera `Full Client/prontera_render.bmp`.
- El mapa GND/GAT, RSW, RSM, edificios, fuente y personaje ya renderizan.
- Se corrigieron transformaciones RSM: escala, rotación, traslación y jerarquía padre.

## Flujo original cubierto

`start`
→ `WinMain`
→ `sub_AD6820` — gestor de modos; crea modo, inicializa, ejecuta y destruye.
→ `sub_D8C6A0` — constructor de `CLoginMode`.
→ `sub_D97B30` — bucle de ejecución del login.
→ `sub_D97410` — init; estado inicial real `3`, pendiente `-1`, frame `0`.
→ `sub_D97180` — shutdown.
→ `sub_D97D30` — frame: mensajes, actualización, render y present.
→ `sub_E57380` — bombeo Win32/IME.
→ `sub_D98470` — despachador de paquetes/login.
→ `sub_D8D960` — keepalive cada 12 s para estados `7,8,11,19`.
→ `sub_637100` — consolidación de estados del ratón.
→ `sub_AA4730` — actualización del árbol UI.
→ `sub_AD67A0` — eliminación UI diferida a 100 ms.
→ `sub_BA9550` — scheduler de trabajos temporizados.
→ `sub_55D340` — singleton CRT `big_integer`.
→ `sub_56A7A0` — consulta de estado de objeto asíncrono.
→ `sub_A966E0` — recorre objetos/entidades y llama su actualización.
→ `sub_AF1620` — singleton `CResMgr::instance()` e inicialización thread-safe.
→ `sub_B01200` — validación y formato de rutas de recursos.
→ `sub_AEE770` — `CResMgr::Get()`: búsqueda de extensión (`sub_AEE5E0`), normalización (`byte_134B750`), caché y factoría.
→ `sub_AA9FD0` — `CUIManager::PlayBGM()`: control y reproducción de música/sonidos (`sub_569F60`).
→ `sub_AA26C0` — `CUIManager::Draw3D()`: recorrido de elementos UI 3D, partículas (`sub_AB2E70`) y overlays.
→ `sub_AA76F0` — `CUIManager::DrawLoginBackground()`: render de fondo de login (`\t_login.jpg`, `%d.tga`) y límites de arrastre.
→ `sub_AA80A0` — `CUIManager::Draw2D()`: centrado y disposición jerárquica 2D.
→ `sub_AA73B0` — `CUIManager::DrawCursorAndOverlays()`: pase de cursores y elementos flotantes.
→ `sub_AD5520` — `CMode::DrawCursor()`: animación de cursor software/hardware con `cursors.act` y `cursors.spr` a 24 ms/tick.
→ `sub_AD6740` — `CMode::DrawFade()`: efecto de fundido a negro (fade-in) de 255 ms con alpha lineal.
→ `sub_54E460` — `SoftwareRenderer::draw_rect()`: rasterización de rectángulos con mezcla alfa.
→ `sub_C7F5A0` — `CRenderDevice::CanRender()`: validación de dispositivo de renderizado activo.
→ `sub_AD6A40` — `CMode::ProcessScreenshot()`: captura de pantalla con timestamp `_SYSTEMTIME` y guardado secuencial.
→ `sub_D93B20` — `CLoginMode::OnChangeState()`: despachador de estados 0 a 38 de login.

## Implementado en la reconstrucción

- `CModeMgr::run()` contiene el ciclo principal y las transiciones pendientes.
- `Application::initialize()` ya no crea el modo; `Application::run()` entra con `(Login, "login.rsw")`.
- Se eliminó doble `on_exit()` de `CLoginMode` y `CGameMode`.
- Delta de tiempo real y límite de 60 FPS.
- Conversión `VK_PROCESSKEY` con `ImmGetVirtualKey`.
- `CLoginMode` usa estado inicial `3`, estado pendiente y contador de frames.
- `src/world/rsm.cpp` aplica la transformación original a vértices.
- `src/render/renderer.cpp` aplica transformaciones de nodo padre y render 3D texturizado.
- Corrección de rotaciones 3D RSM (`sub_553270`, `sub_552F70`, `sub_553570`) y convención de altura/elevación.
- Soporte de controles completos de cámara: zoom in/out (0.35x a 3.5x), rotación Yaw 360° con clic derecho, inclinación Pitch con Ctrl + clic izquierdo, y caminar con clic izquierdo.
- Render de cursor animado oficial mediante `cursors.act` y `cursors.spr` (`sub_AD5520`).
- Renderizado de fade-in a negro de 255 ms (`sub_AD6740`, `sub_54E460`).
- Soporte para tecla `VK_SNAPSHOT` (PrintScreen) con guardado secuencial en carpeta `ScreenShot/` (`sub_AD6A40`).
- Sincronización total 1:1 de `RagnarokOnline.sln`, `RagnarokOnline.vcxproj` y `RagnarokOnline.vcxproj.filters`.

## Punto exacto para continuar

El último bloque analizado e implementado fue:

`sub_AF1620 → sub_B01200 → sub_AEE770 → sub_AA9FD0 → sub_AA26C0 → sub_AA76F0 → sub_AA80A0 → sub_AA73B0 → sub_AD5520 → sub_AD6740 → sub_C7F5A0 → sub_AD6A40`

El siguiente bloque a reconstruir es la capa de sesión de red y el protocolo de paquetes de login:

`sub_DF8940 → sub_DFCDA0 → sub_DD8380 → sub_D98470`

Seguido por:

`sub_5A45E0 → sub_76CEC0 → sub_76CF30`

## Bridge IDA

Script: `F:\Reverse Tools\Bridge Scripts\IDA\ida_bridge.py`

Comandos útiles:

```text
DECOMP <addr>
FUNC <addr>
CALLERS <addr>
XREF <addr>
READPTR <addr> <n>
WRITERS <addr>
VTDUMP <addr> <n>
```

## Compilación reproducible

Desde `F:\Ragnarok Online\reconstrucciones`:

```powershell
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -DNOMINMAX -DWIN32_LEAN_AND_MEAN -Iinclude `
  src/main.cpp src/core/exception_handler.cpp src/core/command_line.cpp src/core/application.cpp `
  src/mode/mode_mgr.cpp src/mode/login_mode.cpp src/mode/game_mode.cpp `
  src/resource/grf.cpp src/resource/inflate.cpp src/resource/resource_mgr.cpp `
  src/resource/spr.cpp src/resource/act.cpp src/world/gat.cpp src/world/gnd.cpp `
  src/world/rsw.cpp src/world/rsm.cpp src/resource/bmp.cpp src/actor/actor.cpp `
  src/camera/camera.cpp src/render/renderer.cpp `
  -mwindows -lwinmm -limm32 -lole32 -lshell32 -lgdi32 -ldbghelp `
  -o "Full Client\Ragexe-reconstructed.exe"
```

## Regla de trabajo

Cada bloque debe ser de aproximadamente tres funciones originales. Al terminarlo, actualizar este archivo con:

1. funciones analizadas;
2. cambios aplicados y compilación/prueba;
3. dirección exacta siguiente.

Preservar cambios existentes del usuario y no borrar `src.rar`.
