#pragma once

#include "Mode.hpp"

#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "GL.hpp"
#include "Sound.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <random>

// The 'GameMode' mode is the main gameplay mode:

struct GameMode : public Mode {
	GameMode();
	virtual ~GameMode();

	//handle_event is called when new mouse or keyboard events are received:
	// (note that this might be many times per frame or never)
	//The function should return 'true' if it handled the event.
	virtual bool handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) override;

	//update is called at the start of a new frame, after events are handled:
	virtual void update(float elapsed) override;

	//draw is called after update:
	virtual void draw(glm::uvec2 const &drawable_size) override;

	float camera_spin = 0.0f;
	float spot_spin = 0.0f;

	struct LetterDisplay {
	    Scene::Object *obj;
	    char letter;
	    uint32_t index;
	};

	std::vector< Scene::Object * > letters;
	std::vector< LetterDisplay > displayed_text;
	std::vector< char > letter_queue;
	std::vector< bool > used_letters = {false, false, false, false, false};
	std::vector< bool > used_columns = {false, false, false, false, false};

	float cleared_time = 0.0f;

	static constexpr const float TimeBetweenDisplays = 1.5f;
    static constexpr const float MaxLetterOffset = 2.0f;
    static constexpr const float RowMargin = 1.0f;
	static constexpr const float XBoundMin = -3.75f;
	static constexpr const float XBoundMax = 4.0f;
	static constexpr const float YBound = 3.0f;
    static constexpr const float Volume = 10.0f;

	int current_index = -1;
	uint32_t current_word = 0;
	std::vector< std::string > messages;
	std::vector< std::string > current_message;

	uint32_t num_right = 0;
	uint32_t num_wrong = 0;
	std::shared_ptr< Sound::PlayingSample > loop;

	Scene::Object *find_available_letter();
	void hide_letter(uint32_t i);
	void show_letter(uint32_t i, glm::vec2 position, char letter);
	void show_string(std::string message);
    void reset_letters();
    void play_right();
    void play_wrong();
};
