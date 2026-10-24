#pragma once
#include "imgui.h"
#include "chams.h"

inline void DrawChamsMenu() {
    ImGui::SetNextWindowSize(ImVec2(340, 500), ImGuiCond_Once);
    ImGui::SetNextWindowPos(ImVec2(10, 10),   ImGuiCond_Once);
    ImGui::Begin("ChickenGun Mod", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    ImGui::Separator(); ImGui::Text("Chams"); ImGui::Separator();

    int cm = GetChamsMode();
    if (ImGui::Combo("Mode##chams", &cm, kChamsModes, kChamsModeCount))
        SetChamsMode(cm);

    bool isVisMode = (cm == 5 || cm == 6 || cm == 7);

    if (!isVisMode) {
        static float col[3] = {1.f, 1.f, 1.f};
        if (ImGui::ColorEdit3("Color##chams", col))
            { SetR(col[0]); SetG(col[1]); SetB(col[2]); }

        static bool rainbow = false;
        if (ImGui::Checkbox("Rainbow", &rainbow)) SetRainbow(rainbow);
    }

    static float lw = 10.f;
    if (ImGui::SliderFloat("Line Width", &lw, 1.f, 30.f))
        g_lineWidth = lw;

    if (isVisMode) {
        ImGui::Spacing();
        ImGui::Separator(); ImGui::Text("Visibility Colors"); ImGui::Separator();
        ImGui::Spacing();

        static float visCol[3]  = {0.f, 1.f, 0.f};
        static bool  visInited  = false;
        if (!visInited) {
            SetVisibleColor({visCol[0], visCol[1], visCol[2], 1.f});
            visInited = true;
        }

        ImGui::TextColored(ImVec4(0.2f,1.f,0.2f,1.f), "Visible");
        ImGui::SameLine(90);
        if (ImGui::ColorEdit3("##vis", visCol,
                ImGuiColorEditFlags_NoLabel)) {
            SetVisibleColor({visCol[0], visCol[1], visCol[2], 1.f});
        }

        ImGui::Spacing();

        static float wallCol[3] = {1.f, 0.f, 0.f};
        static bool  wallInited = false;
        if (!wallInited) {
            SetWallColor({wallCol[0], wallCol[1], wallCol[2], 1.f});
            wallInited = true;
        }

        ImGui::TextColored(ImVec4(1.f,0.2f,0.2f,1.f), "Walls");
        ImGui::SameLine(90);
        if (ImGui::ColorEdit3("##wall", wallCol,
                ImGuiColorEditFlags_NoLabel)) {
            SetWallColor({wallCol[0], wallCol[1], wallCol[2], 1.f});
        }

        ImGui::Spacing();

        ImGui::Text("Preview:");
        ImGui::SameLine();
        ImGui::ColorButton("##prevVis",
            ImVec4(visCol[0],visCol[1],visCol[2],1),
            ImGuiColorEditFlags_NoTooltip, ImVec2(22,22));
        ImGui::SameLine(); ImGui::Text("Visible");
        ImGui::SameLine(0,16);
        ImGui::ColorButton("##prevWall",
            ImVec4(wallCol[0],wallCol[1],wallCol[2],1),
            ImGuiColorEditFlags_NoTooltip, ImVec2(22,22));
        ImGui::SameLine(); ImGui::Text("Wall");
    }

    ImGui::Spacing();
    ImGui::Separator(); ImGui::Text("Shader Finder"); ImGui::Separator();

    static char shaderBuf[64] = "u_MVPMatrix";
    ImGui::InputText("Uniform##sh", shaderBuf, sizeof(shaderBuf));

    if (ImGui::Button("Apply"))      setShader(shaderBuf);
    ImGui::SameLine();
    if (ImGui::Button("Dump")) {
        g_seenUniforms.clear();
        SetDumpShaders(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))       SetDumpShaders(false);

    ImGui::TextDisabled("Active: %s", getShader());

    ImGui::End();
}
