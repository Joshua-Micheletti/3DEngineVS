#include "renderer.h"
#include "entity.h"
#include "camera.h"
#include "shader.h"
#include <vector>
#include <glad\glad.h>
#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>
#include <glm\gtc\type_ptr.hpp>
#include <SFML\Graphics.hpp>
#include "init.h"
#include <iostream>
#include <string>

// constructor method, sets up the renderer (reflection and post processing)
Renderer::Renderer() {
	
	// sets the color to clear the color buffer with
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

	// enable multisampling
	glEnable(GL_MULTISAMPLE);

	// enables back-face culling:
	// polygons aren't rendered if the vertices that define the triangle are seen clockwise or counterclockwise,
	// a face with vertices indexed in the opposite order suggest that it's being viewed from the other side,
	// which is usually the inside of the model, which doesn't need to be rendered
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	// generate 1 generic buffer and assigns its ID to the variable tmpBuffer
	glGenBuffers(1, &this->tmpBuffer);


	/*--------------------------------------------------------------------------*/
	/*                             REFLECTION SETUP                             */
	/*--------------------------------------------------------------------------*/

	// defines the resolution of the reflection cubemap
	this->reflectionRes = 2048;

	// generate the framebuffer that is gonna store the view from the reflection camera
	glGenFramebuffers(1, &this->reflectionFBO);
	// bind the newly generated framebuffer to the default framebuffer, both in read and write
	glBindFramebuffer(GL_FRAMEBUFFER, this->reflectionFBO);

	/* ACTIVE FRAMEBUFFER: reflectionFBO */

	// generate a generic texture for the cubemap reflection
	glGenTextures(1, &this->reflectionCubemap);
	// actually creates and binds the cubemap placeholder to the actual cubemap
	glBindTexture(GL_TEXTURE_CUBE_MAP, this->reflectionCubemap);

	/* ACTIVE TEXTURE: reflectionCubemap */

	// cycle through the faces of the cubemap
	for (int i = 0; i < 6; i++) {
		// generate an empty (NULL) texture at the target (active texture @ GL_TEXTURE_CUBE_MAP_POSITIVE_X ... GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		// with no mipmap level, RGB color internal format, reflectionRex x reflectionRes for the resolution, no border, RGB color format,
		// UNSIGNED_BYTE pixel data format
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, this->reflectionRes, this->reflectionRes, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	}

	// set texture parameters (GL_TEXTURE_CUBE_MAP = target (in this case it refers to cubemap))
	// set the texture display filter when switching mipmaps (needs more testing and studying)
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// set the texture display filter when switching mipmaps (needs more testing and studying)
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	// set the texture to shrink or stretch to the edge of the texture space in the S, T and R axis
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	// generates one render buffer to attach to the framebuffer to store the image data (more optimized than textures for targets)
	glGenRenderbuffers(1, &this->reflectionRBO);
	// bind renderBuffer to the default GL_RENDERBUFFER
	glBindRenderbuffer(GL_RENDERBUFFER, this->reflectionRBO);

	/* ACTIVE RENDERBUFFER: reflectionRBO */

	// define render buffer multisample (needs more study)
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, 8, GL_DEPTH24_STENCIL8, this->reflectionRes, this->reflectionRes);
	// attach renderBuffer to the current framebuffer (frameBuffer)
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, this->reflectionRBO);

	// set back the renderbuffer to the default renderbuffer
	glBindRenderbuffer(GL_RENDERBUFFER, 0);

	/* ACTIVE RENDERBUFFER: Default */


	/*-------------------------------------------------------------------------------*/
	/*                             POST PROCESSING SETUP                             */
	/*-------------------------------------------------------------------------------*/
	
	// create an empty texture object for screenTexture, this texture will be bound to the screenFBO
	// and will store the screen view image
	glGenTextures(1, &this->screenTexture);
	// bind the screenTexture texture as the main texture
	//glBindTexture(GL_TEXTURE_2D, this->screenTexture);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, this->screenTexture);
	
	/* ACTIVE TEXTURE: screenTexture */

	// create the actual texture for image with res: screenWidth x screenHeight
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGB, screenWidth, screenHeight, false);

	glGenTextures(1, &this->screenDepthTexture);
	// bind the screenTexture texture as the main texture
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, this->screenDepthTexture);
	
	/* ACTIVE TEXTURE: screenDepthTexture */

	// create the actual texture for image with res: screenWidth x screenHeight
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth, screenHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_DEPTH_COMPONENT, screenWidth, screenHeight, false);

	// create the screenFBO (used for rendering the screen view to a texture)
	glGenFramebuffers(1, &this->screenFBO);
	// bind the screenFBO to be the default FBO
	glBindFramebuffer(GL_FRAMEBUFFER, this->screenFBO);

	/* ACTIVE FRAMEBUFFER: screenFBO */

	// attach the screenTexture to the screenFBO, so that the stuff rendered on the screenFBO can be saved to the screenTexture
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, this->screenTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, this->screenDepthTexture, 0);

	// create a square of vertices for the post processing shader
	std::vector<float> square;
	square.push_back(-1.0f); // bottom-left
	square.push_back(1.0f);

	square.push_back(-1.0f); // top-left
	square.push_back(-1.0f);

	square.push_back(1.0f); //  top-right
	square.push_back(-1.0f);


	square.push_back(-1.0f); // bottom-left
	square.push_back(1.0f);

	square.push_back(1.0f); //  top-right
	square.push_back(-1.0f);

	square.push_back(1.0f); //  bottom-right
	square.push_back(1.0f);

	// create the VBO to store the square vertices
	glGenBuffers(1, &this->screenVBO);
	// set the current active VBO to the screenVBO
	glBindBuffer(GL_ARRAY_BUFFER, this->screenVBO);

	/* ACTIVE BUFFER: screenVBO */

	// pass the vertices to the buffer
	glBufferData(GL_ARRAY_BUFFER, square.size() * sizeof(float), &square[0], GL_STATIC_DRAW);

	// create the UV coordinates to map the screen texture to the screen square
	std::vector<float> uv;
	uv.push_back(0.0f); // bottom-left
	uv.push_back(1.0f);

	uv.push_back(0.0f); // top-left
	uv.push_back(0.0f);

	uv.push_back(1.0f); // top-right
	uv.push_back(0.0f);


	uv.push_back(0.0f); // bottom-left
	uv.push_back(1.0f);

	uv.push_back(1.0f); // top-right
	uv.push_back(0.0f);

	uv.push_back(1.0f); // bottom-right
	uv.push_back(1.0f);

	// create the VBO to store the UV vertices
	glGenBuffers(1, &this->screenUVVBO);
	// set the current active VBO to the screenUVVBO
	glBindBuffer(GL_ARRAY_BUFFER, this->screenUVVBO);

	/* ACTIVE BUFFER: screenUVVBO*/

	// pass the UV coordinates to the buffer
	glBufferData(GL_ARRAY_BUFFER, uv.size() * sizeof(float), &uv[0], GL_STATIC_DRAW);

	// create the shader object to hold the post processing shader
	this->screenShader = new Shader((char*)"screen shader");
	// load the post processing shader
	this->screenShader->loadShader((char*)"../Shader/screen/screen.vert", (char*)"../Shader/screen/screen.frag");

	this->depthShader = new Shader((char*)"depth shader");
	this->depthShader->loadShader((char*)"../Shader/depth/depth.vert", (char*)"../Shader/depth/depth.frag");
}

// public method for rendering the scene
void Renderer::render() {
	// clear the color and depth buffers before drawing again
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// enable depth testing (draw a fragment only if there's nothing in front of it)
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_MULTISAMPLE);

	if (updateResolution) {
		this->resizeScreen();
	}

	// check if the program should render the reflection cubemap
	if (doReflection) {
		// render the reflection cubemap
		this->renderReflectionCubemap();
	}

	// set the active frame buffer to the screenFBO
	glBindFramebuffer(GL_FRAMEBUFFER, this->screenFBO);
	glClear(GL_DEPTH_BUFFER_BIT);

	/* ACTIVE FRAMEBUFFER: screenFBO */

	// render all entities
	this->renderEntities(false);

	// draw the bounding box for each entity
	this->displayBoundingBox();

	// render the screen (post processing)
	this->renderScreen();

	// reset the renderer for the next render
	this->resetRender();
}

// render the cubemap view from the reflection camera to later calculate reflections on
void Renderer::renderReflectionCubemap() {
	// set the current camera to the camera inside the reflective object
	defaultCamera = 1;

	/* ACTIVE CAMERA: camera2 */

	// set the reflectionFBO as the current framebuffer to render on
	glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);

	/* ACTIVE FRAMEBUFFER: reflectionFBO */

	// set the viewport to fit the reflection texture resolution
	glViewport(0, 0, reflectionRes, reflectionRes);

	// clear the color and depth buffers from the reflectionFBO
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// cycle all the faces of the cubemap
	for (int i = 0; i < 6; i++) {
		// attach the positive X texture of the reflectionCubemap to the color buffer of the reflectionFBO
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, this->reflectionCubemap, 0);
		// aim the camera to face the correct direction
		if (i == 0)      // FRONT
			camera2.setOrientation(glm::vec3(0.0, 0.0, 0.0));
		else if (i == 1) // BACK
			camera2.setOrientation(glm::vec3(0.0, 180.0, 0.0));
		else if (i == 2) // TOP
			camera2.setOrientation(glm::vec3(0.0, -90.0, 90.0));
		else if (i == 3) // BOTTOM
			camera2.setOrientation(glm::vec3(0.0, -90.0, -90.0));
		else if (i == 4) // RIGHT
			camera2.setOrientation(glm::vec3(0.0, 90.0, 0.0));
		else             // LEFT
			camera2.setOrientation(glm::vec3(0.0, 270.0, 0.0));

		// render all entities except for the ones that shouldn't be rendered in the reflection
		this->renderEntities(true);
		// clear the depth buffers from the reflectionFBO
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	// reset the viewport
	glViewport(0, 0, screenWidth, screenHeight);

	// set the render camera to the default camera
	defaultCamera = 0;

	/* ACTIVE CAMERA: camera1 */
}

// render all entities with their corresponding shader (forward rendering)
void Renderer::renderEntities(bool reflection) {
	// cycle through all the entities
	for (int i = 0; i < entityBuffer.size(); i++) {
		// if it's rendering entities to be displayed in the reflection:
		if (reflection) {
			// check what entities are supposed to be rendered in the reflection
			if (entityBuffer[i]->getToReflect() == true) {
				// if it's rendering the skybox
				if (entityBuffer[i]->getName().compare("skybox") == 0) {
					// disable the depth mask (the rendering won't write into the depth buffer)
					glDepthMask(GL_FALSE);
				}

				// installs the shader to render the entity (it gets the shader from the entity)
				glUseProgram(shaderBuffer[entityBuffer[i]->getShader()].getID());

				// pass the values for the shader uniforms.
				// shader uniforms are global variables for shaders that can be set by the user.
				// uniforms are global to all shaders, so they don't need to be set by every shader call if the value doesn't change
				// for example matrices for 3D rendering usually don't change between shaders (some optimization is possible)
				this->attachUniforms(entityBuffer[i], shaderBuffer[entityBuffer[i]->getShader()].getUniformBuffer());

				// link the layouts to the data origin.
				// layouts define where the data for a certain variable comes from
				this->linkLayouts(entityBuffer[i], shaderBuffer[entityBuffer[i]->getShader()].getLayoutBuffer());

				// if the entity has a texture attached to it
				if (entityBuffer[i]->getTexture() != 0) {
					// bind it as the current active texture
					glBindTexture(entityBuffer[i]->getTextureType(), entityBuffer[i]->getTexture());
				}

				// check which mode things should be rendered as
				// if we're rendering the skybox, always render as triangles (weird results if you render with different primitives)
				if (entityBuffer[i]->getName().compare("skybox") == 0) {
					// render the skybox
					glDrawArrays(GL_TRIANGLES, 0, entityBuffer[i]->getVertices().size());
					// re-enable the depth mask (now rendering also affects the depth buffer as well)
					glDepthMask(GL_TRUE);
				}

				else {
					switch (renderMode) {
						// draw lines
					case wireframe:
						glDrawArrays(GL_LINES, 0, entityBuffer[i]->getVertices().size());
						break;

						// draw points
					case vertices:
						glPointSize(2.0f);
						glDrawArrays(GL_POINTS, 0, entityBuffer[i]->getVertices().size());
						break;

						// draw in the element's primitive (mainly triangles)
					default:
						glDrawArrays(entityBuffer[i]->getElements(), 0, entityBuffer[i]->getVertices().size());
					}
				}
			}
		}

		else {
			if (entityBuffer[i]->getName().compare("skybox") == 0) {
				glDepthMask(GL_FALSE);
			}

			glUseProgram(shaderBuffer[entityBuffer[i]->getShader()].getID());

			attachUniforms(entityBuffer[i], shaderBuffer[entityBuffer[i]->getShader()].getUniformBuffer());

			linkLayouts(entityBuffer[i], shaderBuffer[entityBuffer[i]->getShader()].getLayoutBuffer());

			if (entityBuffer[i]->getTexture() != 0) {
				glBindTexture(entityBuffer[i]->getTextureType(), entityBuffer[i]->getTexture());
			}

			if (entityBuffer[i]->getName().compare("man") == 0 || entityBuffer[i]->getName().compare("monkey") == 0) {
				glBindTexture(GL_TEXTURE_CUBE_MAP, this->reflectionCubemap);
			}

			// check which mode things should be rendered as
			if (entityBuffer[i]->getName().compare("skybox") == 0) {
				// render the skybox
				glDrawArrays(GL_TRIANGLES, 0, entityBuffer[i]->getVertices().size());
				// re-enable the depth mask (now rendering also affects the depth buffer as well)
				glDepthMask(GL_TRUE);
			}

			else {
				switch (renderMode) {
				case wireframe:
					glDrawArrays(GL_LINES, 0, entityBuffer[i]->getVertices().size());
					break;

				case vertices:
					glPointSize(2.0f);
					glDrawArrays(GL_POINTS, 0, entityBuffer[i]->getVertices().size());
					break;

				default:
					glDrawArrays(entityBuffer[i]->getElements(), 0, entityBuffer[i]->getVertices().size());

				}				
			}
		}

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
	}
}

// render the screen texture applying post processing shaders
void Renderer::renderScreen() {
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST); // disable depth test so screen-space quad isn't discarded due to depth test.
	// clear all relevant buffers
	glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (depthBuffer) {
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, this->screenDepthTexture);
		glUseProgram(this->depthShader->getID());
		glUniform1i(glGetUniformLocation(this->screenShader->getID(), "samples"), samples);
	}
	else {
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, this->screenTexture);
		glUseProgram(this->screenShader->getID());
		glUniform1i(glGetUniformLocation(this->screenShader->getID(), "samples"), samples);
	}

	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, this->screenVBO);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, this->screenUVVBO);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	//if (!depthBuffer) {
	//glBindFramebuffer(GL_READ_FRAMEBUFFER, this->screenFBO);
	//glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); //draw to to the default framebuffer
	//glDrawBuffer(GL_BACK);
	//glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	//}
	//else {
	//	glBindFramebuffer(GL_READ_FRAMEBUFFER, this->screenFBO);
	//	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); //draw to to the default framebuffer
	//	glDrawBuffer(GL_BACK);
	//	glBlitFramebuffer(0, 0, screenWidth, screenHeight, 0, 0, screenWidth, screenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	//}

	glEnable(GL_DEPTH_TEST);

	glBindBuffer(GL_ARRAY_BUFFER, tmpBuffer);
}

// pass the correct values to the corresponding uniforms in the shader
void Renderer::attachUniforms(Entity* entity, std::vector<uniform_t> uniformBuffer) {
	// cycle through the uniformBuffer of the shader
	for (int i = 0; i < uniformBuffer.size(); i++) {
		// if the uniform is "modelMatrix", set it to the entity modelMatrix
		if (strcmp(uniformBuffer[i].name, "modelMatrix") == 0) {
			glUniformMatrix4fv(uniformBuffer[i].id, 1, GL_FALSE, &(entity->getModelMatrix()[0][0]));
		}

		// if the uniform is "viewMatrix", set it to the camera viewMatrix
		else if (strcmp(uniformBuffer[i].name, "viewMatrix") == 0) {
			// if the entity we're rendering is the skybox, 
			if (entity->getName().compare("skybox") == 0) {
				// this process removes all the translations from the camera, this way camera movement is not taken into account
				glm::mat4 staticCameraView = glm::mat4(glm::mat3(cameraBuffer[defaultCamera]->getViewMatrix()));
				glUniformMatrix4fv(uniformBuffer[i].id, 1, GL_FALSE, &(staticCameraView[0][0]));
			}
			// otherwise pass the camera view matrix, including all translations
			else {
				glUniformMatrix4fv(uniformBuffer[i].id, 1, GL_FALSE, &(cameraBuffer[defaultCamera]->getViewMatrix()[0][0]));
			}
		}

		// if the uniform is "projectionMatrix", set it to the main camera projection matrix in the projectionBuffer
		else if (strcmp(uniformBuffer[i].name, "projectionMatrix") == 0) {
			glUniformMatrix4fv(uniformBuffer[i].id, 1, GL_FALSE, &(projectionBuffer[defaultCamera][0][0]));
		}

		// if the uniform is "lightPosition", pass the light position (x, y, z)
		else if (strcmp(uniformBuffer[i].name, "lightPosition") == 0) {
			glUniform3f(uniformBuffer[i].id, light->getWorldPosition().x, light->getWorldPosition().y, light->getWorldPosition().z);
		}

		// if the uniform is "eyePosition", pass the camera position (x, y, z)
		else if (strcmp(uniformBuffer[i].name, "eyePosition") == 0) {
			glUniform3f(uniformBuffer[i].id, cameraBuffer[defaultCamera]->getPosition().x, cameraBuffer[defaultCamera]->getPosition().y, cameraBuffer[defaultCamera]->getPosition().z);
		}
	}
}

// link layouts to the data origin (mainly VAO)
void Renderer::linkLayouts(Entity* entity, std::vector<char*> layoutBuffer) {
	// cycle all the layouts in the layout buffer of the shader
	for (int i = 0; i < layoutBuffer.size(); i++) {
		// if the layout is named "vertex" (contains the entity vertices that make the geometry of the entity)
		if (strcmp(layoutBuffer[i], "vertex") == 0) {
			// enable the VAO in position 0
			glEnableVertexAttribArray(0);
			// bind the geometry VBO of the entity
			glBindBuffer(GL_ARRAY_BUFFER, entity->getVertexBuffer());
			// setup the VAO to reference the VBO (needs more study)
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		}
		// if the layout is named "uv" (contains the UV coordinates to map the texture to the geometry)
		else if (strcmp(layoutBuffer[i], "uv") == 0) {
			// enable the VAO in position 1
			glEnableVertexAttribArray(1);
			// bind the UV VBO of the entity
			glBindBuffer(GL_ARRAY_BUFFER, entity->getTexBuffer());
			// setup the VAO to reference the VBO (needs more study)
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		}
		// if the layout is named "color" (contains the color vectors that make the entity color)
		else if (strcmp(layoutBuffer[i], "color") == 0) {
			// enable the VAO in position 1
			glEnableVertexAttribArray(1);
			// bind the color VBO of the entity
			glBindBuffer(GL_ARRAY_BUFFER, entity->getTexBuffer());
			// setup the VAO to reference the VBO (needs more study)
			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		}
		// if the layout is named "normal" (contains the normals to the vertices, used for light calculation)
		else if (strcmp(layoutBuffer[i], "normal") == 0) {
			// enable the VAO in position 1
			glEnableVertexAttribArray(2);
			// bind the normal VBO of the entity
			glBindBuffer(GL_ARRAY_BUFFER, entity->getNormalBuffer());
			// setup the VAO to reference the VBO (needs more study)
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		}
	}
}


void Renderer::resetRender() {
	glBufferData(GL_ARRAY_BUFFER, data1.size() * sizeof(float), &data1[0], GL_STATIC_DRAW);

	glUseProgram(shaderBuffer[1].getID());
	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[0].id, 1, GL_FALSE, &(glm::mat4(1.0f)[0][0]));
	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[1].id, 1, GL_FALSE, &(camera.getViewMatrix()[0][0]));
	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[2].id, 1, GL_FALSE, &(Projection[0][0]));
	glUniform3f(shaderBuffer[1].getUniformBuffer()[3].id, 255, 255, 255);

	glEnableVertexAttribArray(0);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glDrawArrays(GL_LINES, 0, data1.size());

	glDisableVertexAttribArray(0);
}

void Renderer::resizeScreen() {
	// create an empty texture object for screenTexture, this texture will be bound to the screenFBO
	// and will store the screen view image
	glGenTextures(1, &this->screenTexture);
	// bind the screenTexture texture as the main texture
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, this->screenTexture);

	/* ACTIVE TEXTURE: screenTexture */

	// create the actual texture for image with res: screenWidth x screenHeight
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGB, screenWidth, screenHeight, false);


	// set the texture filters for mipmaps
	glGenTextures(1, &this->screenDepthTexture);
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, this->screenDepthTexture);

	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_DEPTH_COMPONENT, screenWidth, screenHeight, false);

	// create the screenFBO (used for rendering the screen view to a texture)
	glGenFramebuffers(1, &this->screenFBO);
	// bind the screenFBO to be the default FBO
	glBindFramebuffer(GL_FRAMEBUFFER, this->screenFBO);

	/* ACTIVE FRAMEBUFFER: screenFBO */

	// attach the screenTexture to the screenFBO, so that the stuff rendered on the screenFBO can be saved to the screenTexture
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, this->screenTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, this->screenDepthTexture, 0);

	glViewport(0, 0, screenWidth, screenHeight);

	projectionBuffer[0] = glm::perspective(glm::radians(45.0f), (float)screenWidth / (float)screenHeight, 0.1f, 10000.0f);
	updated = true;
}


void Renderer::createCube(std::vector<float>* array, std::vector<glm::vec3> faces) {
	array->push_back(faces[0].x);
	array->push_back(faces[0].y);
	array->push_back(faces[0].z);
	array->push_back(faces[1].x);
	array->push_back(faces[1].y);
	array->push_back(faces[1].z);

	array->push_back(faces[1].x);
	array->push_back(faces[1].y);
	array->push_back(faces[1].z);
	array->push_back(faces[2].x);
	array->push_back(faces[2].y);
	array->push_back(faces[2].z);

	array->push_back(faces[2].x);
	array->push_back(faces[2].y);
	array->push_back(faces[2].z);
	array->push_back(faces[3].x);
	array->push_back(faces[3].y);
	array->push_back(faces[3].z);

	array->push_back(faces[3].x);
	array->push_back(faces[3].y);
	array->push_back(faces[3].z);
	array->push_back(faces[0].x);
	array->push_back(faces[0].y);
	array->push_back(faces[0].z);

	array->push_back(faces[0].x);
	array->push_back(faces[0].y);
	array->push_back(faces[0].z);
	array->push_back(faces[5].x);
	array->push_back(faces[5].y);
	array->push_back(faces[5].z);

	array->push_back(faces[1].x);
	array->push_back(faces[1].y);
	array->push_back(faces[1].z);
	array->push_back(faces[6].x);
	array->push_back(faces[6].y);
	array->push_back(faces[6].z);

	array->push_back(faces[2].x);
	array->push_back(faces[2].y);
	array->push_back(faces[2].z);
	array->push_back(faces[7].x);
	array->push_back(faces[7].y);
	array->push_back(faces[7].z);

	array->push_back(faces[3].x);
	array->push_back(faces[3].y);
	array->push_back(faces[3].z);
	array->push_back(faces[4].x);
	array->push_back(faces[4].y);
	array->push_back(faces[4].z);

	array->push_back(faces[5].x);
	array->push_back(faces[5].y);
	array->push_back(faces[5].z);
	array->push_back(faces[4].x);
	array->push_back(faces[4].y);
	array->push_back(faces[4].z);

	array->push_back(faces[4].x);
	array->push_back(faces[4].y);
	array->push_back(faces[4].z);
	array->push_back(faces[7].x);
	array->push_back(faces[7].y);
	array->push_back(faces[7].z);

	array->push_back(faces[7].x);
	array->push_back(faces[7].y);
	array->push_back(faces[7].z);
	array->push_back(faces[6].x);
	array->push_back(faces[6].y);
	array->push_back(faces[6].z);

	array->push_back(faces[6].x);
	array->push_back(faces[6].y);
	array->push_back(faces[6].z);
	array->push_back(faces[5].x);
	array->push_back(faces[5].y);
	array->push_back(faces[5].z);
}

void Renderer::createSphere(glm::vec3 center, float dist, int sides, std::vector<float>* data) {

	double pi = 3.1415926535897;

	glm::vec3 tmp;

	glm::vec3 tmp2;

	tmp = glm::vec3(dist, 0, 0) + center;

	for (float i = 0; i < 2 * pi; i += (2 * pi) / (float)sides) {
		tmp2.x = dist * cos(i);
		tmp2.y = dist * sin(i);
		tmp2.z = 0;

		data->push_back(tmp.x);
		data->push_back(tmp.y);
		data->push_back(tmp.z);

		tmp = tmp2 + center;

		data->push_back(tmp.x);
		data->push_back(tmp.y);
		data->push_back(tmp.z);
	}

	data->push_back(tmp.x);
	data->push_back(tmp.y);
	data->push_back(tmp.z);

	tmp = glm::vec3(dist, 0, 0) + center;

	data->push_back(tmp.x);
	data->push_back(tmp.y);
	data->push_back(tmp.z);



	tmp = glm::vec3(0, 0, dist) + center;

	for (float i = 0; i < 2 * pi; i += (2 * pi) / (float)sides) {
		tmp2.x = 0;
		tmp2.y = dist * sin(i);
		tmp2.z = dist * cos(i);

		data->push_back(tmp.x);
		data->push_back(tmp.y);
		data->push_back(tmp.z);

		tmp = tmp2 + center;

		data->push_back(tmp.x);
		data->push_back(tmp.y);
		data->push_back(tmp.z);
	}

	data->push_back(tmp.x);
	data->push_back(tmp.y);
	data->push_back(tmp.z);

	tmp = glm::vec3(0, 0, dist) + center;

	data->push_back(tmp.x);
	data->push_back(tmp.y);
	data->push_back(tmp.z);



	tmp = glm::vec3(0, 0, dist) + center;

	for (float i = 0; i < 2 * pi; i += (2 * pi) / (float)sides) {
		tmp2.x = dist * sin(i);
		tmp2.y = 0;
		tmp2.z = dist * cos(i);

		data->push_back(tmp.x);
		data->push_back(tmp.y);
		data->push_back(tmp.z);

		tmp = tmp2 + center;

		data->push_back(tmp.x);
		data->push_back(tmp.y);
		data->push_back(tmp.z);
	}

	data->push_back(tmp.x);
	data->push_back(tmp.y);
	data->push_back(tmp.z);

	tmp = glm::vec3(0, 0, dist) + center;

	data->push_back(tmp.x);
	data->push_back(tmp.y);
	data->push_back(tmp.z);
}

void Renderer::drawBoundingBox(bounds_t bounds, glm::vec3 color) {
	std::vector<float> data;
	std::vector<glm::vec3> faces;
	faces.push_back(bounds.a);
	faces.push_back(bounds.b);
	faces.push_back(bounds.c);
	faces.push_back(bounds.d);
	faces.push_back(bounds.e);
	faces.push_back(bounds.f);
	faces.push_back(bounds.g);
	faces.push_back(bounds.h);
	createCube(&data, faces);

	glBindBuffer(GL_ARRAY_BUFFER, tmpBuffer);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);

	glUseProgram(shaderBuffer[1].getID());

	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[0].id, 1, GL_FALSE, &(glm::mat4(1.0f)[0][0]));
	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[1].id, 1, GL_FALSE, &(camera.getViewMatrix()[0][0]));
	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[2].id, 1, GL_FALSE, &(projectionBuffer[0][0][0]));
	glUniform3f(shaderBuffer[1].getUniformBuffer()[3].id, color.x, color.y, color.z);

	glEnableVertexAttribArray(0);
	// glBindBuffer(GL_ARRAY_BUFFER, tmpBuffer2);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glDrawArrays(GL_LINES, 0, data.size());

	glDisableVertexAttribArray(0);
}

void Renderer::drawBoundingSphere(float radius, glm::vec3 center, glm::vec3 color) {
	std::vector<float> data;

	createSphere(center, radius, 100, &data);
	glBindBuffer(GL_ARRAY_BUFFER, tmpBuffer);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);

	glUseProgram(shaderBuffer[1].getID());

	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[0].id, 1, GL_FALSE, &(glm::mat4(1.0f)[0][0]));
	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[1].id, 1, GL_FALSE, &(camera.getViewMatrix()[0][0]));
	glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[2].id, 1, GL_FALSE, &(Projection[0][0]));
	glUniform3f(shaderBuffer[1].getUniformBuffer()[3].id, color.x, color.y, color.z);

	glEnableVertexAttribArray(0);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

	glDrawArrays(GL_LINES, 0, data.size());

	glDisableVertexAttribArray(0);
}

void Renderer::displayBoundingBox() {
	for (int i = 0; i < entityBuffer.size(); i++) {
		if (drawOBB) {
			drawBoundingBox(entityBuffer[i]->getObjectBoundingBox(true), glm::vec3(1, 0, 0));
		}

		if (drawAABB1) {
			drawBoundingBox(entityBuffer[i]->getExternalAxisAlignedBoundingBox(true), glm::vec3(0, 1, 0));
		}

		if (drawAABB2) {
			drawBoundingBox(entityBuffer[i]->getInternalAxisAlignedBoundingBox(true), glm::vec3(0, 0, 1));
		}

		if (drawAABB3) {
			bounds_t internalWorldBounds;
			internalWorldBounds = entityBuffer[i]->getInternalAxisAlignedBoundingBox(true);

			bounds_t worldBounds;
			worldBounds = entityBuffer[i]->getExternalAxisAlignedBoundingBox(true);

			std::vector<float> data5;

			glm::vec3 a2 = glm::vec3((worldBounds.a.x + internalWorldBounds.a.x) / 2, (worldBounds.a.y + internalWorldBounds.a.y) / 2, (worldBounds.a.z + internalWorldBounds.a.z) / 2);
			glm::vec3 b2 = glm::vec3((worldBounds.b.x + internalWorldBounds.b.x) / 2, (worldBounds.b.y + internalWorldBounds.b.y) / 2, (worldBounds.b.z + internalWorldBounds.b.z) / 2);
			glm::vec3 c2 = glm::vec3((worldBounds.c.x + internalWorldBounds.c.x) / 2, (worldBounds.c.y + internalWorldBounds.c.y) / 2, (worldBounds.c.z + internalWorldBounds.c.z) / 2);
			glm::vec3 d2 = glm::vec3((worldBounds.d.x + internalWorldBounds.d.x) / 2, (worldBounds.d.y + internalWorldBounds.d.y) / 2, (worldBounds.d.z + internalWorldBounds.d.z) / 2);
			glm::vec3 e2 = glm::vec3((worldBounds.e.x + internalWorldBounds.e.x) / 2, (worldBounds.e.y + internalWorldBounds.e.y) / 2, (worldBounds.e.z + internalWorldBounds.e.z) / 2);
			glm::vec3 f2 = glm::vec3((worldBounds.f.x + internalWorldBounds.f.x) / 2, (worldBounds.f.y + internalWorldBounds.f.y) / 2, (worldBounds.f.z + internalWorldBounds.f.z) / 2);
			glm::vec3 g2 = glm::vec3((worldBounds.g.x + internalWorldBounds.g.x) / 2, (worldBounds.g.y + internalWorldBounds.g.y) / 2, (worldBounds.g.z + internalWorldBounds.g.z) / 2);
			glm::vec3 h2 = glm::vec3((worldBounds.h.x + internalWorldBounds.h.x) / 2, (worldBounds.h.y + internalWorldBounds.h.y) / 2, (worldBounds.h.z + internalWorldBounds.h.z) / 2);

			std::vector<glm::vec3> faces4;
			faces4.push_back(a2);
			faces4.push_back(b2);
			faces4.push_back(c2);
			faces4.push_back(d2);
			faces4.push_back(e2);
			faces4.push_back(f2);
			faces4.push_back(g2);
			faces4.push_back(h2);
			createCube(&data5, faces4);
			glBindBuffer(GL_ARRAY_BUFFER, tmpBuffer);
			glBufferData(GL_ARRAY_BUFFER, data5.size() * sizeof(float), &data5[0], GL_STATIC_DRAW);

			glUseProgram(shaderBuffer[1].getID());

			glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[0].id, 1, GL_FALSE, &(glm::mat4(1.0f)[0][0]));
			glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[1].id, 1, GL_FALSE, &(camera.getViewMatrix()[0][0]));
			glUniformMatrix4fv(shaderBuffer[1].getUniformBuffer()[2].id, 1, GL_FALSE, &(Projection[0][0]));
			glUniform3f(shaderBuffer[1].getUniformBuffer()[3].id, 0, 1, 1);

			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

			glPointSize(10.0f);

			glDrawArrays(GL_LINES, 0, data5.size());

			glDisableVertexAttribArray(0);
		}

		if (drawAABB4) {
			drawBoundingBox(entityBuffer[i]->getAxisAlignedBoundingBox(true), glm::vec3(1, 0, 1));
		}

		if (drawBS) {
			drawBoundingSphere(entityBuffer[i]->getInternalBoundingSphere(false), entityBuffer[i]->getWorldPosition(), glm::vec3(1, 1, 0));
		}

		if (drawBS2) {
			drawBoundingSphere(entityBuffer[i]->getExternalBoundingSphere(false), entityBuffer[i]->getWorldPosition(), glm::vec3(1, 0.5, 0));
		}

		if (drawBS3) {
			drawBoundingSphere(entityBuffer[i]->getBoundingSphere(false), entityBuffer[i]->getWorldPosition(), glm::vec3(0.5, 1, 0));
		}
	}
}


