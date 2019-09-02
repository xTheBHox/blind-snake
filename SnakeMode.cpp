#include "SnakeMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>

SnakeMode::SnakeMode() {

  mt.seed(static_cast<unsigned int>( time(NULL) ));
  arena_x_dist = std::uniform_real_distribution<float>(
    arena_pos.x - arena_radius.x + wall_radius,
    arena_pos.x + arena_radius.x - wall_radius
  );
  arena_y_dist = std::uniform_real_distribution<float>(
    arena_pos.y - arena_radius.y + wall_radius,
    arena_pos.y + arena_radius.y - wall_radius
  );

  // generate exit and starting positions
  {
    std::uniform_int_distribution<uint32_t> side_dist(0, 3);
    uint32_t side = side_dist(mt);
    if (side == 0) {
      exit_pos.x = arena_pos.x - arena_radius.x;
      exit_pos.y = arena_y_dist(mt);
      snake_pos.x = arena_pos.x + arena_radius.x;
      snake_pos.y = 2.0f * arena_pos.y - exit_pos.y;
    }
    else if (side == 1) {
      exit_pos.x = arena_pos.x + arena_radius.x;
      exit_pos.y = arena_y_dist(mt);
      snake_pos.x = arena_pos.x - arena_radius.x;
      snake_pos.y = 2.0f * arena_pos.y - exit_pos.y;
    }
    else if (side == 2) {
      exit_pos.x = arena_x_dist(mt);
      exit_pos.y = arena_pos.y - arena_radius.y;
      snake_pos.x = 2.0f * arena_pos.x - exit_pos.x;
      snake_pos.y = arena_pos.y + arena_radius.y;
    }
    else if (side == 3) {
      exit_pos.x = arena_x_dist(mt);
      exit_pos.y = arena_pos.y + arena_radius.y;
      snake_pos.x = 2.0f * arena_pos.x - exit_pos.x;
      snake_pos.y = arena_pos.y - arena_radius.y;
    }

    // clamp to certain buffer
    snake_pos.x = std::max(snake_pos.x, arena_pos.x - arena_radius.x + snake_start_margin.x);
    snake_pos.x = std::min(snake_pos.x, arena_pos.x + arena_radius.x - snake_start_margin.x);

    snake_pos.y = std::max(snake_pos.y, arena_pos.y - arena_radius.y + snake_start_margin.y);
    snake_pos.y = std::min(snake_pos.y, arena_pos.y + arena_radius.y - snake_start_margin.y);

  }

  // generate snake
  snake_body.emplace_back(snake_pos, 0.0f);

  // generate obstacles
  {
    std::uniform_real_distribution<float> obs_r_dist(obs_r_min, obs_r_max);
    for (uint32_t i = 0; i < obs_count_init; i++) {

      float x = arena_x_dist(mt);
      float y = arena_y_dist(mt);
      float r = obs_r_dist(mt);
      if (std::abs(snake_pos.x - x) < obs_buffer && std::abs(snake_pos.y - y) < obs_buffer) {
        continue;
      }
      else if (std::abs(exit_pos.x - x) < obs_buffer && std::abs(exit_pos.y - y) < obs_buffer) {
        continue;
      }
      float x1 = arena_x_dist(mt); // the target position
      float y1 = arena_y_dist(mt);
      obstacles.emplace_back(glm::vec2(x, y), r, glm::vec2(x1, y1));
    }
  }

	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of SnakeMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

SnakeMode::~SnakeMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

bool SnakeMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

  if (evt.type == SDL_MOUSEMOTION) {
    //convert mouse from window pixels (top-left origin, +y is down) to clip space ([-1,1]x[-1,1], +y is up):
    glm::vec2 clip_mouse = glm::vec2(
      (evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
      (evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f
    );

    if (clip_mouse.x != 0.0f && clip_mouse.y != 0.0f) {
      float scaling = snake_speed / std::sqrt(clip_mouse.y * clip_mouse.y + clip_mouse.x * clip_mouse.x);
      snake_vel = clip_mouse * scaling;
    }

    return true;

  }
  else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == SDL_BUTTON_LEFT) {
    snake_mouth_open = false;

    return true;
  }
  else if (evt.type == SDL_MOUSEBUTTONUP && evt.button.button == SDL_BUTTON_LEFT) {
    snake_mouth_open = true;

    return true;
  }

  return false;
}

void SnakeMode::update(float elapsed) {

  // ---- snake movement ----

  if (over) return;

  snake_pos_prev = snake_pos;

  snake_pos += snake_vel * elapsed;

  for (auto &s : snake_body) {
    s.z += elapsed;
  }

  if (!snake_body.empty() && snake_body.back().z > snake_body_interval) {
    float dt = snake_body.back().z - snake_body_interval;
    float scale_front = dt / elapsed;
    float scale_back = 1.0f - scale_front;
    snake_body.emplace_back(snake_pos * scale_back + snake_pos_prev * scale_front, dt);
  }

  while (snake_body.size() > snake_len) {
    snake_body.pop_front();
  }

  auto isCirclesCollide = [](glm::vec2 const &c0, float const &r0, glm::vec2 const &c1, float const &r1) {
    return (c0.x - c1.x) * (c0.x - c1.x) + (c0.y - c1.y) * (c0.y - c1.y) < 0.9f * (r0 + r1) * (r0 + r1);
  };

  // ---- snake v snake tail collision ----

  for (uint32_t i = 0; i + snake_body_solid_index < snake_body.size(); i++) {
    if (isCirclesCollide(snake_pos, snake_r, glm::vec2(snake_body[i].x, snake_body[i].y), snake_r)) {
      over = true;
    }
  }

  // ---- snake v obstacle collision ----

  for (struct Obstacle &ob : obstacles) {
    if (isCirclesCollide(snake_pos, snake_r, ob.pos, ob.r)) {
      over = true;
    }
  }

  // ---- snake v wall collision (except exit area) ----

  if (!isCirclesCollide(snake_pos, snake_r, exit_pos, exit_r)) {
    if (std::abs(arena_pos.x - snake_pos.x) > arena_radius.x - snake_r - wall_radius ||
        std::abs(arena_pos.y - snake_pos.y) > arena_radius.y - snake_r - wall_radius) {
      over = true;
    }
  }

  // ---- snake v food collision ----
  if (snake_mouth_open) {
    for (std::list<glm::vec3>::iterator i = foods.begin(); i != foods.end(); i++) {
      glm::vec3 &f = *i;
      if (isCirclesCollide(snake_pos, snake_r, glm::vec2(f.x, f.y), f.z)) {
        snake_r_actual += snake_r_food_step;
        snake_len += 1;
        foods.erase(i);
        break;
      }
    }
  }

  // ---- food generation ----

  food_counter += elapsed;
  if (food_counter > food_gen_rate) {
    food_counter -= food_gen_rate;
    bool done = false;
    while (!done) {
      float x = arena_x_dist(mt);
      float y = arena_y_dist(mt);
      for (struct Obstacle &ob : obstacles) {
        done = true;
        if (isCirclesCollide(glm::vec2(x, y), food_r, ob.pos, ob.r)) {
          done = false;
        }
      }
      if (done) {
        foods.emplace_back(x, y, food_r);
      }
    }
  }

  // ---- snake growth/decay ----
  if (std::abs(snake_r_actual - snake_r) > 0.5f * snake_r_lag_step) {
    snake_r_lag_counter += elapsed;
    if (snake_r_lag_counter > snake_r_lag_rate) {
      snake_r_lag_counter -= snake_r_lag_rate;
      if (snake_r_actual > snake_r) snake_r += snake_r_lag_step;
      else snake_r -= snake_r_lag_step;
      snake_body_interval = snake_r;
    }
  }
  if (snake_r_actual > snake_r_min) {
    snake_decay_counter += elapsed;
    if (snake_decay_counter > snake_decay_rate) {
      snake_decay_counter -= snake_decay_rate;
      snake_r_actual -= snake_decay_step;
    }
  }

  // ---- obstacle movement ----
  for (struct Obstacle &ob : obstacles) {
    ob.mv_timer += elapsed;
    if (ob.mv_timer > ob.r * obs_mv_rate_mod) {
      ob.mv_timer -= ob.r;
      glm::vec2 dir = ob.dest - ob.pos;
      float dist_sq = dir.x * dir.x + dir.y * dir.y;
      if (dist_sq < obs_mv_step_sq) {
        ob.dest = glm::vec2(arena_x_dist(mt), arena_y_dist(mt));
      }
      else {
        float scaling = obs_mv_step_sq / std::sqrt(dist_sq);
        ob.pos += dir * scaling;
      }
    }

  }
}

void SnakeMode::draw(glm::uvec2 const &drawable_size) {
	//some nice colors from the course web page:
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0xf3ffc6ff);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0x000040ff);
	const glm::u8vec4 wall_color = HEX_TO_U8VEC4(0x602020ff);
	const glm::u8vec4 food_color = HEX_TO_U8VEC4(0xff4040ff);
	// const glm::u8vec4 obstacle_color = HEX_TO_U8VEC4(0x804040ff);
	// const glm::u8vec4 white_color = HEX_TO_U8VEC4(0xffffffff);
	// const glm::u8vec4 shadow_color = HEX_TO_U8VEC4(0xa5df40ff);
  const std::vector< glm::u8vec4 > obstacle_colors = {
    HEX_TO_U8VEC4(0x804040ff), HEX_TO_U8VEC4(0x705820ff), HEX_TO_U8VEC4(0x606050ff),
    HEX_TO_U8VEC4(0x407050ff), HEX_TO_U8VEC4(0x308040ff), HEX_TO_U8VEC4(0x509000ff)
  };
	const std::vector< glm::u8vec4 > rainbow_colors = {
		HEX_TO_U8VEC4(0xe2ff70ff), HEX_TO_U8VEC4(0xcbff70ff), HEX_TO_U8VEC4(0xaeff5dff),
		HEX_TO_U8VEC4(0x88ff52ff), HEX_TO_U8VEC4(0x6cff47ff), HEX_TO_U8VEC4(0x3aff37ff),
		HEX_TO_U8VEC4(0x2eff94ff), HEX_TO_U8VEC4(0x2effa5ff), HEX_TO_U8VEC4(0x17ffc1ff),
		HEX_TO_U8VEC4(0x00f4e7ff), HEX_TO_U8VEC4(0x00cbe4ff), HEX_TO_U8VEC4(0x00b0d8ff),
		HEX_TO_U8VEC4(0x00a5d1ff), HEX_TO_U8VEC4(0x0098cfd8)
	};
	#undef HEX_TO_U8VEC4

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//split rectangle into two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

  auto draw_circle = [&vertices](glm::vec2 const &center, float const &radius, glm::u8vec4 const &color) {

    static const uint8_t sides = 36;
    static const float x_offsets[sides+1] =
      {-1.0f, -0.98480775301f, -0.93969262078f, -0.86602540378f, -0.76604444311f,
        -0.64278760968f, -0.5f, -0.34202014332f, -0.17364817766f,
        0.0f, 0.17364817766f, 0.34202014332f, 0.5f, 0.64278760968f,
        0.76604444311f, 0.86602540378f, 0.93969262078f, 0.98480775301f,
        1.0f,  0.98480775301f,  0.93969262078f,  0.86602540378f, 0.76604444311f,
        0.64278760968f, 0.5f, 0.34202014332f, 0.17364817766f,
        0.0f, -0.17364817766f, -0.34202014332f, -0.5f, -0.64278760968f,
        -0.76604444311f, -0.86602540378f, -0.93969262078f, -0.98480775301f, -1.0f
      };

    static const float y_offsets[sides+1] =
      {0.0f, -0.17364817766f, -0.34202014332f, -0.5f, -0.64278760968f,
        -0.76604444311f, -0.86602540378f, -0.93969262078f, -0.98480775301f,
        -1.0f, -0.98480775301f, -0.93969262078f, -0.86602540378f, -0.76604444311f,
        -0.64278760968f, -0.5f, -0.34202014332f, -0.17364817766f,
        0.0f, 0.17364817766f, 0.34202014332f, 0.5f, 0.64278760968f,
        0.76604444311f, 0.86602540378f, 0.93969262078f, 0.98480775301f,
        1.0f,  0.98480775301f,  0.93969262078f,  0.86602540378f, 0.76604444311f,
        0.64278760968f, 0.5f, 0.34202014332f, 0.17364817766f, 0.0f
      };

    //just draw a <sides>-gon with CCW-oriented triangles:
    for (uint8_t i = 0; i < sides; i++) {
      vertices.emplace_back(glm::vec3(center.x+x_offsets[i]*radius, center.y+y_offsets[i]*radius, 0.0f), color, glm::vec2(0.5f, 0.5f));
      vertices.emplace_back(glm::vec3(center.x+x_offsets[i+1]*radius, center.y+y_offsets[i+1]*radius, 0.0f), color, glm::vec2(0.5f, 0.5f));
      vertices.emplace_back(glm::vec3(center.x, center.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
    }

  };

  { // ---- draw walls ----
    draw_rectangle(glm::vec2(arena_pos.x - arena_radius.x, arena_pos.y), glm::vec2(wall_radius, arena_radius.y), wall_color);
    draw_rectangle(glm::vec2(arena_pos.x + arena_radius.x, arena_pos.y), glm::vec2(wall_radius, arena_radius.y), wall_color);
    draw_rectangle(glm::vec2(arena_pos.x, arena_pos.y - arena_radius.y), glm::vec2(arena_radius.x, wall_radius), wall_color);
    draw_rectangle(glm::vec2(arena_pos.x, arena_pos.y + arena_radius.y), glm::vec2(arena_radius.x, wall_radius), wall_color);
  }

  { // ---- draw exit ----
    draw_circle(exit_pos, wall_radius * 2.0f, bg_color);
  }

  { // ---- draw snake ----

    uint32_t color_index = 0;
    for (auto bi = snake_body.rbegin(); bi != snake_body.rend(); bi++) {
      draw_circle(glm::vec2(bi->x, bi->y), snake_r, rainbow_colors[color_index]);
      color_index++;
      if (color_index >= rainbow_colors.size()) color_index = 0;
    }

    draw_circle(snake_pos, snake_r, fg_color);

    // snake eyes
    // float eye_scale = snake_r / snake_speed;
    // draw_circle(snake_pos);

  }

  { // ---- draw food ----
    for (glm::vec3 f : foods) {
      draw_circle(glm::vec2(f.x, f.y), f.z, food_color);
    }
  }

  { // ---- draw obstacles ----
    uint32_t i = 0;
    for (struct Obstacle &ob : obstacles) {
      draw_circle(ob.pos, ob.r, obstacle_colors[i]);
      i++;
      if (i >= obstacle_colors.size()) i = 0;
    }
  }

	//------ compute court-to-window transform ------
  //compute area that should be visible:
  glm::vec2 scene_min = arena_pos - arena_radius;
  glm::vec2 scene_max = arena_pos + arena_radius;

  //compute window aspect ratio:
  float aspect = drawable_size.x / float(drawable_size.y);
  //we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

  //compute scale factor for court given that...
  float scale =  2.0f / snake_fovx_small;
  glm::vec2 camera_pos = snake_pos;
  if (over) {
    scale = std::min(
      (2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
      (2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
    );
    camera_pos = arena_pos;
  }
  else {
    if (snake_mouth_open) {
      scale = 2.0f / snake_fovx_large;
    }
  }

  //build matrix that scales and translates appropriately:
  glm::mat4 arena_to_clip = glm::mat4(
    glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
    glm::vec4(0.0f, scale, 0.0f, 0.0f),
    glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
    glm::vec4(-camera_pos.x * (scale / aspect), -camera_pos.y * scale, 0.0f, 1.0f)
  );
  //NOTE: glm matrices are specified in *Column-Major* order,
  // so this matrix is actually transposed from how it appears.

  //also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
  clip_to_arena = glm::mat3x2(
    glm::vec2(aspect / scale, 0.0f),
    glm::vec2(0.0f, 1.0f / scale),
    snake_pos
  );

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(arena_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);


	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.

}
