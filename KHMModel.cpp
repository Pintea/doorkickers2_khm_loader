
#include "KHMModel.h"
#include "Kernel/Log.h"

namespace KHM {

//
// sModelDefinition
//

const sObjectBase* sModelDefinition::GetObjectById(const unsigned int uiId) const
{
    if (uiId < (unsigned int)numBones)
    {
        const sObjectBase* obj = &lBones[uiId];
        ASSERT(obj->uiId == uiId);
        return obj;
    }

    if ((uiId - numBones) < (unsigned int)numHelpers)
    {
        const sObjectBase* obj = &lHelpers[uiId - numBones];
        ASSERT(obj->uiId == uiId);
        return obj;
    }

    //if (pMesh && pMesh->uiId == uiId)
    //  return pMesh;

    return NULL;
}

const KHM::sObjectBase* sModelDefinition::GetObjectByName(const char* nodeName) const
{
    // search in helpers first, this is usually what we want
    for (int i = 0; i < numHelpers; ++i)
    {
        if (strcmp(nodeName, lHelpers[i].szName) == 0)
            return &lHelpers[i];
    }

    for (int i = 0; i < numBones; ++i)
    {
        if (strcmp(nodeName, lBones[i].szName) == 0)
            return &lBones[i];
    }

    // look through meshes
    if (pMesh && strcmp(pMesh->szName, nodeName) == 0)
        return pMesh;

    return NULL;
}

//
// CLoader
//

CLoader::CLoader() :
    buff(0),
    buffsize(0),
    buffread(0)
{
}

void CLoader::ReadBytesSkip(int numBytesSkip)
{
    buffread += numBytesSkip;
    //bool result = m_pFile->Seek(numBytesSkip, File::SEEK_POS_CUR);
    //ASSERT(result);
}

unsigned char* CLoader::ReadBytes(int sizeToRead)
{
    buffread += sizeToRead;
    return (buff + buffread - sizeToRead);
}

void CLoader::ReadBytes( void* pDest, int sizeToRead )
{
    pDest = buff + buffread;
    buffread += sizeToRead;
    ASSERT(buffread <= (int)buffsize);

    //unsigned int bytesRead = m_pFile->Read(pDest, sizeToRead);
    //ASSERT(bytesRead == (unsigned int)sizeToRead);
}

void CLoader::ReadText( char* szBuffer, unsigned int uiBufferLen )
{
    unsigned int uiTextLen;
    ReadBytes( &uiTextLen, sizeof( uiTextLen ) );

    memset( szBuffer, 0x00, uiBufferLen * sizeof(char) );
    ASSERT( uiTextLen < uiBufferLen );
    if( uiTextLen < uiBufferLen )
    {
        ReadBytes( szBuffer, uiTextLen );
    }
    else
    {
        ReadBytes( szBuffer, uiBufferLen );
        szBuffer[ uiBufferLen - 1 ] = '\0';

        char tempBuff[1024];
        ReadBytes(tempBuff, uiTextLen - uiBufferLen);
    }
}

void CLoader::ReadSkin( sObjectMesh* pMesh )
{
    const unsigned char hasSkin = *(unsigned char*)ReadBytes(sizeof(hasSkin));
    if (!hasSkin)
        return; // no skin

    pMesh->pSkinWeights = (Vector4*)ReadBytes(sizeof(Vector4) * pMesh->numVertices);
    pMesh->pSkinBoneIndices = (sBoneIndices*)ReadBytes(sizeof(sBoneIndices) * pMesh->numVertices);
}

void CLoader::ReadCollisionData( sObjectMesh* pMesh )
{
    pMesh->numCollisions = *(int*)ReadBytes(sizeof(int));
    if (!pMesh->numCollisions)
        return;

    pMesh->pCollisions = new sCollisionShape[pMesh->numCollisions];
    for (int i = 0; i < pMesh->numCollisions; ++i)
    {
        sCollisionShape& col = pMesh->pCollisions[i];

        col.type = (sCollisionShape::eCollisionType)*(unsigned int*)ReadBytes(sizeof(unsigned int));
        memcpy(&col.transform, ReadBytes(sizeof(float) * 16), sizeof(float) * 16);

        switch (col.type)
        {
            case sCollisionShape::SPHERE:
                col.params.sphere.radius = *(float*)ReadBytes(sizeof(float));
                break;

            case sCollisionShape::BOX:
                memcpy(col.params.box.extents, ReadBytes(sizeof(float) * 3), sizeof(float) * 3);
                break;

            case sCollisionShape::CAPSULE:
                col.params.capsule.radius = *(float*)ReadBytes(sizeof(float));
                col.params.capsule.halfHeight = *(float*)ReadBytes(sizeof(float));
                break;

            case sCollisionShape::CONVEX_MESH:
                col.bShared = true;

                col.params.mesh.numPolys = *(int*)ReadBytes(sizeof(int));
                col.params.mesh.pPolygons = (sCollisionPolygon*)ReadBytes(sizeof(sCollisionPolygon) * col.params.mesh.numPolys);

                col.params.mesh.numIndices = *(int*)ReadBytes(sizeof(int));
                col.params.mesh.pIndices = (unsigned short*)ReadBytes(sizeof(unsigned short) * col.params.mesh.numIndices);

                col.params.mesh.numVertices = *(int*)ReadBytes(sizeof(int));
                col.params.mesh.pVertices = (Vector3*)ReadBytes(sizeof(Vector3) * col.params.mesh.numVertices);
                break;

            default:
                DEBUG_BREAK();
                break;
        }
    }
}

void CLoader::ReadGeometry(sObjectMesh* pMesh)
{
    // read verts
    pMesh->numVertices = *(int*)ReadBytes(sizeof(int));
    pMesh->pVertices = (Vector3*)ReadBytes(sizeof(Vector3) * pMesh->numVertices);

    // read normals
    pMesh->pNormals = (Vector3*)ReadBytes(sizeof(Vector3) * pMesh->numVertices);

    // read triangle indices
    pMesh->numIndices = *(int*)ReadBytes(sizeof(int));
    pMesh->pIndices = (unsigned short*)ReadBytes(sizeof(unsigned short) * pMesh->numIndices);

    // read face normals
    const unsigned int uiNumFaces = pMesh->numIndices / 3;
    pMesh->pFaceNormals = (Vector3*)ReadBytes(sizeof(Vector3) * uiNumFaces);

    // read vtx colors
    unsigned char hasVertColors = *(unsigned char*)ReadBytes(sizeof(hasVertColors));
    if (hasVertColors)
    {
        pMesh->pColors = (unsigned int*)ReadBytes(sizeof(unsigned int) * pMesh->numVertices);
    }

    // read tx coords
    unsigned int uiNumTxCoordMaps = *(unsigned int*)ReadBytes(sizeof(uiNumTxCoordMaps));
    for(unsigned int i(0); i < uiNumTxCoordMaps; ++i)
    {
        if (i == 1)
        {
            ReadBytesSkip(sizeof(Vector2) * pMesh->numVertices);
            //m_pFile->Seek(sizeof(Vector2) * pMesh->numVertices, File::SEEK_POS_CUR);
            continue; // TODO: we don't need these for now,  will see if necessary
        }

        pMesh->pTexCoords[i] = (Vector2*)ReadBytes(sizeof(Vector2) * pMesh->numVertices);
    }

    // read skin
    ReadSkin(pMesh);

    // read collision data
    ReadCollisionData(pMesh);

    // read bounds
    pMesh->min = *(Vector3*)ReadBytes(sizeof(Vector3));
    pMesh->max = *(Vector3*)ReadBytes(sizeof(Vector3));

    // compute volume at load time (TODO: this is exporter's job)
    pMesh->volume = 0.0f;
    for (int i = 0; i < pMesh->numCollisions; ++i)
    {
        const sCollisionShape& col = pMesh->pCollisions[i];
        if (col.type == sCollisionShape::CONVEX_MESH || col.type == sCollisionShape::MESH)
        {
            Vector3 s = pMesh->max - pMesh->min;
            pMesh->volume += (s.x * s.y * s.z);
        }
        else
        {
            pMesh->volume += col.GetVolume();
        }
    }
}

void CLoader::ReadMeshes( sModelDefinition* pModelDefinition )
{
    const unsigned char hasMesh = *ReadBytes(sizeof(hasMesh));
    if (!hasMesh)
        return;

    pModelDefinition->pMesh = new sObjectMesh();

    sObjectBase* pObject = pModelDefinition->pMesh;
    memcpy(pObject, ReadBytes(sizeof(sObjectBase)), sizeof(sObjectBase));

    ASSERT(pObject->uiId < 256);
    if (pObject->uiId >= 256) 
        pObject->uiId = 255;
    if (pObject->uiParentId > 256 && pObject->uiParentId < (unsigned)-1)
    {
        ASSERT(false);
        pObject->uiParentId = 255;
    }

    ReadGeometry(pModelDefinition->pMesh);

    //g_pLog->Write("mesh: %s, id=%d, parentId=%d\n", pObject->szName, pObject->uiId, pObject->uiParentId);
}

void CLoader::ReadBones( sModelDefinition* pModelDefinition )
{
    const unsigned char count = *(unsigned char*)ReadBytes(sizeof(count));
    if (!count)
        return;

    ASSERT(sizeof(sObjectBase) == (KHM_MAX_OBJECT_NAME + 4 + 4 + 64 + 64));
    pModelDefinition->numBones = count;
    pModelDefinition->lBones = (sObjectBase*)ReadBytes(sizeof(sObjectBase) * count);

    //for (int i = 0; i < pModelDefinition->numBones; ++i) {
    //  const KHM::sObjectBase* pBone = &pModelDefinition->lBones[i];
    //  g_pLog->Write("bone: %s, id=%d, parentId=%d\n", pBone->szName, pBone->uiId, pBone->uiParentId);
    //}
}

void CLoader::ReadHelpers( sModelDefinition* pModelDefinition )
{
    const unsigned char count = *(unsigned char*)ReadBytes(sizeof(count));
    if (!count)
        return;

    ASSERT(sizeof(sObjectBase) == (KHM_MAX_OBJECT_NAME + 4 + 4 + 64 + 64));
    pModelDefinition->numHelpers = count;
    pModelDefinition->lHelpers = (sObjectBase*)ReadBytes(sizeof(sObjectBase) * count);

    //for (int i = 0; i < pModelDefinition->numHelpers; ++i) {
    //  const KHM::sObjectBase* pBone = &pModelDefinition->lHelpers[i];
    //  g_pLog->Write("helper: %s, id=%d, parentId=%d\n", pBone->szName, pBone->uiId, pBone->uiParentId);
    //}
}

void CLoader::ReadAnimation( sModelDefinition* pModelDefinition )
{
    const unsigned char hasAnim = *ReadBytes(sizeof(hasAnim));
    if (!hasAnim)
        return;

    sAnimation* pAnimation = new sAnimation();
    pModelDefinition->pAnimation = pAnimation;

    // read data
    const int numNodes = *(int*)ReadBytes(sizeof(int));
    const float startTimeS = *(float*)ReadBytes(sizeof(float)); // this is not 0 when the animation is exported from a clip and not the entire timeline
    //ASSERT(startTimeS == 0.f);
    const float endTimeS = *(float*)ReadBytes(sizeof(float)) - startTimeS; //NOTE: Shift everything so it starts at time 0
    const int numFrames = *(int*)ReadBytes(sizeof(int));

    // alloc space for node animations; node animations = animation track for a object
    pAnimation->numNodes = numNodes;

    //if (strstr(pModelDefinition->sModelName.GetString(), "rpg/reload_aim_fire"))
    //  DEBUG_BREAK();
    //const sNodeAnimation* na = (const sNodeAnimation*)ReadBytes(sizeof(sNodeAnimation) * numNodes);

    // no longer needed (TODO: remove from exporter as well)
    ReadBytesSkip(sizeof(sNodeAnimation) * numNodes);

    pAnimation->numNodeFrames = numFrames;
    pAnimation->frameDurationMs = (endTimeS * 1000.f) / (float)(Max(numFrames - 1, 1));
    pAnimation->pNodeTransforms = (sNodeTransform*)ReadBytes(sizeof(sNodeTransform) * (numFrames * numNodes));

    //for (int i = 0; i < numNodes; ++i) {
    //  const KHM::sNodeAnimation* pNode = &pAnimation->pNodeAnimations[i];
    //  g_pLog->Write("anim: %s, id=%d\n", pNode->szNodeName, pNode->uiNodeId);
    //}
}

void CLoader::ReadAnimationMask( sModelDefinition* pModelDefinition )
{
    const unsigned char hasAnimMask = *ReadBytes(sizeof(hasAnimMask));
    if (!hasAnimMask)
        return;

    sAnimationMask* pMask = new sAnimationMask();
    pModelDefinition->pAnimationMask = pMask;

    const unsigned int numNodes = *(unsigned int*)ReadBytes(sizeof(unsigned int));
    
    pMask->pNodes = (sAnimationMaskEntry*)ReadBytes(sizeof(sAnimationMaskEntry) * numNodes);
    pMask->numNodes = numNodes; 

    //for (int i = 0; i < numNodes; ++i) {
    //  if (pNewAnimationMask->lObjectMask[i].mask)
    //      g_pLog->Write("mask: %s\n", pNewAnimationMask->lObjectMask[i].szObjectName);
    //}
}

bool CLoader::LoadModel(const char* pszFilePath, unsigned char* fileBuff, unsigned int fileSize, sModelDefinition* pModelDefinition)
{
    buff = fileBuff;
    buffsize = fileSize;

    pModelDefinition->Init();

    // read file signature
    sHeader* fileHeader = (sHeader*)ReadBytes(sizeof(sHeader));

    if( fileHeader->uiSig[0] != 'K' ||
        fileHeader->uiSig[1] != 'H' ||
        fileHeader->uiSig[2] != 'M' )
    {
        LOG_ERROR("[Error] CLoader::LoadModel(%s) - KHM header mismatch.\n", pszFilePath);
        return false; // different kind of file
    }

    if( fileHeader->uiVer != KHM_VERSION )
    {
        LOG_ERROR("[Error] CLoader::LoadModel(%s) - wrong file version %u, expected %u\n", pszFilePath, fileHeader->uiVer, KHM_VERSION);
        return false; // version mismatch!
    }

    strcpy(pModelDefinition->szFileName, pszFilePath);

    //
    // read bones
    ReadBones(pModelDefinition);

    //
    // read helpers
    ReadHelpers(pModelDefinition);
    
    //
    // read meshes
    ReadMeshes(pModelDefinition);

    //
    // read animation
    ReadAnimation(pModelDefinition);

    //
    // read animation mask
    ReadAnimationMask(pModelDefinition);

    return true;
}

}; // end namespace KHM
