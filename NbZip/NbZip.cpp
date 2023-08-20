#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#include <iostream>
#include <fstream>
#include <string>
#include <raylib.h>
#include <filesystem>
#include <pthread.h>
#include <imgui_internal.h>

#include "rlImGui.h"

#include "nbt/NBTWriter.hpp"
#include "nbt/NBTReader.hpp"

#include "gui_file_dialogs.h"

#include "icon.h"

using namespace ImNBT;

namespace fs = std::filesystem;

bool busy = false;
int file_count = 0;
int finalized_files = 0;
pthread_t work_thread;

namespace ImGui {

	bool BufferingBar(const char* label, float value, const ImVec2& size_arg, const ImU32& bg_col, const ImU32& fg_col) {
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size = size_arg;
		size.x -= style.FramePadding.x * 2;

		const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ItemSize(bb, style.FramePadding.y);
		if (!ItemAdd(bb, id))
			return false;

		// Render
		const float circleStart = size.x * 0.7f;
		const float circleEnd = size.x;
		const float circleWidth = circleEnd - circleStart;

		const float t = g.Time;
		const float r = size.y / 2;
		const float speed = 1.5f;

		const float a = speed * 0;
		const float b = speed * 0.333f;
		const float c = speed * 0.666f;

		const float o1 = (circleWidth + r) * (t + a - speed * (int)((t + a) / speed)) / speed;
		const float o2 = (circleWidth + r) * (t + b - speed * (int)((t + b) / speed)) / speed;
		const float o3 = (circleWidth + r) * (t + c - speed * (int)((t + c) / speed)) / speed;

		window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o1, bb.Min.y + r), r, bg_col);
		window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o2, bb.Min.y + r), r, bg_col);
		window->DrawList->AddCircleFilled(ImVec2(pos.x + circleEnd - o3, bb.Min.y + r), r, bg_col);

		window->DrawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart, bb.Max.y), bg_col);
		window->DrawList->AddRectFilled(bb.Min, ImVec2((pos.x + circleStart) * value, bb.Max.y), fg_col);
	}

	bool Spinner(const char* label, float radius, int thickness, const ImU32& color) {
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size((radius) * 2, (radius + style.FramePadding.y) * 2);

		const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ItemSize(bb, style.FramePadding.y);
		if (!ItemAdd(bb, id))
			return false;

		// Render
		window->DrawList->PathClear();

		int num_segments = 30;
		int start = abs(ImSin(g.Time * 1.8f) * (num_segments - 5));

		const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
		const float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;

		const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

		for (int i = 0; i < num_segments; i++) {
			const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
			window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + g.Time * 8) * radius,
				centre.y + ImSin(a + g.Time * 8) * radius));
		}

		window->DrawList->PathStroke(color, false, thickness);
	}

}

void SetGuiTheme() {
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.6000000238418579f;
	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.WindowRounding = 0.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2(32.0f, 32.0f);
	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(4.0f, 3.0f);
	style.FrameRounding = 0.0f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
	style.CellPadding = ImVec2(4.0f, 2.0f);
	style.IndentSpacing = 20.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 10.30000019073486f;
	style.ScrollbarRounding = 0.0f;
	style.GrabMinSize = 6.300000190734863f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 4.599999904632568f;
	style.TabBorderSize = 0.0f;
	style.TabMinWidthForCloseButton = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.4980392158031464f, 0.4980392158031464f, 0.4980392158031464f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.2000000029802322f, 0.2000000029802322f, 0.2000000029802322f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.2000000029802322f, 0.2000000029802322f, 0.2000000029802322f, 1.0f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.4431372582912445f, 0.4431372582912445f, 0.4431372582912445f, 1.0f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.09019608050584793f, 0.09019608050584793f, 0.09019608050584793f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.105882354080677f, 0.1098039224743843f, 0.1098039224743843f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.0784313753247261f, 0.0784313753247261f, 0.0784313753247261f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.03921568766236305f, 0.03921568766236305f, 0.03921568766236305f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.03921568766236305f, 0.03921568766236305f, 0.03921568766236305f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.5099999904632568f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1372549086809158f, 0.1372549086809158f, 0.1372549086809158f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.1529411822557449f, 0.1490196138620377f, 0.1490196138620377f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3176470696926117f, 0.3176470696926117f, 0.3176470696926117f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.3607843220233917f, 0.3607843220233917f, 0.3607843220233917f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.05098039284348488f, 0.8196078538894653f, 0.4000000059604645f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.0f, 0.5215686559677124f, 0.2588235437870026f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.0f, 0.3921568691730499f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.0f, 0.5215686559677124f, 0.2588235437870026f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.05098039284348488f, 0.8196078538894653f, 0.4000000059604645f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.3921568691730499f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.1450980454683304f, 0.1450980454683304f, 0.1450980454683304f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.0313725508749485f, 0.5176470875740051f, 0.2666666805744171f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.3921568691730499f, 0.1921568661928177f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.2823529541492462f, 0.2823529541492462f, 0.2823529541492462f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.2823529541492462f, 0.2823529541492462f, 0.2823529541492462f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.4431372582912445f, 0.4431372582912445f, 0.4431372582912445f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.2823529541492462f, 0.2823529541492462f, 0.2823529541492462f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.4431372582912445f, 0.4431372582912445f, 0.4431372582912445f, 1.0f);
	style.Colors[ImGuiCol_Tab] = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.09803921729326248f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.1294117718935013f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(1.0f, 0.582548975944519f, 0.2575107216835022f, 0.9724000096321106f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1333333402872086f, 0.2588235437870026f, 0.4235294163227081f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392217636108f, 0.6980392336845398f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6000000238418579f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.3499999940395355f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.8999999761581421f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.05490196123719215f, 0.05098039284348488f, 0.05098039284348488f, 0.5843137502670288f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05490196123719215f, 0.05098039284348488f, 0.05098039284348488f, 0.5843137502670288f);
}

void ColumnWidth() {
	ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
}

void CountFiles(fs::path path, int& count) {
	for (const fs::path entry : fs::directory_iterator(path)) {
		if (fs::is_regular_file(entry))
			count++;
		else if (fs::is_directory(entry))
			CountFiles(entry, count);
	}
}

void CompressFile(Writer& writer, fs::path path) {
	std::ifstream in(path, std::ios::binary);
	std::string temp;
	std::string file = "";
	bool first = true;
	while (std::getline(in, temp)) {
		first ? file += temp : file += ("\n" + temp);
		first = false;
	}
	in.close();
	writer.WriteByteArray((int8_t*)file.c_str(), file.size(), path.filename().string());
}

void CompressDirectory(Writer& writer, fs::path path) {
	if (writer.BeginCompound(path.filename().string())) {
		for (const fs::path& entry : fs::directory_iterator(path)) {
			if (fs::is_regular_file(entry)) {
				CompressFile(writer, entry.string());
				finalized_files++;
			}
			else if (fs::is_directory(entry)) {
				CompressDirectory(writer, entry.string());
			}
		}
		writer.EndCompound();
	}
}

void DecompressFile(Reader& reader, fs::path name, std::string filepath) {
	if (name.extension().string() != ".nbzipvar") {
		std::string newfile = "";
		std::vector<int8_t> arr = reader.ReadByteArray(name.string());
		for (const int8_t byte : arr) {
			newfile += byte;
		}
		std::ofstream out(filepath + "\\" + name.string(), std::ios::binary);
		out << newfile;
		out.close();
		finalized_files++;
	}
}

void DecompressDirectory(Reader& reader, std::string name, std::string filepath) {
	filepath += "\\" + name;
	fs::create_directory(filepath);
	for (StringView entryname : reader.Names()) {

		if (reader.OpenCompound(entryname)) {
			DecompressDirectory(reader, (std::string)entryname, filepath);
			reader.CloseCompound();
		}
		else {
			DecompressFile(reader, entryname, filepath);
		}

	}
}

enum ActionType {
	COMPRESSING, DECOMPRESSING
};

ActionType current_action;

void* StartCompressionThread(void* args) {
	FilePathList list = LoadDroppedFiles();
	char** files = list.paths;
	int count = list.count;
	Writer writer;
	for (int z = 0; z < count; z++) {
		if (fs::is_directory((std::string)files[z]))
			CountFiles((std::string)files[z], file_count);
		else
			file_count++;
	}
	for (int i = 0; i < count; i++) {
		if (fs::is_directory(files[i])) {
			CompressDirectory(writer, (std::string)files[i]);
		}
		else if (fs::is_regular_file(files[i])) {
			finalized_files++;
			CompressFile(writer, (std::string)files[i]);
		}
	}
	writer.WriteInt(file_count, "file_count.nbzipvar");
	writer.Finalize();
	UnloadDroppedFiles(list);
	char path[1024] = { 0 };
	int result = GuiFileDialog(DIALOG_SAVE_FILE, "Export compressed file", path, "*.nbzip", "NbZip file (.nbzip)");
	if (result == 1) {
		writer.ExportBinaryFile((std::string)path);
	}

	file_count = 0;
	finalized_files = 0;
	busy = false;
	return NULL;
}

void* StartDecompressionThread(void* args) {
	FilePathList list = LoadDroppedFiles();
	fs::path file = (std::string)list.paths[0];

	if (file.extension().string() == ".nbzip") {
		char filepath[1024] = { 0 };
		int result = GuiFileDialog(DIALOG_OPEN_DIRECTORY, "Export file contents to folder", filepath, NULL, NULL);
		if (result == 1) {
			Reader reader;
			reader.ImportBinaryFile(file.string());
			file_count = reader.ReadInt("file_count.nbzipvar");

			for (StringView name : reader.Names()) {
				fs::path path = (fs::path)name;
				std::string save_dir = (std::string)filepath;

				if (reader.OpenCompound(name)) {
					DecompressDirectory(reader, path.string(), save_dir);
					reader.CloseCompound();
				}
				else {
					DecompressFile(reader, name, save_dir);
				}

			}

		}
	}

	UnloadDroppedFiles(list);

	file_count = 0;
	finalized_files = 0;
	busy = false;
	return NULL;
}

void main() {

    InitWindow(300, 240, "NbZip");
    rlImGuiSetup(true);
	SetGuiTheme();
	SetTargetFPS(144);

	Image icon = { 0 };
	icon.width = ICON_WIDTH;
	icon.height = ICON_HEIGHT;
	icon.format = ICON_FORMAT;
	icon.data = ICON_DATA;
	icon.mipmaps = 1;

	SetWindowIcon(icon);

    while (!WindowShouldClose()) {
        ClearBackground(WHITE);
        rlImGuiBegin();

		ImGui::SetNextWindowSize({ (float)GetScreenWidth(), (float)GetScreenHeight() }, ImGuiCond_Once);
		ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Once);
        if (ImGui::Begin("main", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize)) {
            
			ImGui::Text("Compress file/folder");
			ImGui::BeginChildFrame(1, { ImGui::GetColumnWidth(), 90 });
			if (ImGui::IsWindowHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Drop items here...");
				ImGui::EndTooltip();
				if (IsFileDropped() && !busy) {
					busy = true;
					current_action = COMPRESSING;
					pthread_create(&work_thread, NULL, StartCompressionThread, NULL);
				}
			}
			ImGui::EndChildFrame();

			ImGui::Spacing();

			ImGui::Text("Decompress file/folder");
			ImGui::BeginChildFrame(2, { ImGui::GetColumnWidth(), 90 });
			if (ImGui::IsWindowHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Drop items here...");
				ImGui::EndTooltip();
				if (IsFileDropped() && !busy) {
					busy = true;
					current_action = DECOMPRESSING;
					pthread_create(&work_thread, NULL, StartDecompressionThread, NULL);
				}
			}
			ImGui::EndChildFrame();

            ImGui::End();
        }

		if (busy) {
			ImGui::SetNextWindowSize({ 270, 55 }, ImGuiCond_Once);
			ImGui::SetNextWindowPos({ (float)(GetScreenWidth() - 270) / 2, (float)(GetScreenHeight() - 55) / 2 }, ImGuiCond_Once);
			if (!ImGui::IsPopupOpen("busy"))
				ImGui::OpenPopup("busy");
			if (ImGui::BeginPopupModal("busy", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize)) {
					std::string info = ((current_action == COMPRESSING) ? "Compressing file " : "Decompressing file ") + std::to_string(finalized_files) + " out of " + std::to_string(file_count);
					if ((file_count == finalized_files) && current_action != DECOMPRESSING)
						info = "Generating archive...";
					ImGui::Text(info.c_str());
					ImGui::SameLine();
					const ImU32 col = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
					const ImU32 bg = ImGui::GetColorU32(ImGuiCol_Button);
					ImGui::Spinner("##spinner", 8, 2, col);
					if (file_count > 0 && finalized_files > 0) {
						float value = (float(finalized_files) / float(file_count));
						ImGui::BufferingBar("##buffer_bar", value, ImVec2(255, 6), bg, col);
					}
					else
						ImGui::BufferingBar("##buffer_bar", 0, ImVec2(255, 6), bg, col);
				ImGui::End();
			}
		}

        rlImGuiEnd();
        BeginDrawing();
        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();

}
