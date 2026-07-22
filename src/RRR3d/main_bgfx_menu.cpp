#include "PortableEngine.h"
#include "PortableMenu.h"
#include "renderer/BgfxGraphicsDevice.h"
#include "resource/ResourceFileSystem.h"
#include "xplatform.h"

#ifdef RRR3D_GAMEPAD_INPUT
#include "PortableInput.h"
#include "SdlInputManager.h"
#include "SdlInputSmoke.h"
#endif

#ifdef RRR3D_AUDIO
#include "SdlAudioBackend.h"
#include "SdlAudioSmoke.h"
#include "audio/AudioBackend.h"
#endif

#ifdef RRR3D_PHYSICS
#include "PortableRace.h"
#include "PortableRaceRenderer.h"
#include "physics/PhysicsBackend.h"
#endif

#include <SDL3/SDL.h>
#include <bx/math.h>

#include "rrr3d_fs_static_scene.bin.h"
#include "rrr3d_vs_static_scene.bin.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace r3d::renderer;

constexpr int initial_width = 1280;
constexpr int initial_height = 733;
constexpr float virtual_width = 1920.0F;
constexpr float virtual_height = 1100.0F;
constexpr std::string_view smoke_prefix = "--smoke-test-frames=";
constexpr std::string_view data_prefix = "--data-dir=";

struct Options
{
	std::uint32_t smokeFrames = 0;
	std::filesystem::path dataDirectory;
	bool verifyResources = false;
#ifdef RRR3D_GAMEPAD_INPUT
	bool inputSmokeTest = false;
#endif
#ifdef RRR3D_AUDIO
	bool audioSmokeTest = false;
#endif
#ifdef RRR3D_PHYSICS
	bool physicsSmokeTest = false;
#endif
};

struct TextVisual
{
	Mesh mesh;
	float width = 0.0F;
	float height = 0.0F;
};

std::optional<Options> parseOptions(int argc, char **argv)
{
	Options options;
	for (int index = 1; index < argc; ++index)
	{
		const std::string_view argument(argv[index]);
		if (argument == "--verify-resources")
		{
			options.verifyResources = true;
			continue;
		}
#ifdef RRR3D_GAMEPAD_INPUT
		if (argument == "--input-smoke-test")
		{
			options.inputSmokeTest = true;
			if (options.smokeFrames == 0)
				options.smokeFrames = 120;
			continue;
		}
#endif
#ifdef RRR3D_AUDIO
		if (argument == "--audio-smoke-test")
		{
			options.audioSmokeTest = true;
			if (options.smokeFrames == 0)
				options.smokeFrames = 180;
			continue;
		}
#endif
#ifdef RRR3D_PHYSICS
		if (argument == "--physics-smoke-test")
		{
			options.physicsSmokeTest = true;
			if (options.smokeFrames == 0)
				options.smokeFrames = 240;
			continue;
		}
#endif
		if (argument.substr(0, data_prefix.size()) == data_prefix)
		{
			const auto value = argument.substr(data_prefix.size());
			if (value.empty())
			{
				std::cerr << "--data-dir requires a path\n";
				return std::nullopt;
			}
			options.dataDirectory = std::filesystem::path(value);
			continue;
		}
		if (argument.substr(0, smoke_prefix.size()) == smoke_prefix)
		{
			const std::string_view value = argument.substr(smoke_prefix.size());
			const auto result = std::from_chars(value.data(), value.data() + value.size(), options.smokeFrames);
			if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || options.smokeFrames == 0)
			{
				std::cerr << "Invalid smoke-test frame count: " << value << '\n';
				return std::nullopt;
			}
			continue;
		}

		std::cerr << "Unknown argument: " << argument << '\n';
		return std::nullopt;
	}
	return options;
}

bool valid(Shader shader)
{
	return shader.value != invalid_resource;
}

bool valid(Texture texture)
{
	return texture.value != invalid_resource;
}

bool valid(Mesh mesh)
{
	return mesh.vertices.value != invalid_resource && mesh.indices.value != invalid_resource;
}

std::uint32_t packAbgr(r3d::portable::RgbColor color)
{
	return 0xff000000U | (static_cast<std::uint32_t>(color.blue) << 16U) |
	       (static_cast<std::uint32_t>(color.green) << 8U) | static_cast<std::uint32_t>(color.red);
}

Camera makeMenuCamera(const GraphicsDevice &device)
{
	Camera camera;
	bx::mtxIdentity(camera.view.data());
	bx::mtxOrtho(camera.projection.data(), 0.0F, virtual_width, virtual_height, 0.0F, 0.0F, 100.0F, 0.0F,
	             device.usesHomogeneousDepth());
	return camera;
}

Transform makeTransform(float scale_x, float scale_y, float x, float y, float z)
{
	Transform transform;
	bx::mtxSRT(transform.matrix.data(), scale_x, scale_y, 1.0F, 0.0F, 0.0F, 0.0F, x, y, z);
	return transform;
}

constexpr std::array<Vertex, 4> quadVertices = {{
	{-0.5F, -0.5F, 0.0F, 0xffffffffU, 0.0F, 0.0F},
	{0.5F, -0.5F, 0.0F, 0xffffffffU, 1.0F, 0.0F},
	{-0.5F, 0.5F, 0.0F, 0xffffffffU, 0.0F, 1.0F},
	{0.5F, 0.5F, 0.0F, 0xffffffffU, 1.0F, 1.0F},
}};

constexpr std::array<std::uint16_t, 6> quadIndices = {{0, 1, 2, 1, 3, 2}};

TextVisual createTextVisual(GraphicsDevice &device, const r3d::portable::BitmapFont &font, std::string_view text,
                            r3d::portable::RgbColor color)
{
	constexpr float glyph_width = 5.0F;
	constexpr float glyph_height = 7.0F;
	constexpr float glyph_advance = 6.0F;

	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;
	const std::uint32_t packed_color = packAbgr(color);
	float cursor = 0.0F;
	for (const char character : text)
	{
		if (character != ' ')
		{
			const auto &glyph = font.glyph(character);
			for (std::size_t row = 0; row < glyph.rows.size(); ++row)
			{
				for (std::size_t column = 0; column < 5; ++column)
				{
					const auto mask = static_cast<std::uint8_t>(1U << static_cast<unsigned int>(4U - column));
					if ((glyph.rows[row] & mask) == 0)
						continue;
					if (vertices.size() > static_cast<std::size_t>(UINT16_MAX) - 4U)
					{
						throw std::runtime_error("Menu text exceeds the 16-bit mesh limit");
					}

					const auto base = static_cast<std::uint16_t>(vertices.size());
					const float x = cursor + static_cast<float>(column);
					const float y = static_cast<float>(row);
					vertices.insert(vertices.end(), {
														{x, y, 0.0F, packed_color, 0.0F, 0.0F},
														{x + 1.0F, y, 0.0F, packed_color, 1.0F, 0.0F},
														{x, y + 1.0F, 0.0F, packed_color, 0.0F, 1.0F},
														{x + 1.0F, y + 1.0F, 0.0F, packed_color, 1.0F, 1.0F},
													});
					indices.insert(indices.end(), {
													  base,
													  static_cast<std::uint16_t>(base + 1U),
													  static_cast<std::uint16_t>(base + 2U),
													  static_cast<std::uint16_t>(base + 1U),
													  static_cast<std::uint16_t>(base + 3U),
													  static_cast<std::uint16_t>(base + 2U),
												  });
				}
			}
		}
		cursor += glyph_advance;
	}

	if (vertices.empty())
		throw std::runtime_error("Menu text cannot be blank");
	return {device.createMesh(vertices.data(), vertices.size(), indices.data(), indices.size()),
	        std::max(cursor - (glyph_advance - glyph_width), glyph_width), glyph_height};
}

Texture createSolidTexture(GraphicsDevice &device, r3d::portable::RgbColor color)
{
	const std::array<std::uint8_t, 4> pixel = {color.red, color.green, color.blue, 255};
	return device.createTextureRgba8(1, 1, pixel.data(), pixel.size());
}

Texture createImageTexture(GraphicsDevice &device, const r3d::portable::PortableImage &image)
{
	return device.createTextureRgba8(image.width, image.height, image.rgba.data(), image.rgba.size());
}

void drawCenteredText(GraphicsDevice &device, const TextVisual &visual, Shader shader, Texture white, float center_x,
                      float center_y, float scale, float z, const PipelineState &pipeline)
{
	const float x = center_x - visual.width * scale * 0.5F;
	const float y = center_y - visual.height * scale * 0.5F;
	device.draw(visual.mesh, shader, white, makeTransform(scale, scale, x, y, z), pipeline);
}

#ifdef RRR3D_GAMEPAD_INPUT
std::optional<std::size_t> hoveredMenuItem(SDL_Window *window, float window_x, float window_y, std::size_t item_count,
                                           float selection_width, float selection_height)
{
	int window_width = 0;
	int window_height = 0;
	if (!SDL_GetWindowSize(window, &window_width, &window_height) || window_width <= 0 || window_height <= 0)
	{
		return std::nullopt;
	}

	const float virtual_x = window_x * virtual_width / static_cast<float>(window_width);
	const float virtual_y = window_y * virtual_height / static_cast<float>(window_height);
	constexpr float center_x = virtual_width * 0.5F + 5.0F;
	constexpr float first_item_y = virtual_height * 0.5F - 100.0F;
	constexpr float item_spacing = 53.0F;
	if (std::abs(virtual_x - center_x) > selection_width * 0.5F)
		return std::nullopt;

	for (std::size_t index = 0; index < item_count; ++index)
	{
		const float center_y = first_item_y + static_cast<float>(index) * item_spacing;
		if (std::abs(virtual_y - center_y) <= selection_height * 0.5F)
			return index;
	}
	return std::nullopt;
}
#endif

} // namespace

int main(int argc, char **argv)
{
	const auto options = parseOptions(argc, argv);
	if (!options)
		return EXIT_FAILURE;

	std::string directory_error;
	if (!rrr3d::platform::ensure_application_directories(directory_error))
	{
		std::cerr << "Application-directory setup failed: " << directory_error << '\n';
		return EXIT_FAILURE;
	}

	const auto data_directory =
		options->dataDirectory.empty() ? r3d::resource::defaultGameDataDirectory() : options->dataDirectory;

	std::optional<r3d::portable::PortableMenuData> menu_data;
	std::optional<r3d::resource::ResourceFileSystem> resources;
#ifdef RRR3D_PHYSICS
	std::optional<rrr3d::race::PortableRaceData> race_data;
#endif
	try
	{
		resources.emplace(data_directory);
		menu_data.emplace(r3d::portable::loadPortableMenu(*resources));
#ifdef RRR3D_PHYSICS
		race_data.emplace(rrr3d::race::loadPortableRace(*resources));
#endif
	}
	catch (const std::exception &exception)
	{
		std::cerr << "Resource validation failed: " << exception.what() << '\n';
		return EXIT_FAILURE;
	}

	std::cout << "Game data: " << resources->root() << '\n'
			  << "Save data: " << rrr3d::platform::save_directory() << '\n'
			  << "Logs: " << rrr3d::platform::log_directory() << '\n'
			  << "Resource validation: " << menu_data->items.size() << " menu items, " << menu_data->background.width
			  << 'x' << menu_data->background.height << " background, " << menu_data->legacy_asset_count
			  << " original assets (" << menu_data->legacy_asset_bytes << " bytes)\n";
	if (options->verifyResources)
	{
#ifdef RRR3D_PHYSICS
		std::cout << "Milestone 9 resource verification completed: " << race_data->sourceTrackPieces
		          << " map track pieces, " << race_data->sourceCollisionMeshes << " source collision meshes\n";
#elif defined(RRR3D_AUDIO)
		std::cout << "Milestone 8 resource verification completed\n";
#elif defined(RRR3D_GAMEPAD_INPUT)
		std::cout << "Milestone 7 resource verification completed\n";
#else
		std::cout << "Milestone 6 resource verification completed\n";
#endif
		return EXIT_SUCCESS;
	}

#ifdef RRR3D_PHYSICS
	rrr3d::race::RaceSmokeResult race_smoke_result;
	if (options->physicsSmokeTest)
	{
		std::string physics_error;
		if (!rrr3d::race::runPortableRaceSmokeTest(*race_data, race_smoke_result, physics_error))
		{
			std::cerr << "Milestone 9 physics smoke test failed: " << physics_error << '\n';
			return EXIT_FAILURE;
		}
		std::cout << "Milestone 9 physics smoke: acceleration, braking, steering, wall collision, ramp jump/landing, "
		             "reset, deterministic replay, checkpoints, and one-lap finish passed; finish "
		          << race_smoke_result.finishSeconds << " s, max " << race_smoke_result.maximumSpeed * 3.6F
		          << " km/h, collisions " << race_smoke_result.collisions << '\n';
	}
#endif

	if (!SDL_SetAppMetadata("Motor Rock", "1.3.1", "org.rrr3d.motorrock"))
	{
		std::cerr << "Unable to set SDL metadata: " << SDL_GetError() << '\n';
		return EXIT_FAILURE;
	}
	SDL_InitFlags sdl_subsystems = SDL_INIT_VIDEO;
#ifdef RRR3D_GAMEPAD_INPUT
	sdl_subsystems |= SDL_INIT_GAMEPAD;
#endif
#ifdef RRR3D_AUDIO
	sdl_subsystems |= SDL_INIT_AUDIO;
#endif
	if (!SDL_Init(sdl_subsystems))
	{
		std::cerr << "Unable to initialize SDL3: " << SDL_GetError() << '\n';
		return EXIT_FAILURE;
	}

	SDL_Window *window = SDL_CreateWindow(
#ifdef RRR3D_PHYSICS
		"Motor Rock - Physics Race (Milestone 9)", initial_width, initial_height,
#elif defined(RRR3D_AUDIO)
		"Motor Rock - Main Menu (Milestone 8)", initial_width, initial_height,
#elif defined(RRR3D_GAMEPAD_INPUT)
		"Motor Rock - Main Menu (Milestone 7)", initial_width, initial_height,
#else
		"Motor Rock - Main Menu (Milestone 6)", initial_width, initial_height,
#endif
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	if (window == nullptr)
	{
		std::cerr << "Unable to create window: " << SDL_GetError() << '\n';
		SDL_Quit();
		return EXIT_FAILURE;
	}

	const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
	void *native_window = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
	int pixel_width = 0;
	int pixel_height = 0;
	if (native_window == nullptr || !SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height))
	{
		std::cerr << "Unable to obtain Cocoa window/drawable size: " << SDL_GetError() << '\n';
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	auto device = createBgfxGraphicsDevice();
	std::string renderer_error;
	if (!device->initialize(
			{{native_window}, static_cast<std::uint32_t>(pixel_width), static_cast<std::uint32_t>(pixel_height), true},
			renderer_error))
	{
		std::cerr << "Renderer initialization failed: " << renderer_error << '\n';
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

#ifdef RRR3D_GAMEPAD_INPUT
	rrr3d::input::SdlInputManager input;
	std::string input_error;
	if (!input.initialize(input_error))
	{
		std::cerr << "Input initialization failed: " << input_error << '\n';
		device.reset();
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}
	std::cout << "Input: SDL3 keyboard/mouse/gamepad, " << input.connectedGamepadCount()
			  << " gamepad(s), stick dead zone " << rrr3d::input::SdlInputManager::stickDeadZone << '\n';
	if (options->inputSmokeTest && !rrr3d::input::runSdlInputSmokeTest(input, input_error))
	{
		std::cerr << "Milestone 7 input smoke test failed: " << input_error << '\n';
		input.shutdown();
		device.reset();
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}
	if (options->inputSmokeTest)
	{
		std::cout << "Milestone 7 input smoke: keyboard, mouse, focus reset, "
					 "gamepad hot-plug, dead zones, analog axes, buttons, "
					 "triggers, and rumble passed\n";
	}
#endif

#ifdef RRR3D_AUDIO
	rrr3d::audio::SdlAudioBackend audio;
	std::string audio_error;
	if (!audio.initialize(audio_error))
	{
		std::cerr << "Audio initialization failed: " << audio_error << '\n';
#ifdef RRR3D_GAMEPAD_INPUT
		input.shutdown();
#endif
		device.reset();
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}
	std::cout << "Audio: SDL3/" << audio.driverName() << ", 48 kHz stereo float mixer, default output '"
			  << audio.outputDeviceName() << "'\n";
	if (options->audioSmokeTest && !rrr3d::audio::runSdlAudioSmokeTest(audio, *resources, audio_error))
	{
		std::cerr << "Milestone 8 audio smoke test failed: " << audio_error << '\n';
		audio.shutdown();
#ifdef RRR3D_GAMEPAD_INPUT
		input.shutdown();
#endif
		device.reset();
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}
	if (options->audioSmokeTest)
	{
		std::cout << "Milestone 8 audio smoke: Ogg music/UI/gameplay decode, mixing, volumes, pause/resume, loop, "
					 "device events, and resource release passed\n";
	}

	r3d::audio::SoundInfo music_info;
	r3d::audio::SoundInfo navigation_info;
	r3d::audio::SoundInfo confirmation_info;
	r3d::audio::SoundInfo gameplay_info;
#ifdef RRR3D_PHYSICS
	r3d::audio::SoundInfo engine_info;
	r3d::audio::SoundInfo crash_info;
#endif
	auto load_audio_resource = [&](std::string_view virtual_path, r3d::audio::SoundInfo &info) {
		try
		{
			return audio.loadOgg(resources->resolve(virtual_path), info, audio_error);
		}
		catch (const std::exception &exception)
		{
			audio_error = exception.what();
			return r3d::audio::invalidSound;
		}
	};
	const auto music_sound = load_audio_resource("Data/Music/Track1.ogg", music_info);
	const auto navigation_sound = music_sound == r3d::audio::invalidSound
	                                  ? r3d::audio::invalidSound
	                                  : load_audio_resource("Data/Sounds/UI/navedenie.ogg", navigation_info);
	const auto confirmation_sound = navigation_sound == r3d::audio::invalidSound
	                                    ? r3d::audio::invalidSound
	                                    : load_audio_resource("Data/Sounds/UI/acception.ogg", confirmation_info);
	const auto gameplay_sound = confirmation_sound == r3d::audio::invalidSound
	                                ? r3d::audio::invalidSound
	                                : load_audio_resource("Data/Sounds/fireGun.ogg", gameplay_info);
#ifdef RRR3D_PHYSICS
	const auto engine_sound = gameplay_sound == r3d::audio::invalidSound
	                              ? r3d::audio::invalidSound
	                              : load_audio_resource("Data/Sounds/engine_player_heavy_mot.ogg", engine_info);
	const auto crash_sound = engine_sound == r3d::audio::invalidSound
	                             ? r3d::audio::invalidSound
	                             : load_audio_resource("Data/Sounds/carcrash05.ogg", crash_info);
#endif
	if (music_sound == r3d::audio::invalidSound || navigation_sound == r3d::audio::invalidSound ||
	    confirmation_sound == r3d::audio::invalidSound || gameplay_sound == r3d::audio::invalidSound
#ifdef RRR3D_PHYSICS
	    || engine_sound == r3d::audio::invalidSound || crash_sound == r3d::audio::invalidSound
#endif
	)
	{
		std::cerr << "Menu audio loading failed: " << audio_error << '\n';
		audio.shutdown();
#ifdef RRR3D_GAMEPAD_INPUT
		input.shutdown();
#endif
		device.reset();
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	audio.setMasterVolume(0.85F);
	audio.setBusVolume(r3d::audio::Bus::Music, 0.45F);
	audio.setBusVolume(r3d::audio::Bus::Effects, 0.85F);
	r3d::audio::PlayOptions music_options;
	music_options.bus = r3d::audio::Bus::Music;
	music_options.loop = true;
	auto music_voice = audio.play(music_sound, music_options, audio_error);
	if (music_voice == r3d::audio::invalidVoice)
	{
		std::cerr << "Menu music start failed: " << audio_error << '\n';
		audio.shutdown();
#ifdef RRR3D_GAMEPAD_INPUT
		input.shutdown();
#endif
		device.reset();
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}
	std::cout << "Menu music: Data/Music/Track1.ogg, " << music_info.durationSeconds
	          << " s, looping; effects: UI navigation/confirm and gameplay fireGun"
#ifdef RRR3D_PHYSICS
	          << "; race engine/crash audio"
#endif
	          << '\n';

	auto play_effect = [&](r3d::audio::SoundHandle sound, float volume) {
		r3d::audio::PlayOptions effect_options;
		effect_options.bus = r3d::audio::Bus::Effects;
		effect_options.volume = volume;
		std::string effect_error;
		if (audio.play(sound, effect_options, effect_error) == r3d::audio::invalidVoice)
			SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Unable to play effect: %s", effect_error.c_str());
	};
	auto release_audio_resources = [&]() {
		audio.stopAll();
#ifdef RRR3D_PHYSICS
		audio.unloadSound(crash_sound);
		audio.unloadSound(engine_sound);
#endif
		audio.unloadSound(gameplay_sound);
		audio.unloadSound(confirmation_sound);
		audio.unloadSound(navigation_sound);
		audio.unloadSound(music_sound);
		audio.shutdown();
	};
#endif

	Shader shader = device->createShader({rrr3d_vs_static_scene, sizeof(rrr3d_vs_static_scene)},
	                                     {rrr3d_fs_static_scene, sizeof(rrr3d_fs_static_scene)}, "portable menu");
	Mesh quad = device->createMesh(quadVertices.data(), quadVertices.size(), quadIndices.data(), quadIndices.size());
	Texture background = createImageTexture(*device, menu_data->background);
	Texture top_panel = createImageTexture(*device, menu_data->top_panel);
	Texture bottom_panel = createImageTexture(*device, menu_data->bottom_panel);
	Texture selection = createImageTexture(*device, menu_data->selection);
	Texture white = createSolidTexture(*device, {255, 255, 255});

	std::vector<TextVisual> text_visuals;
	TextVisual version;
	std::vector<TextVisual> item_visuals;
	std::vector<TextVisual> selected_item_visuals;
	try
	{
		version = createTextVisual(*device, menu_data->font, menu_data->version, menu_data->text_color);
		item_visuals.reserve(menu_data->items.size());
		selected_item_visuals.reserve(menu_data->items.size());
		for (const auto &item : menu_data->items)
		{
			item_visuals.push_back(createTextVisual(*device, menu_data->font, item, menu_data->text_color));
			selected_item_visuals.push_back(
				createTextVisual(*device, menu_data->font, item, menu_data->selected_text_color));
		}
	}
	catch (const std::exception &exception)
	{
		std::cerr << "Menu mesh creation failed: " << exception.what() << '\n';
	}

	text_visuals.push_back(version);
	text_visuals.insert(text_visuals.end(), item_visuals.begin(), item_visuals.end());
	text_visuals.insert(text_visuals.end(), selected_item_visuals.begin(), selected_item_visuals.end());

#ifdef RRR3D_PHYSICS
	rrr3d::race::PortableRaceSession race(*race_data);
	rrr3d::race::PortableRaceRenderer race_renderer;
	std::string race_renderer_error;
	const bool race_resources_valid = race_renderer.initialize(*device, race, race_renderer_error);
	if (!race_resources_valid)
		std::cerr << "Race renderer initialization failed: " << race_renderer_error << '\n';
#endif

	const bool resources_valid = valid(shader) && valid(quad) && valid(background) && valid(white) &&
	                             valid(top_panel) && valid(bottom_panel) && valid(selection) && valid(version.mesh) &&
	                             item_visuals.size() == menu_data->items.size() &&
	                             selected_item_visuals.size() == menu_data->items.size() &&
	                             std::all_of(item_visuals.begin(), item_visuals.end(),
	                                         [](const TextVisual &visual) { return valid(visual.mesh); }) &&
	                             std::all_of(selected_item_visuals.begin(), selected_item_visuals.end(),
	                                         [](const TextVisual &visual) { return valid(visual.mesh); })
#ifdef RRR3D_PHYSICS
	                             && race_resources_valid
#endif
		;

	auto release_gpu_resources = [&]() {
#ifdef RRR3D_PHYSICS
		race_renderer.shutdown(*device);
#endif
		for (const auto &visual : text_visuals)
			device->destroy(visual.mesh);
		device->destroy(white);
		device->destroy(selection);
		device->destroy(bottom_panel);
		device->destroy(top_panel);
		device->destroy(background);
		device->destroy(quad);
		device->destroy(shader);
	};

	if (!resources_valid)
	{
		std::cerr << "Unable to create portable menu/race resources\n";
		release_gpu_resources();
#ifdef RRR3D_AUDIO
		release_audio_resources();
#endif
#ifdef RRR3D_GAMEPAD_INPUT
		input.shutdown();
#endif
		device.reset();
		SDL_DestroyWindow(window);
		SDL_Quit();
		return EXIT_FAILURE;
	}

	std::cout << "RRR3D renderer backend: bgfx/" << device->backendName() << '\n'
	          << "Main menu: original DDS/PNG artwork, portable font, "
	             "selection state\n";
#ifdef RRR3D_PHYSICS
	std::cout << "Physics: portable deterministic vehicle backend; map " << race_data->mapVirtualPath << ", "
	          << race_data->sourceTrackPieces << " original track placements, " << race_data->sourceCollisionMeshes
	          << " source pxTrack meshes audited; controls WASD/gamepad, P pause, Tab reset, Esc menu\n";
#endif
	r3d::portable::log_engine_capabilities();

	if (options->smokeFrames != 0 && !SDL_SetWindowSize(window, 1024, 587))
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "menu smoke-test resize failed: %s", SDL_GetError());
	}

	PipelineState pipeline;
	pipeline.faceCulling = PipelineState::FaceCulling::None;
	PipelineState alpha_pipeline = pipeline;
	alpha_pipeline.alphaBlend = true;
	const Camera menu_camera = makeMenuCamera(*device);
	bool running = true;
	std::uint32_t rendered_frames = 0;
	std::size_t selected_item = 0;
#ifdef RRR3D_AUDIO
	bool audio_paused = false;
#endif
#ifdef RRR3D_PHYSICS
	enum class Scene
	{
		Menu,
		Race
	};
	Scene scene = Scene::Menu;
	float race_throttle = 0.0F;
	float race_brake = 0.0F;
	float race_left = 0.0F;
	float race_right = 0.0F;
	bool race_paused = false;
	bool finish_announced = false;
	std::uint32_t observed_collisions = 0;
	std::uint64_t previous_ticks = SDL_GetTicksNS();
	double physics_accumulator = 0.0;
#ifdef RRR3D_AUDIO
	r3d::audio::VoiceHandle engine_voice = r3d::audio::invalidVoice;
#endif

	auto start_race = [&]() {
		race.reset();
		scene = Scene::Race;
		race_throttle = 0.0F;
		race_brake = 0.0F;
		race_left = 0.0F;
		race_right = 0.0F;
		race_paused = false;
		finish_announced = false;
		observed_collisions = 0;
		physics_accumulator = 0.0;
		previous_ticks = SDL_GetTicksNS();
#ifdef RRR3D_AUDIO
		audio_paused = false;
		audio.setPaused(false);
		if (music_voice != r3d::audio::invalidVoice)
			audio.stop(music_voice);
		music_voice = r3d::audio::invalidVoice;
		r3d::audio::PlayOptions engine_options;
		engine_options.bus = r3d::audio::Bus::Effects;
		engine_options.volume = 0.42F;
		engine_options.loop = true;
		std::string effect_error;
		engine_voice = audio.play(engine_sound, engine_options, effect_error);
		if (engine_voice == r3d::audio::invalidVoice)
			SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Unable to start race engine: %s", effect_error.c_str());
#endif
		std::cout << "Race started: one lap on Data/Map/debugTrack.r3dMap\n";
	};

	auto return_to_menu = [&]() {
		scene = Scene::Menu;
		race_paused = false;
		race_throttle = race_brake = race_left = race_right = 0.0F;
#ifdef RRR3D_AUDIO
		audio_paused = false;
		audio.setPaused(false);
		if (engine_voice != r3d::audio::invalidVoice)
			audio.stop(engine_voice);
		engine_voice = r3d::audio::invalidVoice;
		if (music_voice == r3d::audio::invalidVoice)
		{
			std::string music_error;
			music_voice = audio.play(music_sound, music_options, music_error);
			if (music_voice == r3d::audio::invalidVoice)
				SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Unable to restart menu music: %s", music_error.c_str());
		}
#endif
		SDL_SetWindowTitle(window, "Motor Rock - Physics Race (Milestone 9)");
	};

	auto activate_menu_item = [&]() {
		if (selected_item == 0)
		{
			start_race();
		}
		else if (selected_item + 1 == menu_data->items.size())
		{
			running = false;
		}
		else
		{
			std::cout << "Menu action is not available in Milestone 9: " << menu_data->items[selected_item] << '\n';
		}
	};

	if (options->physicsSmokeTest)
		start_race();
#endif

	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
#ifdef RRR3D_AUDIO
			if ((event.type == SDL_EVENT_AUDIO_DEVICE_ADDED || event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED ||
			     event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) &&
			    !event.adevice.recording)
			{
				if (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED)
					audio.notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent::Added, event.adevice.which);
				else if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED)
					audio.notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent::Removed, event.adevice.which);
				else if (event.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED)
					audio.notifyPlaybackDeviceEvent(r3d::audio::PlaybackDeviceEvent::FormatChanged,
					                                event.adevice.which);
			}
#endif
#ifdef RRR3D_GAMEPAD_INPUT
			const bool menu_active =
#ifdef RRR3D_PHYSICS
				scene == Scene::Menu;
#else
				true;
#endif
			if (menu_active && event.type == SDL_EVENT_MOUSE_MOTION)
			{
				const auto hovered = hoveredMenuItem(window, event.motion.x, event.motion.y, menu_data->items.size(),
				                                     static_cast<float>(menu_data->selection.width),
				                                     static_cast<float>(menu_data->selection.height));
				if (hovered)
				{
#ifdef RRR3D_AUDIO
					if (*hovered != selected_item)
						play_effect(navigation_sound, 0.55F);
#endif
					selected_item = *hovered;
				}
			}
			else if (menu_active && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
			{
				const auto hovered = hoveredMenuItem(window, event.button.x, event.button.y, menu_data->items.size(),
				                                     static_cast<float>(menu_data->selection.width),
				                                     static_cast<float>(menu_data->selection.height));
				if (hovered)
					selected_item = *hovered;
			}
			const auto action_events = input.processEvent(event);
#endif
			if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
			{
				running = false;
			}
			else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				pixel_width = std::max(event.window.data1, 1);
				pixel_height = std::max(event.window.data2, 1);
				device->resize(static_cast<std::uint32_t>(pixel_width), static_cast<std::uint32_t>(pixel_height));
				SDL_Log("Menu drawable resized to %dx%d pixels", pixel_width, pixel_height);
			}
#ifndef RRR3D_GAMEPAD_INPUT
			else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
			{
				if (event.key.key == SDLK_ESCAPE)
				{
					running = false;
				}
				else if (event.key.key == SDLK_UP)
				{
					selected_item = selected_item == 0 ? menu_data->items.size() - 1 : selected_item - 1;
				}
				else if (event.key.key == SDLK_DOWN)
				{
					selected_item = (selected_item + 1) % menu_data->items.size();
				}
				else if (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER)
				{
					if (selected_item + 1 == menu_data->items.size())
					{
						running = false;
					}
					else
					{
						std::cout << "Menu action is not available before a "
									 "later milestone: "
								  << menu_data->items[selected_item] << '\n';
					}
				}
			}
#endif

#ifdef RRR3D_GAMEPAD_INPUT
			#ifdef RRR3D_PHYSICS
			if (scene == Scene::Race)
			{
				const bool back_requested = std::any_of(action_events.begin(), action_events.end(), [](const auto &action) {
					return action.action == rrr3d::input::Action::MenuBack && action.active && !action.repeated;
				});
				if (back_requested)
				{
					return_to_menu();
					continue;
				}
				for (const auto &action_event : action_events)
				{
					const float value = action_event.active ? std::clamp(action_event.value, 0.0F, 1.0F) : 0.0F;
					if (action_event.action == rrr3d::input::Action::Accelerate)
						race_throttle = value;
					else if (action_event.action == rrr3d::input::Action::Brake)
						race_brake = value;
					else if (action_event.action == rrr3d::input::Action::TurnLeft)
						race_left = value;
					else if (action_event.action == rrr3d::input::Action::TurnRight)
						race_right = value;
					else if (action_event.active && !action_event.repeated &&
					         action_event.action == rrr3d::input::Action::Pause)
					{
						race_paused = !race_paused;
#ifdef RRR3D_AUDIO
						audio_paused = race_paused;
						audio.setPaused(audio_paused);
#endif
					}
					else if (action_event.active && !action_event.repeated &&
					         action_event.action == rrr3d::input::Action::ChangeWeapon)
					{
						race.reset();
						observed_collisions = 0;
						finish_announced = false;
					}
					else if (action_event.active && !action_event.repeated &&
					         action_event.action == rrr3d::input::Action::UseWeapon)
					{
#ifdef RRR3D_AUDIO
						play_effect(gameplay_sound, 0.7F);
#endif
					}
					else if (action_event.active && !action_event.repeated && race.vehicle().finished &&
					         action_event.action == rrr3d::input::Action::MenuConfirm)
					{
						return_to_menu();
					}
				}
				continue;
			}
			#endif
			for (const auto &action_event : action_events)
			{
				if (!action_event.active)
					continue;
				if (action_event.repeated && action_event.action != rrr3d::input::Action::MenuUp &&
				    action_event.action != rrr3d::input::Action::MenuDown)
				{
					continue;
				}

				if (action_event.action == rrr3d::input::Action::MenuUp)
				{
					selected_item = selected_item == 0 ? menu_data->items.size() - 1 : selected_item - 1;
#ifdef RRR3D_AUDIO
					play_effect(navigation_sound, 0.55F);
#endif
				}
				else if (action_event.action == rrr3d::input::Action::MenuDown)
				{
					selected_item = (selected_item + 1) % menu_data->items.size();
#ifdef RRR3D_AUDIO
					play_effect(navigation_sound, 0.55F);
#endif
				}
				else if (action_event.action == rrr3d::input::Action::MenuConfirm)
				{
#ifdef RRR3D_AUDIO
					play_effect(confirmation_sound, 0.7F);
#endif
#ifdef RRR3D_PHYSICS
					activate_menu_item();
#else
					if (selected_item + 1 == menu_data->items.size())
						running = false;
					else
						std::cout << "Menu action is not available before a later milestone: "
						          << menu_data->items[selected_item] << '\n';
#endif
				}
				else if (action_event.action == rrr3d::input::Action::MenuBack)
				{
					running = false;
				}
#ifdef RRR3D_AUDIO
				else if (action_event.action == rrr3d::input::Action::UseWeapon)
				{
					play_effect(gameplay_sound, 0.7F);
				}
				else if (action_event.action == rrr3d::input::Action::ChangeWeapon)
				{
					play_effect(navigation_sound, 0.55F);
				}
				else if (action_event.action == rrr3d::input::Action::Pause)
				{
					audio_paused = !audio_paused;
					audio.setPaused(audio_paused);
				}
#endif
			}
#endif
		}

		#ifdef RRR3D_PHYSICS
		if (scene == Scene::Race)
		{
			const std::uint64_t current_ticks = SDL_GetTicksNS();
			const double frame_seconds = options->physicsSmokeTest
			                                 ? 1.0 / 60.0
			                                 : std::clamp(static_cast<double>(current_ticks - previous_ticks) / 1.0e9,
			                                              0.0, 0.1);
			previous_ticks = current_ticks;
			if (!race_paused)
			{
				physics_accumulator += frame_seconds;
				constexpr double fixed_step = 1.0 / 120.0;
				while (physics_accumulator >= fixed_step)
				{
					r3d::physics::VehicleInput race_input;
					if (options->physicsSmokeTest)
						race_input = race.autopilotInput();
					else
					{
						race_input.throttle = race_throttle;
						race_input.brake = race_brake;
						race_input.steering = race_right - race_left;
					}
					race.step(static_cast<float>(fixed_step), race_input);
					physics_accumulator -= fixed_step;
				}
			}

			if (race.vehicle().collisionCount != observed_collisions)
			{
				observed_collisions = race.vehicle().collisionCount;
#ifdef RRR3D_AUDIO
				play_effect(crash_sound, 0.78F);
#endif
			}
			if (race.vehicle().finished && !finish_announced)
			{
				finish_announced = true;
#ifdef RRR3D_AUDIO
				if (engine_voice != r3d::audio::invalidVoice)
					audio.stop(engine_voice);
				engine_voice = r3d::audio::invalidVoice;
				play_effect(confirmation_sound, 0.9F);
#endif
				std::cout << "Race finished in " << race.vehicle().elapsedSeconds << " seconds with "
				          << race.vehicle().collisionCount << " collision(s)\n";
			}
			if (rendered_frames % 12U == 0U)
			{
				const int speed_kmh = static_cast<int>(std::lround(race.vehicle().speed * 3.6F));
				const int progress = static_cast<int>(std::lround(race.vehicle().lapProgress * 100.0F));
				const std::string title = race.vehicle().finished
				                              ? "Motor Rock - FINISH - Enter: menu"
				                              : "Motor Rock - Lap 1/1 - " + std::to_string(speed_kmh) + " km/h - " +
				                                    std::to_string(progress) + "% - CP " +
				                                    std::to_string(static_cast<unsigned int>(race.vehicle().checkpointsPassed)) +
				                                    "/3" + (race_paused ? " - PAUSED" : "");
				SDL_SetWindowTitle(window, title.c_str());
			}

			const auto race_camera = race_renderer.makeCamera(*device, race, static_cast<std::uint32_t>(pixel_width),
			                                                  static_cast<std::uint32_t>(pixel_height));
			device->beginFrame(race_camera, 0x6b8ca5ffU);
			race_renderer.draw(*device, shader, race, pipeline);
		}
		else
		#endif
		{
			device->beginFrame(menu_camera, 0x040818ffU);
			device->draw(quad, shader, background,
			             makeTransform(virtual_width, virtual_height, virtual_width * 0.5F, virtual_height * 0.5F, 90.0F),
			             pipeline);
			device->draw(quad, shader, top_panel,
			             makeTransform(static_cast<float>(menu_data->top_panel.width),
			                           static_cast<float>(menu_data->top_panel.height), virtual_width * 0.5F, 200.0F,
			                           70.0F),
			             alpha_pipeline);

			constexpr float first_item_y = virtual_height * 0.5F - 100.0F;
			constexpr float item_spacing = 53.0F;
			for (std::size_t index = 0; index < item_visuals.size(); ++index)
			{
				const float item_y = first_item_y + static_cast<float>(index) * item_spacing;
				if (index == selected_item)
				{
					device->draw(quad, shader, selection,
					             makeTransform(static_cast<float>(menu_data->selection.width),
					                           static_cast<float>(menu_data->selection.height), virtual_width * 0.5F + 5.0F,
					                           item_y, 50.0F),
					             alpha_pipeline);
				}
				const auto &text = index == selected_item ? selected_item_visuals[index] : item_visuals[index];
				drawCenteredText(*device, text, shader, white, virtual_width * 0.5F + 5.0F, item_y, 4.5F, 25.0F,
				                 pipeline);
			}
			constexpr float version_scale = 3.0F;
			const float version_center_x = virtual_width - 25.0F - version.width * version_scale * 0.5F;
			const float version_center_y = virtual_height - 25.0F - version.height * version_scale * 0.5F;
			drawCenteredText(*device, version, shader, white, version_center_x, version_center_y, version_scale, 25.0F,
			                 pipeline);
		}
		device->endFrame();
		++rendered_frames;

		if (options->smokeFrames != 0 && rendered_frames >= options->smokeFrames)
		{
#ifdef RRR3D_PHYSICS
			std::cout << (options->physicsSmokeTest ? "Milestone 9 physics/race/render smoke test completed after "
			                                         : "Milestone 9 menu smoke test completed after ")
			          << rendered_frames << " frames\n";
#elif defined(RRR3D_AUDIO)
			std::cout << "Milestone 8 audio/input/menu smoke test completed after " << rendered_frames << " frames\n";
#elif defined(RRR3D_GAMEPAD_INPUT)
			std::cout << "Milestone 7 input/menu smoke test completed after " << rendered_frames << " frames\n";
#else
			std::cout << "Milestone 6 menu smoke test completed after " << rendered_frames << " frames\n";
#endif
			running = false;
		}
	}

	release_gpu_resources();
#ifdef RRR3D_AUDIO
	release_audio_resources();
#endif
#ifdef RRR3D_GAMEPAD_INPUT
	input.shutdown();
#endif
	device.reset();
	SDL_DestroyWindow(window);
	SDL_Quit();
	return EXIT_SUCCESS;
}
