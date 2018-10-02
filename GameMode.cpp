#include "GameMode.hpp"

#include "MenuMode.hpp"
#include "Load.hpp"
#include "MeshBuffer.hpp"
#include "Scene.hpp"
#include "Sound.hpp"
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "check_fb.hpp" //helper for checking currently bound OpenGL framebuffer
#include "read_chunk.hpp" //helper for reading a vector of structures from a file
#include "data_path.hpp" //helper to get paths relative to executable
#include "compile_program.hpp" //helper to compile opengl shader programs
#include "draw_text.hpp" //helper to... um.. draw text
#include "load_save_png.hpp"
#include "texture_program.hpp"
#include "depth_program.hpp"
#include "bloom_program.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>
#include <string>
#include <sstream>
#include <iterator>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#define BUFF_SIZE 5000

static std::random_device rd;
static std::mt19937 rng(rd());
static std::ifstream in(data_path("message.txt"));

Load< Sound::Sample > hum(LoadTagDefault, [](){
    return new Sound::Sample(data_path("hum.wav"));
});

Load< Sound::Sample > right1(LoadTagDefault, [](){
    return new Sound::Sample(data_path("right-001.wav"));
});

Load< Sound::Sample > right2(LoadTagDefault, [](){
    return new Sound::Sample(data_path("right-002.wav"));
});

Load< Sound::Sample > right3(LoadTagDefault, [](){
    return new Sound::Sample(data_path("right-003.wav"));
});

Load< Sound::Sample > right4(LoadTagDefault, [](){
    return new Sound::Sample(data_path("right-004.wav"));
});

Load< Sound::Sample > right5(LoadTagDefault, [](){
    return new Sound::Sample(data_path("right-005.wav"));
});

Load< Sound::Sample > right6(LoadTagDefault, [](){
    return new Sound::Sample(data_path("right-006.wav"));
});

Load< Sound::Sample > wrong1(LoadTagDefault, [](){
    return new Sound::Sample(data_path("wrong-001.wav"));
});

Load< Sound::Sample > wrong2(LoadTagDefault, [](){
    return new Sound::Sample(data_path("wrong-002.wav"));
});

Load< Sound::Sample > wrong3(LoadTagDefault, [](){
    return new Sound::Sample(data_path("wrong-003.wav"));
});

Load< Sound::Sample > wrong4(LoadTagDefault, [](){
    return new Sound::Sample(data_path("wrong-004.wav"));
});

Load< Sound::Sample > wrong5(LoadTagDefault, [](){
    return new Sound::Sample(data_path("wrong-005.wav"));
});

Load< Sound::Sample > wrong6(LoadTagDefault, [](){
    return new Sound::Sample(data_path("wrong-006.wav"));
});

Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("glow.pnct"));
});

Load< GLuint > meshes_for_texture_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(texture_program->program));
});

Load< GLuint > meshes_for_depth_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(depth_program->program));
});

//used for fullscreen passes:
Load< GLuint > empty_vao(LoadTagDefault, [](){
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glBindVertexArray(0);
	return new GLuint(vao);
});

Load< GLuint > vignette_program(LoadTagDefault, [](){
    GLuint program = compile_program(
            //this draws a triangle that covers the entire screen:
            "#version 330\n"
            "void main() {\n"
            "	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
            "}\n"
            ,
            //NOTE on reading screen texture:
            //texelFetch() gives direct pixel access with integer coordinates, but accessing out-of-bounds pixel is undefined:
            //	vec4 color = texelFetch(tex, ivec2(gl_FragCoord.xy), 0);
            //texture() requires using [0,1] coordinates, but handles out-of-bounds more gracefully (using wrap settings of underlying texture):
            //	vec4 color = texture(tex, gl_FragCoord.xy / textureSize(tex,0));

            "#version 330\n"
            "uniform sampler2D tex;\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "	vec2 at = (gl_FragCoord.xy - 0.5 * textureSize(tex, 0)) / textureSize(tex, 0).y;\n"
            //make blur amount more near the edges and less in the middle:
            "	float amt = (0.01 * textureSize(tex,0).y) * max(0.0,(length(at) - 0.3)/0.2);\n"
            //pick a vector to move in for blur using function inspired by:
            //https://stackoverflow.com/questions/12964279/whats-the-origin-of-this-glsl-rand-one-liner
            "	vec2 ofs = amt * normalize(vec2(\n"
            "		fract(dot(gl_FragCoord.xy ,vec2(12.9898,78.233))),\n"
            "		fract(dot(gl_FragCoord.xy ,vec2(96.3869,-27.5796)))\n"
            "	));\n"
            //do a four-pixel average to blur:
            "	vec4 blur =\n"
            "		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.x,ofs.y)) / textureSize(tex, 0))\n"
            "		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.y,ofs.x)) / textureSize(tex, 0))\n"
            "		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(-ofs.x,-ofs.y)) / textureSize(tex, 0))\n"
            "		+ 0.25 * texture(tex, (gl_FragCoord.xy + vec2(ofs.y,-ofs.x)) / textureSize(tex, 0))\n"
            "	;\n"
            "	fragColor = vec4(blur.rgb, 1.0);\n" //blur;\n"
            "}\n"
    );

    glUseProgram(program);

    glUniform1i(glGetUniformLocation(program, "tex"), 0);

    glUseProgram(0);

    return new GLuint(program);
});

Load< GLuint > blur_program(LoadTagDefault, [](){
    GLuint program = compile_program(
            //this draws a triangle that covers the entire screen:
            "#version 330\n"
            "void main() {\n"
            "	gl_Position = vec4(4 * (gl_VertexID & 1) - 1,  2 * (gl_VertexID & 2) - 1, 0.0, 1.0);\n"
            "}\n"
            ,

            "#version 330\n"
            "uniform sampler2D tex;\n"
            "uniform float weight[121] = float[] "
			"(0.000086, 0.00026, 0.000614, 0.001132, 0.001634, 0.001847, 0.001634, 0.001132, 0.000614, 0.00026, 0.000086,"
            "0.00026, 0.000784, 0.001848, 0.003408, 0.00492, 0.005561, 0.00492, 0.003408, 0.001848, 0.000784, 0.00026,"
            "0.000614, 0.001848, 0.004354, 0.00803, 0.011594, 0.013104, 0.011594, 0.00803, 0.004354, 0.001848, 0.000614,"
            "0.001132, 0.003408, 0.00803, 0.014812, 0.021385, 0.02417, 0.021385, 0.014812, 0.00803, 0.003408, 0.001132,"
            "0.001634, 0.00492, 0.011594, 0.021385, 0.030875, 0.034896, 0.030875, 0.021385, 0.011594, 0.00492, 0.001634,"
            "0.001847, 0.005561, 0.013104, 0.02417, 0.034896, 0.03944, 0.034896, 0.02417, 0.013104, 0.005561, 0.001847,"
            "0.001634, 0.00492, 0.011594, 0.021385, 0.030875, 0.034896, 0.030875, 0.021385, 0.011594, 0.00492, 0.001634,"
            "0.001132, 0.003408, 0.00803, 0.014812, 0.021385, 0.02417, 0.021385, 0.014812, 0.00803, 0.003408, 0.001132,"
            "0.000614, 0.001848, 0.004354, 0.00803, 0.011594, 0.013104, 0.011594, 0.00803, 0.004354, 0.001848, 0.000614,"
            "0.00026, 0.000784, 0.001848, 0.003408, 0.00492, 0.005561, 0.00492, 0.003408, 0.001848, 0.000784, 0.00026,"
            "0.000086, 0.00026, 0.000614, 0.001132, 0.001634, 0.001847, 0.001634, 0.001132, 0.000614, 0.00026, 0.000086);\n"
            "out vec4 fragColor;\n"
            "void main() {\n"
            "    vec4 color = texture(tex, gl_FragCoord.xy / textureSize(tex, 0));\n"
            "    vec3 result = vec3(0.0, 0.0, 0.0);\n"
            "    for(int i = -5; i < 5; ++i) {\n"
            "        for (int j = -5; j < 5; ++j) {\n"
            "            result += texture(tex, (gl_FragCoord.xy + vec2(i, j))/ textureSize(tex, 0)).rgb * weight[7*(i+3)+(j+3)];\n"
            "        }\n"
            "    }\n"
            "fragColor = vec4(result, 1.0);"
            "}\n"
    );

    glUseProgram(program);

    glUniform1i(glGetUniformLocation(program, "tex"), 0);

    glUseProgram(0);

    return new GLuint(program);
});

GLuint load_texture(std::string const &filename) {
	glm::uvec2 size;
	std::vector< glm::u8vec4 > data;
	load_png(filename, &size, &data, LowerLeftOrigin);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	GL_ERRORS();

	return tex;
}

Load< GLuint > wood_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wood.png")));
});

Load< GLuint > marble_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/marble.png")));
});

Load< GLuint > white_tex(LoadTagDefault, [](){
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glm::u8vec4 white(0xff, 0xff, 0xff, 0xff);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, glm::value_ptr(white));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	return new GLuint(tex);
});


Scene::Transform *camera_parent_transform = nullptr;
Scene::Camera *camera = nullptr;
Scene::Transform *spot_parent_transform = nullptr;
Scene::Lamp *spot = nullptr;
Scene::Object *l0 = nullptr;
Scene::Object *l1 = nullptr;
Scene::Object *l2 = nullptr;
Scene::Object *l3 = nullptr;
Scene::Object *l4 = nullptr;

Load< Scene > scene(LoadTagDefault, [](){
	Scene *ret = new Scene;

	//pre-build some program info (material) blocks to assign to each object:
	Scene::Object::ProgramInfo texture_program_info;
	texture_program_info.program = texture_program->program;
	texture_program_info.vao = *meshes_for_texture_program;
	texture_program_info.mvp_mat4  = texture_program->object_to_clip_mat4;
	texture_program_info.mv_mat4x3 = texture_program->object_to_light_mat4x3;
	texture_program_info.itmv_mat3 = texture_program->normal_to_light_mat3;

	Scene::Object::ProgramInfo depth_program_info;
	depth_program_info.program = depth_program->program;
	depth_program_info.vao = *meshes_for_depth_program;
	depth_program_info.mvp_mat4  = depth_program->object_to_clip_mat4;

	Scene::Object::ProgramInfo bloom_program_info;
	bloom_program_info.program = bloom_program->program;
	bloom_program_info.vao = *meshes_for_texture_program;
	bloom_program_info.mvp_mat4  = bloom_program->object_to_clip_mat4;
	bloom_program_info.mv_mat4x3 = bloom_program->object_to_light_mat4x3;
	bloom_program_info.itmv_mat3 = bloom_program->normal_to_light_mat3;

	//load transform hierarchy:
	ret->load(data_path("glow.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m){
		bool is_object = false;
		Scene::Object *obj = nullptr;

        if (!l0) { obj = s.new_object(t); l0 = obj;  is_object = true;}
        else if (!l1) { obj = s.new_object(t); l1 = obj;  is_object = true;}
        else if (!l2) { obj = s.new_object(t); l2 = obj;  is_object = true;}
		else if (!l3) { obj = s.new_object(t); l3 = obj;  is_object = true;}
		else if (!l4) { obj = s.new_object(t); l4 = obj;  is_object = true;}

        if (is_object) {
			obj->programs[Scene::Object::ProgramTypeDefault] = texture_program_info;
			obj->programs[Scene::Object::ProgramTypeDefault].textures[0] = *white_tex;

			obj->programs[Scene::Object::ProgramTypeShadow] = depth_program_info;
			obj->programs[Scene::Object::ProgramTypeBloom] = bloom_program_info;

			MeshBuffer::Mesh const &mesh = meshes->lookup(m);

			obj->programs[Scene::Object::ProgramTypeBloom].start = mesh.start;
			obj->programs[Scene::Object::ProgramTypeBloom].count = mesh.count;
        }
	});

	//look up the camera:
	for (Scene::Camera *c = ret->first_camera; c != nullptr; c = c->alloc_next) {
		if (c->transform->name == "Camera") {
			if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
			camera = c;
		}
	}
	if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

	return ret;
});

GameMode::GameMode() {
    letters.emplace_back(l0);
    letters.emplace_back(l1);
    letters.emplace_back(l2);
    letters.emplace_back(l3);
    letters.emplace_back(l4);

    std::string str;
	while (std::getline(in, str)) {
		messages.emplace_back(str);
	}
    in.close();

    reset_letters();
    show_string("PLAY");
    loop = hum->play(camera->transform->position, Volume / 2.0f, Sound::Loop);
}

GameMode::~GameMode() {
	if (loop) loop->stop();
}

bool GameMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	//ignore any keys that are the result of automatic key repeat:
	if (evt.type == SDL_KEYDOWN && evt.key.repeat) {
		return false;
	}

	char pressed = '\0';
    if (evt.type == SDL_KEYDOWN) {
        switch (evt.key.keysym.scancode) {
        	case SDL_SCANCODE_A: pressed = 'A'; break;
			case SDL_SCANCODE_B: pressed = 'B'; break;
			case SDL_SCANCODE_C: pressed = 'C'; break;
			case SDL_SCANCODE_D: pressed = 'D'; break;
			case SDL_SCANCODE_E: pressed = 'E'; break;
			case SDL_SCANCODE_F: pressed = 'F'; break;
			case SDL_SCANCODE_G: pressed = 'G'; break;
			case SDL_SCANCODE_H: pressed = 'H'; break;
			case SDL_SCANCODE_I: pressed = 'I'; break;
			case SDL_SCANCODE_J: pressed = 'J'; break;
			case SDL_SCANCODE_K: pressed = 'K'; break;
			case SDL_SCANCODE_L: pressed = 'L'; break;
			case SDL_SCANCODE_M: pressed = 'M'; break;
			case SDL_SCANCODE_N: pressed = 'N'; break;
			case SDL_SCANCODE_O: pressed = 'O'; break;
			case SDL_SCANCODE_P: pressed = 'P'; break;
			case SDL_SCANCODE_Q: pressed = 'Q'; break;
			case SDL_SCANCODE_R: pressed = 'R'; break;
			case SDL_SCANCODE_S: pressed = 'S'; break;
			case SDL_SCANCODE_T: pressed = 'T'; break;
			case SDL_SCANCODE_U: pressed = 'U'; break;
			case SDL_SCANCODE_V: pressed = 'V'; break;
			case SDL_SCANCODE_W: pressed = 'W'; break;
			case SDL_SCANCODE_X: pressed = 'X'; break;
			case SDL_SCANCODE_Y: pressed = 'Y'; break;
			case SDL_SCANCODE_Z: pressed = 'Z'; break;
            default: break;
        }
    }

    if (pressed != '\0') {
    	auto d = displayed_text.end();
    	auto it = displayed_text.begin();
    	for (; it != displayed_text.end(); ++it) {
    		if (it->letter == pressed) {
    			d = it;
    			break;
    		}
    	}

		if (d == displayed_text.end()) {
			std::cout << "You made a mistake" << std::endl;
			play_wrong();
			++num_wrong;
		}
		else {
			play_right();
			++num_right;
			hide_letter(d->index);
            displayed_text.erase(d);
		}

		if (!letter_queue.size() && !displayed_text.size()) {
		    ++current_word;
		}

		if (!letter_queue.size() && !displayed_text.size() && current_word >= current_message.size()) {
		    cleared_time = 0.0f;
		    ++current_index;

		    if (current_index < (int)messages.size()) {
                current_message.clear();
                current_word = 0;
                std::string str;
                std::istringstream iss(messages[current_index]);
                while ( getline( iss, str, ' ' ) ) {
                    current_message.emplace_back(str);
		        }
            }
		}

		return true;
    }

	return false;
}

void GameMode::update(float elapsed) {
//	camera_parent_transform->rotation = glm::angleAxis(camera_spin, glm::vec3(0.0f, 0.0f, 1.0f));
//	spot_parent_transform->rotation = glm::angleAxis(spot_spin, glm::vec3(0.0f, 0.0f, 1.0f));

	cleared_time += elapsed;

	if (cleared_time > TimeBetweenDisplays && !displayed_text.size() && !letter_queue.size() && current_index < (int)messages.size()) {
        show_string(current_message[current_word]);
	}
}

//GameMode will render to some offscreen framebuffer(s).
//This code allocates and resizes them as needed:
struct Framebuffers {
	glm::uvec2 size = glm::uvec2(0,0); //remember the size of the framebuffer

	//This framebuffer is used for fullscreen effects:
	GLuint color_tex = 0;
	GLuint depth_rb = 0;
	GLuint fb = 0;

	//This framebuffer is used for shadow maps:
	glm::uvec2 shadow_size = glm::uvec2(0,0);
	GLuint shadow_color_tex = 0; //DEBUG
	GLuint shadow_depth_tex = 0;
	GLuint shadow_fb = 0;

    GLuint bloom_color_tex = 0;
    GLuint bloom_fb = 0;

	void allocate(glm::uvec2 const &new_size, glm::uvec2 const &new_shadow_size) {
		//allocate full-screen framebuffer:
		if (size != new_size) {
			size = new_size;

			if (color_tex == 0) glGenTextures(1, &color_tex);
			glBindTexture(GL_TEXTURE_2D, color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	//GL_NEAREST?
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (depth_rb == 0) glGenRenderbuffers(1, &depth_rb);
			glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
	
			if (fb == 0) glGenFramebuffers(1, &fb);
			glBindFramebuffer(GL_FRAMEBUFFER, fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
//			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, bloom_color_tex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}

		//allocate shadow map framebuffer:
		if (shadow_size != new_shadow_size) {
			shadow_size = new_shadow_size;

			if (shadow_color_tex == 0) glGenTextures(1, &shadow_color_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, shadow_size.x, shadow_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);


			if (shadow_depth_tex == 0) glGenTextures(1, &shadow_depth_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_size.x, shadow_size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (shadow_fb == 0) glGenFramebuffers(1, &shadow_fb);
			glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadow_color_tex, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_depth_tex, 0);
			check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}

        //allocate bloom frame buffers
        //https://learnopengl.com/Advanced-Lighting/Bloom
        if (bloom_color_tex == 0) glGenTextures(1, &bloom_color_tex);
        glBindTexture(GL_TEXTURE_2D, bloom_color_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, size.x, size.y, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (bloom_fb == 0) glGenFramebuffers(1, &bloom_fb);
        glBindFramebuffer(GL_FRAMEBUFFER, bloom_fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_color_tex, 0);
        check_fb();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        GL_ERRORS();
	}
} fbs;

void GameMode::draw(glm::uvec2 const &drawable_size) {
	fbs.allocate(drawable_size, glm::uvec2(512, 512));

    //Bloom:
    glBindFramebuffer(GL_FRAMEBUFFER, fbs.bloom_fb);
    glViewport(0,0,drawable_size.x, drawable_size.y);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //set up basic OpenGL state:
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //set up light positions:
    glUseProgram(bloom_program->program);

    //don't use distant directional light at all (color == 0):
    glUniform3fv(bloom_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.98f, 0.76f, 0.42f)));
    glUniform3fv(bloom_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
    //use hemisphere light for subtle ambient light:
    glUniform3fv(bloom_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.98f, 0.76f, 0.42f)));
    glUniform3fv(bloom_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

    scene->draw(camera, Scene::Object::ProgramTypeBloom);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GL_ERRORS();

	//Copy scene from color buffer to screen, performing post-processing effects
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fbs.bloom_color_tex);
	glUseProgram(*blur_program);
	glBindVertexArray(*empty_vao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glUseProgram(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void GameMode::hide_letter(uint32_t i) {
	Scene::Object *l = letters[i];
	l->transform->position = camera->transform->position;
	l->transform->position.z += 10.0f;

	used_letters[i] = false;
}

void GameMode::show_letter(uint32_t i, glm::vec2 position, char letter) {
	if (used_letters[i]) return;

	Scene::Object *l = letters[i];

	std::string letter_name = "glo" + std::string(1, letter);
	MeshBuffer::Mesh const &mesh = meshes->lookup(letter_name);
	l->programs[Scene::Object::ProgramTypeBloom].start = mesh.start;
	l->programs[Scene::Object::ProgramTypeBloom].count = mesh.count;
	l->transform->position.x = position.x;
	l->transform->position.y = position.y;
	l->transform->position.z = 0.0f;

	LetterDisplay d;
	d.letter = letter;
	d.index = i;
	d.obj = l;

	displayed_text.emplace_back(d);

	used_letters[i] = true;
}

Scene::Object *GameMode::find_available_letter() {
	for (uint32_t i = 0; i < 5; ++i) {
		if (!used_letters[i]) return letters[i];
	}
	return nullptr;
}

//void GameMode::show_from_queue() {
//    if (!letter_queue.size()) return;
//}

void GameMode::show_string(std::string message) {

	std::vector<char> letters(message.begin(), message.end());
	if (!letters.size() || letters.size() > 5) return;

	uint32_t num_cols = glm::min(float(letters.size()), 5.0f);
	float spacing = (XBoundMax - XBoundMin) / (num_cols + 1);
	glm::vec2 range = glm::vec2(-YBound, YBound);

	float prev_y = range.x + (range.y - range.x) / 2.0f;
	for (uint32_t i = 0; i < letters.size(); ++i) {
		float x_offset = XBoundMin + spacing * (i + 1);

		float max = glm::min(prev_y + MaxLetterOffset, YBound);
		float min = glm::max(prev_y - MaxLetterOffset, -YBound);
		std::uniform_real_distribution<float> dist6(min, max);
		float y_offset = dist6(rng);

		y_offset = glm::clamp(y_offset, range.x, range.y);
		glm::vec2 position = glm::vec2(x_offset, y_offset);

		Scene::Object *l = find_available_letter();
		if (!l) {
			for (uint32_t j = i; j < letters.size(); ++j) letter_queue.emplace_back(letters[j]);
			return;
		}

		show_letter(i, position, letters[i]);
		prev_y = y_offset;
	}
}

void GameMode::reset_letters() {
    for (uint32_t i = 0; i < 5; ++i) {
        hide_letter(i);
        used_letters[i] = false;
    }
}

void GameMode::play_right() {
	switch (num_right % 6) {
		case 0: right1->play(camera->transform->position, Volume, Sound::Once); break;
		case 1: right2->play(camera->transform->position, Volume, Sound::Once); break;
		case 2: right3->play(camera->transform->position, Volume, Sound::Once); break;
		case 3: right4->play(camera->transform->position, Volume, Sound::Once); break;
		case 4: right5->play(camera->transform->position, Volume, Sound::Once); break;
		case 5: right6->play(camera->transform->position, Volume, Sound::Once); break;
	}
}

void GameMode::play_wrong() {
	switch (num_wrong % 6) {
		case 0: wrong1->play(camera->transform->position, Volume, Sound::Once); break;
		case 1: wrong2->play(camera->transform->position, Volume, Sound::Once); break;
		case 2: wrong3->play(camera->transform->position, Volume, Sound::Once); break;
		case 3: wrong4->play(camera->transform->position, Volume, Sound::Once); break;
		case 4: wrong5->play(camera->transform->position, Volume, Sound::Once); break;
		case 5: wrong6->play(camera->transform->position, Volume, Sound::Once); break;
	}
}
