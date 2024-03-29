#include "ColorTextureProgram.hpp"

#include "Mode.hpp"
#include "GL.hpp"

#include <vector>
#include <deque>
#include <list>
#include <cmath>
#include <ctime>
#include <random>

/*
 * SnakeMode is a game mode that implements a single-player game of Snake.
 */

struct SnakeMode : Mode {
	SnakeMode();
	virtual ~SnakeMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

  glm::vec2 snake_pos = glm::vec2(0.0f, 0.0f);
  glm::vec2 snake_vel = glm::vec2(1.0f, 0.0f);

  float snake_speed = 1.0f;
  float snake_r = 0.2f;
  float snake_r_actual = snake_r;
  float snake_r_lag_counter = 0.0f;
  float snake_r_lag_rate = 0.1f;
  float snake_r_lag_step = 0.005f;

  float snake_r_food_step = 0.04f;

  uint16_t snake_len = 15;

  glm::vec2 snake_pos_prev = glm::vec2(0.0f, 0.0f);
  float snake_body_interval = 0.2f;
  std::deque<glm::vec3> snake_body; // (x, y, age)

  uint32_t snake_body_solid_index = 6;

  bool snake_mouth_open = true;
  float snake_fovx_large = 5.0f;
  float snake_fovx_small = 1.0f;

  float snake_decay_counter = 0.0f;
  float snake_decay_rate = 8.0f;
  float snake_decay_step = 0.01f;
  float snake_r_min = 0.15f;

  struct Obstacle {
    Obstacle(glm::vec2 const &pos_, float const &r_, glm::vec2 const &dest_) :
      pos(pos_), r(r_), dest(dest_) { }
    glm::vec2 pos;
    float r;
    glm::vec2 dest;
    float mv_timer = 0.0f;
  };

  std::vector<Obstacle> obstacles;
  uint32_t obs_count_init = 60;
  float obs_r_max = 1.6f;
  float obs_r_min = 0.5f;
  float obs_buffer = 1.0f;
  float obs_mv_step_sq = 0.015f;
  float obs_mv_rate_mod = 0.15f;

  float food_gen_rate = 0.35f;
  float food_counter = 0.0f;
  float food_r = 0.1f;
  std::list<glm::vec3> foods; // (x, y, r)

  glm::vec2 arena_radius = glm::vec2(10.0f, 10.0f);
  glm::vec2 arena_pos = glm::vec2(0.0f, 0.0f);
  glm::vec2 snake_start_margin = glm::vec2(1.0f, 1.0f);

  float wall_radius = 0.2f;
  glm::vec2 exit_pos = glm::vec2(0.0f, 0.0f);
  float exit_r = 0.4f;

  bool over = false;

  // generators

  std::mt19937 mt;
  std::uniform_real_distribution<float> arena_x_dist;
  std::uniform_real_distribution<float> arena_y_dist;

	//----- opengl assets / helpers ------

	//draw functions will work on vectors of vertices, defined as follows:
	struct Vertex {
		Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) :
			Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
		glm::vec3 Position;
		glm::u8vec4 Color;
		glm::vec2 TexCoord;
	};
	static_assert(sizeof(Vertex) == 4*3 + 1*4 + 4*2, "SnakeMode::Vertex should be packed");

	//Shader program that draws transformed, vertices tinted with vertex colors:
	ColorTextureProgram color_texture_program;

	//Buffer used to hold vertex data during drawing:
	GLuint vertex_buffer = 0;

	//Vertex Array Object that maps buffer locations to color_texture_program attribute locations:
	GLuint vertex_buffer_for_color_texture_program = 0;

	//Solid white texture:
	GLuint white_tex = 0;

	//matrix that maps from clip coordinates to court-space coordinates:
	glm::mat3x2 clip_to_arena = glm::mat3x2(1.0f);
	// computed in draw() as the inverse of OBJECT_TO_CLIP
	// (stored here so that the mouse handling code can use it to position the paddle)

};
