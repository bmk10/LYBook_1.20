/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

namespace LyViewPane
{
    static const char* const CategoryTools = "Tools";
    static const char* const CategoryOther = "Other";
    static const char* const CategoryPlugIns = "Plug-Ins";
    static const char* const CategoryAnimation = "LegacyAnimation";
    //----
    static const char* const CategoryEditor = "Editor";
    static const char* const CategoryViewport = "Viewport";
    static const char* const CategoryCloudCanvas = "Cloud Canvas";


    // Putting these names here so that when view panes are opened
    // from other areas of Editor code, they still work when the name changes.
    static const char* const SceneSettings = "Scene Settings (PREVIEW)";
    static const char* const AssetBrowser = "Asset Browser";
    static const char* const AssetEditor = "Asset Editor";
    static const char* const EntityOutliner = "Entity Outliner";
    static const char* const EntityInspector = "Entity Inspector";
    static const char* const EntityInspectorPinned = "Pinned Entity Inspector";
    static const char* const LevelInspector = "Level Inspector";
    static const char* const DeploymentTool = "Deployment Tool";
    static const char* const ProjectSettingsTool = "Project Settings Tool";
    static const char* const ErrorReport = "Error Report";
    static const char* const Console = "Console";
    static const char* const ConsoleMenuName = "&Console";
    static const char* const ConsoleVariables = "Console Variables";
    static const char* const TrackView = "Track View";
    static const char* const ScriptCanvas = "Script Canvas (PREVIEW)";

    static const char* const AIDebugger = "AI Debugger";
    static const char* const EditorSettingsManager = "Editor Settings Manager";
    static const char* const TerrainEditor = "Terrain Editor";
    static const char* const TerrainTool = "Terrain Tool";
    static const char* const TerrainTextureLayers = "Terrain Texture Layers";
    static const char* const MaterialEditor = "Material Editor";
    static const char* const DatabaseView = "Database View";
    static const char* const AudioControlsEditor = "Audio Controls Editor";
    static const char* const SubstanceEditor = "Substance Editor";
    static const char* const VegetationEditor = "Vegetation Editor";
    static const char* const LandscapeCanvas = "Landscape Canvas";
    static const char* const AnimationEditor = "EMotion FX Animation Editor (PREVIEW)";
    static const char* const PhysXConfigurationEditor = "PhysX Configuration (PREVIEW)";
    static const char* const PhysXVehiclesConfigurationEditor = "PhysX Vehicles Configuration (PREVIEW)";

    static const char* const LegacyAssetBrowser = "Asset Browser (LEGACY)";
    static const char* const LegacyFlowGraph = "Flow Graph (LEGACY)";
    static const char* const LegacyLayerEditor = "Layer Editor (LEGACY)";
    static const char* const LegacyObjectSelector = "Object Selector (LEGACY)";
    static const char* const LegacyRollupBar = "RollupBar (LEGACY)";
    static const char* const LegacyRollupBarMenuName = "&RollupBar (LEGACY)";

    static const char* const SliceRelationships = "Slice Relationship View (PREVIEW)";
    static const char* const LegacyGeppetto = "Geppetto Editor (LEGACY)";
    static const char* const LegacyMannequin = "Mannequin Editor (LEGACY)";

    const int NO_BUILTIN_ACTION = -1;
}

