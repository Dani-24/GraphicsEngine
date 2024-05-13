#pragma once

#include "platform.h"
#include "BufferFunc.h"
#include "ModelLoadingFunc.h"

const VertexV3V2 vertices[] = {
	{glm::vec3(-1.0, -1.0, 0.0), glm::vec2(0.0, 0.0)},
	{glm::vec3(1.0, -1.0, 0.0), glm::vec2(1.0, 0.0)},
	{glm::vec3(1.0, 1.0, 0.0), glm::vec2(1.0, 1.0)},
	{glm::vec3(-1.0, 1.0, 0.0), glm::vec2(0.0, 1.0)}
};

const u16 indices[] = {
	0, 1, 2,
	0, 2, 3
};

struct App
{
	void UpdateEntityBuffer();

	void ConfigureFrameBuffer(FrameBuffer& aConfigFB);

	void RenderGeometry(const Program& aBindedProgram);

	GLuint CreateColorAttachment(const bool isFloatingPoint);

	// Loop
	f32  deltaTime;
	bool isRunning;

	float iTime;

	// Input
	Input input;

	// Graphics
	char gpuName[64];
	char openGlVersion[64];

	ivec2 displaySize;

	// === Hacer funciones getMaterial y tal que comprueben si ya esta cargado o no haciendo pushback o devolviendo directamente. ===
	std::vector<Texture>	textures;
	std::vector<Material>	materials;
	std::vector<Mesh>		meshes;
	std::vector<Model>		models;
	std::vector<Program>	programs;

	// program indices
	GLuint renderToBackBufferShader;
	GLuint renderToFrameBufferShader;
	GLuint frameBufferToQuadShader;

	GLuint texturedMeshProgram_uTexture;

	// texture indices
	u32 diceTexIdx;
	u32 whiteTexIdx;
	u32 blackTexIdx;
	u32 normalTexIdx;
	u32 magentaTexIdx;

	// Mode
	Mode mode;

	// Embedded geometry (in-editor simple meshes such as
	// a screen filling quad, a cube, a sphere...)
	GLuint embeddedVertices;
	GLuint embeddedElements;

	// Location of the texture uniform in the textured quad shader
	GLuint programUniformTexture;

	// VAO object to link our screen filling quad with our textured quad shader
	GLuint vao;

	std::string openGlDebugInfo;

	GLint maxUniformBufferSize;
	GLint uniformBlockAligment;
	Buffer localUniformBuffer;

	std::vector<Entity> entities;
	std::vector<Light> lights;

	GLint globalParamsOffset;
	GLint globalParamsSize;

	FrameBuffer deferredFrameBuffer;

	void MouseMove(int x, int y);

	// Input
	vec3 target = vec3(0.0f);
	vec3 camPos = vec3(0.0f, 5.0f, 7.0f);
	float camSpeed = 10.f;

	int lastX = 0, lastY = 0;
	float rotateX = 0.0f;
	float rotateY = 0.0f;
};


void Init(App* app);

void Gui(App* app);

void Update(App* app);

void Render(App* app);