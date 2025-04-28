#include "Pch.h"

#ifdef _550

template<typename T>
static void toPtrArray(const hkArray<T>& array, T*& ptr, hkInt32& num)
{
    num = array.getSize();
    ptr = new T[num];

    memcpy(ptr, &array[0], num * sizeof(T));
}

template<typename T>
static void toPtrArray(const hkArray<T>& array, T**& ptr, hkInt32& num)
{
    num = array.getSize();
    ptr = new T*[num];

    for (int i = 0; i < num; i++)
        ptr[i] = new T(array[i]);
}

#endif

static hkVector4 toHavok(const FbxVector4& v)
{
    return hkVector4((hkReal)v[0], (hkReal)v[1], (hkReal)v[2], (hkReal)v[3]);
}

static hkQuaternion toHavok(const FbxQuaternion& v)
{
    return hkQuaternion((hkReal)v[0], (hkReal)v[1], (hkReal)v[2], (hkReal)v[3]);
}

static hkQsTransform toHavok(const FbxAMatrix& v)
{
    return hkQsTransform(toHavok(v.GetT()), toHavok(v.GetQ()), toHavok(v.GetS()));
}

static bool checkIsSkeleton(FbxNode* pNode)
{
    FbxNodeAttribute* lAttribute = pNode->GetNodeAttribute();
    return lAttribute == nullptr || lAttribute->GetAttributeType() == FbxNodeAttribute::eNull || lAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton;
}

static void createBonesRecursively(FbxNode* pNode, int parentIndex,
    hkArray<hkaBone>& bones, hkArray<hkInt16>& parentIndices, hkArray<hkQsTransform>& referencePose, hkArray<bool>& newTags)
{
    if (!checkIsSkeleton(pNode))
        return;

    hkaBone bone;
    bone.m_name = (char*)pNode->GetName();
    bone.m_lockTranslation = false;

    const int index = bones.getSize();

    bones.pushBack(bone);
    parentIndices.pushBack(parentIndex);

    const FbxAMatrix lLocalTransform = pNode->EvaluateLocalTransform();

    referencePose.pushBack(toHavok(lLocalTransform));

    std::string name(pNode->GetName());
    std::transform(name.begin(), name.end(), name.begin(), tolower);
    newTags.pushBack(name.find("@new") != std::string::npos);

    // Replicate Havok Content Tools' case insensitive child ordering.
    std::vector<FbxNode*> nodes;

    for (int i = 0; i < pNode->GetChildCount(); i++)
        nodes.push_back(pNode->GetChild(i));

    std::sort(nodes.begin(), nodes.end(), [](auto lhs, auto rhs) { return _stricmp(lhs->GetName(), rhs->GetName()) < 0; });

    for (auto pNode : nodes)
        createBonesRecursively(pNode, index, bones, parentIndices, referencePose, newTags);
}

static hkaSkeleton* createSkeleton(FbxNode* pNode, const char* name)
{
    hkaSkeleton* skeleton = new hkaSkeleton();
    skeleton->m_name = name;

    hkArray<hkaBone> bones;
    hkArray<hkInt16> parentIndices;
    hkArray<hkQsTransform> referencePose;
    hkArray<bool> newTags;

    createBonesRecursively(pNode, -1, bones, parentIndices, referencePose, newTags);

    // Check for bones with @NEW tag, and push them to the end of the list.
    // This can be used to remain compatible with existing animations in games 
    // that map animations to the skeleton by bone index, instead of bone name.
    hkArray<hkInt16> boneIndices;
    boneIndices.reserve(bones.getSize());

    for (hkInt16 i = 0; i < bones.getSize(); i++)
    {
        if (!newTags[i])
            boneIndices.pushBack(i);
    }  
    for (hkInt16 i = 0; i < bones.getSize(); i++)
    {
        if (newTags[i])
            boneIndices.pushBack(i);
    }

    hkArray<hkaBone> bonesSorted(bones.getSize());
    hkArray<hkInt16> parentIndicesSorted(bones.getSize());
    hkArray<hkQsTransform> referencePoseSorted(bones.getSize());

    for (int i = 0; i < bones.getSize(); i++)
    {
        hkInt16 boneIndex = boneIndices[i];
        bonesSorted[i] = bones[boneIndex];
        referencePoseSorted[i] = referencePose[boneIndex];

        hkInt16 parentIndex = parentIndices[boneIndex];
        if (parentIndex >= 0 && parentIndex != boneIndices[parentIndex])
        {
            for (int j = 0; j < boneIndices.getSize(); j++)
            {
                if (parentIndex == boneIndices[j])
                {
                    parentIndex = j;
                    break;
                }
            }
        }

        parentIndicesSorted[i] = parentIndex;
    }

#if _2010 || _2012
    skeleton->m_bones = std::move(bonesSorted);
    skeleton->m_parentIndices = std::move(parentIndicesSorted);
    skeleton->m_referencePose = std::move(referencePoseSorted);

    return !skeleton->m_bones.isEmpty() ? skeleton : nullptr;
#elif _550
    toPtrArray(bonesSorted, skeleton->m_bones, skeleton->m_numBones);
    toPtrArray(parentIndicesSorted, skeleton->m_parentIndices, skeleton->m_numParentIndices);
    toPtrArray(referencePoseSorted, skeleton->m_referencePose, skeleton->m_numReferencePose);

    return !bones.isEmpty() ? skeleton : nullptr;
#endif
}

extern std::vector<unsigned char> endianSwapHKX(const void* data, size_t dataSize);

static hkaSkeleton* loadSkeleton(const char* filePath)
{
#if _2010 || _2012
    hkSerializeUtil::ErrorDetails errorDetails;
    hkResource* resource = hkSerializeUtil::load(filePath, &errorDetails);

    if (resource == nullptr || errorDetails.id != hkSerializeUtil::ErrorDetails::ERRORID_NONE)
        return nullptr;

    hkRootLevelContainer* levelContainer = resource->getContents<hkRootLevelContainer>();
#elif _550
    FILE* file = fopen(filePath, "rb");
    if (file == nullptr)
        return nullptr;

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    std::vector<uint8_t> data(fileSize);
    fread(data.data(), 1, fileSize, file);
    fclose(file);

    hkBinaryPackfileReader* reader = new hkBinaryPackfileReader();

    if (reader->loadFileHeader(hkIstream(data.data(), data.size()).getStreamReader()) != HK_SUCCESS)
        return nullptr;

    if (memcmp(reader->getFileHeader().m_layoutRules, &hkStructureLayout::HostLayoutRules, sizeof(hkStructureLayout::HostLayoutRules)) != 0)
        data = endianSwapHKX(data.data(), data.size());

    if (reader->loadEntireFile(hkIstream(data.data(), data.size()).getStreamReader()) != HK_SUCCESS)
        return nullptr;

    hkRootLevelContainer* levelContainer = (hkRootLevelContainer*)reader->getContents("hkRootLevelContainer");
#endif

    if (levelContainer == nullptr)
        return nullptr;

    hkaAnimationContainer* animationContainer = (hkaAnimationContainer*)levelContainer->findObjectByType("hkaAnimationContainer");

    if (animationContainer == nullptr)
        return nullptr;

#if _2010 || _2012
    if (animationContainer->m_skeletons.isEmpty())
        return nullptr;
#elif _550
    if (animationContainer->m_numSkeletons == 0)
        return nullptr;
#endif

    return animationContainer->m_skeletons[0];
}

static void havokErrorReportFunction(const char*, void*)
{
}

static hkaAnimationBinding* createAnimationAndBinding(FbxScene* pScene, hkaSkeleton* skeleton, const char* originalSkeletonName, bool compress, double fps)
{
    FbxAnimStack* pAnimStack = pScene->GetCurrentAnimationStack();

    if (pAnimStack == nullptr)
        return nullptr;

    const FbxTimeSpan lTimeSpan = pAnimStack->GetLocalTimeSpan();
    const FbxTime lDuration = lTimeSpan.GetDuration();

    // Align duration to target FPS to prevent flickering with spline compressed animations.
    double lSecondDouble = lDuration.GetSecondDouble();
    const FbxLongLong lFrameCount = std::max<FbxLongLong>(1, (FbxLongLong)round(lSecondDouble * fps)) + 1;
    lSecondDouble = static_cast<double>(lFrameCount - 1) / fps;

    InterleavedUncompressedAnimation* animation = new InterleavedUncompressedAnimation();
    animation->m_duration = (hkReal)lSecondDouble;

    hkArray<FbxNode*> nodes;
    hkArray<hkaAnnotationTrack> annotationTracks;

#if _2010 || _2012
    for (int i = 0; i < skeleton->m_bones.getSize(); i++)
    {
        const hkaBone& bone = skeleton->m_bones[i];

        FbxNode* lNode = pScene->FindNodeByName(bone.m_name.cString());

        hkaAnnotationTrack track;
        track.m_trackName = bone.m_name;

#elif _550
    for (int i = 0; i < skeleton->m_numBones; i++)
    {
        hkaBone* bone = skeleton->m_bones[i];

        FbxNode* lNode = pScene->FindNodeByName(bone->m_name);

        hkaAnnotationTrack track {};
        track.m_name = bone->m_name;
#endif

        annotationTracks.pushBack(track);
        nodes.pushBack(lNode);
    }

    if (nodes.isEmpty())
        return nullptr;

    hkaAnimationBinding* animationBinding = new hkaAnimationBinding();

    hkArray<hkInt16> transformTrackToBoneIndices;

    for (int i = 0; i < annotationTracks.getSize(); i++)
    {
        const hkaAnnotationTrack& track = annotationTracks[i];

        int index = -1;

#if _2010 || _2012
        for (int j = 0; j < skeleton->m_bones.getSize(); j++)
        {
            if (track.m_trackName != skeleton->m_bones[j].m_name)
                continue;

            index = j;
            break;
        }

#elif _550
        for (int j = 0; j < skeleton->m_numBones; j++)
        {
            if (strcmp(track.m_name, skeleton->m_bones[j]->m_name) != 0)
                continue;

            index = j;
            break;
        }
#endif

        transformTrackToBoneIndices.pushBack((hkInt16)index);
    }

#if _2010 || _2012
    animationBinding->m_transformTrackToBoneIndices = std::move(transformTrackToBoneIndices);
#elif _550
    toPtrArray(transformTrackToBoneIndices, animationBinding->m_transformTrackToBoneIndices, animationBinding->m_numTransformTrackToBoneIndices);
#endif

#if _2010 || _2012
    animation->m_annotationTracks = std::move(annotationTracks);
#elif _550
    toPtrArray(annotationTracks, animation->m_annotationTracks, animation->m_numAnnotationTracks);
#endif

    animation->m_numberOfTransformTracks = nodes.getSize();

    hkArray<hkQsTransform> localTransforms(nodes.getSize() * (int)lFrameCount);
    hkArray<hkQsTransform> modelTransforms(nodes.getSize());

    hkaSkeletonUtils::transformLocalPoseToModelPose(nodes.getSize(), &skeleton->m_parentIndices[0], &skeleton->m_referencePose[0], &modelTransforms[0]);

    for (FbxLongLong i = 0; i < lFrameCount; i++)
    {
        const FbxTime lTime = lTimeSpan.GetStart() + FbxTimeSeconds((double)i / (double)(lFrameCount - 1) * lSecondDouble);

        for (int j = 0; j < nodes.getSize(); j++)
        {
            if (nodes[j] != nullptr)
                modelTransforms[j] = toHavok(nodes[j]->EvaluateGlobalTransform(lTime));
        }

        hkaSkeletonUtils::transformModelPoseToLocalPose(nodes.getSize(), &skeleton->m_parentIndices[0], &modelTransforms[0], &localTransforms[(int)i * nodes.getSize()]);
    }

#if _2010 || _2012
    animation->m_transforms = std::move(localTransforms);
#elif _550
    toPtrArray(localTransforms, animation->m_transforms, animation->m_numTransforms);
#endif

#if _2010 || _2012
    animationBinding->m_originalSkeletonName = originalSkeletonName;
#endif

    SplineCompressedAnimation::TrackCompressionParams params;
    params.m_rotationTolerance = 0.00001f; // Default value makes it very lossy, so set it to a lower value.

    animationBinding->m_animation = compress ? new SplineCompressedAnimation(*animation,
        params, SplineCompressedAnimation::AnimationCompressionParams()) : (Animation*)animation;

    return animationBinding;
}

static std::string getFileNameWithoutExtension(std::string filePath)
{
    size_t index = filePath.find_last_of("\\/");
    if (index != std::string::npos)
        filePath = filePath.substr(index + 1);

    index = filePath.find('.');
    if (index != std::string::npos)
        filePath = filePath.substr(0, index);

    return filePath;
}

static std::string getDirectoryName(const std::string& filePath)
{
    return filePath.substr(0, filePath.find_last_of("\\/") + 1);
}

static void savePackfile(const char* dstFilePath, hkRootLevelContainer* levelContainer, const hkStructureLayout& layout)
{
    hkOstream stream(dstFilePath);

    hkBinaryPackfileWriter* writer = new hkBinaryPackfileWriter();
    writer->setContents(levelContainer, hkRootLevelContainerClass);

    hkBinaryPackfileWriter::Options options = {};
    options.m_layout = layout;

    writer->save(stream.getStreamWriter(), options);
}

int main(int argc, const char** argv)
{
    std::string srcFileName;
    std::string dstFileName;
    std::string sklFileName;

    hkStructureLayout layout =
#ifdef _550
        hkStructureLayout::Xbox360LayoutRules;
#else
        hkStructureLayout::MsvcWin32LayoutRules;
#endif

    bool compress = true;
    double fps = 60.0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-s") == 0 || 
            strcmp(argv[i], "--skl") == 0)
        {
            if (i < argc - 1)
                sklFileName = argv[++i];
        }

        else if (strcmp(argv[i], "-u") == 0 ||
            strcmp(argv[i], "--uncompressed") == 0)
        {
            compress = false;
        }

        else if (strcmp(argv[i], "-f") == 0 ||
            strcmp(argv[i], "--fps") == 0)
        {
            fps = atof(argv[++i]);
        }

#ifdef _550
        else if (strcmp(argv[i], "-w") == 0 ||
            strcmp(argv[i], "--windows") == 0)
        {
            layout = hkStructureLayout::MsvcWin32LayoutRules;
        }
#endif

        else if (strcmp(argv[i], "-x") == 0 ||
            strcmp(argv[i], "--xbox360") == 0)
        {
            layout = hkStructureLayout::Xbox360LayoutRules;
        }     
        
        else if (strcmp(argv[i], "-p") == 0 ||
            strcmp(argv[i], "--ps3") == 0)
        {
            layout = hkStructureLayout::GccPs3LayoutRules;
        }

#ifdef _2012
        else if (strcmp(argv[i], "-w") == 0 ||
            strcmp(argv[i], "--wiiu") == 0)
        {
            layout = hkStructureLayout::GhsWiiULayoutRules;
        }
#endif

        else if (srcFileName.empty())
            srcFileName = argv[i];

        else if (dstFileName.empty())
            dstFileName = argv[i];
    }

    if (srcFileName.empty())
    {
        printf("ERROR: Insufficient amount of arguments were given.\n\n");
        printf("Havok Animation Exporter\n");
        printf(" Usage: [source] [destination] [options]\n\n");
        printf(" Options:\n");
        printf("  -s or --skl:          Path to skeleton HKX file when generating animation data.\n");
        printf("  -u or --uncompressed: Whether animation data is going to be uncompressed.\n");
        printf("  -f or --fps:          Frames per second when generating animation data. 60 by default.\n\n");
#ifdef _550
        printf("  -w or --windows:      Convert for Windows.\n");
#endif
        printf("  -x or --xbox360:      Convert for Xbox 360.\n");
        printf("  -p or --ps3:          Convert for PS3.\n");
#ifdef _2012
        printf("  -w or --wiiu:         Convert for Wii U.\n");
#endif
        printf("If no skeleton path is specified, a skeleton HKX file is going to be created from input.\n");
        printf("If no destination path is specified, it's going to be automatically assumed from input.\n");
        printf("If no output platform is specified, it's going to be exported for "
#ifdef _550
            "Xbox 360"
#else
            "Windows"
#endif
            " by default.");

        getchar();
        return 0;
    }

    if (dstFileName.empty())
    {
        const std::string directoryName = getDirectoryName(srcFileName);
        const std::string fileName = getFileNameWithoutExtension(srcFileName) + (sklFileName.empty() ? ".skl.hkx" : ".anm.hkx");

        dstFileName = directoryName.empty() ? fileName : directoryName + fileName;
    }

#if _2010 || _2012
    hkMemoryRouter* memoryRouter = hkMemoryInitUtil::initDefault(hkMallocAllocator::m_defaultMallocAllocator, hkMemorySystem::FrameInfo(10 * 1024 * 1024));
    hkBaseSystem::init(memoryRouter, havokErrorReportFunction);
#elif _550
    hkMemoryBlockServer* server = new hkSystemMemoryBlockServer(256 * 1024 * 1024);
    hkMemory* memoryManager = new hkFreeListMemory(server);
    hkThreadMemory* threadMemory = new hkThreadMemory(memoryManager, 16);
    threadMemory->setStackArea(new uint8_t[1024 * 1024], 1024 * 1024);

    hkBaseSystem::init(memoryManager, threadMemory, havokErrorReportFunction);
#endif

    FbxManager* lManager = FbxManager::Create();

    FbxImporter* lImporter = FbxImporter::Create(lManager, "FbxImporter");
    if (!lImporter->Initialize(srcFileName.c_str()))
        FATAL_ERROR("Failed to load FBX file.");

    FbxScene* lScene = FbxScene::Create(lManager, "FbxScene");

    if (!lImporter->Import(lScene))
        FATAL_ERROR("Failed to import FBX file.");

#ifdef _550
    hkaAnimationContainer animationContainer {};
#else
    hkaAnimationContainer animationContainer;
#endif

    hkArray<HK_REF_PTR(Animation)> animations;
    hkArray<HK_REF_PTR(hkaAnimationBinding)> bindings;
    hkArray<HK_REF_PTR(hkaSkeleton)> skeletons;

    if (!sklFileName.empty())
    {
        hkaSkeleton* skeleton = loadSkeleton(sklFileName.c_str());

        if (skeleton == nullptr)
            FATAL_ERROR("Failed to load skeleton file.");

        hkaAnimationBinding* animationBinding = createAnimationAndBinding(lScene, skeleton, getFileNameWithoutExtension(sklFileName).c_str(), compress, fps);

        if (animationBinding == nullptr)
            FATAL_ERROR("Failed to find animation data in FBX file.");

        animations.pushBack(animationBinding->m_animation);
        bindings.pushBack(animationBinding);
    }

    else
    {
        FbxNode* lRootNode = lScene->GetRootNode();

        for (int i = 0; i < lRootNode->GetChildCount(); i++)
        {
            FbxNode* lNode = lRootNode->GetChild(i);

            if (!checkIsSkeleton(lNode))
                continue;

            hkaSkeleton* skeleton = createSkeleton(lNode, getFileNameWithoutExtension(dstFileName).c_str());
            if (skeleton == nullptr)
                continue;
            
            skeletons.pushBack(skeleton);
            break;
        }

        if (skeletons.isEmpty())
            FATAL_ERROR("Failed to find skeleton data in FBX file.");
    }

#if _2010 || _2012
    animationContainer.m_animations = std::move(animations);
    animationContainer.m_bindings = std::move(bindings);
    animationContainer.m_skeletons = std::move(skeletons);
#elif _550
    toPtrArray(animations, animationContainer.m_animations, animationContainer.m_numAnimations);
    toPtrArray(bindings, animationContainer.m_bindings, animationContainer.m_numBindings);
    toPtrArray(skeletons, animationContainer.m_skeletons, animationContainer.m_numSkeletons);
#endif

    hkRootLevelContainer levelContainer;

    hkArray<hkRootLevelContainer::NamedVariant> namedVariants;
    namedVariants.pushBack(hkRootLevelContainer::NamedVariant(sklFileName.empty() ? "Animation Container" : "Merged Animation Container", &animationContainer, &hkaAnimationContainerClass));

#if _2010 || _2012
    levelContainer.m_namedVariants = std::move(namedVariants);
#elif _550
    toPtrArray(namedVariants, levelContainer.m_namedVariants, levelContainer.m_numNamedVariants);
#endif

    savePackfile(dstFileName.c_str(), &levelContainer, layout);

    return 0;
}