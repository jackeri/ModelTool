#include "UISystem.h"

#include "imgui.h"
#include <GL/glew.h>

#include <cstdio>

namespace mt {

	static char g_GlslVersionString[32] = "";
	static GLuint g_FontTexture = 0;
	static GLuint g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
	static GLint g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;                                // Uniforms location
	static GLuint g_AttribLocationVtxPos = 0, g_AttribLocationVtxUV = 0, g_AttribLocationVtxColor = 0; // Vertex attributes location
	static unsigned int g_VboHandle = 0, g_ElementsHandle = 0;

	static bool setupGLFW(GLFWwindow* window, bool install_callbacks, GlfwClientApi client_api)
	{
		g_Window = window;
		g_Time = 0.0;

		// Setup backend capabilities flags
		ImGuiIO& io = ImGui::GetIO();
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)
		io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;    // We can create multi-viewports on the Platform side (optional)
#if GLFW_HAS_MOUSE_PASSTHROUGH || (GLFW_HAS_WINDOW_HOVERED && defined(_WIN32))
		io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can set io.MouseHoveredViewport correctly (optional, not easy)
#endif
		io.BackendPlatformName = "imgui_impl_glfw";

		// Keyboard mapping. Dear ImGui will use those indices to peek into the io.KeysDown[] array.
		io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
		io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
		io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
		io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

		io.SetClipboardTextFn = ImGui_ImplGlfw_SetClipboardText;
		io.GetClipboardTextFn = ImGui_ImplGlfw_GetClipboardText;
		io.ClipboardUserData = g_Window;

		// Create mouse cursors
		// (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
		// GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
		// Missing cursors will return NULL and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
		GLFWerrorfun prev_error_callback = glfwSetErrorCallback(NULL);
		g_MouseCursors[ImGuiMouseCursor_Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_TextInput] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNS] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeEW] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
#if GLFW_HAS_NEW_CURSORS
		g_MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
    g_MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
#else
		g_MouseCursors[ImGuiMouseCursor_ResizeAll] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
		g_MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
#endif
		glfwSetErrorCallback(prev_error_callback);

		// Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
		g_PrevUserCallbackMousebutton = NULL;
		g_PrevUserCallbackScroll = NULL;
		g_PrevUserCallbackKey = NULL;
		g_PrevUserCallbackChar = NULL;
		g_PrevUserCallbackMonitor = NULL;
		if (install_callbacks)
		{
			g_InstalledCallbacks = true;
			g_PrevUserCallbackMousebutton = glfwSetMouseButtonCallback(window, ImGui_ImplGlfw_MouseButtonCallback);
			g_PrevUserCallbackScroll = glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
			g_PrevUserCallbackKey = glfwSetKeyCallback(window, ImGui_ImplGlfw_KeyCallback);
			g_PrevUserCallbackChar = glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
			g_PrevUserCallbackMonitor = glfwSetMonitorCallback(ImGui_ImplGlfw_MonitorCallback);
		}

		// Update monitors the first time (note: monitor callback are broken in GLFW 3.2 and earlier, see github.com/glfw/glfw/issues/784)
		ImGui_ImplGlfw_UpdateMonitors();
		glfwSetMonitorCallback(ImGui_ImplGlfw_MonitorCallback);

		// Our mouse update function expect PlatformHandle to be filled for the main viewport
		ImGuiViewport* main_viewport = ImGui::GetMainViewport();
		main_viewport->PlatformHandle = (void*)g_Window;
#ifdef _WIN32
		main_viewport->PlatformHandleRaw = glfwGetWin32Window(g_Window);
#endif
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			ImGui_ImplGlfw_InitPlatformInterface();

		g_ClientApi = client_api;
		return true;
	}

	static void ImGui_ImplOpenGL3_SetupRenderState(ImDrawData* draw_data, int fb_width, int fb_height, GLuint vertex_array_object)
	{
		// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glEnable(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
		if (g_GlVersion >= 310)
        glDisable(GL_PRIMITIVE_RESTART);
#endif
#ifdef GL_POLYGON_MODE
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

		// Support for GL 4.5 rarely used glClipControl(GL_UPPER_LEFT)
#if defined(GL_CLIP_ORIGIN) && !defined(__APPLE__)
		bool clip_origin_lower_left = true;
    GLenum current_clip_origin = 0; glGetIntegerv(GL_CLIP_ORIGIN, (GLint*)&current_clip_origin);
    if (current_clip_origin == GL_UPPER_LEFT)
        clip_origin_lower_left = false;
#endif

		// Setup viewport, orthographic projection matrix
		// Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
		glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
		float L = draw_data->DisplayPos.x;
		float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
		float T = draw_data->DisplayPos.y;
		float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
#if defined(GL_CLIP_ORIGIN) && !defined(__APPLE__)
		if (!clip_origin_lower_left) { float tmp = T; T = B; B = tmp; } // Swap top and bottom if origin is upper left
#endif
		const float ortho_projection[4][4] =
				{
						{ 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
						{ 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
						{ 0.0f,         0.0f,        -1.0f,   0.0f },
						{ (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
				};
		glUseProgram(g_ShaderHandle);
		glUniform1i(g_AttribLocationTex, 0);
		glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);

#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
		if (g_GlVersion >= 330)
        glBindSampler(0, 0); // We use combined texture/sampler state. Applications using GL 3.3 may set that otherwise.
#endif

		(void)vertex_array_object;
#ifndef IMGUI_IMPL_OPENGL_ES2
		glBindVertexArray(vertex_array_object);
#endif

		// Bind vertex/index buffers and setup attributes for ImDrawVert
		glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ElementsHandle);
		glEnableVertexAttribArray(g_AttribLocationVtxPos);
		glEnableVertexAttribArray(g_AttribLocationVtxUV);
		glEnableVertexAttribArray(g_AttribLocationVtxColor);
		glVertexAttribPointer(g_AttribLocationVtxPos,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
		glVertexAttribPointer(g_AttribLocationVtxUV,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
		glVertexAttribPointer(g_AttribLocationVtxColor, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));
	}

	static void ImGui_ImplOpenGL3_RenderDrawData(ImDrawData *draw_data)
	{
		// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
		int fb_width = (int) (draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
		int fb_height = (int) (draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
		if (fb_width <= 0 || fb_height <= 0)
		{
			return;
		}

		// Backup GL state
		GLenum last_active_texture;
		glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &last_active_texture);
		glActiveTexture(GL_TEXTURE0);
		GLuint last_program;
		glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *) &last_program);
		GLuint last_texture;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *) &last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
		GLuint last_sampler; if (g_GlVersion >= 330) { glGetIntegerv(GL_SAMPLER_BINDING, (GLint*)&last_sampler); } else { last_sampler = 0; }
#endif
		GLuint last_array_buffer;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *) &last_array_buffer);
#ifndef IMGUI_IMPL_OPENGL_ES2
		GLuint last_vertex_array_object;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint *) &last_vertex_array_object);
#endif
#ifdef GL_POLYGON_MODE
		GLint last_polygon_mode[2];
		glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
#endif
		GLint last_viewport[4];
		glGetIntegerv(GL_VIEWPORT, last_viewport);
		GLint last_scissor_box[4];
		glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
		GLenum last_blend_src_rgb;
		glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *) &last_blend_src_rgb);
		GLenum last_blend_dst_rgb;
		glGetIntegerv(GL_BLEND_DST_RGB, (GLint *) &last_blend_dst_rgb);
		GLenum last_blend_src_alpha;
		glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *) &last_blend_src_alpha);
		GLenum last_blend_dst_alpha;
		glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *) &last_blend_dst_alpha);
		GLenum last_blend_equation_rgb;
		glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *) &last_blend_equation_rgb);
		GLenum last_blend_equation_alpha;
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *) &last_blend_equation_alpha);
		GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
		GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
		GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
		GLboolean last_enable_stencil_test = glIsEnabled(GL_STENCIL_TEST);
		GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
		GLboolean last_enable_primitive_restart = (g_GlVersion >= 310) ? glIsEnabled(GL_PRIMITIVE_RESTART) : GL_FALSE;
#endif

		// Setup desired GL state
		// Recreate the VAO every time (this is to easily allow multiple GL contexts to be rendered to. VAO are not shared among GL contexts)
		// The renderer would actually work without any VAO bound, but then our VertexAttrib calls would overwrite the default one currently bound.
		GLuint vertex_array_object = 0;
#ifndef IMGUI_IMPL_OPENGL_ES2
		glGenVertexArrays(1, &vertex_array_object);
#endif
		ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);

		// Will project scissor/clipping rectangles into framebuffer space
		ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		// Render command lists
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList *cmd_list = draw_data->CmdLists[n];

			// Upload vertex/index buffers
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) cmd_list->VtxBuffer.Size * (int) sizeof(ImDrawVert),
						 (const GLvoid *) cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmd_list->IdxBuffer.Size * (int) sizeof(ImDrawIdx),
						 (const GLvoid *) cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback != NULL)
				{
					// User callback, registered via ImDrawList::AddCallback()
					// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
					if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					{
						ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);
					}
					else
					{
						pcmd->UserCallback(cmd_list, pcmd);
					}
				}
				else
				{
					// Project scissor/clipping rectangles into framebuffer space
					ImVec4 clip_rect;
					clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
					clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
					clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
					clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

					if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
					{
						// Apply scissor/clipping rectangle
						glScissor((int) clip_rect.x, (int) (fb_height - clip_rect.w), (int) (clip_rect.z - clip_rect.x),
								  (int) (clip_rect.w - clip_rect.y));

						// Bind texture, Draw
						glBindTexture(GL_TEXTURE_2D, (GLuint) (intptr_t) pcmd->TextureId);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_VTX_OFFSET
						if (g_GlVersion >= 320)
						glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)pcmd->VtxOffset);
					else
#endif
						glDrawElements(GL_TRIANGLES, (GLsizei) pcmd->ElemCount,
									   sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
									   (void *) (intptr_t) (pcmd->IdxOffset * sizeof(ImDrawIdx)));
					}
				}
			}
		}

		// Destroy the temporary VAO
#ifndef IMGUI_IMPL_OPENGL_ES2
		glDeleteVertexArrays(1, &vertex_array_object);
#endif

		// Restore modified GL state
		glUseProgram(last_program);
		glBindTexture(GL_TEXTURE_2D, last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
		if (g_GlVersion >= 330)
		glBindSampler(0, last_sampler);
#endif
		glActiveTexture(last_active_texture);
#ifndef IMGUI_IMPL_OPENGL_ES2
		glBindVertexArray(last_vertex_array_object);
#endif
		glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
		glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
		glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
		if (last_enable_blend)
		{ glEnable(GL_BLEND); }
		else
		{ glDisable(GL_BLEND); }
		if (last_enable_cull_face)
		{ glEnable(GL_CULL_FACE); }
		else
		{ glDisable(GL_CULL_FACE); }
		if (last_enable_depth_test)
		{ glEnable(GL_DEPTH_TEST); }
		else
		{ glDisable(GL_DEPTH_TEST); }
		if (last_enable_stencil_test)
		{ glEnable(GL_STENCIL_TEST); }
		else
		{ glDisable(GL_STENCIL_TEST); }
		if (last_enable_scissor_test)
		{ glEnable(GL_SCISSOR_TEST); }
		else
		{ glDisable(GL_SCISSOR_TEST); }
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
		if (g_GlVersion >= 310) { if (last_enable_primitive_restart) glEnable(GL_PRIMITIVE_RESTART); else glDisable(GL_PRIMITIVE_RESTART); }
#endif

#ifdef GL_POLYGON_MODE
		glPolygonMode(GL_FRONT_AND_BACK, (GLenum) last_polygon_mode[0]);
#endif
		glViewport(last_viewport[0], last_viewport[1], (GLsizei) last_viewport[2], (GLsizei) last_viewport[3]);
		glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei) last_scissor_box[2],
				  (GLsizei) last_scissor_box[3]);
	}

	bool ImGui_ImplOpenGL3_CreateFontsTexture()
	{
		// Build texture atlas
		ImGuiIO &io = ImGui::GetIO();
		unsigned char *pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width,
									 &height);   // Load as RGBA 32-bit (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

		// Upload texture to graphics system
		GLint last_texture;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
		glGenTextures(1, &g_FontTexture);
		glBindTexture(GL_TEXTURE_2D, g_FontTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_UNPACK_ROW_LENGTH
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		// Store our identifier
		io.Fonts->SetTexID((ImTextureID) (intptr_t) g_FontTexture);

		// Restore state
		glBindTexture(GL_TEXTURE_2D, last_texture);

		return true;
	}

	// If you get an error please report on github. You may try different GL context version or GLSL version. See GL<>GLSL version table at the top of this file.
	static bool CheckShader(GLuint handle, const char *desc)
	{
		GLint status = 0, log_length = 0;
		glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
		glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
		if ((GLboolean) status == GL_FALSE)
		{
			fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to compile %s!\n", desc);
		}
		if (log_length > 1)
		{
			ImVector<char> buf;
			buf.resize((int) (log_length + 1));
			glGetShaderInfoLog(handle, log_length, nullptr, (GLchar *) buf.begin());
			fprintf(stderr, "%s\n", buf.begin());
		}
		return (GLboolean) status == GL_TRUE;
	}

	static bool CheckProgram(GLuint handle, const char *desc)
	{
		GLint status = 0, log_length = 0;
		glGetProgramiv(handle, GL_LINK_STATUS, &status);
		glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
		if ((GLboolean) status == GL_FALSE)
		{
			fprintf(stderr, "ERROR: ImGui_ImplOpenGL3_CreateDeviceObjects: failed to link %s! (with GLSL '%s')\n", desc,
					g_GlslVersionString);
		}
		if (log_length > 1)
		{
			ImVector<char> buf;
			buf.resize((int) (log_length + 1));
			glGetProgramInfoLog(handle, log_length, nullptr, (GLchar *) buf.begin());
			fprintf(stderr, "%s\n", buf.begin());
		}
		return (GLboolean) status == GL_TRUE;
	}

	static bool ImGui_ImplOpenGL3_CreateDeviceObjects()
	{
		// Backup GL state
		GLint last_texture, last_array_buffer;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
#ifndef IMGUI_IMPL_OPENGL_ES2
		GLint last_vertex_array;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
#endif

		// Parse GLSL version string
		int glsl_version = 130;
		sscanf(g_GlslVersionString, "#version %d", &glsl_version);

		const GLchar *vertex_shader_glsl_120 =
				"uniform mat4 ProjMtx;\n"
				"attribute vec2 Position;\n"
				"attribute vec2 UV;\n"
				"attribute vec4 Color;\n"
				"varying vec2 Frag_UV;\n"
				"varying vec4 Frag_Color;\n"
				"void main()\n"
				"{\n"
				"    Frag_UV = UV;\n"
				"    Frag_Color = Color;\n"
				"    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
				"}\n";

		const GLchar *vertex_shader_glsl_130 =
				"uniform mat4 ProjMtx;\n"
				"in vec2 Position;\n"
				"in vec2 UV;\n"
				"in vec4 Color;\n"
				"out vec2 Frag_UV;\n"
				"out vec4 Frag_Color;\n"
				"void main()\n"
				"{\n"
				"    Frag_UV = UV;\n"
				"    Frag_Color = Color;\n"
				"    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
				"}\n";

		const GLchar *vertex_shader_glsl_300_es =
				"precision mediump float;\n"
				"layout (location = 0) in vec2 Position;\n"
				"layout (location = 1) in vec2 UV;\n"
				"layout (location = 2) in vec4 Color;\n"
				"uniform mat4 ProjMtx;\n"
				"out vec2 Frag_UV;\n"
				"out vec4 Frag_Color;\n"
				"void main()\n"
				"{\n"
				"    Frag_UV = UV;\n"
				"    Frag_Color = Color;\n"
				"    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
				"}\n";

		const GLchar *vertex_shader_glsl_410_core =
				"layout (location = 0) in vec2 Position;\n"
				"layout (location = 1) in vec2 UV;\n"
				"layout (location = 2) in vec4 Color;\n"
				"uniform mat4 ProjMtx;\n"
				"out vec2 Frag_UV;\n"
				"out vec4 Frag_Color;\n"
				"void main()\n"
				"{\n"
				"    Frag_UV = UV;\n"
				"    Frag_Color = Color;\n"
				"    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
				"}\n";

		const GLchar *fragment_shader_glsl_120 =
				"#ifdef GL_ES\n"
				"    precision mediump float;\n"
				"#endif\n"
				"uniform sampler2D Texture;\n"
				"varying vec2 Frag_UV;\n"
				"varying vec4 Frag_Color;\n"
				"void main()\n"
				"{\n"
				"    gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV.st);\n"
				"}\n";

		const GLchar *fragment_shader_glsl_130 =
				"uniform sampler2D Texture;\n"
				"in vec2 Frag_UV;\n"
				"in vec4 Frag_Color;\n"
				"out vec4 Out_Color;\n"
				"void main()\n"
				"{\n"
				"    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
				"}\n";

		const GLchar *fragment_shader_glsl_300_es =
				"precision mediump float;\n"
				"uniform sampler2D Texture;\n"
				"in vec2 Frag_UV;\n"
				"in vec4 Frag_Color;\n"
				"layout (location = 0) out vec4 Out_Color;\n"
				"void main()\n"
				"{\n"
				"    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
				"}\n";

		const GLchar *fragment_shader_glsl_410_core =
				"in vec2 Frag_UV;\n"
				"in vec4 Frag_Color;\n"
				"uniform sampler2D Texture;\n"
				"layout (location = 0) out vec4 Out_Color;\n"
				"void main()\n"
				"{\n"
				"    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
				"}\n";

		// Select shaders matching our GLSL versions
		const GLchar *vertex_shader = NULL;
		const GLchar *fragment_shader = NULL;
		if (glsl_version < 130)
		{
			vertex_shader = vertex_shader_glsl_120;
			fragment_shader = fragment_shader_glsl_120;
		}
		else if (glsl_version >= 410)
		{
			vertex_shader = vertex_shader_glsl_410_core;
			fragment_shader = fragment_shader_glsl_410_core;
		}
		else if (glsl_version == 300)
		{
			vertex_shader = vertex_shader_glsl_300_es;
			fragment_shader = fragment_shader_glsl_300_es;
		}
		else
		{
			vertex_shader = vertex_shader_glsl_130;
			fragment_shader = fragment_shader_glsl_130;
		}

		// Create shaders
		const GLchar *vertex_shader_with_version[2] = {g_GlslVersionString, vertex_shader};
		g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(g_VertHandle, 2, vertex_shader_with_version, nullptr);
		glCompileShader(g_VertHandle);
		CheckShader(g_VertHandle, "vertex shader");

		const GLchar *fragment_shader_with_version[2] = {g_GlslVersionString, fragment_shader};
		g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(g_FragHandle, 2, fragment_shader_with_version, nullptr);
		glCompileShader(g_FragHandle);
		CheckShader(g_FragHandle, "fragment shader");

		g_ShaderHandle = glCreateProgram();
		glAttachShader(g_ShaderHandle, g_VertHandle);
		glAttachShader(g_ShaderHandle, g_FragHandle);
		glLinkProgram(g_ShaderHandle);
		CheckProgram(g_ShaderHandle, "shader program");

		g_AttribLocationTex = glGetUniformLocation(g_ShaderHandle, "Texture");
		g_AttribLocationProjMtx = glGetUniformLocation(g_ShaderHandle, "ProjMtx");
		g_AttribLocationVtxPos = (GLuint) glGetAttribLocation(g_ShaderHandle, "Position");
		g_AttribLocationVtxUV = (GLuint) glGetAttribLocation(g_ShaderHandle, "UV");
		g_AttribLocationVtxColor = (GLuint) glGetAttribLocation(g_ShaderHandle, "Color");

		// Create buffers
		glGenBuffers(1, &g_VboHandle);
		glGenBuffers(1, &g_ElementsHandle);

		ImGui_ImplOpenGL3_CreateFontsTexture();

		// Restore modified GL state
		glBindTexture(GL_TEXTURE_2D, last_texture);
		glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
#ifndef IMGUI_IMPL_OPENGL_ES2
		glBindVertexArray(last_vertex_array);
#endif

		return true;
	}

	static void RenderWindowData(ImDrawData *draw_data)
	{
		// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
		int fb_width = (int) (draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
		int fb_height = (int) (draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
		if (fb_width <= 0 || fb_height <= 0)
		{
			return;
		}

		// Backup GL state
		GLenum last_active_texture;
		glGetIntegerv(GL_ACTIVE_TEXTURE, (GLint *) &last_active_texture);
		glActiveTexture(GL_TEXTURE0);
		GLuint last_program;
		glGetIntegerv(GL_CURRENT_PROGRAM, (GLint *) &last_program);
		GLuint last_texture;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint *) &last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
		GLuint last_sampler; if (g_GlVersion >= 330) { glGetIntegerv(GL_SAMPLER_BINDING, (GLint*)&last_sampler); } else { last_sampler = 0; }
#endif
		GLuint last_array_buffer;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint *) &last_array_buffer);
#ifndef IMGUI_IMPL_OPENGL_ES2
		GLuint last_vertex_array_object;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, (GLint *) &last_vertex_array_object);
#endif
#ifdef GL_POLYGON_MODE
		GLint last_polygon_mode[2];
		glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
#endif
		GLint last_viewport[4];
		glGetIntegerv(GL_VIEWPORT, last_viewport);
		GLint last_scissor_box[4];
		glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
		GLenum last_blend_src_rgb;
		glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *) &last_blend_src_rgb);
		GLenum last_blend_dst_rgb;
		glGetIntegerv(GL_BLEND_DST_RGB, (GLint *) &last_blend_dst_rgb);
		GLenum last_blend_src_alpha;
		glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *) &last_blend_src_alpha);
		GLenum last_blend_dst_alpha;
		glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *) &last_blend_dst_alpha);
		GLenum last_blend_equation_rgb;
		glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *) &last_blend_equation_rgb);
		GLenum last_blend_equation_alpha;
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *) &last_blend_equation_alpha);
		GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
		GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
		GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
		GLboolean last_enable_stencil_test = glIsEnabled(GL_STENCIL_TEST);
		GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
		GLboolean last_enable_primitive_restart = (g_GlVersion >= 310) ? glIsEnabled(GL_PRIMITIVE_RESTART) : GL_FALSE;
#endif

		// Setup desired GL state
		// Recreate the VAO every time (this is to easily allow multiple GL contexts to be rendered to. VAO are not shared among GL contexts)
		// The renderer would actually work without any VAO bound, but then our VertexAttrib calls would overwrite the default one currently bound.
		GLuint vertex_array_object = 0;
#ifndef IMGUI_IMPL_OPENGL_ES2
		glGenVertexArrays(1, &vertex_array_object);
#endif
		ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);

		// Will project scissor/clipping rectangles into framebuffer space
		ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
		ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

		// Render command lists
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList *cmd_list = draw_data->CmdLists[n];

			// Upload vertex/index buffers
			glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr) cmd_list->VtxBuffer.Size * (int) sizeof(ImDrawVert),
						 (const GLvoid *) cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) cmd_list->IdxBuffer.Size * (int) sizeof(ImDrawIdx),
						 (const GLvoid *) cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
				if (pcmd->UserCallback != NULL)
				{
					// User callback, registered via ImDrawList::AddCallback()
					// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
					if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
					{
						ImGui_ImplOpenGL3_SetupRenderState(draw_data, fb_width, fb_height, vertex_array_object);
					}
					else
					{
						pcmd->UserCallback(cmd_list, pcmd);
					}
				}
				else
				{
					// Project scissor/clipping rectangles into framebuffer space
					ImVec4 clip_rect;
					clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
					clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
					clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
					clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

					if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
					{
						// Apply scissor/clipping rectangle
						glScissor((int) clip_rect.x, (int) (fb_height - clip_rect.w), (int) (clip_rect.z - clip_rect.x),
								  (int) (clip_rect.w - clip_rect.y));

						// Bind texture, Draw
						glBindTexture(GL_TEXTURE_2D, (GLuint) (intptr_t) pcmd->TextureId);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_VTX_OFFSET
						if (g_GlVersion >= 320)
						glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)pcmd->VtxOffset);
					else
#endif
						glDrawElements(GL_TRIANGLES, (GLsizei) pcmd->ElemCount,
									   sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
									   (void *) (intptr_t) (pcmd->IdxOffset * sizeof(ImDrawIdx)));
					}
				}
			}
		}

		// Destroy the temporary VAO
#ifndef IMGUI_IMPL_OPENGL_ES2
		glDeleteVertexArrays(1, &vertex_array_object);
#endif

		// Restore modified GL state
		glUseProgram(last_program);
		glBindTexture(GL_TEXTURE_2D, last_texture);
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_BIND_SAMPLER
		if (g_GlVersion >= 330)
		glBindSampler(0, last_sampler);
#endif
		glActiveTexture(last_active_texture);
#ifndef IMGUI_IMPL_OPENGL_ES2
		glBindVertexArray(last_vertex_array_object);
#endif
		glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
		glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
		glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
		if (last_enable_blend)
		{ glEnable(GL_BLEND); }
		else
		{ glDisable(GL_BLEND); }
		if (last_enable_cull_face)
		{ glEnable(GL_CULL_FACE); }
		else
		{ glDisable(GL_CULL_FACE); }
		if (last_enable_depth_test)
		{ glEnable(GL_DEPTH_TEST); }
		else
		{ glDisable(GL_DEPTH_TEST); }
		if (last_enable_stencil_test)
		{ glEnable(GL_STENCIL_TEST); }
		else
		{ glDisable(GL_STENCIL_TEST); }
		if (last_enable_scissor_test)
		{ glEnable(GL_SCISSOR_TEST); }
		else
		{ glDisable(GL_SCISSOR_TEST); }
#ifdef IMGUI_IMPL_OPENGL_MAY_HAVE_PRIMITIVE_RESTART
		if (g_GlVersion >= 310) { if (last_enable_primitive_restart) glEnable(GL_PRIMITIVE_RESTART); else glDisable(GL_PRIMITIVE_RESTART); }
#endif

#ifdef GL_POLYGON_MODE
		glPolygonMode(GL_FRONT_AND_BACK, (GLenum) last_polygon_mode[0]);
#endif
		glViewport(last_viewport[0], last_viewport[1], (GLsizei) last_viewport[2], (GLsizei) last_viewport[3]);
		glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei) last_scissor_box[2],
				  (GLsizei) last_scissor_box[3]);
	}

	static void RenderWindow(ImGuiViewport *viewport, void *)
	{
		if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear))
		{
			ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
			glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		RenderWindowData(viewport->DrawData);
	}

	bool UISystem::setup(const char *glslVersion)
	{
		ImGui::CreateContext();

		// Setup backend capabilities flags
		ImGuiIO &io = ImGui::GetIO();
		io.BackendRendererName = "ModelTool";

		// We are using Opengl 3.2 < X
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

		IM_ASSERT((int) strlen(glslVersion) + 2 < IM_ARRAYSIZE(g_GlslVersionString));
		strcpy(g_GlslVersionString, glslVersion);
		strcat(g_GlslVersionString, "\n");

		GLint current_texture;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &current_texture);

		ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
		platform_io.Renderer_RenderWindow = RenderWindow;

		return true;
	}

	void UISystem::draw()
	{
		static float f = 0.0f;
		static int counter = 0;
		static bool show_demo_window = true;
		static bool show_another_window = false;
		static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		ImGui::NewFrame();

		ImGui::Begin(
				"Hello, world!");                          // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
		ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
		ImGui::Checkbox("Another Window", &show_another_window);

		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
		ImGui::ColorEdit3("clear color", (float *) &clear_color); // Edit 3 floats representing a color

		if (ImGui::Button(
				"Button"))
		{                            // Buttons return true when clicked (most widgets return true when edited/activated)
			counter++;
		}
		ImGui::SameLine();
		ImGui::Text("counter = %d", counter);

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
					ImGui::GetIO().Framerate);
		ImGui::End();

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}

	void UISystem::shutdown()
	{
		ImGui::DestroyPlatformWindows();
	}
}