#include "SdlInputAdapter.h"

#ifdef RRR3D_GAME_CORE_LINKED
#include "PortableGame.h"
#endif

#include <SDL3/SDL.h>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

namespace
{

constexpr int initial_width = 1280;
constexpr int initial_height = 720;
constexpr std::string_view smoke_test_prefix = "--smoke-test-ms=";

struct Options
{
    bool fullscreen = false;
    std::uint64_t smoke_test_milliseconds = 0;
};

std::optional<Options> parse_options(int argc, char** argv)
{
    Options options;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--fullscreen")
        {
            options.fullscreen = true;
            continue;
        }

        if (argument.substr(0, smoke_test_prefix.size()) == smoke_test_prefix)
        {
            const std::string_view value = argument.substr(smoke_test_prefix.size());
            const auto result = std::from_chars(
                value.data(), value.data() + value.size(),
                options.smoke_test_milliseconds);
            if (result.ec != std::errc() || result.ptr != value.data() + value.size() ||
                options.smoke_test_milliseconds == 0)
            {
                std::cerr << "Invalid smoke-test timeout: " << value << '\n';
                return std::nullopt;
            }
            continue;
        }

        std::cerr << "Unknown argument: " << argument << '\n';
        return std::nullopt;
    }

    return options;
}

bool set_fullscreen(SDL_Window* window, bool fullscreen)
{
    if (!SDL_SetWindowFullscreen(window, fullscreen))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Unable to change fullscreen mode: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Fullscreen %s", fullscreen ? "enabled" : "disabled");
    return true;
}

void push_smoke_event(SDL_Event& event)
{
    if (!SDL_PushEvent(&event))
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Unable to queue smoke-test event: %s",
                    SDL_GetError());
}

void queue_smoke_input_events(SDL_Window* window)
{
    const SDL_WindowID window_id = SDL_GetWindowID(window);

    SDL_Event event{};
    event.key.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = window_id;
    event.key.scancode = SDL_SCANCODE_A;
    event.key.key = SDLK_A;
    event.key.down = true;
    push_smoke_event(event);

    event = {};
    event.key.type = SDL_EVENT_KEY_UP;
    event.key.windowID = window_id;
    event.key.scancode = SDL_SCANCODE_A;
    event.key.key = SDLK_A;
    event.key.down = false;
    push_smoke_event(event);

    event = {};
    event.text.type = SDL_EVENT_TEXT_INPUT;
    event.text.windowID = window_id;
    event.text.text = "A";
    push_smoke_event(event);

    event = {};
    event.motion.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = window_id;
    event.motion.x = 128.0F;
    event.motion.y = 72.0F;
    event.motion.xrel = 4.0F;
    event.motion.yrel = -2.0F;
    push_smoke_event(event);

    event = {};
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = window_id;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    event.button.x = 128.0F;
    event.button.y = 72.0F;
    push_smoke_event(event);

    event.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.down = false;
    push_smoke_event(event);

    event = {};
    event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.windowID = window_id;
    event.wheel.y = 1.0F;
    push_smoke_event(event);

    event = {};
    event.window.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    event.window.windowID = window_id;
    push_smoke_event(event);

    event.window.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    push_smoke_event(event);
}

bool process_event(const SDL_Event& event, SDL_Window* window,
                   bool& fullscreen, bool& focused)
{
    switch (event.type)
    {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        return false;

    case SDL_EVENT_WINDOW_RESIZED:
        SDL_Log("Window resized to %dx%d", event.window.data1, event.window.data2);
        break;

    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        focused = true;
        SDL_Log("Window focus gained");
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        focused = false;
        SDL_Log("Window focus lost");
        break;

    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    {
        const auto input = rrr3d::sdl::translate_key_input(event.key);
        SDL_Log("Key %s: %s%s", SDL_GetKeyName(input.keycode),
                input.state == lsl::ksDown ? "down" : "up",
                input.repeat ? " (repeat)" : "");

        if (input.state == lsl::ksDown && !input.repeat &&
            input.keycode == SDLK_F11)
        {
            const bool requested = !fullscreen;
            if (set_fullscreen(window, requested))
                fullscreen = requested;
        }
        else if (input.state == lsl::ksDown && input.keycode == SDLK_ESCAPE)
        {
            return false;
        }
        break;
    }

    case SDL_EVENT_TEXT_INPUT:
        SDL_Log("Text input (UTF-8): %s", event.text.text);
        break;

    case SDL_EVENT_MOUSE_MOTION:
    {
        const auto input = rrr3d::sdl::translate_mouse_motion_input(event.motion);
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "Mouse motion: %d,%d delta %d,%d",
                     input.position.x, input.position.y,
                     input.relative.x, input.relative.y);
        break;
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        const auto input = rrr3d::sdl::translate_mouse_button_input(event.button);
        if (input)
        {
            SDL_Log("Mouse button %u: %s at %d,%d", event.button.button,
                    input->state == lsl::ksDown ? "down" : "up",
                    input->position.x, input->position.y);
        }
        break;
    }

    case SDL_EVENT_MOUSE_WHEEL:
        SDL_Log("Mouse wheel: %.2f,%.2f", event.wheel.x, event.wheel.y);
        break;

    default:
        break;
    }

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    const auto options = parse_options(argc, argv);
    if (!options)
        return EXIT_FAILURE;

    if (!SDL_SetAppMetadata("Motor Rock", "1.3.1", "org.rrr3d.motorrock"))
    {
        std::cerr << "Unable to set SDL application metadata: "
                  << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::cerr << "Unable to initialize SDL3 video: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    if (options->smoke_test_milliseconds != 0)
        SDL_SetLogPriority(SDL_LOG_CATEGORY_INPUT, SDL_LOG_PRIORITY_DEBUG);

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (options->fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;

    SDL_Window* window = SDL_CreateWindow(
        "Motor Rock", initial_width, initial_height, flags);
    if (!window)
    {
        std::cerr << "Unable to create the Motor Rock window: "
                  << SDL_GetError() << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if (!SDL_StartTextInput(window))
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Unable to start text input: %s",
                    SDL_GetError());

    SDL_Log("Motor Rock SDL3 shell started with video driver '%s'",
            SDL_GetCurrentVideoDriver());
    SDL_Log("Press F11 to toggle fullscreen or Escape to quit");

#ifdef RRR3D_GAME_CORE_LINKED
    r3d::portable::log_game_capabilities();
#endif

    if (options->smoke_test_milliseconds != 0)
    {
        // Exercise the resize path while keeping normal interactive startup at
        // the requested 1280x720 resolution.
        if (!SDL_SetWindowSize(window, 1024, 576))
            SDL_LogWarn(SDL_LOG_CATEGORY_VIDEO, "Smoke-test resize failed: %s",
                        SDL_GetError());
        queue_smoke_input_events(window);
    }

    bool running = true;
    bool fullscreen = options->fullscreen;
    bool focused = true;
    const std::uint64_t start_ticks = SDL_GetTicks();

    while (running)
    {
        SDL_Event event;
        const int idle_timeout = focused ? 16 : 100;
        if (SDL_WaitEventTimeout(&event, idle_timeout))
        {
            running = process_event(event, window, fullscreen, focused);
            while (running && SDL_PollEvent(&event))
                running = process_event(event, window, fullscreen, focused);
        }

        if (options->smoke_test_milliseconds != 0 &&
            SDL_GetTicks() - start_ticks >= options->smoke_test_milliseconds)
        {
            SDL_Log("SDL3 smoke test completed");
            running = false;
        }
    }

    SDL_StopTextInput(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
