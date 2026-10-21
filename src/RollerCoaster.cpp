#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openGLHeader.h"
#include "imageIO.h"

#include "basicPipelineProgram.h"
#include "openGLMatrix.h"
#include "glutHeader.h"

#include <math.h>

#include <iostream>
#include <cstring>

#include <vector>

#include <time.h>

#include <chrono>
#include <iomanip>
#include <sstream>

#if defined(WIN32) || defined(_WIN32)
  #ifdef _DEBUG
    #pragma comment(lib, "glew32d.lib")
  #else
    #pragma comment(lib, "glew32.lib")
  #endif
#endif

#if defined(WIN32) || defined(_WIN32)
  char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
  char shaderBasePath[1024] = "../openGLHelper-starterCode";
#endif

using namespace std;

// represents one control point along the spline 
struct Point 
{
  double x;
  double y;
  double z;
};

// spline struct 
// contains how many control points the spline has, and an array of control points 
struct Spline 
{
  int numControlPoints;
  Point * points;
};

int mousePos[2]; // x,y coordinate of the mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

int keyState = 1;

// state of the world
float landRotate[3] = { 0.0f, 0.0f, 0.0f };
float landTranslate[3] = { 0.0f, 0.0f, 0.0f };
float landScale[3] = { 1.0f, 1.0f, 1.0f };

glm::mat3x3 neg_scalar = { { -1.0f, 0.0f, 0.0f }, 
                           { 0.0f, -1.0f, 0.0f },
                           { 0.0f, 0.0f, -1.0f } };

int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "OpenGL Roller Coaster Simulation";

int sizeTri_numPoints;
int size_of_tex_mem;

OpenGLMatrix matrix;
BasicPipelineProgram * pipelineProgram;
BasicPipelineProgram * pipelineProgramTex;

GLuint splines_VBO, normals_VBO;
GLuint splines_VAO;

GLuint textures_pos_VBO, textures_uv_VBO;
GLuint textures_VAO;

GLint h_modelViewMatrix, h_projectionMatrix;
GLint tex_modelViewMatrix, tex_projectionMatrix;

GLint mode = 0;

GLuint texHandle;

int image_height;
int image_width;
float scale = 0.08f;

std::vector<glm::vec3> spline_positions;
std::vector<glm::vec3> positions;
std::vector<glm::vec3> spline_points;
std::vector<glm::vec4> spline_colors;

std::vector<glm::vec3> cube_vertices;
std::vector<glm::vec3> cube_normals;

std::vector<glm::vec3> spline_tangents;
std::vector<glm::vec3> spline_normals;
std::vector<glm::vec3> spline_binormals;

std::vector<glm::vec3> tex_position;
std::vector<glm::vec2> tex_uv;

float alpha = 0.05f;

std::vector<glm::vec3> cube_triangles;

int screenshot_count = 0;
string screenshots_path = "Screenshots\\";

// the spline array 
Spline * splines;
// total number of splines 
int numSplines;

//Catmull-Rom Spline constants
float u_step = 0.001f;
float s = 0.5f;

//positions of the texture
float x_y_coord = 50.0f;
float z_coord = -1.0f;

size_t curr_spline_pos = 0;

int velocity = 10;

void setTextureUnit(GLint unit);

bool going_forward = true;
bool take_screenshots = false;

clock_t start, stop, increment, mid;

using Clock = std::chrono::steady_clock;

Clock::time_point lastScreenshotTime = Clock::now();

int loadSplines(char * argv) 
{
  //char * cName = (char *) malloc(128 * sizeof(char));
  // Improvment. Now the buffer exists automatically for the duration of the function
  // it is not necessary to call free(cName)
  char cName[128] = {};
  FILE * fileList;
  FILE * fileSpline;
  int iType, i = 0, j, iLength;

  // load the track file 
  fileList = fopen(argv, "r");
  if (fileList == NULL) 
  {
    fprintf(stderr, "Error: Cannot open track file: %s\n", argv);
    exit(1);
  }
  
  // stores the number of splines in a global variable 
  // fscanf(fileList, "%d", &numSplines);
      // This handles missing, malformed, and nonpositive spline counts.
  if (fscanf(fileList, "%d", &numSplines) != 1 || numSplines <= 0)
  {
      fprintf(stderr, "Error: Invalid number of splines.\n");
      fclose(fileList);
      return 1;
  }


  splines = (Spline*) malloc(numSplines * sizeof(Spline));

  if (splines == nullptr)
  {
      fprintf(stderr, "Error: Failed to allocate spline array.\n");
      fclose(fileList);
      return 1;
  }

  // reads through the spline files 
  for (j = 0; j < numSplines; j++) 
  {
    i = 0;
    // Return value ignored here. Use improved version.
    // fscanf(fileList, "%s", cName); 
    // 
      //The %127s limits the input to 127 characters plus the null terminator.
    if (fscanf(fileList, "%127s", cName) != 1)
    {
        fprintf(stderr, "Error: Could not read spline filename.\n");
        fclose(fileList);
        return 1;
    }

    fileSpline = fopen(cName, "r");

    if (fileSpline == NULL) 
    {
      fprintf(stderr, "Error: Cannot open spline file: %s\n", cName);
      exit(1);
    }

    // gets length for spline file
    //fscanf(fileSpline, "%d %d", &iLength, &iType);

    // fixed scanning
    if (fscanf(fileSpline, "%d %d", &iLength, &iType) != 2 || iLength <= 0)
    {
        fprintf(stderr, "Error: Invalid spline header in %s.\n", cName);
        fclose(fileSpline);
        fclose(fileList);
        return 1;
    }


    // allocate memory for all the points
    splines[j].points = (Point *)malloc(iLength * sizeof(Point));

    // addresses the potential null pointer dereference.
    if (splines[j].points == nullptr)
    {
        fprintf(stderr, "Error: Failed to allocate points for %s.\n", cName);
        fclose(fileSpline);
        fclose(fileList);
        return 1;
    }

    splines[j].numControlPoints = iLength;

    //// saves the data to the struct
    //while (fscanf(fileSpline, "%lf %lf %lf", 
	   //&splines[j].points[i].x, 
	   //&splines[j].points[i].y, 
	   //&splines[j].points[i].z) != EOF) 
    //{
    //  i++;
    //}


    // Read exactly the number of control points specified by the spline file
    //This improves code in three ways :
    // Memory safety : The loop never accesses an index greater than or equal to iLength.
    // Input validation : It verifies that each control point contains three valid coordinates.
    // Error reporting : It identifies which control point failed to load.
    for (int i = 0; i < iLength; i++)
    {
        if (fscanf(fileSpline, "%lf %lf %lf",
            &splines[j].points[i].x,
            &splines[j].points[i].y,
            &splines[j].points[i].z) != 3)
        {
            fprintf(stderr,
                "Error: Invalid control point %d in spline file %s.\n",
                i, cName);

            fclose(fileSpline);
            fclose(fileList);
            return 1;
        }
    }
    fclose(fileSpline);
  }

  fclose(fileList);
  //free(cName);

  return 0;
}

int initTexture(const char * imageFilename, GLuint textureHandle)
{
  // read the texture image
  ImageIO img;
  ImageIO::fileFormatType imgFormat;
  ImageIO::errorType err = img.load(imageFilename, &imgFormat);

  if (err != ImageIO::OK) 
  {
    printf("Loading texture from %s failed.\n", imageFilename);
    return -1;
  }

  // check that the number of bytes is a multiple of 4
  if (img.getWidth() * img.getBytesPerPixel() % 4) 
  {
    printf("Error (%s): The width*numChannels in the loaded image must be a multiple of 4.\n", imageFilename);
    return -1;
  }

  // allocate space for an array of pixels
  int width = img.getWidth();
  int height = img.getHeight();
  unsigned char * pixelsRGBA = new unsigned char[4 * width * height]; // we will use 4 bytes per pixel, i.e., RGBA

  // fill the pixelsRGBA array with the image pixels
  memset(pixelsRGBA, 0, 4 * width * height); // set all bytes to 0
  for (int h = 0; h < height; h++)
    for (int w = 0; w < width; w++) 
    {
      // assign some default byte values (for the case where img.getBytesPerPixel() < 4)
      pixelsRGBA[4 * (h * width + w) + 0] = 0; // red
      pixelsRGBA[4 * (h * width + w) + 1] = 0; // green
      pixelsRGBA[4 * (h * width + w) + 2] = 0; // blue
      pixelsRGBA[4 * (h * width + w) + 3] = 255; // alpha channel; fully opaque

      // set the RGBA channels, based on the loaded image
      int numChannels = img.getBytesPerPixel();
      for (int c = 0; c < numChannels; c++) // only set as many channels as are available in the loaded image; the rest get the default value
        pixelsRGBA[4 * (h * width + w) + c] = img.getPixel(w, h, c);
    }

  // bind the texture
  glBindTexture(GL_TEXTURE_2D, textureHandle);

  // initialize the texture
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelsRGBA);

  // generate the mipmaps for this texture
  glGenerateMipmap(GL_TEXTURE_2D);

  // set the texture parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  // query support for anisotropic texture filtering
  GLfloat fLargest;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
  printf("Max available anisotropic samples: %f\n", fLargest);
  // set anisotropic texture filtering
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.5f * fLargest);

  // query for any errors
  GLenum errCode = glGetError();
  if (errCode != 0) 
  {
    printf("Texture initialization error. Error code: %d.\n", errCode);
    return -1;
  }
  
  // de-allocate the pixel array -- it is no longer needed
  delete [] pixelsRGBA;

  return 0;
}

// write a screenshot to the specified filename
void saveScreenshot(const char * filename)
{
  unsigned char * screenshotData = new unsigned char[windowWidth * windowHeight * 3];
  glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

  ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

  if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
    cout << "File " << filename << " saved successfully." << endl;
  else cout << "Failed to save file " << filename << '.' << endl;

  delete [] screenshotData;
}

void displayFunc()
{
  // render some stuff...
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  //Acquire program handle for PipelinePrograms
  GLuint program = pipelineProgram->GetProgramHandle();

  GLuint program2 = pipelineProgramTex->GetProgramHandle();

  //setting up the camera
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.LoadIdentity();
  
  //matrix.LookAt(0, 0, 5, 0, 0, 0, 0, 1, 0);
  //matrix.LookAt(image_width / 2.0f, 100, 20, image_width / 2.0f, 0, image_height / (-2.0f), 0, 0, -1);

  //the position of the camera will bhe the current position on the spline, but shifted "upward" away from the actual rail by a constanst value
  glm::vec3 eye = positions[curr_spline_pos] + (spline_binormals[curr_spline_pos] * (-2 * alpha));

  //the forward, or center, of the eye will be it's current position pointed in the forward direction
  glm::vec3 center = eye + spline_tangents[curr_spline_pos];

  //the up in this case is the "right" of the camera, to alight with the "top" of the tracks, negated to match with the camera orientation
  glm::vec3 up = spline_binormals[curr_spline_pos] * neg_scalar;

  matrix.LookAt(eye.x, eye.y, eye.z, center.x, center.y, center.z, up.x, up.y, up.z);

  //setting the view matrix
  float view[16];
  matrix.GetMatrix(view); //read the view matrix

  glm::mat4x4 view_mat = { {view[0], view[4], view[8], view[12]},
                     {view[1], view[5], view[9], view[13]},
                     {view[2], view[6], view[10], view[14]},
                     {view[3], view[7], view[11], view[15]} };

  GLint h_viewLightDirection = glGetUniformLocation(program, "viewLightDirection");
  glm::vec4 lightDirection = { 0.0f, 1.0f, 0.0f, 0.0f }; // the "Sun" at noon

  glm::vec4 vlightdir4 = view_mat * lightDirection ;

  float viewLightDirection[3] = { vlightdir4.x, vlightdir4.y, vlightdir4.z };

  // upload viewLightDirection to the GPU
  pipelineProgram->Bind();
  glUniform3fv(h_viewLightDirection, 1, viewLightDirection);

  matrix.Scale(landScale[0], landScale[1], landScale[2]);

  matrix.Rotate(landRotate[0], 1, 0, 0);
  matrix.Rotate(landRotate[1], 0, 1, 0);
  matrix.Rotate(landRotate[2], 0, 0, 1);

  matrix.Translate(landTranslate[0], landTranslate[1], landTranslate[2]);

  //SECTION FOR UPLOADING TO GPU
  h_modelViewMatrix = glGetUniformLocation(program, "modelViewMatrix");
  tex_modelViewMatrix = glGetUniformLocation(program2, "modelViewMatrix");

  //setting the ModelView matrix
  float m[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetMatrix(m);

  GLboolean isRowMajor = GL_FALSE;

  //binding the PipeLine program we would like to use
  pipelineProgram->Bind();
  glUniformMatrix4fv(h_modelViewMatrix, 1, isRowMajor, m);

  //binding the PipeLine program we would like to use
  pipelineProgramTex->Bind();
  glUniformMatrix4fv(tex_modelViewMatrix, 1, isRowMajor, m);

  //setting the ProjectionView matrix
  h_projectionMatrix = glGetUniformLocation(program, "projectionMatrix");
  tex_projectionMatrix = glGetUniformLocation(program2, "projectionMatrix");

  float p[16];
  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.GetMatrix(p);

  pipelineProgram->Bind();

  //sending the modelview up to the GPU
  glUniformMatrix4fv(h_projectionMatrix, 1, isRowMajor, p);

  // set variable
  pipelineProgram->SetModelViewMatrix(m);
  pipelineProgram->SetProjectionMatrix(p);

  pipelineProgramTex->Bind();
  //sending the modelview up to the GPU
  glUniformMatrix4fv(tex_projectionMatrix, 1, isRowMajor, p);

  pipelineProgramTex->SetModelViewMatrix(m);
  pipelineProgramTex->SetProjectionMatrix(p);

  //NORMAL MATRIX SECTION
  GLint h_normalMatrix = glGetUniformLocation(program, "normalMatrix");

  float n[16];
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
  matrix.GetNormalMatrix(n);

  pipelineProgram->Bind();
  glUniformMatrix4fv(h_normalMatrix, 1, isRowMajor, n);

  //texture  binding section
  pipelineProgramTex->Bind();

  setTextureUnit(GL_TEXTURE0);

  glBindTexture(GL_TEXTURE_2D, texHandle);

  //binding and drawing the VAO
  glBindVertexArray(textures_VAO);
  glDrawArrays(GL_TRIANGLES, 0, size_of_tex_mem);
  glBindVertexArray(0);

  pipelineProgram->Bind();

    //binding and drawing the VAO
    glBindVertexArray(splines_VAO);
    glDrawArrays(GL_TRIANGLES, 0, sizeTri_numPoints);
    glBindVertexArray(0);


  //this is necessary to be added at the end of displayFunc()
  glutSwapBuffers();
}

void setTextureUnit(GLint unit)
{
    glActiveTexture(unit);

    GLint h_textureImage = glGetUniformLocation(pipelineProgramTex->GetProgramHandle(), "textureImage");

    glUniform1i(h_textureImage, unit - GL_TEXTURE0);
}

void idleFunc()
{
  // Calculate position
    curr_spline_pos += velocity;

    if ((curr_spline_pos >= positions.size() - 50) && going_forward) 
    {
        velocity *= -1;
        going_forward = false;
    }

    if (curr_spline_pos <= 50 && !going_forward)
    {
        velocity *= -1;
        going_forward = true;
    }

    // for example, here, you can save the screenshots to disk (to make the animation)
    // increment += (clock() - mid);

    // mid = clock();

    if (take_screenshots)
    {
        const auto now = Clock::now();

        const float elapsedSeconds =
            std::chrono::duration<float>(
                now - lastScreenshotTime
            ).count();

        if (elapsedSeconds >= 0.15f)
        {
            lastScreenshotTime = now;

            std::ostringstream filename;
            filename << screenshots_path
                << std::setfill('0')
                << std::setw(3)
                << screenshot_count
                << ".jpg";

            saveScreenshot(filename.str().c_str());

            screenshot_count++;
        }
    }

    // this is also necessary to be added to the end of idleFunc()

    // make the screen update 
    glutPostRedisplay();
    //if (take_screenshots)
    //{
    //    if (increment >= 0.15f)
    //    {
    //        increment = 0.0f;

    //        // take a screenshot
    //        if (screenshot_count < 10)
    //        {
    //            saveScreenshot((screenshots_path + "00" + to_string(screenshot_count) + ".jpg").c_str());
    //        }
    //        else if (screenshot_count >= 10 && screenshot_count < 100)
    //        {
    //            saveScreenshot((screenshots_path + "0" + to_string(screenshot_count) + ".jpg").c_str());
    //        }
    //        else
    //        {
    //            saveScreenshot((screenshots_path + to_string(screenshot_count) + ".jpg").c_str());
    //        }

    //        screenshot_count++;
    //    }
    //}

}

void reshapeFunc(int w, int h)
{
  glViewport(0, 0, w, h);

  matrix.SetMatrixMode(OpenGLMatrix::Projection);
  matrix.LoadIdentity();
  matrix.Perspective(60.0f, (float)w / (float)h, 0.01f, 1000.0f);

  //good practice
  matrix.SetMatrixMode(OpenGLMatrix::ModelView);
}

void mouseMotionDragFunc(int x, int y)
{
  // mouse has moved and one of the mouse buttons is pressed (dragging)

  // the change in mouse position since the last invocation of this function
  int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

  switch (controlState)
  {
    // translate the landscape
    case TRANSLATE:
      if (leftMouseButton)
      {
        // control x,y translation via the left mouse button
        landTranslate[0] += mousePosDelta[0] * 0.01f;
        landTranslate[1] -= mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z translation via the middle mouse button
        landTranslate[2] += mousePosDelta[1] * 0.01f;
      }
      break;

    // rotate the landscape
    case ROTATE:
      if (leftMouseButton)
      {
        // control x,y rotation via the left mouse button
        landRotate[0] += mousePosDelta[1];
        landRotate[1] += mousePosDelta[0];
      }
      if (middleMouseButton)
      {
        // control z rotation via the middle mouse button
        landRotate[2] += mousePosDelta[1];
      }
      break;

    // scale the landscape
    case SCALE:
      if (leftMouseButton)
      {
        // control x,y scaling via the left mouse button
        landScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
        landScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      if (middleMouseButton)
      {
        // control z scaling via the middle mouse button
        landScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
      }
      break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
  // mouse has moved
  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
  // a mouse button has has been pressed or depressed

  // keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables
  switch (button)
  {
    case GLUT_LEFT_BUTTON:
      leftMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_MIDDLE_BUTTON:
      middleMouseButton = (state == GLUT_DOWN);
    break;

    case GLUT_RIGHT_BUTTON:
      rightMouseButton = (state == GLUT_DOWN);
    break;
  }

  // keep track of whether CTRL and SHIFT keys are pressed
  switch (glutGetModifiers())
  {
    case GLUT_ACTIVE_CTRL:
      controlState = TRANSLATE;
    break;

    case GLUT_ACTIVE_SHIFT:
      controlState = SCALE;
    break;

    // if CTRL and SHIFT are not pressed, we are in rotate mode
    default:
      controlState = ROTATE;
    break;
  }

  // store the new mouse position
  mousePos[0] = x;
  mousePos[1] = y;
}

void CalcCubesAndNormals(glm::vec3 vert1, glm::vec3 vert2, glm::vec3 vert3, glm::vec3 vert4, glm::vec3 vert5, glm::vec3 vert6, glm::vec3 vert7, glm::vec3 vert8)
{
    cube_triangles.push_back(vert1); cube_triangles.push_back(vert2); cube_triangles.push_back(vert3);
    glm::vec3 norm0 = glm::normalize(glm::cross((vert1 - vert2), (vert3 - vert2)));
    cube_normals.push_back(norm0);
    cube_normals.push_back(norm0);
    cube_normals.push_back(norm0);

    cube_triangles.push_back(vert3); cube_triangles.push_back(vert4); cube_triangles.push_back(vert1);
    glm::vec3 norm1 = glm::normalize(glm::cross((vert1 - vert3), (vert4 - vert3)));
    cube_normals.push_back(norm1);
    cube_normals.push_back(norm1);
    cube_normals.push_back(norm1);

    cube_triangles.push_back(vert3); cube_triangles.push_back(vert2); cube_triangles.push_back(vert6);
    glm::vec3 norm2 = glm::normalize(glm::cross((vert3 - vert2), (vert6 - vert2)));
    cube_normals.push_back(norm2);
    cube_normals.push_back(norm2);
    cube_normals.push_back(norm2);

    cube_triangles.push_back(vert6); cube_triangles.push_back(vert7); cube_triangles.push_back(vert3);
    glm::vec3 norm3 = glm::normalize(glm::cross((vert3 - vert6), (vert7 - vert6)));
    cube_normals.push_back(norm3);
    cube_normals.push_back(norm3);
    cube_normals.push_back(norm3);

    cube_triangles.push_back(vert1); cube_triangles.push_back(vert5); cube_triangles.push_back(vert6);
    glm::vec3 norm4 = glm::normalize(glm::cross((vert6 - vert1), (vert5 - vert1)));
    cube_normals.push_back(norm4);
    cube_normals.push_back(norm4);
    cube_normals.push_back(norm4);

    cube_triangles.push_back(vert6); cube_triangles.push_back(vert2); cube_triangles.push_back(vert1);
    glm::vec3 norm5 = glm::normalize(glm::cross((vert2 - vert1), (vert6 - vert1)));
    cube_normals.push_back(norm5);
    cube_normals.push_back(norm5);
    cube_normals.push_back(norm5);

    cube_triangles.push_back(vert4); cube_triangles.push_back(vert8); cube_triangles.push_back(vert7);
    glm::vec3 norm6 = glm::normalize(glm::cross((vert8 - vert4), (vert7 - vert4)));
    cube_normals.push_back(norm6);
    cube_normals.push_back(norm6);
    cube_normals.push_back(norm6);

    cube_triangles.push_back(vert7); cube_triangles.push_back(vert3); cube_triangles.push_back(vert4);
    glm::vec3 norm7 = glm::normalize(glm::cross((vert7 - vert4), (vert3 - vert4)));
    cube_normals.push_back(norm7);
    cube_normals.push_back(norm7);
    cube_normals.push_back(norm7);

    cube_triangles.push_back(vert4); cube_triangles.push_back(vert1); cube_triangles.push_back(vert5);
    glm::vec3 norm8 = glm::normalize(glm::cross((vert5 - vert1), (vert4 - vert1)));
    cube_normals.push_back(norm8);
    cube_normals.push_back(norm8);
    cube_normals.push_back(norm8);

    cube_triangles.push_back(vert5); cube_triangles.push_back(vert8); cube_triangles.push_back(vert4);
    glm::vec3 norm9 = glm::normalize(glm::cross((vert8 - vert5), (vert4 - vert5)));
    cube_normals.push_back(norm9);
    cube_normals.push_back(norm9);
    cube_normals.push_back(norm9);

    cube_triangles.push_back(vert8); cube_triangles.push_back(vert5); cube_triangles.push_back(vert6);
    glm::vec3 norm10 = glm::normalize(glm::cross((vert6 - vert5), (vert8 - vert5)));
    cube_normals.push_back(norm10);
    cube_normals.push_back(norm10);
    cube_normals.push_back(norm10);

    cube_triangles.push_back(vert6); cube_triangles.push_back(vert7); cube_triangles.push_back(vert8);
    glm::vec3 norm11 = glm::normalize(glm::cross((vert7 - vert6), (vert8 - vert6)));
    cube_normals.push_back(norm11);
    cube_normals.push_back(norm11);
    cube_normals.push_back(norm11);
}

void keyboardFunc(unsigned char key, int x, int y)
{
  switch (key)
  {
    case 27: // ESC key
        stop = clock() - start;
        printf("%s: %f\n", "total time", (stop / 1000.0f));
        getchar();
      exit(0); // exit the program
    break;

    case 'x':
        // take a screenshot
        take_screenshots = !take_screenshots;
    break;

    case '1':
        keyState = 1;
    break;

    case '2':
        keyState = 2;
    break;
  }
}


void initScene(int argc, char* argv[])
{
   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

  //fill data with Catmull-Rom Spline formula

   //basis matrix
   glm::mat4x4 basis = { {-s, s*2, -s, 0}, 
                        {2 -s, s - 3, 0.0f, 1.0f}, 
                        {s - 2, 3 - (2*s), s, 0.0f}, 
                        {s, -s, 0.0f, 0.0f} };

    //fill positions array and also calculate for tangents and normals
    for (int i = 0; i < numSplines; i++) {
        for (int j = 0; j < (splines[i].numControlPoints - 3); j++) {

            Point p0, p1, p2, p3;

            p0 = splines[i].points[j];
            p1 = splines[i].points[j + 1];
            p2 = splines[i].points[j + 2];
            p3 = splines[i].points[j + 3];


            //glm::mat4x3 control = { {x0, y0, z0}, {x1, y1, z1},{x2, y2, z2},{x3, y3, z3} };
            glm::mat3x4 control = { {p0.x, p1.x, p2.x, p3.x}, {p0.y, p1.y, p2.y, p3.y}, {p0.z, p1.z, p2.z, p3.z} };


            for (float u = 0.0; u <= 1.0; u += u_step) {

                glm::vec4 param = { 0.0, 0.0, u , 1.0 };

                //CALCULATING POSITIONS OF THE SPLINE
                //create the parameter vector
                param.x = static_cast<float>(pow(u, 3));
                param.y = static_cast<float>(pow(u, 2));

                positions.push_back(param * (basis * control));

                //TANGENT VECTOR SECTION
                //calculating the tangent vector
                glm::vec4 tangent = { 0.0, 0.0, 1.0, 0.0 };

                tangent.x = 3 * static_cast<float>(pow(u, 2));
                tangent.y = 2 * u;

                glm::vec3 tanToPush = glm::normalize(tangent * (basis * control));

                spline_tangents.push_back(tanToPush);
               }
           }
       }

     //building the spline positions from given positions
       for (size_t i = 0; i < positions.size(); ++i)
       {
           spline_positions.push_back(positions[i]);
           //not the first or the last posittion, add same position again
           if (i > 0 && i < positions.size() - 1)
           {
               spline_positions.push_back(positions[i]);
           }

           //if at last point, add the very first point
           if (i == (positions.size() - 1))
           {
               //spline_positions.push_back(positions[i]);
               spline_positions.push_back(positions[0]);
           }
       }

       //first normal and binormal using the arbitrary vector
       glm::vec3 z_vec = { 0.0f, 0.0f, 1.0f };
       spline_normals.push_back(glm::normalize(glm::cross(spline_tangents[0], z_vec)));
       spline_binormals.push_back(glm::normalize(glm::cross(spline_tangents[0], spline_normals[0])));

       //loop through to the the remaining tangents
       for (size_t i = 1; i < spline_tangents.size(); i++) {
           spline_normals.push_back(glm::normalize(glm::cross(spline_binormals[i - 1], spline_tangents[i])));
           spline_binormals.push_back(glm::normalize(glm::cross(spline_tangents[i], spline_normals[i])));
       }
       
       for (size_t i = 0; i < spline_tangents.size(); i++) {
           glm::vec3 position_0 = positions[i];
           glm::vec3 n_0 = spline_normals[i];
           glm::vec3 b_0 = spline_binormals[i];
           glm::vec3 neg_n_0 = n_0 * neg_scalar;
           glm::vec3 neg_b_0 = b_0 * neg_scalar;

           cube_vertices.push_back(position_0 + ((neg_n_0 + b_0) * alpha));
           cube_vertices.push_back(position_0 + ((n_0 + b_0) * alpha));
           cube_vertices.push_back(position_0 + ((n_0 + neg_b_0) * alpha));
           cube_vertices.push_back(position_0 + ((neg_n_0 + neg_b_0) * alpha));
       }

       
       for (size_t j = 0; j < (positions.size() - 1) * 4; j += 4) {
           glm::vec3 vert_1 = cube_vertices[j];
           glm::vec3 vert_2 = cube_vertices[j + 1];
           glm::vec3 vert_3 = cube_vertices[j + 2];
           glm::vec3 vert_4 = cube_vertices[j + 3];
           glm::vec3 vert_5 = cube_vertices[j + 4];
           glm::vec3 vert_6 = cube_vertices[j + 5];
           glm::vec3 vert_7 = cube_vertices[j + 6];
           glm::vec3 vert_8 = cube_vertices[j + 7];

           CalcCubesAndNormals(vert_1, vert_2, vert_3, vert_4, vert_5, vert_6, vert_7, vert_8);
       }


  glGenBuffers(1, &splines_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, splines_VBO);
  glBufferData(GL_ARRAY_BUFFER, cube_triangles.size() * sizeof(glm::vec3),
           cube_triangles.data(), GL_STATIC_DRAW);


  glGenBuffers(1, &normals_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, normals_VBO);
  glBufferData(GL_ARRAY_BUFFER, cube_normals.size() * sizeof(glm::vec3),
      cube_normals.data(), GL_STATIC_DRAW);

  //pipeline creation
  pipelineProgram = new BasicPipelineProgram;
  int ret = pipelineProgram->Init(shaderBasePath);
  if (ret != 0) abort();
 
  //pipeline for texture creation section
  pipelineProgramTex = new BasicPipelineProgram;
  int ret2 = pipelineProgramTex->BuildShadersFromFiles("../openGLHelper-starterCode", "tex.vertexShader.glsl", "tex.fragmentShader.glsl");
  if (ret2 != 0) abort();

  //bind the texture handle
  glGenTextures(1, &texHandle);

  int code = initTexture("texture/starrysky.jpg", texHandle);
  if (code != 0)
  {
      printf("Error loading the texture image.\n");
      exit(EXIT_FAILURE);
  }

  //Creates the ground 
  glm::vec3 vertA1 = { -x_y_coord, -x_y_coord, z_coord };
  glm::vec2 uv_A1 = { 0.0f, 1.0f };
  tex_position.push_back(vertA1);
  tex_uv.push_back(uv_A1);

  glm::vec3 vertA2 = { -x_y_coord, x_y_coord, z_coord };
  glm::vec2 uv_A2 = { 0.0f, 0.0f };
  tex_position.push_back(vertA2);
  tex_uv.push_back(uv_A2);

  glm::vec3 vertA3 = { x_y_coord, -x_y_coord, z_coord };
  glm::vec2 uv_A3 = { 1.0f, 1.0f };
  tex_position.push_back(vertA3);
  tex_uv.push_back(uv_A3);

  glm::vec3 vertB1 = { x_y_coord, -x_y_coord, z_coord };
  glm::vec2 uv_B1 = { 1.0f, 1.0f };
  tex_position.push_back(vertB1);
  tex_uv.push_back(uv_B1);

  glm::vec3 vertB2 = { x_y_coord, x_y_coord, z_coord };
  glm::vec2 uv_B2 = { 1.0f, 0.0f };
  tex_position.push_back(vertB2);
  tex_uv.push_back(uv_B2);

  glm::vec3 vertB3 = { -x_y_coord, x_y_coord, z_coord };
  glm::vec2 uv_B3 = { 0.0f, 0.0f };
  tex_position.push_back(vertB3);
  tex_uv.push_back(uv_B3);


  glGenBuffers(1, &textures_pos_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, textures_pos_VBO);
  glBufferData(GL_ARRAY_BUFFER, tex_position.size() * sizeof(glm::vec3),
      tex_position.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &textures_uv_VBO);
  glBindBuffer(GL_ARRAY_BUFFER, textures_uv_VBO);
  glBufferData(GL_ARRAY_BUFFER, tex_uv.size() * sizeof(glm::vec2),
      tex_uv.data(), GL_STATIC_DRAW);

  glGenVertexArrays(1, &textures_VAO);
  glBindVertexArray(textures_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, textures_pos_VBO);

  GLuint loc =
      glGetAttribLocation(pipelineProgramTex->GetProgramHandle(), "position");
  glEnableVertexAttribArray(loc);
  glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

  glBindBuffer(GL_ARRAY_BUFFER, textures_uv_VBO);
  loc =
      glGetAttribLocation(pipelineProgramTex->GetProgramHandle(), "texCoord");
  glEnableVertexAttribArray(loc);
  glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);

  //VAO creation of points
  glGenVertexArrays(1, &splines_VAO);
  glBindVertexArray(splines_VAO);
  glBindBuffer(GL_ARRAY_BUFFER, splines_VBO);

  GLuint loc1 =
      glGetAttribLocation(pipelineProgram->GetProgramHandle(), "position");
  glEnableVertexAttribArray(loc1);
  glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

  glBindBuffer(GL_ARRAY_BUFFER, normals_VBO);
  loc1 = glGetAttribLocation(pipelineProgram->GetProgramHandle(), "normal");
  glEnableVertexAttribArray(loc1);
  glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

  glBindVertexArray(0);


  pipelineProgram->Bind();
  GLint La = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "La");
  GLfloat laVal[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  glUniform4fv(La, 1, laVal);

  GLint Ld = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "Ld");
  GLfloat ldVal[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  glUniform4fv(Ld, 1, ldVal);

  GLint Ls = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "Ls");
  GLfloat lsVal[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  glUniform4fv(Ls, 1, lsVal);

  GLint ka = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "ka");
  GLfloat kaVal[4] = { 0.24725f, 0.1995f, 0.0745f, 1.0f };
  glUniform4fv(ka, 1, kaVal);

  GLint kd = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "kd");
  GLfloat kdVal[4] = { 0.75164f, 0.60648f, 0.22648f, 1.0f };
  glUniform4fv(kd, 1, kdVal);

  GLint ks = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "ks");
  GLfloat ksVal[4] = { 0.628281f, 0.555802f, 0.366065f, 1.0f };
  glUniform4fv(ks, 1, ksVal);

  GLint f_alpha = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "alpha");
  GLfloat fragment_alpha = 75.0f;
  glUniform1f(f_alpha, fragment_alpha);

  GLint mode = glGetUniformLocation(pipelineProgram->GetProgramHandle(), "mode");
  glUniform1i(mode, 1);


  glEnable(GL_DEPTH_TEST);

  //number of vertices
  sizeTri_numPoints = cube_triangles.size();
  size_of_tex_mem = tex_position.size() + tex_uv.size();

  //set the rollercoaster's velocity
  //velocity = numberOfVertices / speed;

  std::cout << "GL error: " << glGetError() << std::endl;
}

// Note: You should combine this file
// with the solution of homework 1.

// Note for Windows/MS Visual Studio:
// You should set argv[1] to track.txt.
// To do this, on the "Solution Explorer",
// right click your project, choose "Properties",
// go to "Configuration Properties", click "Debug",
// then type your track file name for the "Command Arguments".
// You can also repeat this process for the "Release" configuration.

int main (int argc, char ** argv)
{
  if (argc<2)
  {  
    printf ("usage: %s <trackfile>\n", argv[0]);
    exit(0);
  }
  
  cout << "Initializing GLUT..." << endl;
  glutInit(&argc,argv);

  cout << "Initializing OpenGL..." << endl;

  #ifdef __APPLE__
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #else
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
  #endif

  glutInitWindowSize(windowWidth, windowHeight);
  glutInitWindowPosition(0, 0);  
  glutCreateWindow(windowTitle);

  cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
  cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
  cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(windowWidth - 1, windowHeight - 1);
  #endif

  // tells glut to use a particular display function to redraw 
  glutDisplayFunc(displayFunc);
  // perform animation inside idleFunc
  glutIdleFunc(idleFunc);
  // callback for mouse drags
  glutMotionFunc(mouseMotionDragFunc);
  // callback for idle mouse movement
  glutPassiveMotionFunc(mouseMotionFunc);
  // callback for mouse button changes
  glutMouseFunc(mouseButtonFunc);
  // callback for resizing the window
  glutReshapeFunc(reshapeFunc);
  // callback for pressing the keys on the keyboard
  glutKeyboardFunc(keyboardFunc);

  // init glew
  #ifdef __APPLE__
    // nothing is needed on Apple
  #else
    // Windows, Linux
    GLint result = glewInit();
    if (result != GLEW_OK)
    {
      cout << "error: " << glewGetErrorString(result) << endl;
      exit(EXIT_FAILURE);
    }
  #endif

    start = clock();

    mid = start;
    // load the splines from the provided filename
    loadSplines(argv[1]);

  // do initialization
  initScene(argc, argv);

  printf("Loaded %d spline(s).\n", numSplines);
  for(int i=0; i<numSplines; i++)
    printf("Num control points in spline %d: %d.\n", i, splines[i].numControlPoints);

  // sink forever into the glut loop
  glutMainLoop();

  //return 0;
}

