#define NOMINMAX

#include <windows.h>			
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>

#include "Extension.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"
#include "stb/stb_image.h"
#include "irrKlang.h"
#include "opengl/shader.h"
#include "lib/ThreadPool.h"

#include "Camera.h"
#include "geom.h"
#include "model.h"
#include "enemy.h"
#include "capsule.h"
#include "spritesheet.h"
#include "bullet_store.h"
#include "player_model.h"
#include "enemy_spawner.h"

#define _USE_MATH_DEFINES
#include <math.h>

const int parallelism = 4;

// Viewport
int viewportWidth = 1500;
int viewportHeight = 1000;

// Texture units
const int texUnit_playerDiffuse = 0;
const int texUnit_gunDiffuse = 1;
const int texUnit_floorDiffuse = 2;
const int texUnit_wigglyBoi = 3;
const int texUnit_bullet = 4;
const int texUnit_floorNormal = 5;
const int texUnit_playerNormal = 6;
const int texUnit_gunNormal = 7;
const int texUnit_shadowMap = 8;
const int texUnit_emissionFBO = 9;
const int texUnit_playerEmission = 10;
const int texUnit_gunEmission = 11;
const int texUnit_scene = 12;
const int texUnit_horzBlur = 13;
const int texUnit_vertBlur = 14;
const int texUnit_impactSpriteSheet = 15;
const int texUnit_muzzleFlashSpriteSheet = 16;
const int texUnit_floorSpec = 18;
const int texUnit_playerSpec = 19;
const int texUnit_gunSpec = 20;

// Camera
const glm::vec3 cameraFollowVec(-4.0f, 4.3f, 0.0f);
const glm::vec3 cameraUp(0.0f, 1.0f, 0.0f);

// Player
glm::vec3 playerPosition(0.0f, 0.0f, 0.0f);
glm::vec2 playerMovementDir(0.0f, 0.0f);
float lastFireTime = 0.0f;
bool isTryingToFire = false;
const float fireInterval = 0.1f; // seconds
const int spreadAmount = 20;
const float playerSpeed = 1.5f;
const float playerCollisionRadius = 0.35f;
bool isAlive = true;
float aimTheta = 0.0f;

// Frame timing
float g_deltaTime = 0.0f;
float lastFrame = 0.0f;

// Mouse input
float mouseClipX = 0.0f;
float mouseClipY = 0.0f;

const glm::vec3 muzzlePointLightColor(1.0f, 0.2f, 0.0f);

// Models
const float playerModelScale = 0.0044f;
const float playerModelGunHeight = 120.0f;       // un-scaled
const float playerModelGunMuzzleOffset = 100.0f; // un-scaled
const float monsterY = playerModelScale * playerModelGunHeight;

// Lighting
const glm::vec3 lightDir = glm::normalize(glm::vec3(-0.8f, 0.0f, -1.0f));
const glm::vec3 playerLightDir = glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f));
const float lightFactor = 0.8;
const float nonBlue = 0.9f;
const glm::vec3 lightColor = lightFactor * 1.0f * glm::vec3(nonBlue * 0.406f, nonBlue * 0.723f, 1.0f);
//const glm::vec3 lightColor = lightFactor * 1.0f * glm::vec3(0.406f, 0.723f, 1.0f);
const glm::vec3 ambientColor = lightFactor * 0.10f * glm::vec3(nonBlue * 0.7f, nonBlue * 0.7f, 0.7f);

const float florrLightFactor = 0.35;
const float floorNonBlue = 0.7f;
const glm::vec3 floorLightColor = florrLightFactor * 1.0f * glm::vec3(floorNonBlue * 0.406f, floorNonBlue * 0.723f, 1.0f);
const glm::vec3 floorAmbientColor = florrLightFactor * 0.50f * glm::vec3(floorNonBlue * 0.7f, floorNonBlue * 0.7f, 0.7f);

// Enemies
const float monsterSpeed = 0.6f;

const float pi = (float)M_PI;

const float unitSquare[] = {
	-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
	1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
	-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
	1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
	-1.0f, 1.0f, 0.0f, 0.0f, 1.0f
};

const float moreObnoxiousQuad[] = {
	-1.0f, -1.0f, -0.9f, 0.0f, 0.0f,
	1.0f, -1.0f, -0.9f, 1.0f, 0.0f,
	1.0f, 1.0f, -0.9f, 1.0f, 1.0f,
	-1.0f, -1.0f, -0.9f, 0.0f, 0.0f,
	1.0f, 1.0f, -0.9f, 1.0f, 1.0f,
	-1.0f, 1.0f, -0.9f, 0.0f, 1.0f
};

const float obnoxiousQuad[] = {
	0.5f, 0.5f, -0.9f, 0.0f, 0.0f,
	1.0f, 0.5f, -0.9f, 1.0f, 0.0f,
	1.0f, 1.0f, -0.9f, 1.0f, 1.0f,
	0.5f, 0.5f, -0.9f, 0.0f, 0.0f,
	1.0f, 1.0f, -0.9f, 1.0f, 1.0f,
	0.5f, 1.0f, -0.9f, 0.0f, 1.0f
};

void chasePlayer(const float deltaTime, std::vector<Enemy>* enemies) {
	const glm::vec3 playerCollisionPosition(playerPosition.x, monsterY, playerPosition.z);
	for (int i = 0; i < enemies->size(); ++i) {
		auto& e = (*enemies)[i];
		glm::vec3 dir = playerPosition - e.position;
		dir.y = 0.0f;
		e.dir = glm::normalize(dir);
		e.position += e.dir * deltaTime * monsterSpeed;
		if (isAlive) {
			const glm::vec3 p1 = e.position - e.dir * (ENEMY_COLLIDER.height / 2);
			const glm::vec3 p2 = e.position + e.dir * (ENEMY_COLLIDER.height / 2);
			const float dist = distanceBetweenPointAndLineSegment(
				playerCollisionPosition,
				p1,
				p2);
			if (dist <= (playerCollisionRadius + ENEMY_COLLIDER.radius)) {
				std::cout << "GOTTEM!" << std::endl;
				isAlive = false;
				playerMovementDir = glm::vec2(0.0f, 0.0f);
			}
		}
	}
}

void drawCapsuleBounds(Shader shader, const glm::vec3& center, const glm::vec3& dir, Capsule c) {
	const float h = c.height;
	const float r = c.radius;
	{
		glm::mat4 pointPos = glm::translate(glm::mat4(1.0f), center + (h / 2 + r) * dir);
		glUniformMatrix4fv(glGetUniformLocation(shader.id, "model"), 1, GL_FALSE, glm::value_ptr(pointPos));
		glDrawArrays(GL_POINTS, 0, 1);
	}
	{
		glm::mat4 pointPos = glm::translate(glm::mat4(1.0f), center - (h / 2 + r) * dir);
		glUniformMatrix4fv(glGetUniformLocation(shader.id, "model"), 1, GL_FALSE, glm::value_ptr(pointPos));
		glDrawArrays(GL_POINTS, 0, 1);
	}
	{
		glm::mat4 pointPos = glm::translate(glm::mat4(1.0f), center - (h / 2) * dir);
		glUniformMatrix4fv(glGetUniformLocation(shader.id, "model"), 1, GL_FALSE, glm::value_ptr(pointPos));
		glDrawArrays(GL_POINTS, 0, 1);
	}
	{
		glm::mat4 pointPos = glm::translate(glm::mat4(1.0f), center + (h / 2) * dir);
		glUniformMatrix4fv(glGetUniformLocation(shader.id, "model"), 1, GL_FALSE, glm::value_ptr(pointPos));
		glDrawArrays(GL_POINTS, 0, 1);
	}
}

const float floorSize = 100.0f;
const float tileSize = 1.0f;
const float numTileWraps = floorSize / tileSize;
const float floorVertices[] = {
	// Vertices                             // TexCoord
	-floorSize / 2, 0.0f, -floorSize / 2, 0.0f,         0.0f,
	-floorSize / 2, 0.0f, floorSize / 2,  numTileWraps, 0.0f,
	floorSize / 2,  0.0f, floorSize / 2,  numTileWraps, numTileWraps,
	-floorSize / 2, 0.0f, -floorSize / 2, 0.0f,         0.0f,
	floorSize / 2,  0.0f, floorSize / 2,  numTileWraps, numTileWraps,
	floorSize / 2,  0.0f, -floorSize / 2, 0.0f,         numTileWraps };

void drawWigglyBois(Model& wigglyBoi, Shader& shader, const std::vector<Enemy>& enemies) {
	shader.use();
	shader.setVec3("nosePos", glm::vec3(1.0f, monsterY, -2.0f));
	// TODO optimise (multithread, instancing, SOA, etc..)
	for (const Enemy& e : enemies) {
		float monsterTheta = atan(e.dir.x / e.dir.z) + (e.dir.z < 0.0f ? 0.0f : pi);
		const glm::mat4 modelTransform =
			glm::rotate(
				glm::rotate(
					glm::rotate(
						glm::scale(
							glm::translate(glm::mat4(1.0f), e.position),
							glm::vec3(0.01f)),
						monsterTheta,
						glm::vec3(0.0f, 1.0f, 0.0f)),
					pi,
					glm::vec3(0.0f, 0.0f, 1.0f)),
				glm::radians(90.0f),
				glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 rotOnly = glm::rotate(
			glm::rotate(
				glm::rotate(
					glm::mat4(1.0f),
					monsterTheta,
					glm::vec3(0.0f, 1.0f, 0.0f)),
				pi,
				glm::vec3(0.0f, 0.0f, 1.0f)),
			glm::radians(90.0f),
			glm::vec3(1.0f, 0.0f, 0.0f));

		glUniformMatrix4fv(glGetUniformLocation(shader.id, "aimRot"), 1, GL_FALSE, glm::value_ptr(rotOnly));
		glUniformMatrix4fv(glGetUniformLocation(shader.id, "model"), 1,
			GL_FALSE, glm::value_ptr(modelTransform));
		wigglyBoi.Draw(shader);
	}
}


struct LoadedTexture {
	int width;
	int height;
	int nrComponents;
	int textureUnit;
	unsigned char* data;
};

LoadedTexture loadTexture(const int textureUnit, const std::string& filename) {
	int width, height, nrComponents;
	unsigned char *data =
		stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
	LoadedTexture result;
	result.width = width;
	result.height = height;
	result.nrComponents = nrComponents;
	result.data = data;
	result.textureUnit = textureUnit;
	return result;
}

void bindLoadedTexture(LoadedTexture& texture) {
	unsigned int textureID;
	glGenTextures(1, &textureID);
	int textureUnit = texture.textureUnit;
	int width = texture.width;
	int height = texture.height;
	int nrComponents = texture.nrComponents;
	unsigned char* data = texture.data;
	glActiveTexture(GL_TEXTURE0 + textureUnit);
	if (data) {
		GLenum format;
		if (nrComponents == 1) {
			format = GL_RED;
		}
		else if (nrComponents == 3) {
			format = GL_RGB;
		}
		else if (nrComponents == 4) {
			format = GL_RGBA;
		}

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
			GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}else {
		std::cerr << stbi_failure_reason() << std::endl;
		stbi_image_free(data);
		exit(1);
	}
	glBindTexture(GL_TEXTURE_2D, textureID);
}

void textureFromFile(const int tU, const std::string &filename) {
	LoadedTexture texture = loadTexture(tU, filename);
	bindLoadedTexture(texture);
}
///////////////////////////////////////////////////////

HHOOK handle;
HWND g_hwnd;

LRESULT CALLBACK mouseCallback(int code, WPARAM wParam, LPARAM lParam){
	
	if (code < 0){
		return CallNextHookEx(0, code, wParam, lParam);
	}
	MSG* lpmsg = (MSG*)lParam;	
	if (lpmsg->message == WM_MOUSEMOVE){

		POINT pt;
		GetCursorPos(&pt);

		ScreenToClient(g_hwnd, &pt);
		mouseClipX = -1.0f + 2.0f * static_cast<double>(pt.x) / viewportWidth;
		mouseClipY = 1.0f - 2.0f * static_cast<double>(pt.y) / viewportHeight;	
		isTryingToFire = lpmsg->wParam == MK_LBUTTON;
	}
	
	if (lpmsg->message == WM_LBUTTONDOWN) {
		isTryingToFire = true;
	}

	if (lpmsg->message == WM_LBUTTONUP) {
		isTryingToFire = false;
	}

	return CallNextHookEx(0, code, wParam, lParam);
}

int g_windowWidth = 640;
int g_windowHeight = 480;

POINT g_OldCursorPos;
bool g_enableVerticalSync;


enum DIRECTION {
	DIR_FORWARD = 1,
	DIR_BACKWARD = 2,
	DIR_LEFT = 4,
	DIR_RIGHT = 8,
	DIR_UP = 16,
	DIR_DOWN = 32,

	DIR_FORCE_32BIT = 0x7FFFFFFF
};

Camera camera;
//prototype funktions
LRESULT CALLBACK winProc(HWND hWnd, UINT message, WPARAM wParma, LPARAM lParam);
void setCursortoMiddle(HWND hwnd);
void enableVerticalSync(bool enableVerticalSync);

void initApp(HWND hWnd);
void processInput(HWND hWnd);
// the main windows entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen("CON", "w", stdout);
	SetConsoleTitle("Debug console");

	MoveWindow(GetConsoleWindow(), 1300, 0, 550, 300, true);
	std::cout << "w, a, s, d, mouse : move player" << std::endl;
	std::cout << "mouse             : rotate and shoot" << std::endl;

	WNDCLASSEX		windowClass;		// window class
	HWND			hwnd;				// window handle
	MSG				msg;				// message
	HDC				hdc;				// device context handle

										// fill out the window class structure
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = winProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);		// default icon
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);			// default arrow
	windowClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);	// white background
	windowClass.lpszMenuName = NULL;									// no menu
	windowClass.lpszClassName = "WINDOWCLASS";
	windowClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);			// windows logo small icon

	// register the windows class
	if (!RegisterClassEx(&windowClass))
		return 0;

	// class registered, so now create our window
	hwnd = CreateWindowEx(NULL,									// extended style
		"WINDOWCLASS",						// class name
		"Isometric",						// app name
		WS_OVERLAPPEDWINDOW,
		0, 0,									// x,y coordinate
		viewportWidth,
		viewportHeight,							// width, height
		NULL,									// handle to parent
		NULL,									// handle to menu
		hInstance,							// application instance
		NULL);								// no extra params

											// check if window creation failed (hwnd would equal NULL)
	if (!hwnd)
		return 0;

	ShowWindow(hwnd, SW_SHOW);			// display the window
	UpdateWindow(hwnd);					// update the window
	g_hwnd = hwnd;

	initApp(hwnd);
	handle = SetWindowsHookExA(WH_GETMESSAGE, mouseCallback, NULL, GetCurrentThreadId());

	const auto appStart = std::chrono::high_resolution_clock::now();
	ThreadPool threadPool(parallelism);

	//audio
	irrklang::ISoundEngine* const soundEngine = irrklang::createIrrKlangDevice();
	auto* const fireSound = soundEngine->addSoundSourceFromFile("assets/player/player_shooting_one.wav", irrklang::ESM_AUTO_DETECT, true);
	auto* const ding = soundEngine->addSoundSourceFromFile("assets/bullet/bullet_hit_metal_enemy_4.wav", irrklang::ESM_AUTO_DETECT, true);
	ding->setDefaultVolume(0.5f);

	//framebuffer
	unsigned int obnoxiousQuadVAO;
	glGenVertexArrays(1, &obnoxiousQuadVAO);
	unsigned int obnoxiousQuadVBO;
	glGenBuffers(1, &obnoxiousQuadVBO);
	glBindVertexArray(obnoxiousQuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, obnoxiousQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(obnoxiousQuad), obnoxiousQuad, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	unsigned int unitSquareVAO;
	glGenVertexArrays(1, &unitSquareVAO);
	unsigned int unitSquareVBO;
	glGenBuffers(1, &unitSquareVBO);
	glBindVertexArray(unitSquareVAO);
	glBindBuffer(GL_ARRAY_BUFFER, unitSquareVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(unitSquare), unitSquare, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	unsigned int moreObnoxiousQuadVAO;
	glGenVertexArrays(1, &moreObnoxiousQuadVAO);
	unsigned int moreObnoxiousQuadVBO;
	glGenBuffers(1, &moreObnoxiousQuadVBO);
	glBindVertexArray(moreObnoxiousQuadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, moreObnoxiousQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(moreObnoxiousQuad), moreObnoxiousQuad, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	//assets
	// TODO this is horribly inefficient. Fix this.
	Shader blurShader = Shader::create("basicer_shader.vert", "blur_shader.frag");
	Shader basicerShader = Shader::create("basicer_shader.vert", "basicer_shader.frag");
	Shader sceneDrawShader = Shader::create("basicer_shader.vert", "texture_merge_shader.frag");
	Shader simpleDepthShader = Shader::create("depth_shader.vert", "depth_shader.frag");
	simpleDepthShader.use();
	const unsigned int lsml = glGetUniformLocation(simpleDepthShader.id, "lightSpaceMatrix");
	Shader wigglyShader = Shader::create("wiggly_shader.vert", "player_shader.frag");
	Model wigglyBoi("assets/EelDog/EelDog.FBX", false);

	Shader playerShader = Shader::create("player_shader.vert", "player_shader.frag");
	playerShader.use();
	const unsigned int playerLightSpaceMatrixLocation = glGetUniformLocation(playerShader.id, "lightSpaceMatrix");
	playerShader.setVec3("directionLight.dir", playerLightDir);
	playerShader.setVec3("directionLight.color", lightColor);
	playerShader.setVec3("ambient", ambientColor);
	playerShader.setInt("texture_spec", texUnit_playerSpec);

	const Spritesheet bulletImpactSpritesheet(texUnit_impactSpriteSheet, 11, 0.05f);
	const Spritesheet muzzleFlashImpactSpritesheet(texUnit_muzzleFlashSpriteSheet, 6, 0.05f);
	BulletStore bulletStore = BulletStore::initialiseBuffersAndCreate(&threadPool);

	{
		std::vector<std::pair<int, std::string>> texturesToLoad;
		texturesToLoad.emplace_back(texUnit_impactSpriteSheet, "assets/bullet/impact_spritesheet_with_00.png");
		texturesToLoad.emplace_back(texUnit_muzzleFlashSpriteSheet, "assets/Player/muzzle_spritesheet.png");
		texturesToLoad.emplace_back(texUnit_bullet, "assets/bullet/BulletTexture.png");
		texturesToLoad.emplace_back(texUnit_wigglyBoi, "assets/EelDog/Eeldog_Albedo.png");
		texturesToLoad.emplace_back(texUnit_floorNormal, "assets/Floor_N.psd");
		texturesToLoad.emplace_back(texUnit_floorDiffuse, "assets/Floor_D.psd");
		texturesToLoad.emplace_back(texUnit_floorSpec, "assets/Floor_M.psd");
		texturesToLoad.emplace_back(texUnit_gunNormal, "assets/Player/Textures/Gun_NRM.tga");
		texturesToLoad.emplace_back(texUnit_playerNormal, "assets/Player/Textures/Player_NRM.tga");
		texturesToLoad.emplace_back(texUnit_gunDiffuse, "assets/Player/Textures/Gun_D.tga");
		texturesToLoad.emplace_back(texUnit_playerEmission, "assets/Player/Textures/Player_E.tga");
		texturesToLoad.emplace_back(texUnit_playerSpec, "assets/Player/Textures/Player_M.tga");
		texturesToLoad.emplace_back(texUnit_gunEmission, "assets/Player/Textures/Gun_E.tga");
		texturesToLoad.emplace_back(texUnit_playerDiffuse, "assets/Player/Textures/Player_D.tga");
		texturesToLoad.emplace_back(texUnit_gunSpec, "assets/Player/Textures/Gun_M.tga");

		std::vector<std::thread> threads;
		std::vector<LoadedTexture> loadedTextures(texturesToLoad.size());
		for (int i = 0; i < texturesToLoad.size(); ++i) {
			const auto& textureToLoad = texturesToLoad[i];
			threads.emplace_back([&textureToLoad, i, &loadedTextures]() {
				loadedTextures[i] = loadTexture(textureToLoad.first, textureToLoad.second);
			});
		}
		for (auto& thread : threads) {
			thread.join();
		}
		for (LoadedTexture& l : loadedTextures) {
			bindLoadedTexture(l);
		}
	}

	PlayerModel playerModel("assets/Player/Player.fbx");

	const glm::mat4 projTransform = glm::perspective(
		glm::radians(45.0f), (float)viewportWidth / viewportHeight, 0.1f, 10.0f);
	const glm::mat4 projInv = glm::inverse(projTransform);

	Shader basicTextureShader = Shader::create("basic_texture_shader.vert", "floor_shader.frag");
	basicTextureShader.use();
	basicTextureShader.setVec3("directionLight.dir", lightDir);
	basicTextureShader.setVec3("directionLight.color", floorLightColor);
	basicTextureShader.setVec3("ambient", floorAmbientColor);

	Shader instancedTextureShader = Shader::create("instanced_texture_shader.vert", "basic_texture_shader.frag");
	Shader nodeShader = Shader::create("redshader.vert", "redshader.frag");
	nodeShader.use();

	wigglyShader.use();
	wigglyShader.use();
	wigglyShader.setInt("texture_diffuse", texUnit_wigglyBoi);
	wigglyShader.setVec3("directionLight.dir", playerLightDir);
	wigglyShader.setVec3("directionLight.color", lightColor);
	wigglyShader.setVec3("ambient", ambientColor);

	Shader spriteShader = Shader::create("geom_shader2.vert", "sprite_shader.frag");
	spriteShader.use();

	unsigned int floorVAO;
	glGenVertexArrays(1, &floorVAO);
	unsigned int floorVBO;
	glGenBuffers(1, &floorVBO);
	glBindVertexArray(floorVAO);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(floorVertices), floorVertices,
		GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
		(void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	const float point[] = { 0.0f, 0.0f, 0.0f };
	unsigned int singlePointVAO;
	glGenVertexArrays(1, &singlePointVAO);
	unsigned int singlePointVBO;
	glGenBuffers(1, &singlePointVBO);
	glBindVertexArray(singlePointVAO);
	glBindBuffer(GL_ARRAY_BUFFER, singlePointVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(point), point, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);

	std::vector<Enemy> enemies;
	EnemySpawner enemySpawner(monsterY, &enemies);

	std::vector<SpritesheetSprite> bulletImpactSprites;
	std::vector<float> muzzleFlashSpritesAge;

	glActiveTexture(GL_TEXTURE0 + texUnit_shadowMap);
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	const unsigned int SHADOW_WIDTH = 6 * 1024, SHADOW_HEIGHT = 6 * 1024;
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
		SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	Shader textureShader = Shader::create("geom_shader.vert", "texture_shader.frag");
	glActiveTexture(GL_TEXTURE0 + texUnit_emissionFBO);
	unsigned int emissionFBO;
	glGenFramebuffers(1, &emissionFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, emissionFBO);
	unsigned int emisionBuffer;
	glGenTextures(1, &emisionBuffer);
	glBindTexture(GL_TEXTURE_2D, emisionBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportWidth, viewportHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor2[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor2);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, emisionBuffer, 0);
	{
		unsigned int rbo;
		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, viewportWidth, viewportHeight);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
	}

	glActiveTexture(GL_TEXTURE0 + texUnit_scene);
	unsigned int sceneRenderFBO;
	glGenFramebuffers(1, &sceneRenderFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, sceneRenderFBO);
	unsigned int sceneBuffer;
	glGenTextures(1, &sceneBuffer);
	glBindTexture(GL_TEXTURE_2D, sceneBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportWidth, viewportHeight, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneBuffer, 0);
	{
		unsigned int rbo;
		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewportWidth, viewportHeight);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
	}

	const int blurScale = 2;
	glActiveTexture(GL_TEXTURE0 + texUnit_horzBlur);
	unsigned int horzBlurFBO;
	glGenFramebuffers(1, &horzBlurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, horzBlurFBO);
	unsigned int horzBlurBuffer;
	glGenTextures(1, &horzBlurBuffer);
	glBindTexture(GL_TEXTURE_2D, horzBlurBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportWidth / blurScale, viewportHeight / blurScale, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, horzBlurBuffer, 0);


	glActiveTexture(GL_TEXTURE0 + texUnit_vertBlur);
	unsigned int vertBlurFBO;
	glGenFramebuffers(1, &vertBlurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, vertBlurFBO);
	unsigned int vertBlurBuffer;
	glGenTextures(1, &vertBlurBuffer);
	glBindTexture(GL_TEXTURE_2D, vertBlurBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, viewportWidth / blurScale, viewportHeight / blurScale, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vertBlurBuffer, 0);

	glActiveTexture(GL_TEXTURE0 + texUnit_scene);
	glEnable(GL_CULL_FACE);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);

	std::chrono::steady_clock::time_point frameStart = std::chrono::high_resolution_clock::now();
	std::chrono::steady_clock::time_point frameEnd = frameStart;
	std::chrono::duration<float> deltaTime;
	std::chrono::duration<float> currentFrame;

	int fpsLogCount = 0;
	//const float startTime = glfwGetTime();
	const int framesPerLog = 100;
	int frameMeasurementCount = 0;
	float totalFrameTime = 0.0f;
	while (true) {

		// Did we recieve a message, or are we idling ?
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			// test if this is a quit
			if (msg.message == WM_QUIT) break;
			// translate and dispatch message
			TranslateMessage(&msg);
			DispatchMessage(&msg);

		}else {			
			frameStart = std::chrono::high_resolution_clock::now();
			deltaTime = frameStart - frameEnd;
			g_deltaTime = deltaTime.count();
			frameEnd = frameStart;			
			currentFrame = frameStart - appStart;

			totalFrameTime += deltaTime.count();
			frameMeasurementCount++;
			if (frameMeasurementCount == framesPerLog) {
				//std::cout << "average frame time: " << (totalFrameTime / framesPerLog) << std::endl;
				totalFrameTime = 0.0f;
				frameMeasurementCount = 0;
			}

			if (muzzleFlashSpritesAge.size() > 0) {
				for (int i = 0; i < muzzleFlashSpritesAge.size(); ++i) {
					muzzleFlashSpritesAge[i] += deltaTime.count();
				}
				const float maxAge = muzzleFlashImpactSpritesheet.numCols * muzzleFlashImpactSpritesheet.timePerSprite;
				muzzleFlashSpritesAge.erase(
					std::remove_if(
						muzzleFlashSpritesAge.begin(),
						muzzleFlashSpritesAge.end(),
						[maxAge](const float f) {
					return f >= maxAge;
				}),
					muzzleFlashSpritesAge.end());
			}
			if (bulletImpactSprites.size() > 0) {
				const float spritesheetDur = bulletImpactSpritesheet.numCols * bulletImpactSpritesheet.timePerSprite;
				for (SpritesheetSprite& s : bulletImpactSprites) {
					s.age += deltaTime.count();
				}
				bulletImpactSprites.erase(
					std::remove_if(
						bulletImpactSprites.begin(),
						bulletImpactSprites.end(),
						[spritesheetDur](const SpritesheetSprite& s) {
					return s.age >= spritesheetDur;
				}),
					bulletImpactSprites.end());
				
			}

			{
				const int origEnemyCount = enemies.size();
				bulletStore.updateBullets(deltaTime.count(), &enemies, &bulletImpactSprites);
				if (enemies.size() < origEnemyCount) {
					soundEngine->play2D(ding, false);
				}
			}
			
			if (isAlive) {
				enemySpawner.update(playerPosition, deltaTime.count());
				chasePlayer(deltaTime.count(), &enemies);
				if (!isAlive) {
					playerModel.setPlayerDead(currentFrame.count());
				}
			}
			
			const glm::vec3 cameraPos = playerPosition + cameraFollowVec;
			const glm::mat4 viewTransform =
				glm::lookAt(cameraPos, playerPosition, cameraUp);
			const glm::mat4 PV = projTransform * viewTransform;

			float dx = 0.0f;
			float dz = 0.0f;
			if (isAlive) {
				// TODO extract to helper fn
				// Aiming
				// Map from screen (clip) coords back to world coords for an infinite plane
				// at monsterY.
				// TODO it'd probably be simpler to do a line intersection from camera pos
				// to the plane along the direction vector. Probably faster too (no matrix
				// inverse).
				const glm::mat4 inv = glm::inverse(viewTransform) * projInv;
				const float t = (inv[0][1] * mouseClipX + inv[1][1] * mouseClipY + inv[3][1] - monsterY *
					(inv[0][3] * mouseClipX + inv[1][3] * mouseClipX + inv[3][3])) /
					(inv[2][3] * monsterY - inv[2][1]);
				const float s = 1.0f / (inv[0][3] * mouseClipX + inv[1][3] * mouseClipY + inv[2][3] * t + inv[3][3]);
				const float us = mouseClipX * s;
				const float vs = mouseClipY * s;
				const float ts = t * s;
				const float worldX = inv[0][0] * us + inv[1][0] * vs + inv[2][0] * ts + inv[3][0] * s;
				const float worldZ = inv[0][2] * us + inv[1][2] * vs + inv[2][2] * ts + inv[3][2] * s;

				// Calculate aim rotation.
				dx = worldX - playerPosition.x;
				dz = worldZ - playerPosition.z;
				aimTheta = atan(dx / dz) + (dz < 0.0f ? pi : 0.0f);
				if (abs(mouseClipX) < 0.005f && abs(mouseClipY) < 0.005f) {
					// TODO shortcut earlier once extracted to helper fn
					aimTheta = 0;
				}

				
			}
			glm::vec3 muzzleWorldPos3;
			bool usePointLight = false;

			playerModel.UpdatePointsForAnim(false, playerMovementDir, aimTheta, currentFrame.count());

			glClearColor(1.0, 0.0, 1.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			wigglyShader.use();
			wigglyShader.setFloat("time", currentFrame.count());
			glUniformMatrix4fv(glGetUniformLocation(wigglyShader.id, "PV"), 1, GL_FALSE, glm::value_ptr(PV));

			const glm::mat4 playerModelTransform = glm::rotate(
				glm::scale(glm::translate(glm::mat4(1.0f), playerPosition),
					glm::vec3(playerModelScale)),
				aimTheta, glm::vec3(0.0f, 1.0f, 0.0f));
			// TODO offset for "left-right" muzzle alignment is not the best. Fix aiming
			// trig to account for this.
			const glm::vec3 projectileSpawnPoint =
				playerModelTransform * glm::vec4(-20.0f, playerModelGunHeight,
					playerModelGunMuzzleOffset, 1.0f);
			if (isAlive && isTryingToFire && (lastFireTime + fireInterval) < currentFrame.count()) {
				const auto bulletSpawnStart = std::chrono::high_resolution_clock::now();
				const glm::vec4 midDir = glm::normalize(glm::vec4(dx, 0.0f, dz, 1.0f));
				bulletStore.createBullets(projectileSpawnPoint, midDir, spreadAmount);
				soundEngine->play2D(fireSound, false);
				lastFireTime = currentFrame.count();
				muzzleFlashSpritesAge.emplace_back(0.0f);
				// logTimeSince("       bullet spawn time: ", bulletSpawnStart);
			}

			const auto drawBullets = [&]() {
				glEnable(GL_BLEND);
				glDepthMask(GL_FALSE);
				glActiveTexture(GL_TEXTURE0 + texUnit_bullet);
				instancedTextureShader.use();
				instancedTextureShader.setInt("texture_diffuse", texUnit_bullet);
				instancedTextureShader.setBool("useLight", false);
				glUniformMatrix4fv(glGetUniformLocation(instancedTextureShader.id, "PV"), 1,
					GL_FALSE, glm::value_ptr(PV));
				bulletStore.renderBulletSprites();
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
			};
			const auto drawFloor = [&](const glm::mat4* const lightSpaceMatrixOrNull) { // Floor
				basicTextureShader.use();
				basicTextureShader.setBool("useLight", !!lightSpaceMatrixOrNull);
				basicTextureShader.setBool("useSpec", !!lightSpaceMatrixOrNull);
				basicTextureShader.setInt("texture_diffuse", texUnit_floorDiffuse);
				basicTextureShader.setInt("texture_normal", texUnit_floorNormal);
				basicTextureShader.setInt("texture_spec", texUnit_floorSpec);
				basicTextureShader.setInt("shadow_map", texUnit_shadowMap);
				basicTextureShader.setBool("usePointLight", usePointLight);
				basicTextureShader.setVec3("pointLight.worldPos", muzzleWorldPos3);
				basicTextureShader.setVec3("pointLight.color", muzzlePointLightColor);
				basicTextureShader.setVec3("viewPos", cameraPos);
				if (lightSpaceMatrixOrNull) {
					glUniformMatrix4fv(glGetUniformLocation(basicTextureShader.id, "lightSpaceMatrix"),
						1, GL_FALSE, glm::value_ptr(*lightSpaceMatrixOrNull));
				}
				glUniformMatrix4fv(glGetUniformLocation(basicTextureShader.id, "model"),
					1, GL_FALSE, glm::value_ptr(glm::rotate(
						glm::mat4(1.0f),
						glm::radians(45.0f),
						glm::vec3(0.0f, 1.0f, 0.0f))));
				glUniformMatrix4fv(glGetUniformLocation(basicTextureShader.id, "PV"), 1,
					GL_FALSE, glm::value_ptr(PV));
				glBindVertexArray(floorVAO);
				glDrawArrays(GL_TRIANGLES, 0, 6);
				basicTextureShader.setBool("useLight", false);
				basicTextureShader.setBool("useSpec", false);
			};

			playerShader.use();
			glm::mat4 lightSpaceMatrix(1.0f);
			playerShader.setVec3("viewPos", cameraPos);
			{ // Draw player
				playerShader.setBool("useLight", true);
				glUniformMatrix4fv(glGetUniformLocation(playerShader.id, "model"), 1,
					GL_FALSE, glm::value_ptr(playerModelTransform));
				glUniformMatrix4fv(glGetUniformLocation(playerShader.id, "aimRot"), 1,
					GL_FALSE, glm::value_ptr(glm::rotate(
						glm::mat4(1.0f),
						aimTheta,
						glm::vec3(0.0f, 1.0f, 0.0f))));
				glUniformMatrix4fv(glGetUniformLocation(playerShader.id, "PV"), 1,
					GL_FALSE, glm::value_ptr(PV));

				{  // Get the player shadow.
					const float nearPlane = 1.0f;
					const float farPlane = 50.0f;
					const float orthoSize = 10.0f;
					const glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
					const glm::mat4 lightView = glm::lookAt(
						playerPosition - 20.0f * playerLightDir,
						playerPosition,
						glm::vec3(0.0f, 1.0f, 0.0f));
					lightSpaceMatrix = lightProj * lightView;
					simpleDepthShader.use();
					glUniformMatrix4fv(lsml, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
					glUniformMatrix4fv(glGetUniformLocation(simpleDepthShader.id, "model"), 1,
						GL_FALSE, glm::value_ptr(playerModelTransform));
					glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
					glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
					glClear(GL_DEPTH_BUFFER_BIT);
					playerModel.Draw(simpleDepthShader, false);
					wigglyShader.use();
					glUniformMatrix4fv(glGetUniformLocation(wigglyShader.id, "PV"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
					drawWigglyBois(wigglyBoi, wigglyShader, enemies);

					glBindFramebuffer(GL_FRAMEBUFFER, sceneRenderFBO);
					glViewport(0, 0, viewportWidth, viewportHeight);

					glUniformMatrix4fv(glGetUniformLocation(wigglyShader.id, "PV"), 1, GL_FALSE, glm::value_ptr(PV));

					// Player emission
					glBindFramebuffer(GL_FRAMEBUFFER, emissionFBO);
					glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					glEnable(GL_DEPTH_TEST);
					textureShader.use();
					glUniformMatrix4fv(glGetUniformLocation(textureShader.id, "PV"), 1, GL_FALSE, glm::value_ptr(PV));
					glUniformMatrix4fv(glGetUniformLocation(textureShader.id, "model"), 1, GL_FALSE, glm::value_ptr(playerModelTransform));
					textureShader.setInt("tex", texUnit_playerEmission);
					playerModel.meshes[0].Draw(textureShader);
					textureShader.setInt("tex", texUnit_gunEmission);
					playerModel.meshes[1].Draw(textureShader);
					glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
					drawFloor(nullptr);
					glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
					drawBullets();
					glBindFramebuffer(GL_FRAMEBUFFER, sceneRenderFBO);
					glClearColor(0.1f, 0.1f, 0.1f, 1.0f);


					playerShader.use();
					glUniformMatrix4fv(playerLightSpaceMatrixLocation, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
					playerShader.setInt("shadow_map", texUnit_shadowMap);
				}
			}
			
			glm::mat4 muzzleTransform(1.0f);
			if (muzzleFlashSpritesAge.size() != 0) {
				// Muzzle pos calc

				// Position in original model of gun muzzle
				const glm::vec3 pointVec(197.0f, 76.143f, -3.054f);
				// Adjust for animation
				const glm::mat4 T = glm::translate(playerModel.getAnimatedGunTransform(), pointVec);
				// Adjust for player
				muzzleTransform = playerModelTransform * T;

				const glm::vec4 muzzleWorldPos = muzzleTransform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
				muzzleWorldPos3 = glm::vec3(
					muzzleWorldPos.x / muzzleWorldPos.w,
					muzzleWorldPos.y / muzzleWorldPos.w,
					muzzleWorldPos.z / muzzleWorldPos.w);

				float minAge = 1000.0f;
				for (const float a : muzzleFlashSpritesAge) {
					minAge = std::min(a, minAge);
				}
				usePointLight = minAge < 0.03f;
				playerShader.setBool("usePointLight", usePointLight);
				playerShader.setVec3("pointLight.worldPos", muzzleWorldPos3);
				playerShader.setVec3("pointLight.color", muzzlePointLightColor);
			}
			else {
				usePointLight = false;
				playerShader.setBool("usePointLight", false);
			}

			playerModel.Draw(playerShader);
			playerShader.setBool("useLight", false);

			drawFloor(&lightSpaceMatrix);
			if (muzzleFlashSpritesAge.size() != 0) {
				// Muzzle flash(es)
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				spriteShader.use();
				glUniformMatrix4fv(glGetUniformLocation(spriteShader.id, "PV"), 1, GL_FALSE,
					glm::value_ptr(PV));
				glBindVertexArray(unitSquareVAO);
				spriteShader.setInt("numCols", muzzleFlashImpactSpritesheet.numCols);
				spriteShader.setInt("spritesheet", muzzleFlashImpactSpritesheet.textureUnit);
				spriteShader.setFloat("timePerSprite", muzzleFlashImpactSpritesheet.timePerSprite);
				const float scale = 50.0f;
				glm::mat4 model = glm::scale(muzzleTransform, glm::vec3(scale, scale, scale));
				model = glm::rotate(model, glm::radians(0.0f), glm::vec3(0.0, 1.0, 0.0));
				model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));
				model = glm::translate(model, glm::vec3(0.7f, 0.0f, 0.0f));
				const glm::vec4 thingo = model * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
				const float yRot = acos(thingo.y);
				const float t = aimTheta >= 0.0f ? aimTheta : aimTheta + 2.0f * pi;
				const float bbRad = 0.5f;
				const float bb =
					(aimTheta >= 0.0f && aimTheta <= pi)
					? (bbRad - 2.0f * bbRad * t / pi)
					: (-3.0f * bbRad + 2.0f * bbRad * t / pi);
				model = glm::rotate(model, bb - yRot + 0.94f, glm::vec3(1.0f, 0.0f, 0.0f));
				glUniformMatrix4fv(glGetUniformLocation(spriteShader.id, "model"), 1, GL_FALSE,
					glm::value_ptr(model));
				for (const float spriteAge : muzzleFlashSpritesAge) {
					spriteShader.setFloat("age", spriteAge);
					glDrawArrays(GL_TRIANGLES, 0, 6);
				}
				glDisable(GL_BLEND);
				glDepthMask(GL_TRUE);
				
			}

			
			wigglyShader.use();
			wigglyShader.setBool("useLight", true);
			drawWigglyBois(wigglyBoi, wigglyShader, enemies);

			

			{  // Bullet impact sprites
				glEnable(GL_BLEND);
				spriteShader.use();
				glUniformMatrix4fv(glGetUniformLocation(spriteShader.id, "PV"), 1, GL_FALSE,
					glm::value_ptr(PV));
				glBindVertexArray(unitSquareVAO);
				spriteShader.setInt("numCols", bulletImpactSpritesheet.numCols);
				spriteShader.setInt("spritesheet", bulletImpactSpritesheet.textureUnit);
				spriteShader.setFloat("timePerSprite", bulletImpactSpritesheet.timePerSprite);
				const float scale = 0.25f;
				for (const auto& sprite : bulletImpactSprites) {
					glm::mat4 model = glm::translate(
						glm::mat4(1.0f),
						sprite.worldPos);
					// Billboarding
					for (int i = 0; i < 3; i++) {
						for (int j = 0; j < 3; j++) {
							model[i][j] = viewTransform[j][i];
						}
					}
					model = glm::scale(model, glm::vec3(scale, scale, scale));

					spriteShader.setFloat("age", sprite.age);
					glUniformMatrix4fv(glGetUniformLocation(spriteShader.id, "model"), 1, GL_FALSE,
						glm::value_ptr(model));

					glDrawArrays(GL_TRIANGLES, 0, 6);
				}
				glDisable(GL_BLEND);
				
			}

			glDisable(GL_DEPTH_TEST);
			glEnable(GL_DEPTH_TEST);

			glDisable(GL_DEPTH_TEST);
			glBindFramebuffer(GL_FRAMEBUFFER, horzBlurFBO);
			glViewport(0, 0, viewportWidth / blurScale, viewportHeight / blurScale);
			glBindVertexArray(moreObnoxiousQuadVAO);
			blurShader.use();
			blurShader.setInt("image", texUnit_emissionFBO);
			blurShader.setInt("horizontal", true);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glBindFramebuffer(GL_FRAMEBUFFER, vertBlurFBO);
			glBindVertexArray(moreObnoxiousQuadVAO);
			blurShader.use();
			blurShader.setInt("image", texUnit_horzBlur);
			blurShader.setInt("horizontal", false);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			glViewport(0, 0, viewportWidth, viewportHeight);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			sceneDrawShader.use();
			glBindVertexArray(moreObnoxiousQuadVAO);
			sceneDrawShader.setInt("base_texture", texUnit_scene);
			sceneDrawShader.setInt("emission_texture", texUnit_vertBlur);
			sceneDrawShader.setInt("bright_texture", texUnit_emissionFBO);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			glEnable(GL_DEPTH_TEST);

			glBindFramebuffer(GL_FRAMEBUFFER, sceneRenderFBO);
			
			processInput(hwnd);
			hdc = GetDC(hwnd);
			SwapBuffers(hdc);
			ReleaseDC(hwnd, hdc);
		}
	} // end while

	UnhookWindowsHookEx(handle);

	return msg.wParam;
}

// the Windows Procedure event handler
LRESULT CALLBACK winProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

	static HGLRC hRC;					// rendering context
	static HDC hDC;						// device context
	POINT pt;
	RECT rect;

	switch (message) {

	case WM_DESTROY: {

		PostQuitMessage(0);
		return 0;
	}

	case WM_CREATE: {		
		return 0;
	}break;

	case WM_LBUTTONDOWN: {		
		return 0;
	} break;

	case WM_KEYDOWN: {

		switch (wParam) {

		case VK_ESCAPE: {

			PostQuitMessage(0);
			return 0;

		}break;
		case VK_SPACE: {

			ReleaseCapture();
			return 0;
		}break;
		case 'v': case 'V': {
			enableVerticalSync(!g_enableVerticalSync);
			return 0;
		}break;		

			return 0;
		}break;

		return 0;
	}break;

	case WM_SIZE: {

		int _height = HIWORD(lParam);		// retrieve width and height
		int _width = LOWORD(lParam);

		if (_height == 0) {					// avoid divide by zero
			_height = 1;
		}

		glViewport(0, 0, _width, _height);
		camera.perspective(45.0f, static_cast<float>(_width) / static_cast<float>(_height), 1.0f, 2000.0f);

		return 0;
	}break;

	default:
		break;
	}
	return (DefWindowProc(hWnd, message, wParam, lParam));
}

void initApp(HWND hWnd) {

	static HGLRC hRC;					// rendering context
	static HDC hDC;						// device context

	hDC = GetDC(hWnd);
	int nPixelFormat;					// our pixel format index

	static PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),	// size of structure
		1,								// default version
		PFD_DRAW_TO_WINDOW |			// window drawing support
		PFD_SUPPORT_OPENGL |			// OpenGL support
		PFD_DOUBLEBUFFER,				// double buffering support
		PFD_TYPE_RGBA,					// RGBA color mode
		32,								// 32 bit color mode
		0, 0, 0, 0, 0, 0,				// ignore color bits, non-palettized mode
		0,								// no alpha buffer
		0,								// ignore shift bit
		0,								// no accumulation buffer
		0, 0, 0, 0,						// ignore accumulation bits
		16,								// 16 bit z-buffer size
		0,								// no stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main drawing plane
		0,								// reserved
		0, 0, 0 };						// layer masks ignored

	nPixelFormat = ChoosePixelFormat(hDC, &pfd);	// choose best matching pixel format
	SetPixelFormat(hDC, nPixelFormat, &pfd);		// set pixel format to device context


													// create rendering context and make it current
	hRC = wglCreateContext(hDC);
	wglMakeCurrent(hDC, hRC);
	enableVerticalSync(true);

	
	camera = Camera(Vector3f(0.0f, 20.0f, 220.f), Vector3f(0.0f, 20.0f, 0.0f), Vector3f(0.0f, 1.0f, 0.0f));
	camera.perspective(45.0f, static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight), 1.0f, 2000.0f);	
}

void setCursortoMiddle(HWND hwnd) {
	RECT rect;

	GetClientRect(hwnd, &rect);
	SetCursorPos(rect.right / 2, rect.bottom / 2);
}

void enableVerticalSync(bool enableVerticalSync) {

	// WGL_EXT_swap_control.
	typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC)(GLint);

	static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
		reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(
			wglGetProcAddress("wglSwapIntervalEXT"));

	if (wglSwapIntervalEXT) {
		wglSwapIntervalEXT(enableVerticalSync ? 1 : 0);
		g_enableVerticalSync = enableVerticalSync;
	}
}

void processInput(HWND hWnd) {
	
	static UCHAR pKeyBuffer[256];
	ULONG        Direction = 0;
	POINT        CursorPos;
	float        X = 0.0f, Y = 0.0f;

	// Retrieve keyboard state
	if (!GetKeyboardState(pKeyBuffer)) return;

	// Check the relevant keys
	if (pKeyBuffer['W'] & 0xF0) Direction |= DIR_FORWARD;
	if (pKeyBuffer['S'] & 0xF0) Direction |= DIR_BACKWARD;
	if (pKeyBuffer['A'] & 0xF0) Direction |= DIR_LEFT;
	if (pKeyBuffer['D'] & 0xF0) Direction |= DIR_RIGHT;


	// Rotate camera
	if (X || Y) {
		camera.rotate(X, Y, 0.0f);

	} // End if any rotation
			
	if (isAlive) {
		glm::vec3 dirVec(0.0f);				
		if (Direction & DIR_FORWARD) {					
			dirVec += glm::vec3(1.0f, 0.0f, 0.0f);
		}
		if (Direction & DIR_BACKWARD) {
		dirVec += glm::vec3(-1.0f, 0.0f, 0.0f);
		}
		if (Direction & DIR_LEFT) {
		dirVec += glm::vec3(0.0f, 0.0f, -1.0f);
		}
		if (Direction & DIR_RIGHT) {
		dirVec += glm::vec3(0.0f, 0.0f, 1.0f);
		}			
		if (glm::length2(dirVec) > 0.01f) {				
		playerPosition += glm::normalize(dirVec) * playerSpeed * g_deltaTime;
		}
		playerMovementDir = glm::vec2(dirVec.x, dirVec.z);
	}
}


