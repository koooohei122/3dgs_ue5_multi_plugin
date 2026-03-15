// Copyright 2024, Gaussian Splatting Plugin. All Rights Reserved.

#include "Rendering/GSGlobalShaders.h"

// ---------------------------------------------------------------------------
// Shader implementations  (links source file ↔ USF entry points)
// ---------------------------------------------------------------------------

IMPLEMENT_GLOBAL_SHADER(FGSSplatVS,      "/Plugin/GaussianSplatting/GSSplat.usf", "MainVS",          SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FGSSplatPS,      "/Plugin/GaussianSplatting/GSSplat.usf", "MainPS",          SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGSSplatOITVS,   "/Plugin/GaussianSplatting/GSSplat.usf", "MainVS",          SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FGSSplatOITPS,   "/Plugin/GaussianSplatting/GSSplat.usf", "MainPS_OIT",      SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FGSOITResolveVS, "/Plugin/GaussianSplatting/GSSplatOIT.usf", "OITResolveVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FGSOITResolvePS, "/Plugin/GaussianSplatting/GSSplatOIT.usf", "OITResolvePS", SF_Pixel);
