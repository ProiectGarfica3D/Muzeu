#include <stdlib.h> // necesare pentru citirea shaderStencilTesting-elor
#include <stdio.h>
#include <math.h> 

#include <GL/glew.h>

#define GLM_FORCE_CTOR_INIT 
#include <GLM.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include <glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "OBJ_Loader.h"
#include <vector>

#pragma comment (lib, "glfw3dll.lib")
#pragma comment (lib, "glew32.lib")
#pragma comment (lib, "OpenGL32.lib")

// settings
object::Loader Loader;
const unsigned int SCR_WIDTH = 1600;
const unsigned int SCR_HEIGHT = 1000;



enum ECameraMovementType
{	
	UNKNOWN,
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT,
	UP,
	DOWN
};

class Camera
{
private:
	// Default camera values
	const float zNEAR = 0.1f;
	const float zFAR = 500.f;
	const float YAW = -90.0f;
	const float PITCH = 0.0f;
	const float FOV = 45.0f;
	glm::vec3 startPosition;

public:
	Camera(const int width, const int height, const glm::vec3& position)
	{
		startPosition = position;
		Set(width, height, position);
	}

	void Set(const int width, const int height, const glm::vec3& position)
	{
		this->isPerspective = true;
		this->yaw = YAW;
		this->pitch = PITCH;

		this->FoVy = FOV;
		this->width = width;
		this->height = height;
		this->zNear = zNEAR;
		this->zFar = zFAR;

		this->worldUp = glm::vec3(0, 1, 0);
		this->position = position;

		lastX = width / 2.0f;
		lastY = height / 2.0f;
		bFirstMouseMove = true;

		UpdateCameraVectors();
	}

	void Reset(const int width, const int height)
	{
		Set(width, height, startPosition);
	}

	void Reshape(int windowWidth, int windowHeight)
	{
		width = windowWidth;
		height = windowHeight;

		// define the viewport transformation
		glViewport(0, 0, windowWidth, windowHeight);
	}

	const glm::vec3 GetPosition() const
	{
		return position;
	}

	const glm::mat4 GetViewMatrix() const
	{
		// Returns the View Matrix
		return glm::lookAt(position, position + forward, up);
	}

	const glm::mat4 GetProjectionMatrix() const
	{
		glm::mat4 Proj = glm::mat4(1);
		if (isPerspective) {
			float aspectRatio = ((float)(width)) / height;
			Proj = glm::perspective(glm::radians(FoVy), aspectRatio, zNear, zFar);
		}
		else {
			float scaleFactor = 2000.f;
			Proj = glm::ortho<float>(
				-width / scaleFactor, width / scaleFactor,
				-height / scaleFactor, height / scaleFactor, -zFar, zFar);
		}
		return Proj;
	}

	void ProcessKeyboard(ECameraMovementType direction, float deltaTime)
	{
		float velocity = (float)(cameraSpeedFactor * deltaTime);
		switch (direction) {
		case ECameraMovementType::FORWARD:
			position += forward * velocity;
			break;
		case ECameraMovementType::BACKWARD:
			position -= forward * velocity;
			break;
		case ECameraMovementType::LEFT:
			position -= right * velocity;
			break;
		case ECameraMovementType::RIGHT:
			position += right * velocity;
			break;
		case ECameraMovementType::UP:
			position += up * velocity;
			break;
		case ECameraMovementType::DOWN:
			position -= up * velocity;
			break;
		}
	}

	void MouseControl(float xPos, float yPos)
	{
		if (bFirstMouseMove) {
			lastX = xPos;
			lastY = yPos;
			bFirstMouseMove = false;
		}

		float xChange = xPos - lastX;
		float yChange = lastY - yPos;
		lastX = xPos;
		lastY = yPos;

		if (fabs(xChange) <= 1e-6 && fabs(yChange) <= 1e-6) {
			return;
		}
		xChange *= mouseSensitivity;
		yChange *= mouseSensitivity;

		ProcessMouseMovement(xChange, yChange);
	}

	void ProcessMouseScroll(float yOffset)
	{
		if (FoVy >= 1.0f && FoVy <= 90.0f) {
			FoVy -= yOffset;
		}
		if (FoVy <= 1.0f)
			FoVy = 1.0f;
		if (FoVy >= 90.0f)
			FoVy = 90.0f;
	}

private:
	void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true)
	{
		yaw += xOffset;
		pitch += yOffset;

		//std::cout << "yaw = " << yaw << std::endl;
		//std::cout << "pitch = " << pitch << std::endl;

		// Avem grijã sã nu ne dãm peste cap
		if (constrainPitch) {
			if (pitch > 89.0f)
				pitch = 89.0f;
			if (pitch < -89.0f)
				pitch = -89.0f;
		}

		// Se modificã vectorii camerei pe baza unghiurilor Euler
		UpdateCameraVectors();
	}

	void UpdateCameraVectors()
	{
		// Calculate the new forward vector
		this->forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		this->forward.y = sin(glm::radians(pitch));
		this->forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
		this->forward = glm::normalize(this->forward);
		// Also re-calculate the Right and Up vector
		right = glm::normalize(glm::cross(forward, worldUp));  // Normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
		up = glm::normalize(glm::cross(right, forward));
	}

protected:
	const float cameraSpeedFactor = 2.5f;
	const float mouseSensitivity = 0.1f;

	// Perspective properties
	float zNear;
	float zFar;
	float FoVy;
	int width;
	int height;
	bool isPerspective;

	glm::vec3 position;
	glm::vec3 forward;
	glm::vec3 right;
	glm::vec3 up;
	glm::vec3 worldUp;

	// Euler Angles
	float yaw;
	float pitch;

	bool bFirstMouseMove = true;
	float lastX = 0.f, lastY = 0.f;
};

class Shader
{
public:
	// constructor generates the shaderStencilTesting on the fly
	// ------------------------------------------------------------------------
	Shader(const char* vertexPath, const char* fragmentPath)
	{
		Init(vertexPath, fragmentPath);
	}

	~Shader()
	{
		glDeleteProgram(ID);
	}

	// activate the shaderStencilTesting
	// ------------------------------------------------------------------------
	void Use() const
	{
		glUseProgram(ID);
	}

	unsigned int GetID() const { return ID; }

	// MVP
	unsigned int loc_model_matrix;
	unsigned int loc_view_matrix;
	unsigned int loc_projection_matrix;

	// utility uniform functions
	void SetInt(const std::string& name, int value) const
	{
		glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
	}
	void SetFloat(const std::string& name, float value) const
	{
		glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
	}
	void SetVec3(const std::string& name, const glm::vec3& value) const
	{
		glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
	}
	void SetVec3(const std::string& name, float x, float y, float z) const
	{
		glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
	}
	void SetMat4(const std::string& name, const glm::mat4& mat) const
	{
		glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
	}

private:
	void Init(const char* vertexPath, const char* fragmentPath)
	{
		// 1. retrieve the vertex/fragment source code from filePath
		std::string vertexCode;
		std::string fragmentCode;
		std::ifstream vShaderFile;
		std::ifstream fShaderFile;
		// ensure ifstream objects can throw exceptions:
		vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try {
			// open files
			vShaderFile.open(vertexPath);
			fShaderFile.open(fragmentPath);
			std::stringstream vShaderStream, fShaderStream;
			// read file's buffer contents into streams
			vShaderStream << vShaderFile.rdbuf();
			fShaderStream << fShaderFile.rdbuf();
			// close file handlers
			vShaderFile.close();
			fShaderFile.close();
			// convert stream into string
			vertexCode = vShaderStream.str();
			fragmentCode = fShaderStream.str();
		}
		catch (std::ifstream::failure e) {
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
		}
		const char* vShaderCode = vertexCode.c_str();
		const char* fShaderCode = fragmentCode.c_str();

		// 2. compile shaders
		unsigned int vertex, fragment;
		// vertex shaderStencilTesting
		vertex = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex, 1, &vShaderCode, NULL);
		glCompileShader(vertex);
		CheckCompileErrors(vertex, "VERTEX");
		// fragment Shader
		fragment = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment, 1, &fShaderCode, NULL);
		glCompileShader(fragment);
		CheckCompileErrors(fragment, "FRAGMENT");
		// shaderStencilTesting Program
		ID = glCreateProgram();
		glAttachShader(ID, vertex);
		glAttachShader(ID, fragment);
		glLinkProgram(ID);
		CheckCompileErrors(ID, "PROGRAM");

		// 3. delete the shaders as they're linked into our program now and no longer necessery
		glDeleteShader(vertex);
		glDeleteShader(fragment);
	}

	// utility function for checking shaderStencilTesting compilation/linking errors.
	// ------------------------------------------------------------------------
	void CheckCompileErrors(unsigned int shaderStencilTesting, std::string type)
	{
		GLint success;
		GLchar infoLog[1024];
		if (type != "PROGRAM") {
			glGetShaderiv(shaderStencilTesting, GL_COMPILE_STATUS, &success);
			if (!success) {
				glGetShaderInfoLog(shaderStencilTesting, 1024, NULL, infoLog);
				std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
		else {
			glGetProgramiv(shaderStencilTesting, GL_LINK_STATUS, &success);
			if (!success) {
				glGetProgramInfoLog(shaderStencilTesting, 1024, NULL, infoLog);
				std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
			}
		}
	}
private:
	unsigned int ID;
};

Camera* pCamera = nullptr;

unsigned int CreateTexture(const std::string& strTexturePath)
{
	unsigned int textureId = -1;

	// load image, create texture and generate mipmaps
	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
	unsigned char* data = stbi_load(strTexturePath.c_str(), &width, &height, &nrChannels, 0);
	if (data) {
		GLenum format;
		if (nrChannels == 1)
			format = GL_RED;
		else if (nrChannels == 3)
			format = GL_RGB;
		else if (nrChannels == 4)
			format = GL_RGBA;

		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else {
		std::cout << "Failed to load texture: " << strTexturePath << std::endl;
	}
	stbi_image_free(data);

	return textureId;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void renderScene(const Shader& shader);

void upperDino();
void renderCube();
void renderDodoBird(const Shader& shader);
void renderDino(const Shader& shader);
void bottomDino();
void renderFloor(); 
void renderWall(const Shader& shader);
void dodoBird();
void diamond();
void renderDiamond(const Shader& shader);



void Room();
void Room1();
void Room2();
void Room3();
void Room4();
void Room5();

double deltaTime = 0.0f;    
double lastFrame = 0.0f;

bool rotateLight = false; 
GLuint VAO, VBO, EBO;
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	float vertices[] = {
-1, -1, 1,0, 0, 1,0.375, 0,
1, -1, 1,0, 0, 1,0.625, 0,
1, 1, 1,0, 0, 1,0.625, 0.25,
-1, 1, 1,0, 0, 1,0.375, 0.25,
-1, 1, 1,0, 1, 0,0.375, 0.25,
1, 1, 1,0, 1, 0,0.625, 0.25,
1, 1, -1,0, 1, 0,0.625, 0.5,
-1, 1, -1,0, 1, 0,0.375, 0.5,
-1, 1, -1,0, 0, -1,0.375, 0.5,
1, 1, -1,0, 0, -1,0.625, 0.5,
1, -1, -1,0, 0, -1,0.625, 0.75,
-1, -1, -1,0, 0, -1,0.375, 0.75,
-1, -1, -1,0, -1, 0,0.375, 0.75,
1, -1, -1,0, -1, 0,0.625, 0.75,
1, -1, 1,0, -1, 0,0.625, 1,
-1, -1, 1,0, -1, 0,0.375, 1,
1, -1, 1,1, 0, 0,0.625, 0,
1, -1, -1,1, 0, 0,0.875, 0,
1, 1, -1,1, 0, 0,0.875, 0.25,
1, 1, 1,1, 0, 0,0.625, 0.25,
-1, -1, -1,-1, 0, 0,0.125, 0,
-1, -1, 1,-1, 0, 0,0.375, 0,
-1, 1, 1,-1, 0, 0,0.375, 0.25,
-1, 1, -1,-1, 0, 0,0.125, 0.25
	};

	unsigned int indices[] = {
0, 1, 3,
1, 2, 3,
4, 5, 7,
5, 6, 7,
8, 9, 11,
9, 10, 11,
12, 13, 15,
13, 14, 15,
16, 17, 19,
17, 18, 19,
 20, 21, 23,
 21, 22, 23
	};


	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// texture coord attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
}



void VBOcreation()
{
	float vertices[] = {
-1, -1, 1,0, 0, 1,0.375, 0,
1, -1, 1,0, 0, 1,0.625, 0,
1, 1, 1,0, 0, 1,0.625, 0.25,
-1, 1, 1,0, 0, 1,0.375, 0.25,
-1, 1, 1,0, 1, 0,0.375, 0.25,
1, 1, 1,0, 1, 0,0.625, 0.25,
1, 1, -1,0, 1, 0,0.625, 0.5,
-1, 1, -1,0, 1, 0,0.375, 0.5,
-1, 1, -1,0, 0, -1,0.375, 0.5,
1, 1, -1,0, 0, -1,0.625, 0.5,
1, -1, -1,0, 0, -1,0.625, 0.75,
-1, -1, -1,0, 0, -1,0.375, 0.75,
-1, -1, -1,0, -1, 0,0.375, 0.75,
1, -1, -1,0, -1, 0,0.625, 0.75,
1, -1, 1,0, -1, 0,0.625, 1,
-1, -1, 1,0, -1, 0,0.375, 1,
1, -1, 1,1, 0, 0,0.625, 0,
1, -1, -1,1, 0, 0,0.875, 0,
1, 1, -1,1, 0, 0,0.875, 0.25,
1, 1, 1,1, 0, 0,0.625, 0.25,
-1, -1, -1,-1, 0, 0,0.125, 0,
-1, -1, 1,-1, 0, 0,0.375, 0,
-1, 1, 1,-1, 0, 0,0.375, 0.25,
-1, 1, -1,-1, 0, 0,0.125, 0.25
	};

	unsigned int indices[] = {
0, 1, 3,
1, 2, 3,
4, 5, 7,
5, 6, 7,
8, 9, 11,
9, 10, 11,
12, 13, 15,
13, 14, 15,
16, 17, 19,
17, 18, 19,
 20, 21, 23,
 21, 22, 23
	};


	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// color attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// texture coord attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);
}

void main(int argc, char** argv)
{
	std::string strFullExeFileName = argv[0];
	std::string strExePath;
	const size_t last_slash_idx = strFullExeFileName.rfind('\\');
	if (std::string::npos != last_slash_idx) {
		strExePath = strFullExeFileName.substr(0, last_slash_idx);
	}

	// glfw: initialize and configure
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// glfw window creation
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Museum trip", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		
	}

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetKeyCallback(window, key_callback);

	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glewInit();

	// Create camera
	pCamera = new Camera(SCR_WIDTH, SCR_HEIGHT, glm::vec3(0.0, 1.0, 3.0));

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);

	// build and compile shaders
	// -------------------------
	Shader shadowMappingShader("ShadowMapping.vs", "ShadowMapping.fs");
	Shader shadowMappingDepthShader("ShadowMappingDepth.vs", "ShadowMappingDepth.fs");

	// load textures
	// -------------
	unsigned int dinosaurTexture = CreateTexture(strExePath + "\\dinosaur.jpg");
	unsigned int floorTexture = CreateTexture(strExePath + "\\ColoredFloor.png");
	unsigned int wallTexture = CreateTexture(strExePath + "\\Wall.jpg");
	unsigned int dodoBirdtexture = CreateTexture(strExePath + "\\dinosaur.jpg");

	// configure depth map FBO
	// -----------------------
	const unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;
	unsigned int depthMapFBO;
	glGenFramebuffers(1, &depthMapFBO);
	// create depth texture
	unsigned int depthMap;
	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	// attach depth texture as FBO's depth buffer
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	// shader configuration
	// --------------------
	shadowMappingShader.Use();
	shadowMappingShader.SetInt("diffuseTexture", 0);
	shadowMappingShader.SetInt("shadowMap", 1);

	// lighting info
	// -------------
	glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);
	float lightFrame = 0.0f;

	glEnable(GL_CULL_FACE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = (float)glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);

		if (rotateLight)
		{
			lightFrame += 0.01f;
			lightPos.x = cos(lightFrame) * 2.0f;
			lightPos.z = sin(lightFrame) * 2.0f;
		}

		// render
		// ------
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 1. render depth of scene to texture (from light's perspective)
		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;
		float near_plane = 1.0f, far_plane = 7.5f;
		lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
		lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
		lightSpaceMatrix = lightProjection * lightView;

		// render scene from light's point of view
		shadowMappingDepthShader.Use();
		shadowMappingDepthShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderScene(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderDiamond(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);


		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, wallTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderWall(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, dinosaurTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderDodoBird(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, dinosaurTexture);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
		renderDino(shadowMappingDepthShader);
		glCullFace(GL_BACK);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);


		// reset viewport
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 2. render scene as normal using the generated depth/shadow map 
		glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		shadowMappingShader.Use();
		glm::mat4 projection = pCamera->GetProjectionMatrix();
		glm::mat4 view = pCamera->GetViewMatrix();
		shadowMappingShader.SetMat4("projection", projection);
		shadowMappingShader.SetMat4("view", view);
		// set light uniforms
		shadowMappingShader.SetVec3("viewPos", pCamera->GetPosition());
		shadowMappingShader.SetVec3("lightPos", lightPos);
		shadowMappingShader.SetMat4("lightSpaceMatrix", lightSpaceMatrix);


		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderScene(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderDiamond(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, wallTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderWall(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, dinosaurTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderDodoBird(shadowMappingShader);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, dinosaurTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glDisable(GL_CULL_FACE);
		renderDino(shadowMappingShader);
		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)




		glfwSwapBuffers(window);
		glfwPollEvents();

	}


	delete pCamera;

	glfwTerminate();
	//return 0;
}

// renders the 3D scene
// --------------------
void renderScene(const Shader& shader)
{
	// floor
	glm::mat4 model;
	shader.SetMat4("model", model);
	//renderFloor();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	Room();
}

unsigned int planeVAO = 0;
void renderFloor()
{
	unsigned int planeVBO;

	if (planeVAO == 0) {
		// set up vertex data (and buffer(s)) and configure vertex attributes
		float planeVertices[] = {
			// positions            // normals         // texcoords
			5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
			-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,

			5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,
			//back wall
			-5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			5.0f, -1.0f, 5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			5.0f, 1.0f, 5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,

			-5.0f, -1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			5.0f, 1.0f, 5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			-5.0f, 1.0f, 5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,
			//front wall
			5.0f, 1.0f,  -5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, 1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,

			5.0f, 1.0f,  -5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,
			//left wall
			-5.0f, 1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, -1.0f, 5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,

			-5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, 1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			-5.0f, 1.0f, 5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			//right wall
			5.0f, -1.0f,  -5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			5.0f, 1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			5.0f, 1.0f, 5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,

			5.0f, 1.0f, 5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			5.0f, -1.0f, 5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			5.0f, -1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			//roof
			5.0f, 1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, 1.0f,  5.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
			-5.0f, 1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,

			5.0f, 1.0f,  5.0f,  0.0f, 1.0f, 0.0f,  5.0f,  0.0f,
			-5.0f, 1.0f, -5.0f,  0.0f, 1.0f, 0.0f,   0.0f, 5.0f,
			5.0f, 1.0f, -5.0f,  0.0f, 1.0f, 0.0f,  5.0f, 5.0f,





		};
		// plane VAO
		glGenVertexArrays(1, &planeVAO);
		glGenBuffers(1, &planeVBO);
		glBindVertexArray(planeVAO);
		glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindVertexArray(0);
	}

	glBindVertexArray(planeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 64);
}
void renderWall(const Shader& shader)
{
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	Room1();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	Room2();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	Room3();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	Room4();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(2.f));
	shader.SetMat4("model", model);
	Room5();
}
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
	// initialize (if necessary)
	if (cubeVAO == 0)
	{
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
			// front face
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
			1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			// left face
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			// right face
			1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
			1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
			// bottom face
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
			1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			// top face
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
			1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

// renderCube() renders a 1x1 3D cube in NDC.
// -------------------------------------------------

float vertices[82000];
unsigned int indices[72000];
GLuint trexTopVAO, trexTopVBO, trexTopEBO;
GLuint animalVAO, animalVBO, animalEBO;
void renderDino(const Shader& shader)
{
	//cube
	glm::mat4 model;
	model = glm::mat4();
	shader.SetMat4("model", model);
	renderCube();

	// duck
	/*model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.99f, 0.0f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	renderDodo();
	*/

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.99f, 0.0f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	upperDino();

	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.99f, 0.0f));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.SetMat4("model", model);
	bottomDino();

}
void renderDodoBird(const Shader& shader)
{
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.99f, 0.0f));
	shader.SetMat4("model", model);
	dodoBird();


}
GLuint dodoVAO, dodoVBO, dodoEBO;
void dodoBird()
{
	if (trexTopVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;


		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/dodo.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &dodoVAO);
		glGenBuffers(1, &dodoVBO);
		glGenBuffers(1, &dodoEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, dodoVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dodoEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(dodoVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(dodoVAO);
	glBindBuffer(GL_ARRAY_BUFFER, dodoVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dodoEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

void renderDiamond(const Shader& shader)
{
	glm::mat4 model;
	model = glm::mat4();
	model = glm::translate(model, glm::vec3(0.0f, 0.99f, 0.0f));
	shader.SetMat4("model", model);
	diamond();


}
GLuint diamondVAO, diamondVBO, diamondEBO;
void diamond()
{
	if (trexTopVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;


		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/hotballoon.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X+5);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y+5);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X+5);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y + 5);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X+5);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y + 5);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &diamondVAO);
		glGenBuffers(1, &diamondVBO);
		glGenBuffers(1, &diamondEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, diamondVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, diamondEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(diamondVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
}


void upperDino()
{
	if (trexTopVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;


		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/Trex.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[1];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X+10);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X+10);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X+10);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &trexTopVAO);
		glGenBuffers(1, &trexTopVBO);
		glGenBuffers(1, &trexTopEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, trexTopVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexTopEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(trexTopVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(trexTopVAO);
	glBindBuffer(GL_ARRAY_BUFFER, trexTopVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexTopEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

GLuint trexBottomVAO, trexBottomVBO, trexBottomEBO;
void bottomDino()
{
	if (trexBottomVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/Trex.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X+10);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y );
			verticess.push_back((float)curMesh.Vertices[j].Position.Z );
			verticess.push_back((float)curMesh.Vertices[j].Normal.X + 10);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y );
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z );
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X + 10);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y );
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &trexBottomVAO);
		glGenBuffers(1, &trexBottomVBO);
		glGenBuffers(1, &trexBottomEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, trexBottomVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexBottomEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(trexBottomVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(trexBottomVAO);
	glBindBuffer(GL_ARRAY_BUFFER, trexBottomVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, trexBottomEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}
GLuint floorVAO, floorVBO, floorEBO;
void Room()
{
	// initialize (if necessary)
	if (floorVAO == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/room.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[0];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO);
		glGenBuffers(1, &floorVBO);
		glGenBuffers(1, &floorEBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}

GLuint floorVAO1, floorVBO1, floorEBO1;
void Room1()
{
	// initialize (if necessary)
	if (floorVAO1 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/room.obj");
		for (int i = 0; i < Loader.LoadedMeshes.size(); i++)
		{
			object::Mesh curMesh = Loader.LoadedMeshes[1];
			int size = curMesh.Vertices.size();

			for (int j = 0; j < curMesh.Vertices.size(); j++)
			{

				verticess.push_back((float)curMesh.Vertices[j].Position.X);
				verticess.push_back((float)curMesh.Vertices[j].Position.Y);
				verticess.push_back((float)curMesh.Vertices[j].Position.Z);
				verticess.push_back((float)curMesh.Vertices[j].Normal.X);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
			}
			for (int j = 0; j < verticess.size(); j++)
			{
				vertices[j] = verticess.at(j);
			}

			for (int j = 0; j < curMesh.Indices.size(); j++)
			{

				indicess.push_back((float)curMesh.Indices[j]);

			}
			for (int j = 0; j < curMesh.Indices.size(); j++)
			{
				indices[j] = indicess.at(j);
			}
		}
		glGenVertexArrays(1, &floorVAO1);
		glGenBuffers(1, &floorVBO1);
		glGenBuffers(1, &floorEBO1);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO1);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO1);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO1);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO1);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO1);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO2, floorVBO2, floorEBO2;

void Room2()
{
	// initialize (if necessary)
	if (floorVAO2 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/room.obj");
		for (int i = 0; i < Loader.LoadedMeshes.size(); i++)
		{
			object::Mesh curMesh = Loader.LoadedMeshes[2];
			int size = curMesh.Vertices.size();

			for (int j = 0; j < curMesh.Vertices.size(); j++)
			{

				verticess.push_back((float)curMesh.Vertices[j].Position.X);
				verticess.push_back((float)curMesh.Vertices[j].Position.Y);
				verticess.push_back((float)curMesh.Vertices[j].Position.Z);
				verticess.push_back((float)curMesh.Vertices[j].Normal.X);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
				verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
				verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
			}
			for (int j = 0; j < verticess.size(); j++)
			{
				vertices[j] = verticess.at(j);
			}

			for (int j = 0; j < curMesh.Indices.size(); j++)
			{

				indicess.push_back((float)curMesh.Indices[j]);

			}
			for (int j = 0; j < curMesh.Indices.size(); j++)
			{
				indices[j] = indicess.at(j);
			}
		}
		glGenVertexArrays(1, &floorVAO2);
		glGenBuffers(1, &floorVBO2);
		glGenBuffers(1, &floorEBO2);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO2);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO2);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO2);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO2);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO2);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO2);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO3, floorVBO3, floorEBO3;

void Room3()
{
	// initialize (if necessary)
	if (floorVAO3 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/room.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[3];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO3);
		glGenBuffers(1, &floorVBO3);
		glGenBuffers(1, &floorEBO3);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO3);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO3);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO3);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO3);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO3);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO3);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO4, floorVBO4, floorEBO4;

void Room4()
{
	// initialize (if necessary)
	if (floorVAO4 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/room.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[4];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO4);
		glGenBuffers(1, &floorVBO4);
		glGenBuffers(1, &floorEBO4);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO4);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO4);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO4);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO4);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO4);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO4);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
GLuint floorVAO5, floorVBO5, floorEBO5;

void Room5()
{
	// initialize (if necessary)
	if (floorVAO5 == 0)
	{

		std::vector<float> verticess;
		std::vector<float> indicess;



		Loader.LoadFile("D:/Visual Studio Projects/Grafica 3D/Clone/Museum/Debug/room.obj");
		object::Mesh curMesh = Loader.LoadedMeshes[5];
		int size = curMesh.Vertices.size();

		for (int j = 0; j < curMesh.Vertices.size(); j++)
		{

			verticess.push_back((float)curMesh.Vertices[j].Position.X+100);
			verticess.push_back((float)curMesh.Vertices[j].Position.Y+100);
			verticess.push_back((float)curMesh.Vertices[j].Position.Z);
			verticess.push_back((float)curMesh.Vertices[j].Normal.X+100);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Y+100);
			verticess.push_back((float)curMesh.Vertices[j].Normal.Z);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.X+100);
			verticess.push_back((float)curMesh.Vertices[j].TextureCoordinate.Y+100);
		}
		for (int j = 0; j < verticess.size(); j++)
		{
			vertices[j] = verticess.at(j);
		}

		for (int j = 0; j < curMesh.Indices.size(); j++)
		{

			indicess.push_back((float)curMesh.Indices[j]);

		}
		for (int j = 0; j < curMesh.Indices.size(); j++)
		{
			indices[j] = indicess.at(j);
		}

		glGenVertexArrays(1, &floorVAO5);
		glGenBuffers(1, &floorVBO5);
		glGenBuffers(1, &floorEBO5);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, floorVBO5);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO5);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices, GL_DYNAMIC_DRAW);
		// link vertex attributes
		glBindVertexArray(floorVAO5);
		glEnableVertexAttribArray(0);


		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(floorVAO5);
	glBindBuffer(GL_ARRAY_BUFFER, floorVBO5);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorEBO5);
	int indexArraySize;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &indexArraySize);
	glDrawElements(GL_TRIANGLES, indexArraySize / sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

}
// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		pCamera->ProcessKeyboard(FORWARD, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		pCamera->ProcessKeyboard(BACKWARD, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
		pCamera->ProcessKeyboard(LEFT, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
		pCamera->ProcessKeyboard(RIGHT, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
		pCamera->ProcessKeyboard(UP, (float)deltaTime);
	if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
		pCamera->ProcessKeyboard(DOWN, (float)deltaTime);

	if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
	{
		rotateLight = true;
	}
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		rotateLight = false;
	}

	if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		pCamera->Reset(width, height);

	}
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	pCamera->Reshape(width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	pCamera->MouseControl((float)xpos, (float)ypos);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yOffset)
{
	pCamera->ProcessMouseScroll((float)yOffset);
}
