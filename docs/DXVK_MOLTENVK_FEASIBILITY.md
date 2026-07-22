# DXVK Native + MoltenVK: feasibility для RRR3D

## Решение

**Milestone 4 завершён аргументированным отказом от DXVK Native + MoltenVK как
renderer backend.** Изолированный target `rrr3d_dxvk_moltenvk_test` создан и
собирается нативно для arm64/macOS 13. Он успешно создаёт SDL3/Cocoa окно,
загружает MoltenVK, инвентаризирует Vulkan instance/device extensions и
передаёт `SDL_Window*` в DXVK Native D3D9 WSI. Штатный DXVK 2.7.1 затем
намеренно отклоняет Apple M3 Max из-за отсутствия обязательного Vulkan feature
`geometryShader`.

Это не ошибка CMake, rpath, SDL WSI или portability enumeration. Metal не
предоставляет необходимую DXVK совокупность возможностей. Временное отключение
проверок выявило ещё три блокера и привело к воспроизводимому падению первого
D3D9 draw call внутри MoltenVK. Эти небезопасные ослабления в поставляемый патч
не включены.

## Что оставлено в репозитории

- `rrr3d_dxvk_moltenvk_test` — отдельный executable, не часть
  `Rock3dEngine`, `Rock3dGame` или `RRR3d`;
- `scripts/build_dxvk_macos.sh` — воспроизводимая отдельная Meson-сборка только
  D3D9 и SDL3 WSI;
- `cmake/patches/dxvk-2.7.1-macos.patch` — минимальный platform/build patch без
  подмены обязательных features;
- CMake option `RRR3D_BUILD_DXVK_MOLTENVK_TEST=ON` и cache paths
  `RRR3D_DXVK_NATIVE_ROOT`, `RRR3D_MOLTENVK_ROOT`;
- post-build staging `libdxvk_d3d9.0.dylib`, `libSDL3.0.dylib`,
  `libMoltenVK.dylib`, `@rpath` install names и ad-hoc codesign;
- custom target `run_rrr3d_dxvk_moltenvk_test`, который устанавливает
  `DXVK_WSI_DRIVER=SDL3`.

Обычная portable game build остаётся с `RRR3D_ENABLE_RENDERER=OFF` и не
получает ни одной из этих dylib.

## Точные версии проверенной среды

| Компонент | Версия |
| --- | --- |
| Hardware | Apple M3 Max, arm64, 48 GiB unified memory |
| macOS | 26.5.2, build 25F84 |
| Minimum deployment target | macOS 13.0 |
| AppleClang | 21.0.0, clang-2100.1.1.101 |
| CMake / Ninja | 4.3.1 / 1.13.2 |
| Meson | 1.11.2 |
| glslang | 16.4.0 |
| SDL3 | 3.4.12, project-built shared dylib |
| DXVK | v2.7.1, commit `c3dd74be6baec53786d4e064a572185b70347a17` |
| MoltenVK formula | Homebrew 1.4.1 |
| MoltenVK runtime-reported Vulkan | instance 1.4.334, device 1.3.334 |
| DXVK driver log | `MoltenVK 0.2.2209` |
| Vulkan headers / loader installed | 1.4.341.0 / 1.4.341.0 |

DXVK submodules также зафиксированы commit-ом superproject:

- native/directx `9df86f2341616ef1888ae59919feaa6d4fad693d`;
- SPIRV-Headers `8b246ff75c6615ba4532fe4fde20f1be090c3764`;
- Vulkan-Headers `234c4b7370a8ea3239a214c9e871e4b17c89f4ab`;
- libdisplay-info `275e6459c7ab1ddd4b125f28d0440716e4888078`.

Homebrew `libvulkan.dylib` и `libSDL3.0.dylib` собраны для minimum macOS 26 и
не входят в staged target. SDL3 собирается проектом с target 13.0; DXVK dylib
тоже имеет `LC_BUILD_VERSION minos 13.0`. MoltenVK dylib имеет minimum 11.0.

## Mac-specific patch DXVK

Unmodified DXVK 2.7.1 не собирается на Darwin. Патч делает только необходимое
для честного эксперимента:

1. включает POSIX `dlopen` compatibility на `__APPLE__`;
2. реализует executable path и однопараметрический `pthread_setname_np` macOS;
3. устраняет две arm64/Clang неоднозначности `size_t`/`lzcnt`;
4. не передаёт GNU ld `--version-script` Apple linker;
5. ищет `libMoltenVK.dylib`/`libvulkan.dylib`, а не Linux `.so`;
6. включает `VK_KHR_portability_enumeration` и
   `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`, если extension доступен;
7. включает device extension `VK_KHR_portability_subset`, если он объявлен
   адаптером.

Linux-only Meson параметры, Wine или MinGW не используются. Полученная
`libdxvk_d3d9.0.dylib` — Mach-O arm64 и не имеет link-time зависимости от SDL
или Vulkan loader: обе библиотеки DXVK Native открывает динамически.

AppleClang также повторяет upstream warning `-Wnontrivial-memcall` для
`memset`/`memcpy` над `DxvkGraphicsPipelineStateInfo` и
`DxvkComputePipelineStateInfo`, а linker сообщает, что Darwin игнорирует
`-static-libstdc++`. Предупреждения не скрыты через `-w`: они относятся к
внутренней реализации pinned DXVK, не к renderer-spike source. В рамках
отклонённого backend нецелесообразно менять layout/copy semantics этих типов,
но при возможном форке их надо отдельно устранить и проверить sanitizers.

## Instance extensions

При прямой загрузке MoltenVK 1.4.1 target получил 18 extensions:

```text
VK_KHR_device_group_creation (1)
VK_KHR_external_fence_capabilities (1)
VK_KHR_external_memory_capabilities (1)
VK_KHR_external_semaphore_capabilities (1)
VK_KHR_get_physical_device_properties2 (2)
VK_KHR_get_surface_capabilities2 (1)
VK_KHR_surface (25)
VK_KHR_surface_maintenance1 (1)
VK_KHR_surface_protected_capabilities (1)
VK_EXT_debug_report (10)
VK_EXT_debug_utils (2)
VK_EXT_headless_surface (1)
VK_EXT_layer_settings (2)
VK_EXT_metal_surface (1)
VK_EXT_surface_maintenance1 (1)
VK_EXT_swapchain_colorspace (5)
VK_MVK_macos_surface (3)
VK_MVK_moltenvk (37)
```

`VK_KHR_portability_enumeration` при direct-link режиме этой сборкой не
экспортирован, поэтому instance flag не потребовался; physical device всё равно
перечислен. Target и DXVK patch проверяют extension и устанавливают flag, когда
используется loader/ICD конфигурация, где extension опубликован. Device при этом
объявляет обязательный `VK_KHR_portability_subset`.

## Device extensions

Apple M3 Max объявил 131 device extension:

```text
VK_KHR_16bit_storage, VK_KHR_8bit_storage, VK_KHR_bind_memory2,
VK_KHR_buffer_device_address, VK_KHR_calibrated_timestamps,
VK_KHR_copy_commands2, VK_KHR_create_renderpass2,
VK_KHR_dedicated_allocation, VK_KHR_deferred_host_operations,
VK_KHR_depth_stencil_resolve, VK_KHR_descriptor_update_template,
VK_KHR_device_group, VK_KHR_driver_properties, VK_KHR_dynamic_rendering,
VK_KHR_dynamic_rendering_local_read, VK_KHR_external_fence,
VK_KHR_external_memory, VK_KHR_external_semaphore,
VK_KHR_format_feature_flags2, VK_KHR_fragment_shader_barycentric,
VK_KHR_get_memory_requirements2, VK_KHR_global_priority,
VK_KHR_image_format_list, VK_KHR_imageless_framebuffer,
VK_KHR_incremental_present, VK_KHR_index_type_uint8,
VK_KHR_line_rasterization, VK_KHR_load_store_op_none,
VK_KHR_maintenance1, VK_KHR_maintenance2, VK_KHR_maintenance3,
VK_KHR_maintenance4, VK_KHR_maintenance5, VK_KHR_maintenance6,
VK_KHR_maintenance7, VK_KHR_maintenance8, VK_KHR_maintenance9,
VK_KHR_map_memory2, VK_KHR_multiview, VK_KHR_portability_subset,
VK_KHR_present_id, VK_KHR_present_id2, VK_KHR_present_wait,
VK_KHR_present_wait2, VK_KHR_push_descriptor, VK_KHR_relaxed_block_layout,
VK_KHR_robustness2, VK_KHR_sampler_mirror_clamp_to_edge,
VK_KHR_sampler_ycbcr_conversion, VK_KHR_separate_depth_stencil_layouts,
VK_KHR_shader_draw_parameters, VK_KHR_shader_expect_assume,
VK_KHR_shader_float_controls, VK_KHR_shader_float_controls2,
VK_KHR_shader_float16_int8, VK_KHR_shader_fma,
VK_KHR_shader_integer_dot_product, VK_KHR_shader_maximal_reconvergence,
VK_KHR_shader_non_semantic_info, VK_KHR_shader_quad_control,
VK_KHR_shader_relaxed_extended_instruction,
VK_KHR_shader_subgroup_extended_types, VK_KHR_shader_subgroup_rotate,
VK_KHR_shader_subgroup_uniform_control_flow,
VK_KHR_shader_terminate_invocation, VK_KHR_spirv_1_4,
VK_KHR_storage_buffer_storage_class, VK_KHR_swapchain,
VK_KHR_swapchain_maintenance1, VK_KHR_swapchain_mutable_format,
VK_KHR_synchronization2, VK_KHR_timeline_semaphore,
VK_KHR_uniform_buffer_standard_layout, VK_KHR_variable_pointers,
VK_KHR_vertex_attribute_divisor, VK_KHR_vulkan_memory_model,
VK_KHR_zero_initialize_workgroup_memory, VK_EXT_4444_formats,
VK_EXT_buffer_device_address, VK_EXT_calibrated_timestamps,
VK_EXT_debug_marker, VK_EXT_depth_clip_control, VK_EXT_descriptor_indexing,
VK_EXT_extended_dynamic_state, VK_EXT_extended_dynamic_state2,
VK_EXT_extended_dynamic_state3, VK_EXT_external_memory_host,
VK_EXT_external_memory_metal, VK_EXT_fragment_shader_interlock,
VK_EXT_global_priority, VK_EXT_global_priority_query, VK_EXT_hdr_metadata,
VK_EXT_host_image_copy, VK_EXT_host_query_reset, VK_EXT_image_2d_view_of_3d,
VK_EXT_image_robustness, VK_EXT_index_type_uint8,
VK_EXT_inline_uniform_block, VK_EXT_line_rasterization,
VK_EXT_load_store_op_none, VK_EXT_memory_budget, VK_EXT_metal_objects,
VK_EXT_pipeline_creation_cache_control, VK_EXT_pipeline_creation_feedback,
VK_EXT_pipeline_robustness, VK_EXT_post_depth_coverage, VK_EXT_private_data,
VK_EXT_robustness2, VK_EXT_sample_locations, VK_EXT_scalar_block_layout,
VK_EXT_separate_stencil_usage, VK_EXT_shader_atomic_float,
VK_EXT_shader_demote_to_helper_invocation, VK_EXT_shader_stencil_export,
VK_EXT_shader_subgroup_ballot, VK_EXT_shader_subgroup_vote,
VK_EXT_shader_viewport_index_layer, VK_EXT_subgroup_size_control,
VK_EXT_swapchain_maintenance1, VK_EXT_texel_buffer_alignment,
VK_EXT_texture_compression_astc_hdr, VK_EXT_tooling_info,
VK_EXT_vertex_attribute_divisor, VK_AMD_gpu_shader_half_float,
VK_AMD_negative_viewport_height, VK_AMD_shader_image_load_store_lod,
VK_AMD_shader_trinary_minmax, VK_GOOGLE_display_timing, VK_IMG_format_pvrtc,
VK_INTEL_shader_integer_functions2, VK_NV_fragment_shader_barycentric
```

Полный target output дополнительно печатает spec version каждого extension.

## Воспроизводимый отказ

Без изменения feature policy DXVK:

```text
Found device: Apple M3 Max (MoltenVK 0.2.2209)
  Skipping: Device does not support required feature 'geometryShader'
DXVK: No adapters found. ... A Vulkan 1.3 capable setup is required.
```

`Direct3DCreate9` выбрасывает внутренний `DxvkError`; target ловит границу и
возвращает exit code 2 вместо аварийного завершения процесса.

Краткий диагностический эксперимент последовательно ослабил только проверки и
обнаружил следующий набор:

| Требование DXVK 2.7.1 | MoltenVK result | Значение |
| --- | --- | --- |
| `geometryShader` | feature bit 0 | первый штатный blocker |
| `shaderCullDistance` | feature bit 0 | второй blocker |
| `VK_EXT_robustness2.robustBufferAccess2` | feature bit 0 | DXVK помечает обязательным для correctness |
| `VK_EXT_robustness2.nullDescriptor` | feature bit 0 | DXVK помечает обязательным для correctness |
| `VK_KHR_pipeline_library` | extension отсутствует | обязательная зависимость DXVK pipeline path |

После отключения всех пяти требований DXVK создал device и swapchain
`B8G8R8A8_UNORM`, но первый `DrawPrimitiveUP` завершился `EXC_BAD_ACCESS` по
адресу `0x58`:

```text
MVKBuffer::getMTLBuffer
MVKCmdBindVertexBuffers<2>::setContent
vkCmdBindVertexBuffers2
dxvk::DxvkContext::updateVertexBufferBindings
dxvk::D3D9DeviceEx::DrawPrimitiveUP
```

Таким образом, «просто сделать requirements optional» не является обходом:
он нарушает внутренние инварианты DXVK и даёт небезопасный runtime.

Проверенный D3D9 surface:

| D3D9 вызов | Штатный DXVK | Только в небезопасном bypass |
| --- | --- | --- |
| `Direct3DCreate9` | внутренний adapter init отклоняет MoltenVK | возвращает interface |
| `IDirect3D9::CreateDevice` | недостижим | device и swapchain создаются |
| `IDirect3DDevice9::Clear` | недостижим | command принимается асинхронно |
| `BeginScene` / `SetFVF` | недостижим | принимаются |
| `DrawPrimitiveUP` | недостижим | SIGSEGV в `vkCmdBindVertexBuffers2` path |
| `EndScene` / `Present` | недостижим | первый кадр до них стабильно не доходит |

Поэтому нельзя утверждать поддержку даже минимального D3D9 device, не говоря о
textures, render targets, depth/stencil, fixed-function emulation, shaders,
queries или fullscreen paths игры.

## Можно ли обойти ограничения

- Loader names, Darwin APIs, Apple linker, SDL3 WSI, rpath, codesign и
  portability flags исправимы небольшим поддерживаемым patch set.
- `geometryShader` нельзя включить настройкой MoltenVK. Для D3D9-only fork
  пришлось бы доказать, что каждый internal geometry-shader path либо не
  используется, либо корректно эмулируется другими Metal stages.
- Cull distance и robustness semantics требуют shader/resource rewriting, а
  не смены CMake option.
- Отсутствие pipeline library можно обойти отдельным pipeline strategy, но это
  уже fork DXVK, который надо сопровождать при каждом обновлении upstream.
- Зафиксированный SIGSEGV показывает, что даже clear/triangle не является
  стабильной базой после локального bypass.

Оценка инженерного времени для исследовательского D3D9-only fork: 6–10
человеко-недель до следующего обоснованного go/no-go, без гарантии результата.
Backend, пригодный для полной игры, shader corpus, resize/fullscreen и всех
ресурсных путей — ориентировочно 3–6 человеко-месяцев плюс постоянное
сопровождение форка. Для проекта такого масштаба это хуже прямого переноса
renderer abstraction.

## Рекомендуемая альтернатива

Следующий spike — не ещё один DXVK fork, а узкая renderer abstraction с
нативным Metal backend. Два разумных варианта:

1. **bgfx/Metal** — предпочтительный первый эксперимент. bgfx является
   engine-agnostic abstraction, официально поддерживает Metal и macOS 13+, но
   это не drop-in D3D9 ABI: ресурсы, states, draw calls и shaders надо
   адаптировать.
2. **SDL_GPU/Metal** — меньше новых platform dependencies, поскольку SDL3 уже
   принят проектом; API современный и имеет Metal/D3D12/Vulkan backends, но
   потребует более прямой переписи legacy D3D9 renderer.

Практический план: сначала портировать один material, static mesh, texture,
camera и depth pass в отдельном target; затем принять решение bgfx против
SDL_GPU по shader/tooling и объёму state translation. Windows D3D9 backend на
этом этапе можно оставить за тем же внутренним interface.

Официальные сведения: [DXVK Native WSI](https://github.com/doitsujin/dxvk),
[MoltenVK и portability](https://github.com/KhronosGroup/MoltenVK),
[MoltenVK Runtime User Guide](https://github.com/KhronosGroup/MoltenVK/blob/main/Docs/MoltenVK_Runtime_UserGuide.md),
[bgfx overview](https://bkaradzic.github.io/bgfx/overview.html),
[SDL_GPU](https://wiki.libsdl.org/SDL3/CategoryGPU).

## Сборка и повтор проверки

Build-only инструменты:

```bash
brew install meson ninja glslang sdl3 molten-vk vulkan-headers
```

DXVK собирается отдельно от проекта:

```bash
scripts/build_dxvk_macos.sh
```

Renderer spike (preset использует install prefix скрипта):

```bash
cmake --preset macos-arm64-m4
cmake --build --preset macos-arm64-m4 -j 6

DXVK_WSI_DRIVER=SDL3 DXVK_LOG_LEVEL=info DXVK_LOG_PATH=none \
  build/macos-arm64-m4/Debug/rrr3d_dxvk_moltenvk_test
```

Ожидаемый итог на проверенной машине: extension inventory, точный отказ на
`geometryShader`, exit code 2. Для Milestone 4 это ожидаемый зелёный результат
feasibility decision, а не тестовый regression pass.
