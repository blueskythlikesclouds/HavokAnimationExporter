#pragma once

#include <string>

#include <fbxsdk.h>

#include <Common/Base/keycode.cxx>

#if _2010 || _2012
#include <Common/Base/Config/hkProductFeatures.cxx>
#elif _550
#define HK_CLASSES_FILE <Common/Serialize/Classlist/hkCompleteClasses.h>
#include <Common/Serialize/Util/hkBuiltinTypeRegistry.cxx>

#define HK_COMPAT_FILE <Common/Compat/hkCompatVersions.h>
#include <Common/Compat/hkCompat_All.cxx>
#endif

#include <Common/Base/hkBase.h>

#if _2010 || _2012
#include <Common/Base/Memory/Allocator/Malloc/hkMallocAllocator.h>
#include <Common/Base/Memory/System/hkMemorySystem.h>
#include <Common/Base/Memory/System/Util/hkMemoryInitUtil.h>
#elif _550
#include <Common/Base/Memory/Memory/FreeList/hkLargeBlockAllocator.h>
#include <Common/Base/Memory/Memory/FreeList/hkFreeListMemory.h>
#include <Common/Base/Memory/Memory/FreeList/SystemMemoryBlockServer/hkSystemMemoryBlockServer.h>
#include <Common/Base/Memory/Memory/Pool/hkPoolMemory.h>
#include <Common/Base/Memory/MemoryClasses/hkMemoryClassDefinitions.h>
#endif

#include <Common/Base/Monitor/hkMonitorStream.h>
#include <Common/Base/Reflection/hkClass.h>
#include <Common/Base/Reflection/Registry/hkTypeInfoRegistry.h>
#include <Common/Base/System/Error/hkDefaultError.h>
#include <Common/Base/System/hkBaseSystem.h>
#include <Common/Base/System/Io/IStream/hkIStream.h>
#include <Common/SceneData/Graph/hkxNode.h>
#include <Common/SceneData/Scene/hkxScene.h>

#ifdef _550
#include <Common/Serialize/Packfile/Binary/hkBinaryPackfileReader.h>
#endif

#include <Common/Serialize/Packfile/Binary/hkBinaryPackfileWriter.h>
#include <Common/Serialize/Util/hkRootLevelContainer.h>
#include <Common/Serialize/Util/hkStructureLayout.h>
#include <Animation/Animation/hkaAnimation.h>
#include <Animation/Animation/hkaAnimationContainer.h>
#include <Animation/Animation/Animation/hkaAnimationBinding.h>
#include <Animation/Animation/Rig/hkaSkeleton.h>
#include <Animation/Animation/Rig/hkaSkeletonUtils.h>

#if _2010 || _2012
#include <Common/Serialize/Util/hkSerializeUtil.h>
#include <Animation/Animation/Animation/SplineCompressed/hkaSplineCompressedAnimation.h>

typedef hkaAnimation Animation;
typedef hkaInterleavedUncompressedAnimation InterleavedUncompressedAnimation;
typedef hkaSplineCompressedAnimation SplineCompressedAnimation;

#define HK_REF_PTR(x) hkRefPtr<x>
#elif _550
#include <Animation/Animation/Animation/SplineCompressed/hkaSplineSkeletalAnimation.h>

typedef hkaSkeletalAnimation Animation;
typedef hkaInterleavedSkeletalAnimation InterleavedUncompressedAnimation;
typedef hkaSplineSkeletalAnimation SplineCompressedAnimation;

#define HK_REF_PTR(x) x*
#endif

#define HK_FEATURE_REFLECTION_ANIMATION
#define HK_EXCLUDE_FEATURE_MemoryTracker
#define HK_EXCLUDE_FEATURE_RegisterVersionPatches 

#define FATAL_ERROR(x) \
    { \
        printf("ERROR: %s\n", x); \
        getchar(); \
        return -1; \
    }