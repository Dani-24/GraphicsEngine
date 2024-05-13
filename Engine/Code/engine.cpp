#include "engine.h"
#include <imgui.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include "ModelLoadingFunc.h"

// Windows Audio
#include <Windows.h>
#include <mmsystem.h>
#include <iostream>

#pragma comment(lib, "winmm.lib")
//

GLuint CreateProgramFromSource(String programSource, const char* shaderName)
{
	GLchar  infoLogBuffer[1024] = {};
	GLsizei infoLogBufferSize = sizeof(infoLogBuffer);
	GLsizei infoLogSize;
	GLint   success;

	char versionString[] = "#version 430\n";
	char shaderNameDefine[128];
	sprintf(shaderNameDefine, "#define %s\n", shaderName);
	char vertexShaderDefine[] = "#define VERTEX\n";
	char fragmentShaderDefine[] = "#define FRAGMENT\n";

	const GLchar* vertexShaderSource[] = {
		versionString,
		shaderNameDefine,
		vertexShaderDefine,
		programSource.str
	};
	const GLint vertexShaderLengths[] = {
		(GLint)strlen(versionString),
		(GLint)strlen(shaderNameDefine),
		(GLint)strlen(vertexShaderDefine),
		(GLint)programSource.len
	};
	const GLchar* fragmentShaderSource[] = {
		versionString,
		shaderNameDefine,
		fragmentShaderDefine,
		programSource.str
	};
	const GLint fragmentShaderLengths[] = {
		(GLint)strlen(versionString),
		(GLint)strlen(shaderNameDefine),
		(GLint)strlen(fragmentShaderDefine),
		(GLint)programSource.len
	};

	GLuint vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vshader, ARRAY_COUNT(vertexShaderSource), vertexShaderSource, vertexShaderLengths);
	glCompileShader(vshader);
	glGetShaderiv(vshader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
		ELOG("glCompileShader() failed with vertex shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
	}

	GLuint fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fshader, ARRAY_COUNT(fragmentShaderSource), fragmentShaderSource, fragmentShaderLengths);
	glCompileShader(fshader);
	glGetShaderiv(fshader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fshader, infoLogBufferSize, &infoLogSize, infoLogBuffer);
		ELOG("glCompileShader() failed with fragment shader %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
	}

	GLuint programHandle = glCreateProgram();
	glAttachShader(programHandle, vshader);
	glAttachShader(programHandle, fshader);
	glLinkProgram(programHandle);
	glGetProgramiv(programHandle, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(programHandle, infoLogBufferSize, &infoLogSize, infoLogBuffer);
		ELOG("glLinkProgram() failed with program %s\nReported message:\n%s\n", shaderName, infoLogBuffer);
	}

	glUseProgram(0);

	glDetachShader(programHandle, vshader);
	glDetachShader(programHandle, fshader);
	glDeleteShader(vshader);
	glDeleteShader(fshader);

	return programHandle;
}

u32 LoadProgram(App* app, const char* filepath, const char* programName)
{
	String programSource = ReadTextFile(filepath);

	Program program = {};
	program.handle = CreateProgramFromSource(programSource, programName);
	program.filepath = filepath;
	program.programName = programName;
	program.lastWriteTimestamp = GetFileLastWriteTimestamp(filepath);

	GLint attributeCount = 0;
	glGetProgramiv(program.handle, GL_ACTIVE_ATTRIBUTES, &attributeCount);

	for (GLuint i = 0; i < attributeCount; i++)
	{
		GLsizei bufSize = 256;
		GLsizei length = 0;
		GLint size = 0;
		GLenum type = 0;
		GLchar name[256];

		glGetActiveAttrib(program.handle, i, ARRAY_COUNT(name), &length, &size, &type, name);

		u8 location = glGetAttribLocation(program.handle, name);
		program.shaderLayout.attributes.push_back(VertexShaderAttribute{ location, (u8)size });
	}

	app->programs.push_back(program);

	return app->programs.size() - 1;
}

Image LoadImage(const char* filename)
{
	Image img = {};
	stbi_set_flip_vertically_on_load(true);
	img.pixels = stbi_load(filename, &img.size.x, &img.size.y, &img.nchannels, 0);
	if (img.pixels)
		img.stride = img.size.x * img.nchannels;
	else
		ELOG("Could not open file %s", filename);

	return img;
}

void FreeImage(Image image)
{
	stbi_image_free(image.pixels);
}

GLuint CreateTexture2DFromImage(Image image)
{
	GLenum internalFormat = GL_RGB8;
	GLenum dataFormat = GL_RGB;
	GLenum dataType = GL_UNSIGNED_BYTE;

	switch (image.nchannels)
	{
	case 3: dataFormat = GL_RGB; internalFormat = GL_RGB8; break;
	case 4: dataFormat = GL_RGBA; internalFormat = GL_RGBA8; break;
	default: ELOG("LoadTexture2D() - Unsupported number of channels");
	}

	GLuint texHandle;
	glGenTextures(1, &texHandle);
	glBindTexture(GL_TEXTURE_2D, texHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.size.x, image.size.y, 0, dataFormat, dataType, image.pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	return texHandle;
}

u32 LoadTexture2D(App* app, const char* filepath)
{
	for (u32 texIdx = 0; texIdx < app->textures.size(); ++texIdx)
		if (app->textures[texIdx].filepath == filepath)
			return texIdx;

	Image image = LoadImage(filepath);

	if (image.pixels)
	{
		Texture tex = {};
		tex.handle = CreateTexture2DFromImage(image);
		tex.filepath = filepath;

		u32 texIdx = app->textures.size();
		app->textures.push_back(tex);

		FreeImage(image);
		return texIdx;
	}
	else
	{
		return UINT32_MAX;
	}
}

GLuint FindVAO(Mesh& mesh, u32 subMeshIdx, const Program& program)
{
	GLuint returnValue = 0;

	SubMesh& subMesh = mesh.submeshes[subMeshIdx];

	for (u32 i = 0; i < (u32)subMesh.vaos.size(); ++i)
	{
		if (subMesh.vaos[i].programHandle == program.handle)
		{
			returnValue = subMesh.vaos[i].handle;
			break;
		}
	}

	if (returnValue == 0)
	{
		GLuint vaoHandle = 0;

		glGenVertexArrays(1, &vaoHandle);
		glBindVertexArray(vaoHandle);

		glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBufferHandle);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBufferHandle);

		// Iteradores que iteran iteraciones en variables iteradas que iteran la iteracion
		auto& shaderLayout = program.shaderLayout.attributes;
		for (auto shaderIt = shaderLayout.cbegin(); shaderIt != shaderLayout.cend(); ++shaderIt) {

			bool attributeWasLinked = false;
			auto subMeshLayout = subMesh.vertexBufferLayout.attributes;
			for (auto subMeshIt = subMeshLayout.cbegin(); subMeshIt != subMeshLayout.cend(); ++subMeshIt)
			{
				if (shaderIt->location == subMeshIt->location)
				{
					const u32 index = subMeshIt->location;
					const u32 nComp = subMeshIt->componentCount;
					const u32 offset = subMeshIt->offset + subMesh.vertexOffset;
					const u32 stride = subMesh.vertexBufferLayout.stride;

					glVertexAttribPointer(index, nComp, GL_FLOAT, GL_FALSE, stride, (void*)(u64)(offset));
					glEnableVertexAttribArray(index);

					attributeWasLinked = true;
					break;
				}
			}
			assert(attributeWasLinked);	// si true bien, si no hace q pete
		}

		glBindVertexArray(0);

		VAO vao = { vaoHandle, program.handle };
		subMesh.vaos.push_back(vao);

		returnValue = vaoHandle;
	}
	return returnValue;
}

mat4 TransformScale(const vec3& scaleFactors)
{
	return glm::scale(scaleFactors);
}

mat4 TransformPositionScale(const vec3& position, const vec3& scaleFactors)
{
	mat4 returnValue = glm::translate(position);
	returnValue = glm::scale(returnValue, scaleFactors);

	return returnValue;
}

void Init(App* app)
{
	// acabar lo del power
	app->openGlDebugInfo += "OpenGL version:\n" + std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
	app->openGlDebugInfo += "\n\nOpenGL vendor:\n" + std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
	app->openGlDebugInfo += "\n\nOpenGL renderer:\n" + std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
	app->openGlDebugInfo += "\n\nOpenGL GLSL version:\n" + std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

	// === Init Buffers ===

	// VBO
	glGenBuffers(1, &app->embeddedVertices);
	glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// EBO
	glGenBuffers(1, &app->embeddedElements);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// VAO
	glGenVertexArrays(1, &app->vao);
	glBindVertexArray(app->vao);
	glBindBuffer(GL_ARRAY_BUFFER, app->embeddedVertices);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)0);	// Primera layout de shaders.glsl
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexV3V2), (void*)sizeof(glm::vec3));	// Segunda layout de shaders.glsl
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, app->embeddedElements);
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	// Programs
	app->renderToBackBufferShader = LoadProgram(app, "RENDER_TO_BB.glsl", "RENDER_TO_BB");
	app->renderToFrameBufferShader = LoadProgram(app, "RENDER_TO_FB.glsl", "RENDER_TO_FB");
	app->frameBufferToQuadShader = LoadProgram(app, "FB_TO_QUAD.glsl", "FB_TO_QUAD");

	// Resources
	const Program& texturedMeshProgram = app->programs[app->renderToBackBufferShader];
	//app->texturedMeshProgram_uTexture = glGetUniformLocation(texturedMeshProgram.handle, "uTexture");
	u32 patrisioModelIndex = ModelLoader::LoadModel(app, "Assets/Patrick.obj");
	u32 groundModelIndex = ModelLoader::LoadModel(app, "Assets/ground.obj");

	/*app->diceTexIdx = LoadTexture2D(app, "Assets/dice.png");
	app->whiteTexIdx = LoadTexture2D(app, "Assets/color_white.png");
	app->blackTexIdx = LoadTexture2D(app, "Assets/color_black.png");
	app->normalTexIdx = LoadTexture2D(app, "Assets/color_normal.png");
	app->magentaTexIdx = LoadTexture2D(app, "Assets/color_magenta.png");*/

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_BLEND);

	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &app->maxUniformBufferSize);
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &app->uniformBlockAligment);

	app->localUniformBuffer = CreateConstantBuffer(app->maxUniformBufferSize);

	app->entities.push_back({ TransformPositionScale(vec3(0.0,1.0,0.0), vec3(1.0,1.0,1.0)), patrisioModelIndex, 0, 0 });
	app->entities.push_back({ TransformPositionScale(vec3(2.0,1.0,5.0), vec3(1.0,1.0,1.0)), patrisioModelIndex, 0, 0 });
	app->entities.push_back({ TransformPositionScale(vec3(5.0,1.0,2.0), vec3(1.0,1.0,1.0)), patrisioModelIndex, 0, 0 });

	app->entities.push_back({ TransformPositionScale(vec3(0.0,-3.0,0.0), vec3(1.0,1.0,1.0)), groundModelIndex, 0, 0 });

	app->lights.push_back({ LightType::LightType_Directional, vec3(1.0,1.0,1.0), vec3(1.0,-1.0,1.0), vec3(0.0,0.0,0.0) });
	app->lights.push_back({ LightType::LightType_Point, vec3(1.0,0.0,0.0), vec3(1.0,1.0,1.0), vec3(0.0,1.0,1.0) });

	app->ConfigureFrameBuffer(app->deferredFrameBuffer);

	app->mode = Mode_Forward;
	//app->mode = Mode_Deferred;

	// Song
	PlaySound(TEXT("Assets/Junes Theme - Persona 4.wav"), NULL, SND_LOOP | SND_ASYNC);
}

void Gui(App* app)
{
	ImGui::Begin("Info");
	ImGui::Text("FPS: %f", 1.0f / app->deltaTime);
	ImGui::Text("%s", app->openGlDebugInfo.c_str());

	ImGui::Text(""); ImGui::Separator(); ImGui::Text(""); //

	// Camera
	ImGui::Text("CAMERA");

	float winSize = ImGui::GetWindowSize().x / 3;

	ImGui::SetNextItemWidth(winSize);
	ImGui::InputFloat("", &app->camPos.x); ImGui::SameLine();
	ImGui::SetNextItemWidth(winSize);
	ImGui::InputFloat("", &app->camPos.y); ImGui::SameLine();
	ImGui::SetNextItemWidth(winSize);
	ImGui::InputFloat("XYZ", &app->camPos.z);

	ImGui::Text(""); //

	ImGui::SliderFloat("Camera Speed", &app->camSpeed, 1.f, 20.f);

	ImGui::Text(""); ImGui::Separator(); ImGui::Text(""); //

	// Render Modes
	const char* RenderModes[] = { "FORWARD", "DEFERRED" };
	if (ImGui::BeginCombo("Render_Mode", RenderModes[app->mode]))
	{
		for (size_t i = 0; i < ARRAY_COUNT(RenderModes); ++i) {
			bool isSelected = (i == app->mode);
			if (ImGui::Selectable(RenderModes[i], isSelected))
				app->mode = static_cast<Mode>(i);
		}

		ImGui::EndCombo();
	}

	if (app->mode == Mode::Mode_Deferred) {
		for (size_t i = 0; i < app->deferredFrameBuffer.colorAttachment.size(); ++i)
		{
			ImGui::Image((ImTextureID)app->deferredFrameBuffer.colorAttachment[i], ImVec2(250, 150), ImVec2(0, 1), ImVec2(1, 0));
		}
	}

	ImGui::End();
}

void App::MouseMove(int x, int y) {
	int deltaX = x - lastX;
	int deltaY = y - lastY;

	lastX = x;
	lastY = y;

	rotateX += deltaY * 0.2f;
	rotateY += deltaX * 0.2f;
}

void Update(App* app)
{
	// Input
	float moveValue = app->camSpeed * app->deltaTime;

	if (app->input.keys[K_A] == BUTTON_PRESSED)
	{
		app->camPos.x -= moveValue;
		app->target.x -= moveValue;
	}
	if (app->input.keys[K_Q] == BUTTON_PRESSED)
	{
		app->camPos.y -= moveValue;
		app->target.y -= moveValue;
	}
	if (app->input.keys[K_D] == BUTTON_PRESSED)
	{
		app->camPos.x += moveValue;
		app->target.x += moveValue;
	}
	if (app->input.keys[K_E] == BUTTON_PRESSED)
	{
		app->camPos.y += moveValue;
		app->target.y += moveValue;
	}
	if (app->input.keys[K_W] == BUTTON_PRESSED)
	{
		app->camPos.z -= moveValue;
		app->target.z -= moveValue;
	}
	if (app->input.keys[K_S] == BUTTON_PRESSED)
	{
		app->camPos.z += moveValue;
		app->target.z += moveValue;
	}

	if (app->input.mouseButtons[LEFT] == BUTTON_PRESSED)
		app->MouseMove(app->input.mousePos.x, app->input.mousePos.y);

	if (app->input.mouseButtons[LEFT] == BUTTON_PRESS)
	{
		app->lastX = app->input.mousePos.x;
		app->lastY = app->input.mousePos.y;
	}

	app->iTime += app->deltaTime;
}

void Render(App* app)
{
	const Program& forwardProgram = app->programs[app->renderToBackBufferShader];
	const Program& deferredProgram = app->programs[app->renderToFrameBufferShader];
	const Program& frameBufferProgram = app->programs[app->frameBufferToQuadShader];

	switch (app->mode)
	{
	case Mode_Forward:

		app->UpdateEntityBuffer();

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, app->displaySize.x, app->displaySize.y);

		//glBindFramebuffer(GL_FRAMEBUFFER, app->deferredFrameBuffer.fbHandle);

		//const Program& forwardProgram = app->programs[app->renderToBackBufferShader];
		glUseProgram(forwardProgram.handle);

		app->RenderGeometry(forwardProgram);

		//glBindFramebuffer(GL_FRAMEBUFFER, 0);

		break;
	case Mode_Deferred:

		app->UpdateEntityBuffer();

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, app->displaySize.x, app->displaySize.y);

		glBindFramebuffer(GL_FRAMEBUFFER, app->deferredFrameBuffer.fbHandle);

		glDrawBuffers(app->deferredFrameBuffer.colorAttachment.size(), app->deferredFrameBuffer.colorAttachment.data());

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//const Program& deferredProgram = app->programs[app->renderToFrameBufferShader];
		glUseProgram(deferredProgram.handle);

		app->RenderGeometry(deferredProgram);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, app->displaySize.x, app->displaySize.y);

		//const Program& deferredProgram = app->programs[app->renderToFrameBufferShader];
		glUseProgram(frameBufferProgram.handle);

		glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), app->localUniformBuffer.handle, app->globalParamsOffset, app->globalParamsSize);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[0]);
		glUniform1i(glGetUniformLocation(frameBufferProgram.handle, "uAlbedo"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[1]);
		glUniform1i(glGetUniformLocation(frameBufferProgram.handle, "uNormals"), 1);

		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[2]);
		glUniform1i(glGetUniformLocation(frameBufferProgram.handle, "uPosition"), 2);

		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, app->deferredFrameBuffer.colorAttachment[3]);
		glUniform1i(glGetUniformLocation(frameBufferProgram.handle, "uViewDir"), 3);

		glBindVertexArray(app->vao);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

		glBindVertexArray(0);
		glUseProgram(0);

		break;
	default:
		ELOG(":(");
		break;
	}
}

void App::UpdateEntityBuffer()
{
	float aspectRatio = (float)displaySize.x / (float)displaySize.y;
	float zNear = 0.1f;
	float zFar = 1000.0f;
	mat4 projection = glm::perspective(glm::radians(60.0f), aspectRatio, zNear, zFar);

	//vec3 target = vec3(0.f, 0.f, 0.f);
	//vec3 camPos = 7.0f * vec3(glm::cos(iTime), 0.25f, glm::sin(iTime));	//vec3(5.0, 5.0, 5.0);

	vec3 zCam = glm::normalize(camPos - target);
	vec3 xCam = glm::cross(zCam, vec3(0, 1, 0));
	vec3 yCam = glm::cross(xCam, zCam);

	mat4 view = glm::lookAt(camPos, target, yCam);

	view = glm::rotate(view, glm::radians(rotateX), glm::vec3(1.0f, 0.0f, 0.0f));
	view = glm::rotate(view, glm::radians(rotateY), glm::vec3(0.0f, 1.0f, 0.0f));

	BufferManager::MapBuffer(localUniformBuffer, GL_WRITE_ONLY);

	// LIGHTOO
	globalParamsOffset = localUniformBuffer.head;
	PushVec3(localUniformBuffer, camPos);
	PushUInt(localUniformBuffer, lights.size());

	for (size_t i = 0; i < lights.size(); i++) {
		BufferManager::AlignHead(localUniformBuffer, sizeof(vec4));

		Light& light = lights[i];
		PushUInt(localUniformBuffer, light.type);
		PushVec3(localUniformBuffer, light.color);
		PushVec3(localUniformBuffer, light.direction);
		PushVec3(localUniformBuffer, light.position);
	}
	globalParamsSize = localUniformBuffer.head - globalParamsOffset;

	for (auto it = entities.begin(); it != entities.end(); ++it)
	{
		mat4 za_warudo = it->worldMatrix;
		mat4 WVP = projection * view * za_warudo;

		Buffer& localBuffer = localUniformBuffer;
		BufferManager::AlignHead(localBuffer, uniformBlockAligment);
		it->localParamsOffset = localBuffer.head;
		PushMat4(localBuffer, za_warudo);
		PushMat4(localBuffer, WVP);
		it->localParamsSize = localBuffer.head - it->localParamsOffset;
	}

	BufferManager::UnmapBuffer(localUniformBuffer);
}

void App::ConfigureFrameBuffer(FrameBuffer& aConfigFB)
{
	aConfigFB.colorAttachment.push_back(CreateColorAttachment(false));
	aConfigFB.colorAttachment.push_back(CreateColorAttachment(true));
	aConfigFB.colorAttachment.push_back(CreateColorAttachment(true));
	aConfigFB.colorAttachment.push_back(CreateColorAttachment(true));

	glGenTextures(1, &aConfigFB.depthHandle);
	glBindTexture(GL_TEXTURE_2D, aConfigFB.depthHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, displaySize.x, displaySize.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &aConfigFB.fbHandle);
	glBindFramebuffer(GL_FRAMEBUFFER, aConfigFB.fbHandle);

	std::vector<GLuint> drawBuffers;
	for (size_t i = 0; i < aConfigFB.colorAttachment.size(); ++i)
	{
		GLuint position = GL_COLOR_ATTACHMENT0 + i;
		glFramebufferTexture(GL_FRAMEBUFFER, position, aConfigFB.colorAttachment[i], 0);
		drawBuffers.push_back(position);
	}

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, aConfigFB.depthHandle, 0);

	glDrawBuffers(drawBuffers.size(), drawBuffers.data());

	GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (framebufferStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (framebufferStatus)
		{
		case GL_FRAMEBUFFER_UNDEFINED:
			ELOG("GL_FRAMEBUFFER_UNDEFINED");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			ELOG("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			ELOG("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
			ELOG("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
			ELOG("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			ELOG("GL_FRAMEBUFFER_UNSUPPORTED");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
			ELOG("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
			ELOG("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS");
			break;
		default:
			ELOG("Unknown framebuffer status error");
			break;
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void App::RenderGeometry(const Program& aBindedProgram)
{
	glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(0), localUniformBuffer.handle, globalParamsOffset, globalParamsSize);
	for (auto it = entities.begin(); it != entities.end(); ++it)
	{
		glBindBufferRange(GL_UNIFORM_BUFFER, BINDING(1), localUniformBuffer.handle, it->localParamsOffset, it->localParamsSize);

		Model& model = models[it->modelIndex];
		Mesh& mesh = meshes[model.meshIdx];

		for (u32 i = 0; i < mesh.submeshes.size(); ++i)
		{
			GLuint vao = FindVAO(mesh, i, aBindedProgram);
			glBindVertexArray(vao);

			u32 subMeshMaterialIdx = model.materialIdx[i];
			Material& subMeshMaterial = materials[subMeshMaterialIdx];

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, textures[subMeshMaterial.albedoTextureIdx].handle);
			//glUniform1i(texturedMeshProgram_uTexture, 0); // Se usa una variable q no tiene nada ni sirve para nada???????????????

			SubMesh& subMesh = mesh.submeshes[i];
			glDrawElements(GL_TRIANGLES, subMesh.indices.size(), GL_UNSIGNED_INT, (void*)(u64)subMesh.indexOffset);
		}
	}
}

GLuint App::CreateColorAttachment(const bool isFloatingPoint)
{
	GLuint textureHandle = 0;
	GLenum internalFormat = isFloatingPoint ? GL_RGBA16F : GL_RGBA8;
	GLenum dataType = isFloatingPoint ? GL_FLOAT : GL_UNSIGNED_BYTE;

	glGenTextures(1, &textureHandle);
	glBindTexture(GL_TEXTURE_2D, textureHandle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, displaySize.x, displaySize.y, 0, GL_RGBA, dataType, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	return textureHandle;
}