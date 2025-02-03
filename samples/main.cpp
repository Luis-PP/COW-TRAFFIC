// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#define _CRT_SECURE_NO_WARNINGS
#define _CRTDBG_MAP_ALLOC
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS 1

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "draw.h"
#include "sample.h"
#include "settings.h"

#include "box2d/base.h"
#include "box2d/box2d.h"
#include "box2d/math_functions.h"

#include <glad/glad.h>
// Keep glad.h before glfw3.h
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#ifdef BOX2D_PROFILE
#include <tracy/Tracy.hpp>
#else
#define FrameMark
#endif

#define IM_MIN(A, B) (((A) < (B)) ? (A) : (B))

#if defined(_WIN32)
#include <crtdbg.h>

static int MyAllocHook(int allocType, void *userData, size_t size, int blockType, long requestNumber,
					   const unsigned char *filename, int lineNumber)
{
	// This hook can help find leaks
	if (size == 143)
	{
		size += 0;
	}

	return 1;
}
#endif

GLFWwindow *g_mainWindow = nullptr;
static int32_t s_selection = 0;
static Sample *s_sample = nullptr;
static Settings s_settings;
static bool s_rightMouseDown = false;
static b2Vec2 s_clickPointWS = b2Vec2_zero;
static float s_windowScale = 1.0f;
static float s_framebufferScale = 1.0f;

inline bool IsPowerOfTwo(int32_t x)
{
	return (x != 0) && ((x & (x - 1)) == 0);
}

void *AllocFcn(uint32_t size, int32_t alignment)
{
	// Allocation must be a multiple of alignment or risk a seg fault
	// https://en.cppreference.com/w/c/memory/aligned_alloc
	assert(IsPowerOfTwo(alignment));
	size_t sizeAligned = ((size - 1) | (alignment - 1)) + 1;
	assert((sizeAligned & (alignment - 1)) == 0);

#if defined(_WIN64)
	void *ptr = _aligned_malloc(sizeAligned, alignment);
#else
	void *ptr = aligned_alloc(alignment, sizeAligned);
#endif
	assert(ptr != nullptr);
	return ptr;
}

void FreeFcn(void *mem)
{
#if defined(_WIN64)
	_aligned_free(mem);
#else
	free(mem);
#endif
}

std::vector<ImU32> color_cell_arr = {IM_COL32(156, 74, 3, 240), IM_COL32(168, 5, 5, 240), IM_COL32(5, 227, 97, 240), IM_COL32(6, 122, 2, 240), IM_COL32(4, 159, 201, 240), IM_COL32(201, 119, 4, 240), IM_COL32(77, 77, 77, 240)};

// Define a vector to store selected cell coordinates
std::vector<SampleFunctionalArea> selected_cells;

int AssertFcn(const char *condition, const char *fileName, int lineNumber)
{
	printf("SAMPLE ASSERTION: %s, %s, line %d\n", condition, fileName, lineNumber);
	return 1;
}

void glfwErrorCallback(int error, const char *description)
{
	fprintf(stderr, "GLFW error occurred. Code: %d. Description: %s\n", error, description);
}

static inline int CompareSamples(const void *a, const void *b)
{
	SampleEntry *sa = (SampleEntry *)a;
	SampleEntry *sb = (SampleEntry *)b;

	int result = strcmp(sa->category, sb->category);
	if (result == 0)
	{
		result = strcmp(sa->name, sb->name);
	}

	return result;
}

static void SortTests()
{
	qsort(g_sampleEntries, g_sampleCount, sizeof(SampleEntry), CompareSamples);
}

static void RestartSample()
{
	delete s_sample;
	s_sample = nullptr;
	s_settings.restart = true;
	s_sample = g_sampleEntries[s_settings.sampleIndex].createFcn(s_settings);
	s_settings.restart = false;
}

static void CreateUI(GLFWwindow *window, const char *glslVersion)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	bool success = ImGui_ImplGlfw_InitForOpenGL(window, false);
	if (success == false)
	{
		printf("ImGui_ImplGlfw_InitForOpenGL failed\n");
		assert(false);
	}

	success = ImGui_ImplOpenGL3_Init(glslVersion);
	if (success == false)
	{
		printf("ImGui_ImplOpenGL3_Init failed\n");
		assert(false);
	}

	// this doesn't look that good
	// Search for font file
	const char *fontPath = "samples/data/droid_sans.ttf";
	FILE *file = fopen(fontPath, "rb");

	if (file != NULL)
	{
		ImFontConfig fontConfig;
		fontConfig.RasterizerMultiply = s_windowScale * s_framebufferScale;
		ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath, 14.0f, &fontConfig);
		ImGui::GetIO().Fonts->AddFontFromFileTTF(fontPath, 48.0f, &fontConfig);
	}
	else
	{
		printf("\n\nERROR: the Box2D samples working directory must be the top level Box2D directory (same as README.md)\n\n");
		exit(EXIT_FAILURE);
	}
}

static void DestroyUI()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

static void ResizeWindowCallback(GLFWwindow *, int width, int height)
{
	g_camera.m_width = int(width / s_windowScale);
	g_camera.m_height = int(height / s_windowScale);
	s_settings.windowWidth = int(width / s_windowScale);
	s_settings.windowHeight = int(height / s_windowScale);
}

static void KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
	if (ImGui::GetIO().WantCaptureKeyboard)
	{
		return;
	}

	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_ESCAPE:
			// Quit
			glfwSetWindowShouldClose(g_mainWindow, GL_TRUE);
			break;

		case GLFW_KEY_LEFT:
			// Pan left
			if (mods == GLFW_MOD_CONTROL)
			{
				b2Vec2 newOrigin = {2.0f, 0.0f};
				s_sample->ShiftOrigin(newOrigin);
			}
			else
			{
				g_camera.m_center.x -= 0.5f;
			}
			break;

		case GLFW_KEY_RIGHT:
			// Pan right
			if (mods == GLFW_MOD_CONTROL)
			{
				b2Vec2 newOrigin = {-2.0f, 0.0f};
				s_sample->ShiftOrigin(newOrigin);
			}
			else
			{
				g_camera.m_center.x += 0.5f;
			}
			break;

		case GLFW_KEY_DOWN:
			// Pan down
			if (mods == GLFW_MOD_CONTROL)
			{
				b2Vec2 newOrigin = {0.0f, 2.0f};
				s_sample->ShiftOrigin(newOrigin);
			}
			else
			{
				g_camera.m_center.y -= 0.5f;
			}
			break;

		case GLFW_KEY_UP:
			// Pan up
			if (mods == GLFW_MOD_CONTROL)
			{
				b2Vec2 newOrigin = {0.0f, -2.0f};
				s_sample->ShiftOrigin(newOrigin);
			}
			else
			{
				g_camera.m_center.y += 0.5f;
			}
			break;

		case GLFW_KEY_HOME:
			g_camera.ResetView();
			break;

		case GLFW_KEY_R:
			RestartSample();
			break;

		case GLFW_KEY_O:
			s_settings.singleStep = true;
			break;

		case GLFW_KEY_P:
			s_settings.pause = !s_settings.pause;
			break;

		case GLFW_KEY_LEFT_BRACKET:
			// Switch to previous test
			--s_selection;
			if (s_selection < 0)
			{
				s_selection = g_sampleCount - 1;
			}
			break;

		case GLFW_KEY_RIGHT_BRACKET:
			// Switch to next test
			++s_selection;
			if (s_selection == g_sampleCount)
			{
				s_selection = 0;
			}
			break;

		case GLFW_KEY_TAB:
			g_draw.m_showUI = !g_draw.m_showUI;

		default:
			if (s_sample)
			{
				s_sample->Keyboard(key);
			}
		}
	}
}

static void CharCallback(GLFWwindow *window, unsigned int c)
{
	ImGui_ImplGlfw_CharCallback(window, c);
}

static void MouseButtonCallback(GLFWwindow *window, int32_t button, int32_t action, int32_t mods)
{
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	if (ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}

	double xd, yd;
	glfwGetCursorPos(g_mainWindow, &xd, &yd);
	b2Vec2 ps = {float(xd) / s_windowScale, float(yd) / s_windowScale};

	// Use the mouse to move things around.
	if (button == GLFW_MOUSE_BUTTON_1)
	{
		b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);
		if (action == GLFW_PRESS)
		{
			s_sample->MouseDown(pw, button, mods);
		}

		if (action == GLFW_RELEASE)
		{
			s_sample->MouseUp(pw, button);
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_2)
	{
		if (action == GLFW_PRESS)
		{
			s_clickPointWS = g_camera.ConvertScreenToWorld(ps);
			s_rightMouseDown = true;
		}

		if (action == GLFW_RELEASE)
		{
			s_rightMouseDown = false;
		}
	}
}

static void MouseMotionCallback(GLFWwindow *window, double xd, double yd)
{
	b2Vec2 ps = {float(xd) / s_windowScale, float(yd) / s_windowScale};

	ImGui_ImplGlfw_CursorPosCallback(window, ps.x, ps.y);

	b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);
	s_sample->MouseMove(pw);

	if (s_rightMouseDown)
	{
		b2Vec2 diff = b2Sub(pw, s_clickPointWS);
		g_camera.m_center.x -= diff.x;
		g_camera.m_center.y -= diff.y;
		s_clickPointWS = g_camera.ConvertScreenToWorld(ps);
	}
}

static void ScrollCallback(GLFWwindow *window, double dx, double dy)
{
	ImGui_ImplGlfw_ScrollCallback(window, dx, dy);
	if (ImGui::GetIO().WantCaptureMouse)
	{
		return;
	}

	if (dy > 0)
	{
		g_camera.m_zoom /= 1.1f;
	}
	else
	{
		g_camera.m_zoom *= 1.1f;
	}
}

static void UpdateUI()
{
	int maxWorkers = enki::GetNumHardwareThreads();

	float menuWidth = 180.0f;
	if (g_draw.m_showUI)
	{
		ImGui::SetNextWindowPos({g_camera.m_width - menuWidth - 10.0f, 10.0f});
		ImGui::SetNextWindowSize({menuWidth, g_camera.m_height - 20.0f});

		ImGui::Begin("Tools", &g_draw.m_showUI,
					 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

		if (ImGui::BeginTabBar("ControlTabs", ImGuiTabBarFlags_None))
		{
			if (ImGui::BeginTabItem("Controls"))
			{
				ImGui::PushItemWidth(100.0f);
				ImGui::SliderInt("Sub-steps", &s_settings.subStepCount, 1, 50);
				ImGui::SliderFloat("Hertz", &s_settings.hertz, 5.0f, 120.0f, "%.0f hz");

				if (ImGui::SliderInt("Workers", &s_settings.workerCount, 1, maxWorkers))
				{
					s_settings.workerCount = b2ClampInt(s_settings.workerCount, 1, maxWorkers);
					RestartSample();
				}
				ImGui::PopItemWidth();

				ImGui::Separator();

				ImGui::Checkbox("Sleep", &s_settings.enableSleep);
				ImGui::Checkbox("Warm Starting", &s_settings.enableWarmStarting);
				ImGui::Checkbox("Continuous", &s_settings.enableContinuous);

				ImGui::Separator();

				ImGui::Checkbox("Shapes", &s_settings.drawShapes);
				ImGui::Checkbox("Joints", &s_settings.drawJoints);
				ImGui::Checkbox("Joint Extras", &s_settings.drawJointExtras);
				ImGui::Checkbox("AABBs", &s_settings.drawAABBs);
				ImGui::Checkbox("Contact Points", &s_settings.drawContactPoints);
				ImGui::Checkbox("Contact Normals", &s_settings.drawContactNormals);
				ImGui::Checkbox("Contact Impulses", &s_settings.drawContactImpulses);
				ImGui::Checkbox("Friction Impulses", &s_settings.drawFrictionImpulses);
				ImGui::Checkbox("Center of Masses", &s_settings.drawMass);
				ImGui::Checkbox("Graph Colors", &s_settings.drawGraphColors);
				ImGui::Checkbox("Counters", &s_settings.drawCounters);
				ImGui::Checkbox("Profile", &s_settings.drawProfile);

				ImVec2 button_sz = ImVec2(-1, 0);
				if (ImGui::Button("Pause (P)", button_sz))
				{
					s_settings.pause = !s_settings.pause;
				}

				if (ImGui::Button("Single Step (O)", button_sz))
				{
					s_settings.singleStep = !s_settings.singleStep;
				}

				if (ImGui::Button("Dump Mem Stats", button_sz))
				{
					b2World_DumpMemoryStats(s_sample->m_worldId);
				}

				if (ImGui::Button("Reset Profile", button_sz))
				{
					s_sample->ResetProfile();
				}

				if (ImGui::Button("Restart (R)", button_sz))
				{
					RestartSample();
				}

				if (ImGui::Button("Quit", button_sz))
				{
					glfwSetWindowShouldClose(g_mainWindow, GL_TRUE);
				}

				ImGui::EndTabItem();
			}

			ImGuiTreeNodeFlags leafNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
			leafNodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

			if (ImGui::BeginTabItem("Tests"))
			{
				int categoryIndex = 0;
				const char *category = g_sampleEntries[categoryIndex].category;
				int i = 0;
				while (i < g_sampleCount)
				{
					bool categorySelected = strcmp(category, g_sampleEntries[s_settings.sampleIndex].category) == 0;
					ImGuiTreeNodeFlags nodeSelectionFlags = categorySelected ? ImGuiTreeNodeFlags_Selected : 0;
					bool nodeOpen = ImGui::TreeNodeEx(category, nodeFlags | nodeSelectionFlags);

					if (nodeOpen)
					{
						while (i < g_sampleCount && strcmp(category, g_sampleEntries[i].category) == 0)
						{
							ImGuiTreeNodeFlags selectionFlags = 0;
							if (s_settings.sampleIndex == i)
							{
								selectionFlags = ImGuiTreeNodeFlags_Selected;
							}
							ImGui::TreeNodeEx((void *)(intptr_t)i, leafNodeFlags | selectionFlags, "%s",
											  g_sampleEntries[i].name);
							if (ImGui::IsItemClicked())
							{
								s_selection = i;
							}
							++i;
						}
						ImGui::TreePop();
					}
					else
					{
						while (i < g_sampleCount && strcmp(category, g_sampleEntries[i].category) == 0)
						{
							++i;
						}
					}

					if (i < g_sampleCount)
					{
						category = g_sampleEntries[i].category;
						categoryIndex = i;
					}
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();

		s_sample->UpdateUI();
	}
}

// Function to handle cell selection
void SelectCell(int type, int orientation, float x, float y)
{
	SampleFunctionalArea func_area;
	func_area.type = type;
	func_area.orientation = orientation;
	func_area.x = x;
	func_area.y = y;

	selected_cells.erase(std::remove_if(selected_cells.begin(), selected_cells.end(), [&](SampleFunctionalArea const &cell)
										{ return (cell.x == x && cell.y == y); }),
						 selected_cells.end());

	if (type != 7)
	{
		selected_cells.push_back(func_area);
	}
}

static void ShowTools()
{
	if (g_draw.m_showUI)
	{
		if (!ImGui::Begin("Settings", NULL, ImGuiWindowFlags_NoMove))
		{
			ImGui::End();
			return;
		}
		if (ImGui::BeginTabBar("Settings"))
		{

			if (ImGui::BeginTabItem("Canvas"))
			{
				// Initialize grid size variables
				static int grid_size_x = 30;
				static int grid_size_y = 30;
				static ImU32 cell_colors[100][100] = {};	 // Assuming a max grid size of 100x100
				static int cell_orientations[100][100] = {}; // Array to store orientation of each cell

				// DragInt widgets to control grid size
				ImGui::DragInt("Grid Size X", &grid_size_x, 0.5, 1, 100);
				ImGui::DragInt("Grid Size Y", &grid_size_y, 0.5, 1, 100);

				std::pair<int, int> corner_layout(grid_size_x, grid_size_y);
				s_sample->corner_layout = corner_layout;

				// Define canvas size
				ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
				ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
				if (canvas_sz.x < 100.0f)
					canvas_sz.x = 100.0f;
				if (canvas_sz.y < 100.0f)
					canvas_sz.y = 100.0f;

				// Adjust canvas size for square cells
				float cell_size = IM_MIN(canvas_sz.x / grid_size_x, canvas_sz.y / grid_size_y);
				canvas_sz.x = cell_size * grid_size_x;
				canvas_sz.y = cell_size * grid_size_y;

				ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

				// Render options on the right
				static int selected_fa = 0;
				ImGui::SetCursorScreenPos(ImVec2(canvas_p1.x + 20, canvas_p0.y + 40));
				ImGui::BeginGroup();
				ImGui::RadioButton("Cubicle", &selected_fa, 0);
				ImGui::RadioButton("Milking Robot", &selected_fa, 1);
				ImGui::RadioButton("Feeder", &selected_fa, 2);
				ImGui::RadioButton("Concentrate", &selected_fa, 3);
				ImGui::RadioButton("Drinker", &selected_fa, 4);
				ImGui::RadioButton("Docking station", &selected_fa, 5);
				ImGui::RadioButton("Obstacle", &selected_fa, 6);
				ImGui::RadioButton("Empty", &selected_fa, 7);
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				static int current_orientation = 2;
				if (selected_fa == 2 || selected_fa == 4 || selected_fa == 5 || selected_fa == 6 || selected_fa == 7)
				{
					current_orientation = 0;
				}
				else
				{
					if (current_orientation == 0)
					{
						current_orientation = 1;
					}
					else
					{
						ImGui::RadioButton("Vertical", &current_orientation, 1);
						ImGui::RadioButton("Horizontal", &current_orientation, 2);
					}
				}
				ImGui::EndGroup();

				// Draw the canvas
				ImDrawList *draw_list = ImGui::GetWindowDrawList();
				draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(195, 182, 181, int(0.75 * 255)));

				// Draw cell borders
				static bool show_grid = true;
				ImGui::SetCursorScreenPos(ImVec2(canvas_p1.x + 20, canvas_p0.y + 10));
				ImGui::Checkbox("Show grid", &show_grid);
				ImU32 border_color = IM_COL32(200, 200, 200, 255);
				if (show_grid)
				{
					for (int x = 0; x <= grid_size_x; x++)
					{
						ImVec2 p0 = ImVec2(canvas_p0.x + x * cell_size, canvas_p0.y);
						ImVec2 p1 = ImVec2(canvas_p0.x + x * cell_size, canvas_p1.y);
						draw_list->AddLine(p0, p1, border_color);
					}
					for (int y = 0; y <= grid_size_y; y++)
					{
						ImVec2 p0 = ImVec2(canvas_p0.x, canvas_p0.y + y * cell_size);
						ImVec2 p1 = ImVec2(canvas_p1.x, canvas_p0.y + y * cell_size);
						draw_list->AddLine(p0, p1, border_color);
					}
				}

				// Handle mouse interaction
				ImGuiIO &io = ImGui::GetIO();
				bool is_mouse_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
				bool is_mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
				ImVec2 mouse_pos = io.MousePos;

				// Handle cell rendering and interaction
				for (int y = 0; y < grid_size_y; y++)
				{
					for (int x = 0; x < grid_size_x; x++)
					{
						ImVec2 cell_p0 = ImVec2(canvas_p0.x + x * cell_size, canvas_p0.y + y * cell_size);
						ImVec2 cell_p1 = ImVec2(cell_p0.x + cell_size, cell_p0.y + cell_size);

						// Render the cells based on their stored orientation
						int orientation = cell_orientations[y][x];
						if (orientation == 1 && y < grid_size_y - 1 && cell_colors[y][x] != 0)
						{
							cell_p1 = ImVec2(cell_p0.x + cell_size, cell_p0.y + 2 * cell_size);
						}
						else if (orientation == 2 && x < grid_size_x - 1 && cell_colors[y][x] != 0)
						{
							cell_p1 = ImVec2(cell_p0.x + 2 * cell_size, cell_p0.y + cell_size);
						}

						// Fill cell with its color if it has one
						if (cell_colors[y][x] != 0)
						{
							draw_list->AddRectFilled(cell_p0, cell_p1, cell_colors[y][x]);
						}

						// Handle cell click and drag only if not dragging a primitive
						if (is_mouse_down && ImGui::IsMouseHoveringRect(cell_p0, cell_p1))
						{

							if (current_orientation == 1) // Vertical
							{
								if (y < grid_size_y - 1) // Check bounds to prevent overflow
								{
									SelectCell(selected_fa, current_orientation, x, y);

									cell_colors[y][x] = color_cell_arr[selected_fa];
									// cell_colors[y + 1][x] = IM_COL32(0, 0, 0, 255);
									cell_orientations[y][x] = 1;
									// cell_orientations[y + 1][x] = 1;
								}
							}
							else if (current_orientation == 2) // Horizontal
							{
								if (x < grid_size_x - 1) // Check bounds to prevent overflow
								{
									SelectCell(selected_fa, current_orientation, x, y);

									cell_colors[y][x] = color_cell_arr[selected_fa];
									// cell_colors[y][x + 1] = IM_COL32(0, 0, 0, 255);
									cell_orientations[y][x] = 2;
									// cell_orientations[y][x + 1] = 2;
								}
							}
							else // Default case
							{
								SelectCell(selected_fa, current_orientation, x, y);

								cell_colors[y][x] = color_cell_arr[selected_fa];
								cell_orientations[y][x] = 0;
							}
						}

						// Re-render the cells with updated colors
						if (cell_colors[y][x] != 0)
						{
							draw_list->AddRectFilled(cell_p0, cell_p1, cell_colors[y][x]);
						}
					}
				}

				// // Adding buttons
				ImGui::SetCursorScreenPos(ImVec2(canvas_p1.x + 20, canvas_p1.y - 80));
				// ImGui::SameLine();
				ImGui::BeginGroup();

				if (ImGui::Button("Render"))
					s_sample->layout = selected_cells;

				if (ImGui::Button("Save"))
					;

				if (ImGui::Button("Load"))
				{
					;
				}
				ImGui::EndGroup();

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Parameters"))
			{
				ImGui::SeparatorText("General");

				ImGui::SeparatorText("Cows");

				static int number_of_cows = 50;
				ImGui::InputInt("Number of cows", &number_of_cows);

				static int evade_probability = 75;
				ImGui::SliderInt("Evade probability [%]", &evade_probability, 0, 100);

				ImGui::SeparatorText("Robots");

				// Using the generic BeginCombo() API, you have full control over how to display the combo contents.
				// (your selection data could be an index, a pointer to the object, an id for the object, a flag intrusively
				// stored in the object itself, etc.)
				const char *strategy_char[] = {"Divide terrain", "Divide terrain shared docking", "Formations"};
				static int strategy_selected_idx = 0; // Here we store our selection data as an index.

				// Pass in the preview value visible before opening the combo (it could technically be different contents or not pulled from items[])
				const char *combo_preview_value = strategy_char[strategy_selected_idx];
				if (ImGui::BeginCombo("Strategy", combo_preview_value))
				{
					for (int n = 0; n < IM_ARRAYSIZE(strategy_char); n++)
					{
						const bool is_selected = (strategy_selected_idx == n);
						if (ImGui::Selectable(strategy_char[n], is_selected))
							strategy_selected_idx = n;

						// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
						if (is_selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				static bool elimination_known = false;
				ImGui::Checkbox("Elimination location is known", &elimination_known);

				static bool avoid_obstacle = false;
				ImGui::Checkbox("Avoid cows", &avoid_obstacle);

				static int robot_size = 120;
				ImGui::SliderInt("Robot size [cm]", &robot_size, 40, 160);

				// ImGui::SetCursorScreenPos(ImVec2(canvas_p1.x + 20, canvas_p1.y - 80));

				if (ImGui::Button("Render"))
				{

					s_sample->number_of_cows = number_of_cows;
					s_sample->evade_probability = evade_probability;
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Plots"))
			{
				static bool animate = true;
				ImGui::Checkbox("Animate", &animate);

				// Plot as lines and plot as histogram
				static float arr[] = {0.6f, 0.1f, 1.0f, 0.5f, 0.92f, 0.1f, 0.2f};
				ImGui::PlotLines("Frame Times", arr, IM_ARRAYSIZE(arr));
				ImGui::PlotHistogram("Histogram", arr, IM_ARRAYSIZE(arr), 0, NULL, 0.0f, 1.0f, ImVec2(0, 80.0f));
				// ImGui::SameLine(); HelpMarker("Consider using ImPlot instead!");

				// Fill an array of contiguous float values to plot
				// Tip: If your float aren't contiguous but part of a structure, you can pass a pointer to your first float
				// and the sizeof() of your structure in the "stride" parameter.
				static float values[90] = {};
				static int values_offset = 0;
				static double refresh_time = 0.0;
				if (!animate || refresh_time == 0.0)
					refresh_time = ImGui::GetTime();
				while (refresh_time < ImGui::GetTime()) // Create data at fixed 60 Hz rate for the demo
				{
					static float phase = 0.0f;
					values[values_offset] = cosf(phase);
					values_offset = (values_offset + 1) % IM_ARRAYSIZE(values);
					phase += 0.10f * values_offset;
					refresh_time += 1.0f / 60.0f;
				}

				// Plots can display overlay texts
				// (in this example, we will display an average value)
				{
					float average = 0.0f;
					for (int n = 0; n < IM_ARRAYSIZE(values); n++)
						average += values[n];
					average /= (float)IM_ARRAYSIZE(values);
					char overlay[32];
					sprintf(overlay, "avg %f", average);
					ImGui::PlotLines("Lines", values, IM_ARRAYSIZE(values), values_offset, overlay, -1.0f, 1.0f, ImVec2(0, 80.0f));
				}

				// Use functions to generate output
				// FIXME: This is actually VERY awkward because current plot API only pass in indices.
				// We probably want an API passing floats and user provide sample rate/count.
				struct Funcs
				{
					static float Sin(void *, int i) { return sinf(i * 0.1f); }
					static float Saw(void *, int i) { return (i & 1) ? 1.0f : -1.0f; }
				};
				static int func_type = 0, display_count = 70;
				ImGui::SeparatorText("Functions");
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
				ImGui::Combo("func", &func_type, "Sin\0Saw\0");
				ImGui::SameLine();
				ImGui::SliderInt("Sample count", &display_count, 1, 400);
				float (*func)(void *, int) = (func_type == 0) ? Funcs::Sin : Funcs::Saw;
				ImGui::PlotLines("Lines", func, NULL, display_count, 0, NULL, -1.0f, 1.0f, ImVec2(0, 80));
				ImGui::PlotHistogram("Histogram", func, NULL, display_count, 0, NULL, -1.0f, 1.0f, ImVec2(0, 80));
				ImGui::Separator();

				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
		ImGui::End();
	}
	s_sample->ShowTools();
}

//
int main(int, char **)
{
#if defined(_WIN32)
	// Enable memory-leak reports
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG | _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	//_CrtSetAllocHook(MyAllocHook);

	// How to break at the leaking allocation, in the watch window enter this variable
	// and set it to the allocation number in {}. Do this at the first line in main.
	// {,,ucrtbased.dll}_crtBreakAlloc = <allocation number> 3970
#endif

	// Install memory hooks
	b2SetAllocator(AllocFcn, FreeFcn);
	b2SetAssertFcn(AssertFcn);

	char buffer[128];

	s_settings.Load();
	s_settings.workerCount = b2MinInt(8, (int)enki::GetNumHardwareThreads() / 2);
	SortTests();

	glfwSetErrorCallback(glfwErrorCallback);

	g_camera.m_width = s_settings.windowWidth;
	g_camera.m_height = s_settings.windowHeight;

	if (glfwInit() == 0)
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

#if __APPLE__
	const char *glslVersion = "#version 150";
#else
	const char *glslVersion = nullptr;
#endif

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// MSAA
	glfwWindowHint(GLFW_SAMPLES, 4);

	b2Version version = b2GetVersion();
	snprintf(buffer, 128, "Cow Traffic powered by Box2D Version %d.%d.%d", version.major, version.minor, version.revision);

	if (GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor())
	{
#ifdef __APPLE__
		glfwGetMonitorContentScale(primaryMonitor, &s_framebufferScale, &s_framebufferScale);
#else
		glfwGetMonitorContentScale(primaryMonitor, &s_windowScale, &s_windowScale);
#endif
	}

	bool fullscreen = false;
	if (fullscreen)
	{
		g_mainWindow = glfwCreateWindow(int(1920 * s_windowScale), int(1080 * s_windowScale), buffer,
										glfwGetPrimaryMonitor(), nullptr);
	}
	else
	{
		g_mainWindow = glfwCreateWindow(int(g_camera.m_width * s_windowScale), int(g_camera.m_height * s_windowScale),
										buffer, nullptr, nullptr);
	}

	if (g_mainWindow == nullptr)
	{
		fprintf(stderr, "Failed to open GLFW g_mainWindow.\n");
		glfwTerminate();
		return -1;
	}

#ifdef __APPLE__
	glfwGetWindowContentScale(g_mainWindow, &s_framebufferScale, &s_framebufferScale);
#else
	glfwGetWindowContentScale(g_mainWindow, &s_windowScale, &s_windowScale);
#endif

	glfwMakeContextCurrent(g_mainWindow);

	// Load OpenGL functions using glad
	if (!gladLoadGL())
	{
		fprintf(stderr, "Failed to initialize glad\n");
		glfwTerminate();
		return -1;
	}

	printf("GL %d.%d\n", GLVersion.major, GLVersion.minor);
	printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

	glfwSetWindowSizeCallback(g_mainWindow, ResizeWindowCallback);
	glfwSetKeyCallback(g_mainWindow, KeyCallback);
	glfwSetCharCallback(g_mainWindow, CharCallback);
	glfwSetMouseButtonCallback(g_mainWindow, MouseButtonCallback);
	glfwSetCursorPosCallback(g_mainWindow, MouseMotionCallback);
	glfwSetScrollCallback(g_mainWindow, ScrollCallback);

	// todo put this in s_settings
	CreateUI(g_mainWindow, glslVersion);
	g_draw.Create();

	s_settings.sampleIndex = b2ClampInt(s_settings.sampleIndex, 0, g_sampleCount - 1);
	s_selection = s_settings.sampleIndex;

	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

	float frameTime = 0.0;

	int32_t frame = 0;

	while (!glfwWindowShouldClose(g_mainWindow))
	{
		double time1 = glfwGetTime();

		if (glfwGetKey(g_mainWindow, GLFW_KEY_Z) == GLFW_PRESS)
		{
			// Zoom out
			g_camera.m_zoom = b2MinFloat(1.005f * g_camera.m_zoom, 100.0f);
		}
		else if (glfwGetKey(g_mainWindow, GLFW_KEY_X) == GLFW_PRESS)
		{
			// Zoom in
			g_camera.m_zoom = b2MaxFloat(0.995f * g_camera.m_zoom, 0.5f);
		}

		glfwGetWindowSize(g_mainWindow, &g_camera.m_width, &g_camera.m_height);
		g_camera.m_width = int(g_camera.m_width / s_windowScale);
		g_camera.m_height = int(g_camera.m_height / s_windowScale);

		int bufferWidth, bufferHeight;
		glfwGetFramebufferSize(g_mainWindow, &bufferWidth, &bufferHeight);
		glViewport(0, 0, bufferWidth, bufferHeight);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		g_draw.DrawBackground();

		double cursorPosX = 0, cursorPosY = 0;
		glfwGetCursorPos(g_mainWindow, &cursorPosX, &cursorPosY);
		ImGui_ImplGlfw_CursorPosCallback(g_mainWindow, cursorPosX / s_windowScale, cursorPosY / s_windowScale);
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui_ImplGlfw_CursorPosCallback(g_mainWindow, cursorPosX / s_windowScale, cursorPosY / s_windowScale);

		ImGuiIO &io = ImGui::GetIO();
		io.DisplaySize.x = float(g_camera.m_width);
		io.DisplaySize.y = float(g_camera.m_height);
		io.DisplayFramebufferScale.x = bufferWidth / float(g_camera.m_width);
		io.DisplayFramebufferScale.y = bufferHeight / float(g_camera.m_height);

		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(float(g_camera.m_width), float(g_camera.m_height)));
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::Begin("Overlay", nullptr,
					 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
						 ImGuiWindowFlags_NoScrollbar);
		ImGui::End();

		if (s_sample == nullptr)
		{
			// delayed creation because imgui doesn't create fonts until NewFrame() is called
			s_sample = g_sampleEntries[s_settings.sampleIndex].createFcn(s_settings);
		}

		if (g_draw.m_showUI)
		{
			const SampleEntry &entry = g_sampleEntries[s_settings.sampleIndex];
			snprintf(buffer, 128, "%s : %s", entry.category, entry.name);
			s_sample->DrawTitle(buffer);
		}

		s_sample->Step(s_settings);

		g_draw.Flush();

		UpdateUI();

		ShowTools();

		ImGui::ShowDemoWindow();

		// if (g_draw.m_showUI)
		{
			snprintf(buffer, 128, "%.1f ms - step %d - camera (%g, %g, %g)", 1000.0f * frameTime, s_sample->m_stepCount,
					 g_camera.m_center.x, g_camera.m_center.y, g_camera.m_zoom);
			// snprintf( buffer, 128, "%.1f ms", 1000.0f * frameTime );

			ImGui::Begin("Overlay", nullptr,
						 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize |
							 ImGuiWindowFlags_NoScrollbar);
			ImGui::SetCursorPos(ImVec2(5.0f, g_camera.m_height - 20.0f));
			ImGui::TextColored(ImColor(153, 230, 153, 255), "%s", buffer);
			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(g_mainWindow);

		// For the Tracy profiler
		FrameMark;

		if (s_selection != s_settings.sampleIndex)
		{
			g_camera.ResetView();
			s_settings.sampleIndex = s_selection;

			// #todo restore all drawing settings that may have been overridden by a sample
			s_settings.subStepCount = 4;
			s_settings.drawJoints = true;
			s_settings.useCameraBounds = false;

			delete s_sample;
			s_sample = nullptr;
			s_sample = g_sampleEntries[s_settings.sampleIndex].createFcn(s_settings);
		}

		glfwPollEvents();

		// Limit frame rate to 60Hz
		double time2 = glfwGetTime();
		double targetTime = time1 + 1.0f / 60.0f;
		int loopCount = 0;
		while (time2 < targetTime)
		{
			b2Yield();
			time2 = glfwGetTime();
			++loopCount;
		}

		frameTime = (float)(time2 - time1);
		// if (frame % 17 == 0)
		//{
		//	printf("loop count = %d, frame time = %.1f\n", loopCount, 1000.0f * frameTime);
		// }
		++frame;
	}

	delete s_sample;
	s_sample = nullptr;

	g_draw.Destroy();

	DestroyUI();
	glfwTerminate();

	s_settings.Save();

#if defined(_WIN32)
	_CrtDumpMemoryLeaks();
#endif

	return 0;
}
